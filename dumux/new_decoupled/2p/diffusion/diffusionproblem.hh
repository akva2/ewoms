// $Id$
/*****************************************************************************
 *   Copyright (C) 2007-2009 by Bernd Flemisch                               *
 *   Copyright (C) 2008-2009 by Markus Wolff                                 *
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
#ifndef DUNE_DIFFUSIONPROBLEM_HH
#define DUNE_DIFFUSIONPROBLEM_HH

#include<iostream>
#include<iomanip>

#include<dune/common/fvector.hh>
#include<dune/common/fmatrix.hh>
#include<dune/common/exceptions.hh>
#include<dune/grid/common/grid.hh>
#include<dune/grid/common/referenceelements.hh>
#include<dumux/operators/boundaryconditions.hh>
#include<dumux/material/twophaserelations.hh>
#include <dumux/material/property_baseclasses.hh>

/**
 * @file
 * @brief  Base class for defining an instance of the diffusion problem
 * @author Bernd Flemisch, Markus Wolff
 */

namespace Dune
{
/*! \ingroup diffusion
 * @brief base class that defines the parameters of a diffusion equation
 *
 * An interface for defining parameters for the stationary diffusion equation
 *  \f[\text{div}\, \boldsymbol{v} = q\f],
 *  where, the velocity \f$\boldsymbol{v} \sim \boldsymbol{K} \nabla p \f$,
 *  \f$p\f$ is a pressure and q a source/sink term
 *
 *    Template parameters are:
 *
 *  - GridView      a DUNE gridview type
 *  - Scalar        type used for scalar quantities
 *  - VC            type of a class containing different variables of the model
 */
template<class GridView, class Scalar, class VC>
class DiffusionProblem
{
    enum
    {
        dim = GridView::dimension, dimWorld = GridView::dimensionworld
    };
    enum
    {
        wetting = 0, nonwetting = 1
    };
typedef    typename GridView::Grid Grid;
    typedef typename GridView::Traits::template Codim<0>::Entity Element;
    typedef typename GridView::Intersection Intersection;
    typedef Dune::FieldVector<Scalar,dim> LocalPosition;
    typedef Dune::FieldVector<Scalar,dimWorld> GlobalPosition;
    typedef Dune::FieldMatrix<Scalar,dim,dim> FieldMatrix;

public:

    //! evaluate source term
    /*! evaluate source term at given location
     @param  globalPos    position in global coordinates
     @param  element      entity of codim 0
     @param  localPos     position in reference element of element
     \return     value of source term (wetting phase: vector position 0, non-wetting phase: vector position 1).
     */
    virtual std::vector<Scalar> source (const GlobalPosition& globalPos, const Element& element,
            const LocalPosition& localPos) = 0;

    //! return type of boundary condition at the given global coordinate
    /*! return type of boundary condition at the given global coordinate
     @param  globalPos    position in global coordinates
     @param  element      entity of codim 0
     @param  localPos     position in reference element of element
     \return     boundary condition type given by enum in this class
     */
    virtual BoundaryConditions::Flags bctypePress (const GlobalPosition& globalPos,
            const Intersection& intersection) const = 0;

    //! return type of boundary condition at the given global coordinate
    /*! return type of boundary condition at the given global coordinate
     @param  globalPos    position in global coordinates
     @param  element      entity of codim 0
     @param  localPos     position in reference element of element
     \return     boundary condition type given by enum in this class
     */
    virtual BoundaryConditions::Flags bctypeSat (const GlobalPosition& globalPos,
            const Intersection& intersection) const
    {
        return BoundaryConditions::dirichlet;
    }

    //! evaluate Dirichlet boundary condition at given position
    /*! evaluate Dirichlet boundary condition at given position
     @param  globalPos    position in global coordinates
     @param  element      entity of codim 0
     @param  localPos     position in reference element of element
     \return     boundary condition value of a dirichlet pressure boundary condition
     */
    virtual Scalar dirichletPress (const GlobalPosition& globalPos,
            const Intersection& intersection) const = 0;

    //! evaluate Dirichlet boundary condition at given position
    /*! evaluate Dirichlet boundary condition at given position
     @param  globalPos    position in global coordinates
     @param  element      entity of codim 0
     @param  localPos     position in reference element of element
     \return     boundary condition value of a dirichlet saturation boundary condition
     */
    virtual Scalar dirichletSat (const GlobalPosition& globalPos,
            const Intersection& intersection) const
    {
        return 1;
    }

