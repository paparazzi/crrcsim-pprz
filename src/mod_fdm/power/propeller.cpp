/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *   Copyright (C) 2005, 2006, 2008, 2009 - Jens Wilhelm Wulf (original author)
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
#include "propeller.h"

#include <iostream>
#include "../../mod_misc/filesystools.h"
#include "../../mod_misc/lib_conversions.h"


#define RHO       1.225 // kg/m^3
#define ETA_PROP  0.65

Power::Propeller::Propeller() : Gearing()
{
  omega_fold = 5;
  J          = 0;
  dirThrust  = CRRCMath::Vector3(1, 0, 0);
  mulForce   = CRRCMath::Vector3(1, 0, 0);
  mulMoment  = CRRCMath::Vector3(0, 0, 0);
}

void Power::Propeller::CalcDownthrust(SimpleXMLTransfer* xml)
{
  int idx = xml->indexOfChild("pos");
  if (idx < 0)
  {
    dirThrust = CRRCMath::Vector3(1, 0, 0);
    mulForce  = CRRCMath::Vector3(1, 0, 0);
    mulMoment = CRRCMath::Vector3(0, 0, 0);
  }
  else    
  {
    SimpleXMLTransfer* sxtpos     = xml->getChildAt(idx);
    double             downthrust = M_PI * sxtpos->getDouble("downthrust", 0) / 180;      
                       dirThrust  = CRRCMath::Vector3(cos(downthrust), 0, sin(downthrust));
    CRRCMath::Vector3  pos        = CRRCMath::Vector3(sxtpos->getDouble("x", 0),
                                                      0,
                                                      sxtpos->getDouble("z", 0));      
    CRRCMath::Vector3 dirForce   = pos * (1/pos.length());
    
    // Split thrust vector into a part parallel to dirForce and a part parallel to dirMoment:
    //   dirThrust   = a * dirForce   + b * dirMoment
    // After simplifying all this (and using the variable expressions above) the solution boils down to:
    double a = sin(downthrust) * dirForce.r[2] + cos(downthrust) * dirForce.r[0];
    double b = cos(downthrust) * dirForce.r[2] - sin(downthrust) * dirForce.r[0];
    
    mulForce  = dirForce * a;
    mulMoment = CRRCMath::Vector3(0, b * pos.length(), 0);    
  }
  mulForce.print("mulForce=", ", ");
  mulMoment.print("mulMoment=", "\n");
}

void Power::Propeller::ReloadParams(SimpleXMLTransfer* xml)
{
  Gearing::ReloadParams(xml);
  
  SimpleXMLTransfer* prop;
  bool               fExtern = true;
  
  if (xml->indexOfAttribute("filename") >= 0)
  {
    prop = new SimpleXMLTransfer(FileSysTools::getDataPath("models/propeller/" + xml->getString("filename") + ".xml", true));
  }
  else
  {
    prop    = xml;
    fExtern = false;
  }
  
  // Der Sturz wird in jedem Fall aus der Modelldatei gelesen, ansonsten muss man ja eine 
  // Propellerdatei fuer jeden Sturz extra haben.
  CalcDownthrust(xml);
      
  D          = prop->getDouble("D");
  H          = prop->getDouble("H");
  J          = prop->getDouble("J");
  omega_fold = prop->attributeAsDouble("n_fold", -1)*2*M_PI;
  
  std::cout << "      Propeller: D=" << D << " m, H=" << H << " m, J=" << J << " kg m^2";
   
  if (fExtern)
    delete prop;    
}

