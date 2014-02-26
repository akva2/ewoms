/*
  Copyright 2014 SINTEF ICT, Applied Mathematics.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef OPM_INITSTATEEQUIL_HEADER_INCLUDED
#define OPM_INITSTATEEQUIL_HEADER_INCLUDED

#include <opm/core/simulator/EquilibrationHelpers.hpp>
#include <opm/core/io/eclipse/EclipseGridParser.hpp>
#include <opm/core/props/BlackoilPropertiesInterface.hpp>
#include <opm/core/props/BlackoilPhases.hpp>
#include <opm/core/utility/RegionMapping.hpp>
#include <opm/core/utility/Units.hpp>

#include <array>
#include <cassert>
#include <utility>
#include <vector>

/**
 * \file
 * Facilities for an ECLIPSE-style equilibration-based
 * initialisation scheme (keyword 'EQUIL').
 */
struct UnstructuredGrid;

namespace Opm
{
    /**
     * Types and routines that collectively implement a basic
     * ECLIPSE-style equilibration-based initialisation scheme.
     *
     * This namespace is intentionally nested to avoid name clashes
     * with other parts of OPM.
     */
    namespace Equil {

        /**
         * Compute initial phase pressures by means of equilibration.
         *
         * This function uses the information contained in an
         * equilibration record (i.e., depths and pressurs) as well as
         * a density calculator and related data to vertically
         * integrate the phase pressure ODE
         * \f[
         * \frac{\mathrm{d}p_{\alpha}}{\mathrm{d}z} =
         * \rho_{\alpha}(z,p_{\alpha})\cdot g
         * \f]
         * in which \f$\rho_{\alpha}$ denotes the fluid density of
         * fluid phase \f$\alpha\f$, \f$p_{\alpha}\f$ is the
         * corresponding phase pressure, \f$z\f$ is the depth and
         * \f$g\f$ is the acceleration due to gravity (assumed
         * directed downwords, in the positive \f$z\f$ direction).
         *
         * \tparam Region Type of an equilibration region information
         *                base.  Typically an instance of the EquilReg
         *                class template.
         *
         * \tparam CellRange Type of cell range that demarcates the
         *                cells pertaining to the current
         *                equilibration region.  Must implement
         *                methods begin() and end() to bound the range
         *                as well as provide an inner type,
         *                const_iterator, to traverse the range.
         *
         * \param[in] G     Grid.
         * \param[in] reg   Current equilibration region.
         * \param[in] cells Range that spans the cells of the current
         *                  equilibration region.
         * \param[in] grav  Acceleration of gravity.
         *
         * \return Phase pressures, one vector for each active phase,
         * of pressure values in each cell in the current
         * equilibration region.
         */
        template <class Region, class CellRange>
        std::vector< std::vector<double> >
        phasePressures(const UnstructuredGrid& G,
                       const Region&           reg,
                       const CellRange&        cells,
                       const double            grav = unit::gravity);