    //! evaluate Neumann boundary condition at given position
    /*! evaluate Neumann boundary condition at given position
     @param  globalPos    position in global coordinates
     @param  element      entity of codim 0
     @param  localPos     position in reference element of element
     \return     boundary condition value of a neumann pressure boundary condition (wetting phase: vector position 0, non-wetting phase: vector position 1).
     */
    virtual std::vector<Scalar> neumannPress (const GlobalPosition& globalPos,
            const Intersection& intersection) const = 0;

    //! gravity constant
    /*! gravity constant
     \return    gravity vector
     */
    virtual const FieldVector<Scalar,dim>& gravity() const
    {
        return gravity_;
    }

    //! local temperature
    /*! local temperature
     \return    temperature
     */
    virtual Scalar temperature (const GlobalPosition& globalPos, const Element& element,
            const LocalPosition& localPos) const
    {
        return 283.15;
    }

    //! properties of the soil
    /*! properties of the soil
     \return    soil
     */
    virtual Matrix2p<Grid, Scalar>& soil () const
    {
        return soil_;
    }

    //! object for definition of material law
    /*! object for definition of material law (e.g. Brooks-Corey, Van Genuchten, ...)
     \return    material law
     */
    virtual TwoPhaseRelations<Grid, Scalar>& materialLaw () const
    {
        return materialLaw_;
    }

    //! object containing different variables
    /*! object containing different variables like the primary pressure and saturation, material laws,...
     \return    variables object
     */
    virtual VC& variables ()
    {
        return variables_;
    }

    //! object containing the wetting phase parameters
    /*! object containing the wetting phase parameters (density, viscosity, ...)
     \return    wetting phase
     */
    virtual Fluid& wettingPhase () const
    {
        return wettingPhase_;
    }

    //! object containing the non-wetting phase parameters
    /*! object containing the non-wetting phase parameters (density, viscosity, ...)
     \return    non-wetting phase
     */
    virtual Fluid& nonWettingPhase () const
    {
        return nonWettingPhase_;
    }

    //! constructor
    /** @param variables object of class VariableClass.
     *  @param wettingPhase implementation of a wetting phase.
     *  @param nonWettingPhase implementation of a non-wetting phase.
     *  @param soil implementation of the solid matrix
     *  @param materialLaw implementation of Material laws. Class TwoPhaseRelations or derived.
     */
    DiffusionProblem(VC& variables,Fluid& wettingPhase, Fluid& nonWettingPhase, Matrix2p<Grid, Scalar>& soil, TwoPhaseRelations<Grid, Scalar>& materialLaw = *(new TwoPhaseRelations<Grid,Scalar>))
    : variables_(variables), materialLaw_(materialLaw),
    wettingPhase_(wettingPhase), nonWettingPhase_(nonWettingPhase), soil_(soil),
    gravity_(0)
    {}

    //! Constructs an object of type DiffusionProblem
    /** @param variables object of class VariableClass.
     *  @param materialLaw implementation of Material laws. Class TwoPhaseRelations or derived.
     */
    DiffusionProblem(VC& variables, TwoPhaseRelations<Grid, Scalar>& materialLaw = *(new TwoPhaseRelations<Grid,Scalar>))
    : variables_(variables), materialLaw_(materialLaw),
    wettingPhase_(materialLaw_.wettingPhase), nonWettingPhase_(materialLaw.nonwettingPhase), soil_(materialLaw.soil), gravity_(0)
    {}

    //! always define virtual destructor in abstract base class
    virtual ~DiffusionProblem ()
    {}

private:
    VC& variables_; //object of type Dune::VariableClass
    TwoPhaseRelations<Grid,Scalar>& materialLaw_; //object of type Dune::TwoPhaseRelations or derived
    Fluid& wettingPhase_; //object derived from Dune::Fluid
    Fluid& nonWettingPhase_; //object derived from Dune::Fluid
    Matrix2p<Grid, Scalar>& soil_; //object derived from Dune::Matrix2p
    FieldVector<Scalar,dim> gravity_; //vector including the gravity constant
};

}
#endif
