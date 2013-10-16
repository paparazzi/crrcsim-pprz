/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *   Copyright (C) 2005, 2006, 2008, 2009 - Jens Wilhelm Wulf (original author)
 *   Copyright (C) 2007 - Jan Reucker
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */
#ifndef PROPELLER_H
# define PROPELLER_H

# include "gearing.h"
# include "values_step.h"
# include "../../mod_math/pt1.h"
# include "../../mod_math/vector3.h"

namespace Power
{
   
   /**
    * This class is part of the power system. To simply use the system, you should not
    * access or call any of its members. Please take a look at Power instead.
    *
    * This class models a propeller. It is far from perfect, so you can't use it to
    * optimize your real airplane, but it should be good enough to achieve the feeling
    * of a realistic propeller in the simulation.
    *
    * The Propeller can be configured to be a folding prop, which folds as soon as it rotates
    * slower than \c omega_fold. From the xml config, \c n_fold is read and converted using
    * <tt>(omega_fold = n_fold * 2 * pi)</tt>.
    * If a prop is folded, it will not create a
    * positive torque to the Shaft (and keep it rotating) and will not create drag to the airplane.
    * If \c n_fold is negative, this is not a folding prop.
    *
    * If your model needs downthrust, you can tell the simulation about the propeller's position 
    * (relative to center of gravity) and angle of downthrust.
    * 
    * Formulas used: see homepage of Martin Hepperle, http://www.mh-aerotools.de/
    * 
    *
    * Example for an xml description for direct connection to shaft:
    * 
    \verbatim
    <propeller D="0.2" H="0.14" J="0" n_fold="5" />
    \endverbatim
    * 
    * 
    * Example for an xml description, connection to shaft via a gearbox:
    * 
    \verbatim
    <propeller D="0.2" H="0.14" J="0" n_fold="5" >
    <gearing i="1.13" J="0" />
    </propeller>
    \endverbatim
    * 
    * Example including position and downthrust:
    * 
    \verbatim
    <propeller D="0.2" H="0.14" J="0" n_fold="5" >
    <pos x="0.22" z="0" downthrust="2"/>
    </propeller>
    \endverbatim
    * The inertia \c J of the propeller is translated to the shaft automatically. The inertia \c J of the gearing is
    * the value seen by the shaft.
    *
    * See Power::Gearing for a description of a gearbox.
    *
    * It is possible to read the parameters of a propeller from a separate file. In this case use
    * something like
    \verbatim
    <propeller filename="10x7" />
    \endverbatim
    \verbatim
    <propeller filename="10x7" >
    <gearing i="1.13" J="0" />
    </propeller>
    \endverbatim
    * instead of writing down the parameters directly. The system will try to load a file 
    * <tt>./models/propeller/10x7.xml</tt> which might look like this:
    \verbatim
    <?xml version="1.0"?>
    <!--
    This is just a sample.
    * 
    Units are SI!
    H, D: m
    J:    kg m^2  
    -->
    * 
    <propeller H="13E-2" D="17E-2" J="1.2E-6" n_fold="5">
    </propeller>
    \endverbatim
    * The parameters regarding position and downthrust are not in the propeller file, as they are not parameters which describe
    * the propeller itself, but how the propeller is mounted.
    * 
    * @author Jens Wilhelm Wulf
    */
   class Propeller : public Gearing
   {
   public:
     
     /**
      * 
      */
     Propeller();
     
     /**
      * virtual base class should have a virtual dtor
      */
     virtual ~Propeller() {};
     
     /** 
      * Load or reload parameters
      */
     void ReloadParams(SimpleXMLTransfer* xml);
     
     /** 
      * Load or reload parameters in case of automagic settings
      */
     void ReloadParams_automagic(SimpleXMLTransfer* xml);
     
     /**
      * Go ahead values->dt seconds in the simulation.
      */
     virtual void step(PowerValuesStep* values);
     
     virtual void InitStates(CRRCMath::Vector3 vInitialVelocity, double& dOmega);
     
   private:
     
     /**
      * Is it folded?
      */
     bool fFolded;
     
     /**
      * Rotational speed below which the props becomes folded [rad/s].
      */
     double omega_fold;
     
     /**
      * Pitch [m]
      */
     double H;
     
     /**
      * Diameter [m]
      */
     double D;
     
     /**
      * a filter: difference in velocity between airflow and (H * rotational speed)
      */
     CRRCMath::PT1 filter;
     
     /**
      * models direction of thrust (body axes)
      */
     CRRCMath::Vector3 mulForce;
     CRRCMath::Vector3 mulMoment;
     CRRCMath::Vector3 dirThrust;

   private:
     void CalcDownthrust(SimpleXMLTransfer* xml);
     
   };
};
#endif