        /**
         * Compute initial phase saturations by means of equilibration.
         *
         * \tparam Region Type of an equilibration region information
         *                base.  Typically an instance of the EquilReg
         *                class template.
         *
         * \tparam CellRange Type of cell range that demarcates the
         *                cells pertaining to the current
         *                equilibration region.  Must implement
         *                methods begin() and end() to bound the range
         *                as well as provide an inner type,
         *                const_iterator, to traverse the range.
         *
         * \param[in] reg             Current equilibration region.
         * \param[in] cells           Range that spans the cells of the current
         *                            equilibration region.
         * \param[in] props           Property object, needed for capillary functions.
         * \param[in] phase_pressures Phase pressures, one vector for each active phase,
         *                            of pressure values in each cell in the current
         *                            equilibration region.
         * \return                    Phase saturations, one vector for each phase, each containing
         *                            one saturation value per cell in the region.
         */
        template <class Region, class CellRange>
        std::vector< std::vector<double> >
        phaseSaturations(const Region&           reg,
                         const CellRange&        cells,
                         const BlackoilPropertiesInterface& props,
                         const std::vector< std::vector<double> >& phase_pressures)
        {
            const double z0   = reg.datum();
            const double zwoc = reg.zwoc ();
            const double zgoc = reg.zgoc ();
            if ((zgoc > z0) || (z0 > zwoc)) {
                OPM_THROW(std::runtime_error, "Cannot initialise: the datum depth must be in the oil zone.");
            }
            if (!reg.phaseUsage().phase_used[BlackoilPhases::Liquid]) {
                OPM_THROW(std::runtime_error, "Cannot initialise: not handling water-gas cases.");
            }

            std::vector< std::vector<double> > phase_saturations = phase_pressures; // Just to get the right size.
            double smin[BlackoilPhases::MaxNumPhases] = { 0.0 };
            double smax[BlackoilPhases::MaxNumPhases] = { 0.0 };

            const bool water = reg.phaseUsage().phase_used[BlackoilPhases::Aqua];
            const bool gas = reg.phaseUsage().phase_used[BlackoilPhases::Vapour];
            const int oilpos = reg.phaseUsage().phase_pos[BlackoilPhases::Liquid];
            const int waterpos = reg.phaseUsage().phase_pos[BlackoilPhases::Aqua];
            const int gaspos = reg.phaseUsage().phase_pos[BlackoilPhases::Vapour];
            std::vector<double>::size_type local_index = 0;
            for (typename CellRange::const_iterator ci = cells.begin(); ci != cells.end(); ++ci, ++local_index) {
                const int cell = *ci;
                props.satRange(1, &cell, smin, smax);
                // Find saturations from pressure differences by
                // inverting capillary pressure functions.
                double sw = 0.0;
                if (water) {
                    const double pcov = phase_pressures[oilpos][local_index] - phase_pressures[waterpos][local_index];
                    sw = satFromPc(props, waterpos, cell, pcov);
                    phase_saturations[waterpos][local_index] = sw;
                }
                double sg = 0.0;
                if (gas) {
                    // Note that pcog is defined to be (pg - po), not (po - pg).
                    const double pcog = phase_pressures[gaspos][local_index] - phase_pressures[oilpos][local_index];
                    const double increasing = true; // pcog(sg) expected to be increasing function
                    sg = satFromPc(props, gaspos, cell, pcog, increasing);
                    phase_saturations[gaspos][local_index] = sg;
                }
                if (gas && water && (sg + sw > 1.0)) {
                    // Overlapping gas-oil and oil-water transition
                    // zones can lead to unphysical saturations when
                    // treated as above. Must recalculate using gas-water
                    // capillary pressure.
                    const double pcgw = phase_pressures[gaspos][local_index] - phase_pressures[waterpos][local_index];
                    sw = satFromSumOfPcs(props, waterpos, gaspos, cell, pcgw);
                    sg = 1.0 - sw;
                    phase_saturations[waterpos][local_index] = sw;
                    phase_saturations[gaspos][local_index] = sg;
                }
                phase_saturations[oilpos][local_index] = 1.0 - sw - sg;
            }
            return phase_saturations;
        }



        namespace DeckDependent {
            inline
            std::vector<EquilRecord>
            getEquil(const EclipseGridParser& deck)
            {
                if (deck.hasField("EQUIL")) {
                    const EQUIL& eql = deck.getEQUIL();

                    typedef std::vector<EquilLine>::size_type sz_t;
                    const sz_t nrec = eql.equil.size();

                    std::vector<EquilRecord> ret;
                    ret.reserve(nrec);
                    for (sz_t r = 0; r < nrec; ++r) {
                        const EquilLine& rec = eql.equil[r];

                        EquilRecord record =
                            {
                                { rec.datum_depth_             ,
                                  rec.datum_depth_pressure_    }
                                ,
                                { rec.water_oil_contact_depth_ ,
                                  rec.oil_water_cap_pressure_  }
                                ,
                                { rec.gas_oil_contact_depth_   ,
                                  rec.gas_oil_cap_pressure_    }
                            };

                        ret.push_back(record);
                    }

                    return ret;
                }
                else {
                    OPM_THROW(std::domain_error,
                              "Deck does not provide equilibration data.");
                }
            }

            inline
            std::vector<int>
            equilnum(const EclipseGridParser& deck,
                     const UnstructuredGrid&  G   )
            {
                std::vector<int> eqlnum;
                if (deck.hasField("EQLNUM")) {
                    eqlnum = deck.getIntegerValue("EQLNUM");
                }
                else {
                    // No explicit equilibration region.
                    // All cells in region zero.
                    eqlnum.assign(G.number_of_cells, 0);
                }

                return eqlnum;
            }

            template <class InputDeck>
            class PhasePressureSaturationComputer;

