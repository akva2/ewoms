// $Id$
/*****************************************************************************
 *   Copyright (C) 2009 by Markus Wolff                                      *
 *   Institute of Hydraulic Engineering                                      *
 *   University of Stuttgart, Germany                                        *
 *   email: <givenname>.<name>@iws.uni-stuttgart.de                          *
 *                                                                           *
 *   This program is free software; you can redistribute it and/or modify    *
 *   it under the terms of the GNU General Public License as published by    *
 *   the Free Software Foundation; either version 2 of the License, or       *
 *   (at your option) any later version, as long as this copyright notice    *
 *   is included in its original form.                                       *
 *                                                                           *
 *   This program is distributed WITHOUT ANY WARRANTY.                       *
 *****************************************************************************/
#ifndef DUNE_FVVELOCITY2P_HH
#define DUNE_FVVELOCITY2P_HH

/**
 * @file
 * @brief  Finite Volume Diffusion Model
 * @author Markus Wolff
 */

#include <dumux/new_decoupled/2p/diffusion/fv/fvpressure2p.hh>

namespace Dune
{
//! \ingroup diffusion
//! Finite Volume Diffusion Model
/*! Calculates non-wetting phase velocities from a known pressure field in context of a Finite Volume implementation for the evaluation
 * of equations of the form
 * \f[\text{div}\, \boldsymbol{v}_{total} = q.\f]
 * The wetting or the non-wetting phase pressure has to be given as piecewise constant cell values.
 * The velocity is calculated following  Darcy's law as
 * \f[\boldsymbol{v}_n = \lambda_n \boldsymbol{K} \left(\text{grad}\, p_n + \rho_n g  \text{grad}\, z\right),\f]
 * where, \f$p_n\f$ denotes the wetting phase pressure, \f$\boldsymbol{K}\f$ the absolute permeability, \f$\lambda_n\f$ the non-wetting phase mobility, \f$\rho_n\f$ the non-wetting phase density and \f$g\f$ the gravity constant.
 * As in the two-phase pressure equation a total flux depending on a total velocity is considered one has to be careful at neumann flux boundaries. Here, a phase velocity is only uniquely defined, if
 * the saturation is at the maximum (\f$1-S_{rw}\f$, \f$\boldsymbol{v}_{total} = \boldsymbol{v}_n\f$) or at the minimum (\f$ S_{rn} \f$, \f$\boldsymbol{v}_n = 0\f$)
 *
 * Template parameters are:
 *
 - GridView      a DUNE gridview type
 - Scalar        type used for scalar quantities
 - VC            type of a class containing different variables of the model
 - Problem       class defining the physical problem
 */

template<class TypeTag>
class FVVelocity2P: public FVPressure2P<TypeTag>
{
    typedef  FVVelocity2P<TypeTag> ThisType;
    typedef typename GET_PROP_TYPE(TypeTag, PTAG(GridView)) GridView;
     typedef typename GET_PROP_TYPE(TypeTag, PTAG(Scalar)) Scalar;
     typedef typename GET_PROP_TYPE(TypeTag, PTAG(Problem)) Problem;
     typedef typename GET_PROP(TypeTag, PTAG(ReferenceElements)) ReferenceElements;
     typedef typename ReferenceElements::Container ReferenceElementContainer;
     typedef typename ReferenceElements::ContainerFaces ReferenceElementFaceContainer;


typedef    typename GridView::Traits::template Codim<0>::Entity Element;
    typedef typename GridView::Grid Grid;
    typedef typename GridView::IndexSet IndexSet;
    typedef typename GridView::template Codim<0>::Iterator ElementIterator;
    typedef typename GridView::IntersectionIterator IntersectionIterator;
    typedef typename Grid::template Codim<0>::EntityPointer ElementPointer;

