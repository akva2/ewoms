// -*- mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
// vi: set et ts=4 sw=4 sts=4:
/*
  Copyright (C) 2011-2013 by Andreas Lauser

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 2 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/
/*!
 * \file
 *
 * \copydoc Ewoms::FvBaseBoundaryContext
 */
#ifndef EWOMS_FV_BASE_BOUNDARY_CONTEXT_HH
#define EWOMS_FV_BASE_BOUNDARY_CONTEXT_HH

#include "fvbaseproperties.hh"

#include <dune/common/fvector.hh>

namespace Ewoms {

/*!
 * \ingroup Discretization
 *
 * \brief Represents all quantities which available on boundary segments
 */
template<class TypeTag>
class FvBaseBoundaryContext
{
    typedef typename GET_PROP_TYPE(TypeTag, Scalar) Scalar;
    typedef typename GET_PROP_TYPE(TypeTag, Problem) Problem;
    typedef typename GET_PROP_TYPE(TypeTag, Model) Model;
    typedef typename GET_PROP_TYPE(TypeTag, Stencil) Stencil;
    typedef typename GET_PROP_TYPE(TypeTag, ElementContext) ElementContext;
    typedef typename GET_PROP_TYPE(TypeTag, VolumeVariables) VolumeVariables;
    typedef typename GET_PROP_TYPE(TypeTag, FluxVariables) FluxVariables;
    typedef typename GET_PROP_TYPE(TypeTag, GradientCalculator) GradientCalculator;

    typedef typename GET_PROP_TYPE(TypeTag, GridView) GridView;
    typedef typename GridView::template Codim<0>::Entity Element;
    typedef typename GridView::IntersectionIterator IntersectionIterator;
    typedef typename GridView::Intersection Intersection;

    enum { dim = GridView::dimension };
    enum { dimWorld = GridView::dimensionworld };

    typedef typename GridView::ctype CoordScalar;
    typedef Dune::FieldVector<CoordScalar, dim> GlobalPosition;
    typedef Dune::FieldVector<Scalar, dimWorld> Vector;

public:
    /*!
     * \brief The constructor.
     */
    explicit FvBaseBoundaryContext(const ElementContext &elemCtx)
        : elemCtx_(elemCtx)
        , intersectionIt_(gridView().ibegin(element()))
    { }

    /*!
     * \copydoc Ewoms::ElementContext::problem()
     */
    const Problem &problem() const
    { return elemCtx_.problem(); }

    /*!
     * \copydoc Ewoms::ElementContext::model()
     */
    const Model &model() const
    { return elemCtx_.model(); }

    /*!
     * \copydoc Ewoms::ElementContext::gridView()
     */
    const GridView &gridView() const
    { return elemCtx_.gridView(); }

    /*!
     * \copydoc Ewoms::ElementContext::element()
     */
    const Element &element() const
    { return elemCtx_.element(); }

    /*!
     * \brief Returns a reference to the element context object.
     */
    const ElementContext &elementContext() const
    { return elemCtx_; }

    /*!
     * \brief Returns a reference to the current gradient calculator.
     */
    const GradientCalculator &gradientCalculator() const
    { return elemCtx_.gradientCalculator(); }

    /*!
     * \copydoc Ewoms::ElementContext::numDof()
     */
    int numDof(int timeIdx) const
    { return elemCtx_.numDof(timeIdx); }

    /*!
     * \copydoc Ewoms::ElementContext::numInteriorFaces()
     */
    int numInteriorFaces(int timeIdx) const
    { return elemCtx_.numInteriorFaces(timeIdx); }

    /*!
     * \copydoc Ewoms::ElementContext::stencil()
     */
    const Stencil &stencil(int timeIdx) const
    { return elemCtx_.stencil(timeIdx); }

    /*!
     * \brief Returns the outer unit normal of the boundary segment
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    Vector normal(int boundaryFaceIdx, int timeIdx) const
    {
        auto tmp = stencil(timeIdx).boundaryFace[boundaryFaceIdx].normal;
        tmp /= tmp.two_norm();
        return tmp;
    }

    /*!
     * \brief Returns the area [m^2] of a given boudary segment.
     */
    Scalar boundarySegmentArea(int boundaryFaceIdx, int timeIdx) const
    { return elemCtx_.stencil(timeIdx).boundaryFace(boundaryFaceIdx).area(); }

    /*!
     * \brief Return the position of a local entity in global coordinates.
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    const GlobalPosition &pos(int boundaryFaceIdx, int timeIdx) const
    { return stencil(timeIdx).boundaryFace(boundaryFaceIdx).integrationPos(); }

    /*!
     * \brief Return the position of a control volume's center in global coordinates.
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    const GlobalPosition &cvCenter(int boundaryFaceIdx, int timeIdx) const
    {
        int scvIdx = stencil(timeIdx).boundaryFace(boundaryFaceIdx).interiorIndex();
        return stencil(timeIdx).subControlVolume(scvIdx).globalPos();
    }

    /*!
     * \brief Return the local sub-control volume index of the
     *        interior of a boundary segment
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    short interiorScvIndex(int boundaryFaceIdx, int timeIdx) const
    {
        return stencil(timeIdx).boundaryFace(boundaryFaceIdx).interiorIndex();
    }

    /*!
     * \brief Return the global space index of the sub-control volume
     *        at the interior of a boundary segment
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    int globalSpaceIndex(int boundaryFaceIdx, int timeIdx) const
    { return elemCtx_.globalSpaceIndex(interiorScvIndex(boundaryFaceIdx, timeIdx), timeIdx); }

    /*!
     * \brief Return the volume variables for the finite volume in the
     *        interiour of a boundary segment
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    const VolumeVariables &volVars(int boundaryFaceIdx, int timeIdx) const
    {
        short interiorScvIdx = this->interiorScvIndex(boundaryFaceIdx, timeIdx);
        return elemCtx_.volVars(interiorScvIdx, timeIdx);
    }

    /*!
     * \brief Return the flux variables for a given boundary face.
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     * \param timeIdx The index of the solution used by the time discretization
     */
    const FluxVariables &fluxVars(int boundaryFaceIdx, int timeIdx) const
    { return elemCtx_.boundaryFluxVars(boundaryFaceIdx, timeIdx); }

    /*!
     * \brief Return the intersection for the neumann segment
     *
     * TODO/HACK: The intersection should take a local index as an
     * argument. since that's not supported efficiently by the DUNE
     * grid interface, we just ignore the index argument here!
     *
     * \param boundaryFaceIdx The local index of the boundary segment
     */
    const Intersection &intersection(int boundaryFaceIdx) const
    { return *intersectionIt_; }

    /*!
     * \brief Return the intersection for the neumann segment
     *
     * TODO/HACK: the intersection iterator can basically be
     * considered as an index which is manipulated externally, but
     * context classes should not store any indices. it is done this
     * way for performance reasons
     */
    IntersectionIterator &intersectionIt()
    { return intersectionIt_; }

protected:
    const ElementContext &elemCtx_;
    IntersectionIterator intersectionIt_;
};

} // namespace Ewoms

#endif