            template <>
            class PhasePressureSaturationComputer<Opm::EclipseGridParser> {
            public:
                PhasePressureSaturationComputer(const BlackoilPropertiesInterface& props,
                                                const EclipseGridParser&           deck ,
                                                const UnstructuredGrid&            G    ,
                                                const double                       grav = unit::gravity)
                    : pp_(props.numPhases(),
                          std::vector<double>(G.number_of_cells)),
                      sat_(props.numPhases(),
                          std::vector<double>(G.number_of_cells))
                {
                    const std::vector<EquilRecord> rec = getEquil(deck);
                    const RegionMapping<> eqlmap(equilnum(deck, G));

                    calcPressII(eqlmap, rec, props, G, grav);
                    calcSat(eqlmap, rec, props, G, grav);
                }

                typedef std::vector<double> PVal;
                typedef std::vector<PVal>   PPress;

                const PPress& press() const { return pp_; }
                const PPress& saturation() const { return sat_; }

            private:
                typedef DensityCalculator<BlackoilPropertiesInterface> RhoCalc;
                typedef EquilReg<RhoCalc> EqReg;

                PPress pp_;
                PPress sat_;

                template <class RMap>
                void
                calcPressII(const RMap&                             reg  ,
                            const std::vector< EquilRecord >&       rec  ,
                            const Opm::BlackoilPropertiesInterface& props,
                            const UnstructuredGrid&                 G    ,
                            const double grav)
                {
                    typedef Miscibility::NoMixing NoMix;

                    for (typename RMap::RegionId
                             r = 0, nr = reg.numRegions();
                         r < nr; ++r)
                    {
                        const typename RMap::CellRange cells = reg.cells(r);

                        const int repcell = *cells.begin();
                        const RhoCalc calc(props, repcell);

                        const EqReg eqreg(rec[r], calc,
                                          std::make_shared<NoMix>(), std::make_shared<NoMix>(),
                                          props.phaseUsage());

                        const PPress& res = phasePressures(G, eqreg, cells, grav);

                        for (int p = 0, np = props.numPhases(); p < np; ++p) {
                            PVal&                d = pp_[p];
                            PVal::const_iterator s = res[p].begin();
                            for (typename RMap::CellRange::const_iterator
                                     c = cells.begin(),
                                     e = cells.end();
                                 c != e; ++c, ++s)
                            {
                                d[*c] = *s;
                            }
                        }
                    }
                }

                template <class RMap>
                void
                calcSat(const RMap&                             reg  ,
                        const std::vector< EquilRecord >&       rec  ,
                        const Opm::BlackoilPropertiesInterface& props,
                        const UnstructuredGrid&                 G    ,
                        const double grav)
                {
                    typedef Miscibility::NoMixing NoMix;

                    for (typename RMap::RegionId
                             r = 0, nr = reg.numRegions();
                         r < nr; ++r)
                    {
                        const typename RMap::CellRange cells = reg.cells(r);

                        const int repcell = *cells.begin();
                        const RhoCalc calc(props, repcell);

                        const EqReg eqreg(rec[r], calc,
                                          std::make_shared<NoMix>(), std::make_shared<NoMix>(),
                                          props.phaseUsage());

                        const PPress press = phasePressures(G, eqreg, cells, grav);
                        const PPress sat = phaseSaturations(eqreg, cells, props, press);

                        for (int p = 0, np = props.numPhases(); p < np; ++p) {
                            PVal&                d = pp_[p];
                            PVal::const_iterator s = press[p].begin();
                            for (typename RMap::CellRange::const_iterator
                                     c = cells.begin(),
                                     e = cells.end();
                                 c != e; ++c, ++s)
                            {
                                d[*c] = *s;
                            }
                        }
                        for (int p = 0, np = props.numPhases(); p < np; ++p) {
                            PVal&                d = sat_[p];
                            PVal::const_iterator s = sat[p].begin();
                            for (typename RMap::CellRange::const_iterator
                                     c = cells.begin(),
                                     e = cells.end();
                                 c != e; ++c, ++s)
                            {
                                d[*c] = *s;
                            }
                        }
                    }
                }


            };
        } // namespace DeckDependent
    } // namespace Equil
} // namespace Opm

#include <opm/core/simulator/initStateEquil_impl.hpp>

#endif // OPM_INITSTATEEQUIL_HEADER_INCLUDED