    enum
    {
        dim = GridView::dimension, dimWorld = GridView::dimensionworld
    };
    enum
    {
        pw = TwoPCommonIndices::pressureW,
        pn = TwoPCommonIndices::pressureNW,
        pglobal = TwoPCommonIndices::pressureGlobal,
        vw = TwoPCommonIndices::velocityW,
        vn = TwoPCommonIndices::velocityNW,
        vt = TwoPCommonIndices::velocityTotal,
        Sw = TwoPCommonIndices::saturationW,
        Sn = TwoPCommonIndices::saturationNW,
        other = 999
    };
    enum
    {
        wetting = TwoPCommonIndices::wPhase, nonwetting = TwoPCommonIndices::nPhase
    };

    typedef Dune::FieldVector<Scalar,dim> LocalPosition;
    typedef Dune::FieldVector<Scalar,dimWorld> GlobalPosition;
    typedef Dune::FieldMatrix<Scalar,dim,dim> FieldMatrix;

public:
    //! Constructs a FVNonWettingPhaseVelocity2P object
    /**
     * \param gridView gridView object of type GridView
     * \param problem a problem class object
     * \param pressureType a string giving the type of pressure used (could be: pw, pn, pglobal)
     * \param satType a string giving the type of saturation used (could be: Sw, Sn)
     */
    FVVelocity2P(Problem& problem)
    : FVPressure2P<TypeTag>(problem)
    {
        if (GET_PROP_VALUE(TypeTag, PTAG(EnableCompressibility)) && velocityType_ == vt)
        {
            DUNE_THROW(NotImplemented, "Total velocity - global pressure - model cannot be used with compressible fluids!");
        }
        if (velocityType_ == other)
        {
            DUNE_THROW(NotImplemented, "Velocity type not supported!");
        }
    }
    //! Constructs a FVNonWettingPhaseVelocity2P object
    /**
     * \param gridView gridView object of type GridView
     * \param problem a problem class object
     * \param pressureType a string giving the type of pressure used (could be: pw, pn, pglobal)
     * \param satType a string giving the type of saturation used (could be: Sw, Sn)
     * \param solverName a string giving the type of solver used (could be: CG, BiCGSTAB, Loop)
     * \param preconditionerName a string giving the type of the matrix preconditioner used (could be: SeqILU0, SeqPardiso)
     */
    FVVelocity2P(Problem& problem, std::string solverName,
            std::string preconditionerName)
    : FVPressure2P<TypeTag>(problem, solverName, preconditionerName)
    {
        if (GET_PROP_VALUE(TypeTag, PTAG(EnableCompressibility)) && velocityType_ == vt)
        {
            DUNE_THROW(NotImplemented, "Total velocity - global pressure - model cannot be used with compressible fluids!");
        }
        if (velocityType_ == other)
        {
            DUNE_THROW(NotImplemented, "Velocity type not supported!");
        }
    }