void Power::Propeller::step(PowerValuesStep* values)
{
  double  omega = i*values->omega;
  double  n     = omega/(2*M_PI);
    
  if (omega < omega_fold && omega_fold > 0)
  {
    fFolded           = true;
    values->dPropFreq = 0;
  }
  else
  {
    fFolded           = false;
    values->dPropFreq = n;
    
    // force
    double V_p = values->inputs->pitch * H * n;
    double V_X = values->VRelAir.r[0];
    filter.step(values->dt, V_p - V_X);
    double F_X = M_PI * 0.25 * D*D * RHO * fabs(V_X + filter.val/2) * filter.val;
  
    double P = F_X * (V_X + filter.val/2);
    double M = 0;

    if (F_X > 0)
    {
      // Effective Translational Lift, see 
      //   http://user.cs.tu-berlin.de/~calle/marvin/dissertation/aerodynamik.html
      // This lift is 'for free', is does not mean more P or M!
      // It is important to model this effect for helicopters, it is unimportant for
      // fixed wing planes (but does no harm in this case).
      float vw      = sqrt(values->VRelAir.r[1]*values->VRelAir.r[1] + values->VRelAir.r[2]*values->VRelAir.r[2]);
      float x       = fabs(vw/V_p);
      const float c = -0.20037;   
      const float d = 0.0825119;  
      const float e = -0.00997873;
      
      if (isfinite(x))
      {
        // I don't know about x>3. Maybe it will never happen, but limiting is save:
        if (x>3)
          x=3;
        
        float fact    = 1+c*(x*x)+d*(x*x*x)+e*(x*x*x*x);
        F_X = F_X / fact;
      }
    }
    
    *values->force += mulForce * (F_X * ETA_PROP);
  
    if (fabs(omega) > 1E-5)
      M = P/omega *i;    
    
    values->moment_shaft -= M;
    *values->moment += dirThrust * M + mulMoment * (F_X * ETA_PROP);

#define _BLA
#ifdef BLA
    static int blacnt = 0;
    if (blacnt++ > 200)
    {
      blacnt = 0;
      
      std::cout.setf(std::ios_base::fixed, std::ios_base::floatfield);
      std::cout.precision(4);
      std::cout.width(7);
      std::cout << (F_X*ETA_PROP) << ", ";
      std::cout << omega << ", ";
      std::cout << P << ", ";
      std::cout << "\n";
    }
#endif    
    /*
    std::cout << "propeller: ";
    
    
    std::cout.width(7);
    std::cout << (values->omega*30/M_PI) << "/min, ";
    
    std::cout.width(7);
    std::cout << M << ", ";
    
     */
  }
  

  /*
  std::cout << "propeller: ";
  
   */

  /*
  std::cout.width(7);
  std::cout << V_p << " , ";
  
  std::cout.width(7);
  std::cout << V_X << ", ";
  */
  
  /*
  std::cout.width(7);
  std::cout << (P/eta) << ", ";
   */
  
  /*
  std::cout.width(7);
  std::cout << (F_X*eta_prop) << ", ";
  
  std::cout.width(7);
  std::cout << (values->omega*30/M_PI) << "/min, ";
  */ 
  
  
/*  std::cout.width(7);
  std::cout << values->U << " V, ";
  std::cout.width(7);
  std::cout << M_M << " N, ";*/
  
//  std::cout << "\n";
}

void Power::Propeller::ReloadParams_automagic(SimpleXMLTransfer* xml)
{
  SimpleXMLTransfer* p = xml->getChild("battery.shaft.propeller");
  
  D = p->getDouble("D");
  H = p->getDouble("H");
  J = p->getDouble("J");
  
  double F = xml->getDouble("F");
  double V = xml->getDouble("V");
  

  // Der Sturz wird in jedem Fall aus der Modelldatei gelesen, ansonsten muss man ja eine 
  // Propellerdatei fuer jeden Sturz extra haben.
  CalcDownthrust(p);
  
  {
    // Calculate rotational speed and torque needed:
    //  F = M_PI * 0.25 * D*D * RHO * (V_X + filter.val/2) * filter.val * ETA_PROP;
    //  F = M_PI * 0.25 * D*D * RHO * (V + (Hn-V)/2) * (Hn-V) * ETA_PROP;
    //  F = M_PI * 0.25 * D*D * RHO * (V/2 + Hn/2) * (Hn-V) * ETA_PROP;
    double n = sqrt( (8*F/(M_PI*D*D*RHO*ETA_PROP)) + V*V)/H;    
    double M = F * (V + (V + H*n)/2) / (2*M_PI*n) * i;
    
    // Save these values so the engine can adjust itself to them:
    p->setAttribute("automagic.n_P", doubleToString(n));
    p->setAttribute("automagic.M_P", doubleToString(M));
  }
  
  omega_fold = p->attributeAsDouble("n_fold", -1)*2*M_PI;
}

void Power::Propeller::InitStates(CRRCMath::Vector3 vInitialVelocity, double& dOmega)
{
  filter.init(0, 0);
  fFolded = true;
  if (omega_fold < 0)
    dOmega = 2 * M_PI * vInitialVelocity.r[0] / H;
}