    //! Calculate the velocity.
    /*!
     *  \param t time
     *
     *
     *  Given the piecewise constant pressure \f$p\f$,
     *  this method calculates the velocity
     *  The method is needed in the IMPES (Implicit Pressure Explicit Saturation) algorithm which is used for a fractional flow formulation
     *  to provide the velocity field required for the solution of the saturation equation.
     */
    void calculateVelocity(const Scalar t);

private:
    static const int velocityType_ = GET_PROP_VALUE(TypeTag, PTAG(VelocityFormulation)); //!< gives kind of velocity used (\f$ 0 = v_w\f$, \f$ 1 = v_n\f$, \f$ 2 = v_t\f$)
};
template<class TypeTag>
void FVVelocity2P<TypeTag>::calculateVelocity(const Scalar t=0)
{
    // compute update vector
    ElementIterator eItEnd = this->problem().gridView().template end<0>();
    for (ElementIterator eIt = this->problem().gridView().template begin<0>(); eIt != eItEnd; ++eIt)
    {
        // cell geometry type
        Dune::GeometryType gt = eIt->geometry().type();

        // cell center in reference element
        const LocalPosition
        &localPos = ReferenceElementContainer::general(gt).position(0, 0);

        //
        GlobalPosition globalPos = eIt->geometry().global(localPos);

        // cell index
        int globalIdxI = this->problem().variables().index(*eIt);

        Scalar pressI = this->problem().variables().pressure()[globalIdxI];
        Scalar pcI = this->problem().variables().capillaryPressure(globalIdxI);
        Scalar lambdaWI = this->problem().variables().mobilityWetting(globalIdxI);
        Scalar lambdaNWI = this->problem().variables().mobilityNonwetting(globalIdxI);
        Scalar fractionalWI = this->problem().variables().fracFlowFuncWetting(globalIdxI);
        Scalar fractionalNWI = this->problem().variables().fracFlowFuncNonwetting(globalIdxI);
        Scalar densityWI = this->problem().variables().densityWetting(globalIdxI);
        Scalar densityNWI = this->problem().variables().densityNonwetting(globalIdxI);

        // run through all intersections with neighbors and boundary
        int isIndex = -1;
        IntersectionIterator
        isItEnd = this->problem().gridView().template iend(*eIt);
        for (IntersectionIterator
                isIt = this->problem().gridView().template ibegin(*eIt); isIt
                !=isItEnd; ++isIt)
        {
            // local number of facet
            //int isIndex = isIt->indexInInside();
            isIndex++;
            
            // get geometry type of face
            Dune::GeometryType faceGT = isIt->geometryInInside().type();

            // center in face's reference element
            const Dune::FieldVector<Scalar,dim-1>&
            faceLocal = ReferenceElementFaceContainer::general(faceGT).position(0,0);

            // center of face inside volume reference element
            const LocalPosition localPosFace(0);

            Dune::FieldVector<Scalar,dimWorld> unitOuterNormal = isIt->unitOuterNormal(faceLocal);

            // get absolute permeability
            FieldMatrix permeabilityI(this->problem().soil().K(globalPos, *eIt, localPos));

            // handle interior face
            if (isIt->neighbor())
            {
                // access neighbor
                ElementPointer neighborPointer = isIt->outside();
                int globalIdxJ = this->problem().variables().index(*neighborPointer);

                // compute factor in neighbor
                Dune::GeometryType neighborGT = neighborPointer->geometry().type();
                const LocalPosition&
                localPosNeighbor = ReferenceElementContainer::general(neighborGT).position(0,0);

                // cell center in global coordinates
                const GlobalPosition& globalPos = eIt->geometry().global(localPos);

                // neighbor cell center in global coordinates
                const GlobalPosition& globalPosNeighbor = neighborPointer->geometry().global(localPosNeighbor);

                // distance vector between barycenters
                Dune::FieldVector<Scalar,dimWorld> distVec = globalPosNeighbor - globalPos;

                // compute distance between cell centers
                Scalar dist = distVec.two_norm();

                FieldVector<Scalar, dimWorld> unitDistVec(distVec);
                unitDistVec /= dist;

                // get absolute permeability
                FieldMatrix permeabilityJ(this->problem().soil().K(globalPosNeighbor, *neighborPointer, localPosNeighbor));

                // compute vectorized permeabilities
                FieldMatrix meanPermeability(0);

                // harmonic mean of permeability
                for (int x = 0;x<dim;x++)
                {
                    for (int y = 0; y < dim;y++)
                    {
                        if (permeabilityI[x][y] && permeabilityJ[x][y])
                        {
                            meanPermeability[x][y]= 2*permeabilityI[x][y]*permeabilityJ[x][y]/(permeabilityI[x][y]+permeabilityJ[x][y]);
                        }
                    }
                }

                FieldVector<Scalar,dim> permeability(0);
                meanPermeability.mv(unitDistVec,permeability);

                Scalar pressJ = this->problem().variables().pressure()[globalIdxJ];
                Scalar pcJ = this->problem().variables().capillaryPressure(globalIdxJ);
                Scalar lambdaWJ = this->problem().variables().mobilityWetting(globalIdxJ);
                Scalar lambdaNWJ = this->problem().variables().mobilityNonwetting(globalIdxJ);
                Scalar fractionalWJ = this->problem().variables().fracFlowFuncWetting(globalIdxJ);
                Scalar fractionalNWJ = this->problem().variables().fracFlowFuncNonwetting(globalIdxJ);
                Scalar densityWJ = this->problem().variables().densityWetting(globalIdxJ);
                Scalar densityNWJ = this->problem().variables().densityNonwetting(globalIdxJ);

                //calculate potential gradients
                Scalar potentialW = 0;
                Scalar potentialNW = 0;

                potentialW = this->problem().variables().potentialWetting(globalIdxI, isIndex);
                potentialNW = this->problem().variables().potentialNonwetting(globalIdxI, isIndex);

                Scalar densityW = (potentialW> 0.) ? densityWI : densityWJ;
                Scalar densityNW = (potentialNW> 0.) ? densityNWI : densityNWJ;

                densityW = (potentialW == 0.) ? 0.5 * (densityWI + densityWJ) : densityW;
                densityNW = (potentialNW == 0.) ? 0.5 * (densityNWI + densityNWJ) : densityNW;

                if (this->pressureType == pw)
                {
                    potentialW = (pressI - pressJ) / dist;
                    potentialNW = (pressI - pressJ+ pcI - pcJ) / dist;
                }
                if (this->pressureType == pn)
                {
                    potentialW = (pressI - pressJ - pcI + pcJ) / dist;
                    potentialNW = (pressI - pressJ) / dist;
                }
                if (this->pressureType == pglobal)
                {
                    potentialW = (pressI - pressJ - 0.5 * (fractionalNWI+fractionalNWJ)*(pcI - pcJ)) / dist;
                    potentialNW = (pressI - pressJ + 0.5 * (fractionalWI+fractionalWJ)*(pcI - pcJ)) / dist;
                }

                potentialW += densityW * (unitDistVec * this->gravity);//delta z/delta x in unitDistVec[z]
                potentialNW += densityNW * (unitDistVec * this->gravity);

                //store potentials for further calculations (velocity, saturation, ...)
                this->problem().variables().potentialWetting(globalIdxI, isIndex) = potentialW;
                this->problem().variables().potentialNonwetting(globalIdxI, isIndex) = potentialNW;

                //do the upwinding of the mobility depending on the phase potentials
                Scalar lambdaW = (potentialW > 0.) ? lambdaWI : lambdaWJ;
                lambdaW = (potentialW == 0) ? 0.5 * (lambdaWI + lambdaWJ) : lambdaW;
                Scalar lambdaNW = (potentialNW > 0.) ? lambdaNWI : lambdaNWJ;
                lambdaNW = (potentialNW == 0) ? 0.5 * (lambdaNWI + lambdaNWJ) : lambdaNW;
                densityW = (potentialW> 0.) ? densityWI : densityWJ;
                densityW = (potentialW == 0.) ? 0.5 * (densityWI + densityWJ) : densityW;
                densityNW = (potentialNW> 0.) ? densityNWI : densityNWJ;
                densityNW = (potentialNW == 0.) ? 0.5 * (densityNWI + densityNWJ) : densityNW;

                //calculate the gravity term
                FieldVector<Scalar,dimWorld> velocityW(permeability);
                FieldVector<Scalar,dimWorld> velocityNW(permeability);
                FieldVector<Scalar,dimWorld> gravityTermW(unitDistVec);
                FieldVector<Scalar,dimWorld> gravityTermNW(unitDistVec);

                gravityTermW *= (this->gravity*permeability)*(lambdaW * densityW);
                gravityTermNW *= (this->gravity*permeability)*(lambdaNW * densityNW);

                //calculate velocity depending on the pressure used -> use pc = pn - pw
                if (this->pressureType == pw)
                {
                    velocityW *= lambdaW * (pressI - pressJ)/dist;
                    velocityNW *= lambdaNW * (pressI - pressJ)/dist + 0.5 * (lambdaNWI + lambdaNWJ) * (pcI - pcJ) / dist;
                    velocityW += gravityTermW;
                    velocityNW += gravityTermNW;
                }
                if (this->pressureType == pn)
                {
                    velocityW *= lambdaW * (pressI - pressJ)/dist - 0.5 * (lambdaWI + lambdaWJ) * (pcI - pcJ) / dist;
                    velocityNW *= lambdaNW * (pressI - pressJ) / dist;
                    velocityW += gravityTermW;
                    velocityNW += gravityTermNW;
                }
                if (this->pressureType == pglobal)
                {
                    this->problem().variables().velocity()[globalIdxI][isIndex] = permeability;
                    this->problem().variables().velocity()[globalIdxI][isIndex]*= (lambdaW + lambdaNW)* (pressI - pressJ )/ dist;
                    this->problem().variables().velocity()[globalIdxI][isIndex] += gravityTermW;
                    this->problem().variables().velocity()[globalIdxI][isIndex] += gravityTermNW;
                }

                //store velocities
                if (velocityType_ == vw)
                {
                    this->problem().variables().velocity()[globalIdxI][isIndex] = velocityW;
                    this->problem().variables().velocitySecondPhase()[globalIdxI][isIndex] = velocityNW;
                }
                if (velocityType_ == vn)
                {
                    this->problem().variables().velocity()[globalIdxI][isIndex] = velocityNW;
                    this->problem().variables().velocitySecondPhase()[globalIdxI][isIndex] = velocityW;
                }
                if (velocityType_ == vt)
                {
                    if (this->pressureType == pw || this->pressureType == pn)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = velocityW + velocityNW;
                    }
                }
            }//end intersection with neighbor

            // handle boundary face
            if (isIt->boundary())
            {
                // center of face in global coordinates
                GlobalPosition globalPosFace = isIt->geometry().global(faceLocal);

                //get boundary type
                BoundaryConditions::Flags bcTypeSat = this->problem().bctypeSat(globalPosFace, *isIt);
                BoundaryConditions::Flags bcTypePress = this->problem().bctypePress(globalPosFace, *isIt);

                // cell center in global coordinates
                GlobalPosition globalPos = eIt->geometry().global(localPos);

                // distance vector between barycenters
                Dune::FieldVector<Scalar,dimWorld> distVec = globalPosFace - globalPos;

                // compute distance between cell centers
                Scalar dist = distVec.two_norm();

                FieldVector<Scalar, dimWorld> unitDistVec(distVec);
                unitDistVec /= dist;

                //multiply with normal vector at the boundary
                FieldVector<Scalar,dim> permeability(0);
                permeabilityI.mv(unitDistVec, permeability);

                Scalar satBound = 0;
                if (bcTypeSat == BoundaryConditions::dirichlet)
                {
                    satBound = this->problem().dirichletSat(globalPosFace, *isIt);
                }
                else
                {
                    satBound = this->problem().variables().saturation()[globalIdxI];
                }

                if (bcTypePress == BoundaryConditions::dirichlet)
                {
                    //determine phase saturations from primary saturation variable
                    Scalar satW;
                    Scalar satNW;
                    if (this->saturationType == Sw)
                    {
                        satW = satBound;
                        satNW = 1-satBound;
                    }
                    else if (this->saturationType == Sn)
                    {
                        satW = 1-satBound;
                        satNW = satBound;
                    }
                    else
                    {
                        DUNE_THROW(RangeError, "saturation type not implemented");
                    }
                    Scalar pressBound = this->problem().dirichletPress(globalPosFace, *isIt);
                    Scalar pcBound = this->problem().materialLaw().pC(satW, globalPosFace, *eIt, localPosFace);

                    //determine phase pressures from primary pressure variable
                    Scalar pressW=0;
                    Scalar pressNW=0;
                    if (this->pressureType == pw)
                    {
                        pressW = pressBound;
                        pressNW = pressBound + pcBound;
                    }
                    else if (this->pressureType == pn)
                    {
                        pressW = pressBound - pcBound;
                        pressNW = pressBound;
                    }

                    //get temperature at current position
                    Scalar temperature = this->problem().temperature(globalPosFace, *eIt, localPosFace);

                    Scalar densityWBound = 0;
                    Scalar densityNWBound = 0;
                    Scalar lambdaWBound = 0;
                    Scalar lambdaNWBound = 0;
                    if (this->compressibility)
                    {
                        densityWBound = this->problem().wettingPhase().density(temperature,pressW);
                        densityNWBound = this->problem().nonwettingPhase().density(temperature,pressNW);
                        lambdaWBound = this->problem().materialLaw().mobW(satW,globalPosFace, *eIt, localPosFace, temperature,pressW)*densityWBound;
                        lambdaNWBound = this->problem().materialLaw().mobN(satNW,globalPosFace, *eIt, localPosFace, temperature,pressNW)*densityNWBound;
                    }
                    else
                    {
                        densityWBound = this->problem().wettingPhase().density(temperature);
                        densityNWBound = this->problem().nonwettingPhase().density(temperature);
                        lambdaWBound = this->problem().materialLaw().mobW(satW,globalPosFace, *eIt, localPosFace, temperature);
                        lambdaNWBound = this->problem().materialLaw().mobN(satNW,globalPosFace, *eIt, localPosFace, temperature);
                    }

                    Scalar potentialW = 0;
                    Scalar potentialNW = 0;

                    potentialW = this->problem().variables().potentialWetting(globalIdxI, isIndex);
                    potentialNW = this->problem().variables().potentialNonwetting(globalIdxI, isIndex);

                    Scalar densityW = (potentialW> 0.) ? densityWI : densityWBound;
                    Scalar densityNW = (potentialNW> 0.) ? densityNWI : densityNWBound;

                    densityW = (potentialW == 0.) ? 0.5 * (densityWI + densityWBound) : densityW;
                    densityNW = (potentialNW == 0.) ? 0.5 * (densityNWI + densityNWBound) : densityNW;

                    //calculate potential gradient
                    if (this->pressureType == pw)
                    {
                        potentialW = (pressI - pressBound) / dist;
                        potentialNW = (pressI + pcI - pressBound - pcBound) / dist;
                    }
                    if (this->pressureType == pn)
                    {
                        potentialW = (pressI - pcI - pressBound + pcBound) / dist;
                        potentialNW = (pressI - pressBound) / dist;
                    }
                    if (this->pressureType == pglobal)
                    {
                        potentialW = (pressI - pressBound - fractionalNWI * (pcI - pcBound)) / dist;
                        potentialNW = (pressI - pressBound + fractionalWI * (pcI - pcBound)) / dist;
                    }

                    potentialW += densityW * (unitDistVec * this->gravity);
                    potentialNW += densityNW * (unitDistVec * this->gravity);

                    //store potential gradients for further calculations
                    this->problem().variables().potentialWetting(globalIdxI, isIndex) = potentialW;
                    this->problem().variables().potentialNonwetting(globalIdxI, isIndex) = potentialNW;

                    //do the upwinding of the mobility depending on the phase potentials
                    Scalar lambdaW = (potentialW > 0.) ? lambdaWI : lambdaWBound;
                    lambdaW = (potentialW == 0) ? 0.5 * (lambdaWI + lambdaWBound) : lambdaW;
                    Scalar lambdaNW = (potentialNW > 0.) ? lambdaNWI : lambdaNWBound;
                    lambdaNW = (potentialNW == 0) ? 0.5 * (lambdaNWI + lambdaNWBound) : lambdaNW;
                    densityW = (potentialW> 0.) ? densityWI : densityWBound;
                    densityW = (potentialW == 0.) ? 0.5 * (densityWI + densityWBound) : densityW;
                    densityNW = (potentialNW> 0.) ? densityNWI : densityNWBound;
                    densityNW = (potentialNW == 0.) ? 0.5 * (densityNWI + densityNWBound) : densityNW;

                    //calculate the gravity term
                    FieldVector<Scalar,dimWorld> velocityW(permeability);
                    FieldVector<Scalar,dimWorld> velocityNW(permeability);
                    FieldVector<Scalar,dimWorld> gravityTermW(unitDistVec);
                    FieldVector<Scalar,dimWorld> gravityTermNW(unitDistVec);

                    gravityTermW *= (this->gravity*permeability)*(lambdaW * densityW);
                    gravityTermNW *= (this->gravity*permeability)*(lambdaNW * densityNW);

                    //calculate velocity depending on the pressure used -> use pc = pn - pw
                    if (this->pressureType == pw)
                    {
                        velocityW *= lambdaW * (pressI - pressBound)/dist;
                        velocityNW *= lambdaNW * (pressI - pressBound)/dist + 0.5 * (lambdaNWI + lambdaNWBound) * (pcI - pcBound) / dist;
                        velocityW += gravityTermW;
                        velocityNW += gravityTermNW;
                    }
                    if (this->pressureType == pn)
                    {
                        velocityW *= lambdaW * (pressI - pressBound)/dist - 0.5 * (lambdaWI + lambdaWBound) * (pcI - pcBound) / dist;
                        velocityNW *= lambdaNW * (pressI - pressBound) / dist;
                        velocityW += gravityTermW;
                        velocityNW += gravityTermNW;
                    }
                    if (this->pressureType == pglobal)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = permeability;
                        this->problem().variables().velocity()[globalIdxI][isIndex]*= (lambdaW + lambdaNW)* (pressI - pressBound )/ dist;
                        this->problem().variables().velocity()[globalIdxI][isIndex] += gravityTermW;
                        this->problem().variables().velocity()[globalIdxI][isIndex] += gravityTermNW;
                    }

                    //store velocities
                    if (velocityType_ == vw)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = velocityW;
                        this->problem().variables().velocitySecondPhase()[globalIdxI][isIndex] = velocityNW;
                    }
                    if (velocityType_ == vn)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = velocityNW;
                        this->problem().variables().velocitySecondPhase()[globalIdxI][isIndex] = velocityW;
                    }
                    if (velocityType_ == vt)
                    {
                        if (this->pressureType == pw || this->pressureType == pn)
                        {
                            this->problem().variables().velocity()[globalIdxI][isIndex] = velocityW + velocityNW;
                        }
                    }
                }//end dirichlet boundary

                else
                {
                    std::vector<Scalar> J = this->problem().neumannPress(globalPosFace, *isIt);
                    FieldVector<Scalar,dimWorld> velocityW(unitDistVec);
                    FieldVector<Scalar,dimWorld> velocityNW(unitDistVec);

                    velocityW *= J[wetting];
                    velocityNW *= J[nonwetting];

                    if (!this->compressibility)
                    {
                        velocityW /= densityWI;
                        velocityNW /= densityNWI;
                    }

                    if (velocityType_ == vw)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = velocityW;
                        this->problem().variables().velocitySecondPhase()[globalIdxI][isIndex] = velocityNW;
                    }
                    if (velocityType_ == vn)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = velocityNW;
                        this->problem().variables().velocitySecondPhase()[globalIdxI][isIndex] = velocityW;
                    }
                    if (velocityType_ == vt)
                    {
                        this->problem().variables().velocity()[globalIdxI][isIndex] = velocityW + velocityNW;
                    }
                }//end neumann boundary
            }//end boundary
        }// end all intersections
    }// end grid traversal
//                        printvector(std::cout, this->problem().variables().velocity(), "velocity", "row", 4, 1, 3);
    return;
}
}
#endif
