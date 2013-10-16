/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 * Copyright (C) 2000, 2001 Jan Edward Kansky (original author)
 * Copyright (C) 2005, 2006, 2008 Jens Wilhelm Wulf
 * Copyright (C) 2005-2009 Jan Reucker
 * Copyright (C) 2009 Joel Lienard
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

/** \file windfield.cpp
 *
 *  This file contains the windfield and thermal simulation.
 */

#include "windfield.h"

#include "../global.h"
#include "../include_gl.h"
#include <math.h>
#include <stdio.h>
#include <plib/ssg.h>   // for ssgSimpleState
#include "../mod_windfield_config.h"
#include "../mod_misc/ls_constants.h"
//jwtodo #include "../config.h"
// remaining calls to cfg:
//    cfg->thermal->density     on init
//    cfg->wind->getVelocity()  on update and calculate_wind
//    cfg->wind->getDirection() on update and calculate_wind
//    cfg->getDynamicSoaring()  on calculate_wind
//  random_init:
//      cfg->thermal->radius_sigma
//      cfg->thermal->radius_mean
//      ....

#include "../crrc_main.h"
#include "../global_video.h"
#include "thermal03/tschalen.h"
#include "../mod_misc/crrc_rand.h"
#include "../mod_landscape/crrc_scenery.h"

#if (THERMAL_CODE == 1)
# include "thermalprofile.h"
#endif

#if DEBUG_THERMAL_SCRSHOT == 1
# include <fstream>
#endif

#define USE_CRRCRAND 1

// THERMAL_TEST == 0
//   Simulation as usual
// THERMAL_TEST == 1
// THERMAL_TEST == 2
//   There is only one thermal.
//   It is created at a fixed position with fixed parameters.
//   See documentation/thermals/thermalsim.html
#define THERMAL_TEST 0

#define THERMAL_NEWPOSLOG 1

/**
 * Which version of thermal model to use?
 * Everthing less than 3 results in the 'old' code (which depends on THERMAL_CODE
 * in crrc_config.h), 3 results in version 3.
 */
unsigned int ThermalVersion;

/**
 * Variation of wind velocity
 */
double dWindVelVar;

/**
 * There is some 2D grid. Its area is
 *    (occupancy_grid_size * occupancy_grid_res)^2
 * It is divided into occupancy_grid_size^2 squares.
 */
#define occupancy_grid_size_exp 7
#define occupancy_grid_size     (1 << occupancy_grid_size_exp)
#define occupancy_grid_res      100
Thermal* thermal_occupancy_grid[occupancy_grid_size][occupancy_grid_size];

#if (THERMAL_NEWPOSLOG != 0)
unsigned int NewPosLogArray[occupancy_grid_size][occupancy_grid_size];
unsigned int PosLogArray[occupancy_grid_size][occupancy_grid_size];
#endif

/**
 * How many grid places should be free next to a thermal?
 */
const int nGridDistMin = 1;

/**
 * The number of thermals in the above grid is
 *    (occupancy_grid_size * occupancy_grid_res)^2 * cfg->thermal->density
 */
int num_thermals;

/**
 * The pointer to the first thermal in the linked list.
 */
Thermal* thermals = NULL;

/**
 * One thermal influences an area of
 *    (nInfluenceDist*occupancy_grid_res)^2
 * or less.
 * Radius of the largest (since init) thermal in lengths of the grid.
 */
int nInfluenceDist = 5;

/**
 * To draw thermals, one of two methods is used:
 * 1. loop over linked list of thermals and draw every thermal which
 *    is near the aircraft
 * 2. Look at grid around aircraft and draw present thermals. This
 *    method means less effort if the thermal density is high. It also
 *    doesn't draw thermal which are in the list but not in the grid (may
 *    happen when thermal density is high).
 * If the second method is used, this variable represents a distance
 * in grid coordinates.
 */
int nDrawThermalsFromGrid;

/**
 * Draw thermals with this maximum distance from aircraft.
 * Currently this is not the real distance, but the one in x or y.
 */
const float flThermalDistMax = 1000;

/**
 * Standard thermal according to thermal model version 3.
 */
ThermikSchalen thermalv3;

/**
 * There is a three-dimensional turbulence grid.
 * Little code is present yet. Variables are initialized,
 * but never do anything to the simulation, so I exclude it
 * using #if.
 * Does anyone remember who did this? Can it be finished?
 * Jens Wilhelm Wulf, 17.01.2005
 */
#define USE_TURB_GRID 0
#if (USE_TURB_GRID != 0)
# define turb_grid_size 1024
float turb_x_velocity[turb_grid_size];
float turb_y_velocity[turb_grid_size];
float turb_z_velocity[turb_grid_size];
float turb_x_grad[turb_grid_size];
float turb_y_grad[turb_grid_size];
float turb_z_grad[turb_grid_size];
#endif

#if (THERMAL_CODE == 1)
/**
 * Thermals get weaker before they die. Time in seconds.
 */
const double dFadeOutTime = 10;

/**
 * In the old implementation there has been a value similar to this one. It had
 * been set to 50 feet.
 */
# if THERMAL_TEST != 0
const double dAltitudeFullStrength =  0.5/0.3048;
const double dAltitudeZeroStrength =  0/0.3048;
# else
const double dAltitudeFullStrength = 20/0.3048;
const double dAltitudeZeroStrength =  4/0.3048;
# endif

#endif

/**
 *  OpenGL states for thermal drawing.
 */
static ssgSimpleState   *td_state_noblend = NULL;
static ssgSimpleState   *td_state_blend   = NULL;

/**
 *  A GLU quadric object for thermal drawing
 */
static GLUquadricObj *therm_quadric;

/**
 * calculates grid coordinate from absolute coordinate
 */
inline int absToGridCoor(float flAbsKoor)
{
  return( (int)(
                (flAbsKoor+ (occupancy_grid_size*occupancy_grid_res/2) ) / occupancy_grid_res
                ));
}

/**
 * Calculates absolute coordinate from grid coordinate.
 * Position is at center of grid square with rel=0.5.
 */
inline float gridToAbsCoor(int gridKoor, float rel=0.5)
{
  //
  // gridKoor  = (flAbsKoor+ (occupancy_grid_size*occupancy_grid_res/2) ) / occupancy_grid_res
  // flAbsKoor = gridKoor * occupancy_grid_res - (occupancy_grid_size*occupancy_grid_res/2)
  // should be at center of square:
  // flAbsKoor = gridKoor * occupancy_grid_res - (occupancy_grid_size*occupancy_grid_res/2) + occupancy_grid_res/2
  return(gridKoor * occupancy_grid_res
         - (occupancy_grid_size*occupancy_grid_res/2)
         + rel*occupancy_grid_res
         );
}

/**
 * Returns random numbers with normal (gaussian) distribution.
 *
 * Thanks to rhoads@paul.rutgers.edu for this code!
 * http://remus.rutgers.edu/~rhoads/Code/code.html
 */
double gaussrand()
{
  static double V2, fac;
  static int phase = 0;
  double S, Z, U1, U2, V1;

  if (phase)
    Z = V2 * fac;
  else
  {
    do
    {
      U1 = (double)rand() / RAND_MAX;
      U2 = (double)rand() / RAND_MAX;

      V1 = 2 * U1 - 1;
      V2 = 2 * U2 - 1;
      S = V1 * V1 + V2 * V2;
    }
    while(S >= 1);

    fac = sqrt (-2 * log(S) / S);
    Z = V1 * fac;
  }

  phase = 1 - phase;

  return Z;
}

/**
 * Returns true if there is a thermal nearby.
 * <code>xcoord</code> and <code>ycoord</code> are the indices into
 * the grid.
 */
bool isThermalNearby(int xcoord, int ycoord)
{
  int xmin = xcoord-nGridDistMin;
  int xmax = xcoord+nGridDistMin;
  int ymin = ycoord-nGridDistMin;
  int ymax = ycoord+nGridDistMin;

  if (xmin < 0)
    xmin = 0;
  else if (xmax >= occupancy_grid_size)
    xmax = occupancy_grid_size-1;

  if (ymin < 0)
    ymin = 0;
  else if (ymax >= occupancy_grid_size)
    ymax = occupancy_grid_size-1;

  for (int x=xmin; x<=xmax; x++)
  {
    for (int y=ymin; y<=ymax; y++)
    {
      if (thermal_occupancy_grid[x][y]!=0)
        return(true);
    }
  }

  return(false);
}

/**
 * Calculates position for a new thermal.
 * <code>xpos</code> and <code>ypos</code> are the absolute position,
 * <code>xcoord</code> and <code>ycoord</code> are the indices into
 * the grid.
 *
 * Returns true if no new thermal position could be found.
 */
bool find_new_thermal_position(float *xpos, float *ypos,
                               int *xcoord, int *ycoord)
{
  // The problem of the old method has been that it heavily relied on random
  // values to find a new position, which needed lots of processing time.
  // Therefore it was a two-step algorithm: if the first step failed (time limit), the second
  // step went linearly through the grid to find the next place to put a thermal,
  // which makes a pair or larger number of thermals next to each other, which in
  // turn increases the probability of the first step failing at such a thermal burst,
  // which leads to this burst growing bigger again in the second step.
  //
  // So what we need is a way to browse through the whole grid, but not linearily.
  // We simply use a CRC to count non-linearily.
  int counter = occupancy_grid_size*occupancy_grid_size;
  // Some polynomials which do work:
  unsigned int aPoly[] = { 1494, 2020, 2682, 5548, 5836, 5932, 5976, 6374, 
      6580, 6934, 7064, 7136, 7372, 7474, 7586, 7592 };

  // choose a poly
  unsigned int uPoly = aPoly[CRRC_Random::rand() % (sizeof(aPoly)/sizeof(unsigned int))];
    
  // find an initial value
  unsigned int uCRCVal = CRRC_Random::rand();
  while (uCRCVal == 0)
    CRRC_Random::rand();

  *xcoord = (uCRCVal >> occupancy_grid_size_exp) & (occupancy_grid_size-1);
  *ycoord = uCRCVal & (occupancy_grid_size-1);
  
  while(isThermalNearby(*xcoord, *ycoord) &&
        counter > 0)
  {
    counter--;
    
    uCRCVal <<= 1;
    if ((uCRCVal & (1<<(2*occupancy_grid_size_exp))) != 0)
    {
      uCRCVal ^= uPoly;
      uCRCVal |= 1;
    }
    
    *xcoord = (uCRCVal >> occupancy_grid_size_exp) & (occupancy_grid_size-1);
    *ycoord = uCRCVal & (occupancy_grid_size-1);
  }
  *xpos = gridToAbsCoor(*xcoord, (float)(CRRC_Random::rand())/CRRC_Random::max());
  *ypos = gridToAbsCoor(*ycoord, (float)(CRRC_Random::rand())/CRRC_Random::max());

  // If no such square could be found, thermal density is set way too high.
  // No visible thermal should be created.
  if (counter == 0)
  {
    std::cerr << "No thermal position found\n";
    return(true);
  }
  else
    return(false);
}

// Description: see header file
void clear_wind_field()
{
  // remove linked list
  Thermal* tptr0 = thermals;
  Thermal* tptr1;

  while (tptr0 != NULL)
  {
    tptr1 = tptr0->next_thermal;
    //~ free(tptr0);
    delete tptr0;
    tptr0 = tptr1;
  }
  thermals = NULL;

  delete td_state_noblend;
  td_state_noblend = NULL;
  delete td_state_blend;
  td_state_blend = NULL;

  gluDeleteQuadric(therm_quadric);
}

// Description: see header file
void initialize_wind_field(SimpleXMLTransfer* el)
{
  int loop;
  Thermal *temp_thermal;
  int xloop,yloop;

  dWindVelVar = 1;

  ThermalVersion = THERMAL_CODE;
  // Use version 3?
  {
    int index;

    index = el->indexOfChild("thermal");
    if (index >= 0)
    {
      el = el->getChildAt(index);
      index = el->indexOfChild("v3");
      if (index >= 0)
      {
        ThermalVersion = 3;
        thermalv3.init(el->getChildAt(index));
      }
    }
  }
  std::cout << "Using Thermal Simulation v" << ThermalVersion << "\n";

  // calculate number of thermals in the grid
  {
    double dDensity    = cfg->thermal->density;
    double dDensityMax = getMaxThermalDensity();

    if (dDensity > dDensityMax)
      dDensity = dDensityMax;

    num_thermals = (int)(pow(occupancy_grid_size*occupancy_grid_res,2)*
                         dDensity);
  }

#if THERMAL_TEST != 0
  num_thermals = 1;
#endif

  // How to draw thermals?
  {
    // How many grid elements to check?
    nDrawThermalsFromGrid = (int)(flThermalDistMax / occupancy_grid_res);
    int nGridCnt = (2*nDrawThermalsFromGrid+1) * (2*nDrawThermalsFromGrid+1);

    // The fast methods doesn't need that much computing power to determine
    // whether a thermal has to be drawn or not, so I use an additional factor.
    if (nGridCnt/3 > num_thermals)
      nDrawThermalsFromGrid = 0;

    if (td_state_noblend == NULL)
    {
      td_state_noblend = new ssgSimpleState();
      td_state_noblend->disable(GL_CULL_FACE);
      td_state_noblend->disable(GL_COLOR_MATERIAL);
      td_state_noblend->disable(GL_TEXTURE_2D);
      td_state_noblend->disable(GL_LIGHTING);
      td_state_noblend->disable(GL_BLEND);
    }
    if (td_state_blend == NULL)
    {
      td_state_blend = new ssgSimpleState();
      td_state_blend->disable(GL_CULL_FACE);
      td_state_blend->disable(GL_COLOR_MATERIAL);
      td_state_blend->disable(GL_TEXTURE_2D);
      td_state_blend->disable(GL_LIGHTING);
      td_state_blend->enable(GL_BLEND);
    }
  }

#if (THERMAL_CODE == 1)
  nInfluenceDist = 0;
#endif

  // initialize thermal grid with NULL
  for (xloop=0;xloop<occupancy_grid_size;xloop++)
  {
    for(yloop=0;yloop<occupancy_grid_size;yloop++)
    {
      thermal_occupancy_grid[xloop][yloop] = NULL;
#if (THERMAL_NEWPOSLOG != 0)
      NewPosLogArray[xloop][yloop] = 0;
      PosLogArray[xloop][yloop] = 0;
#endif
    }
  }

  // Create the said number of thermals as a linked list.
  thermals = NULL;
  for (loop=0;loop<num_thermals;loop++)
  {
    //~ temp_thermal=(Thermal *)malloc(sizeof(Thermal));
    temp_thermal = new Thermal();

    temp_thermal->next_thermal = thermals;
    thermals = temp_thermal;
  }

#if (USE_TURB_GRID != 0)
  // Fill turbulence grid
  turb_x_velocity[0]=0;
  turb_y_velocity[0]=0;
  turb_z_velocity[0]=0;
  for (loop=1;loop<turb_grid_size;loop++)
  {
    turb_x_velocity[loop]=turb_x_velocity[loop-1]+(0.03*gaussrand());
    turb_y_velocity[loop]=turb_y_velocity[loop-1]+(0.03*gaussrand());
    turb_z_velocity[loop]=turb_y_velocity[loop-1]+(0.03*gaussrand());
    turb_x_grad[loop]=gaussrand();
    turb_y_grad[loop]=gaussrand();
    turb_z_grad[loop]=gaussrand();
  }
#endif

  therm_quadric = gluNewQuadric();  
}

SimpleXMLTransfer* GetDefaultConf_Thermal()
{
  SimpleXMLTransfer* tex = new SimpleXMLTransfer();
  
  tex->setName("thermal");
  tex->addAttribute("strength_mean",        "5");
  tex->addAttribute("strength_sigma",       "1");
  tex->addAttribute("radius_mean",         "70");
  tex->addAttribute("radius_sigma",        "10");
  tex->addAttribute("density",              "2.4e-06");
  tex->addAttribute("lifetime_mean",      "240");
  tex->addAttribute("lifetime_sigma",      "60");

  tex->setAttribute("v3.vRefExp",    "2");
  tex->setAttribute("v3.dz_m",      "50");
  tex->setAttribute("v3.height_m", "600");
  
  tex->setAttribute("v3.inside.upper.r_m",     "30");
  tex->setAttribute("v3.inside.upper.sl_r",    "0.8");
  tex->setAttribute("v3.inside.upper.sl_dz_r", "0.2");
  tex->setAttribute("v3.inside.lower.r_m",     "20");
  tex->setAttribute("v3.inside.lower.sl_r",    "0.8");
  tex->setAttribute("v3.inside.lower.sl_dz_r", "0.2");
  
  tex->setAttribute("v3.outside.upper.r_m",     "65");
  tex->setAttribute("v3.outside.upper.sl_r",    "0");
  tex->setAttribute("v3.outside.upper.sl_dz_r", "0.7");
  tex->setAttribute("v3.outside.lower.r_m",     "65");
  tex->setAttribute("v3.outside.lower.sl_r",    "0");
  tex->setAttribute("v3.outside.lower.sl_dz_r", "0.7");
    
  return(tex);
}

// Description: see header file
void update_thermals(float flDeltaT)
{
  Thermal *thermal_ptr;
  float x_motion;   // How much has a thermal moved in X in the last timestep
  float y_motion;   // How much has a thermal moved in Y in the last timestep
  float x_wind_velocity,y_wind_velocity;
  float flWindVel = dWindVelVar * cfg->wind->getVelocity();

  // Wind velocity is the same everywhere, so every thermals relative movement is:
  x_wind_velocity = -1 * flWindVel * cos(M_PI*cfg->wind->getDirection()/180);
  y_wind_velocity = -1 * flWindVel * sin(M_PI*cfg->wind->getDirection()/180);
  x_motion        = flDeltaT * x_wind_velocity;
  y_motion        = flDeltaT * y_wind_velocity;

  // loop over linked list of thermals
  thermal_ptr = thermals;
  while (thermal_ptr != NULL)
  {
    thermal_ptr->update(flDeltaT, x_motion, y_motion);
    thermal_ptr = thermal_ptr->next_thermal;
  }
}

// Description: see header file
int calculate_wind(double  X_cg,      double  Y_cg,     double  Z_cg,
                   double& Vel_north, double& Vel_east, double& Vel_down)
{
  float    x_wind_velocity,y_wind_velocity,z_wind_velocity;//JL
  Thermal* thermal_ptr;
  int      aircraft_xcoord,aircraft_ycoord;
  int      xloop,yloop;
  double   thermal_wind_x=0;
  double   thermal_wind_y=0;
#if (THERMAL_CODE == 0)
  double distance_from_core;
  float total_up_airmass = 0;
  float sink_area;
  float sink_strength;
  float thermal_area;
  float lift_area  = 0;
  int   in_thermal = FALSE;
  float angle_in;
  float v_in_max;      // Max velocity of thermal vacuum cleaner wind.
#endif

  Vel_north = 0;
  Vel_east  = 0;
  Vel_down  = 0;
  Global::scenery->getWindComponents(X_cg, Y_cg, Z_cg, &x_wind_velocity, &y_wind_velocity, &z_wind_velocity);
  x_wind_velocity *= dWindVelVar;
  y_wind_velocity *= dWindVelVar;
  z_wind_velocity *= dWindVelVar;
  // calculate indices of the aircraft in the grid
  aircraft_xcoord = absToGridCoor(X_cg);
  aircraft_ycoord = absToGridCoor(Y_cg);

  // Is the aircraft in a part of the grid?
  if ((aircraft_xcoord > nInfluenceDist) &&
      (aircraft_xcoord < occupancy_grid_size-nInfluenceDist-1) &&
      (aircraft_ycoord > nInfluenceDist) &&
      (aircraft_ycoord < occupancy_grid_size-nInfluenceDist-1))
  {
    // Check all squares of the grid surrounding the aircraft in
    // a distance of at most (nInfluenceDist*occupancy_grid_res)
    // of the aircraft.
    // Sum lift_area and total_up_airmass.
    for (xloop=(-1*nInfluenceDist);xloop<=nInfluenceDist;xloop++)
    {
      for (yloop=(-1*nInfluenceDist);yloop<=nInfluenceDist;yloop++)
      {
        // is there a thermal in this part of the grid?
        thermal_ptr = thermal_occupancy_grid[aircraft_xcoord+xloop][aircraft_ycoord+yloop];

        if (thermal_ptr != 0)
        {
          switch (ThermalVersion)
          {
           case 3:
            thermal_ptr->sumVelocity(X_cg, Y_cg, Z_cg,
                                     thermalv3,
                                     thermal_wind_x, thermal_wind_y, Vel_down);
            break;

           default:
#if (THERMAL_CODE == 0)
            // area of this thermal
            thermal_area = (M_PI*thermal_ptr->radius*thermal_ptr->radius);
            //
            lift_area   += thermal_area;
            total_up_airmass+=thermal_area*thermal_ptr->strength;
#endif
#if (THERMAL_CODE == 1)
            Vel_down += thermal_ptr->getVelocity(X_cg, Y_cg, Z_cg);
#endif
            break;
          }
        }
      }
    }

    switch (ThermalVersion)
    {
     case 3:
      // there is no need to do something here
      break;

     default:
#if (THERMAL_CODE == 0)
      // Sink area and strength
      sink_area=((2*nInfluenceDist+1)*(2*nInfluenceDist+1)*
                 occupancy_grid_res*occupancy_grid_res)-lift_area;
      sink_strength= total_up_airmass/sink_area;

      // Check all squares of the grid surrounding the aircraft in
      // a distance of at most (nInfluenceDist*occupancy_grid_res)
      // of the aircraft.
      // Sum up thermal_wind_x and thermal_wind_y.
      for (xloop=(-1*nInfluenceDist);xloop<=nInfluenceDist;xloop++)
      {
        for (yloop=(-1*nInfluenceDist);yloop<=nInfluenceDist;yloop++)
        {
          if (thermal_occupancy_grid[aircraft_xcoord+xloop][aircraft_ycoord+yloop] != 0)
          {
            thermal_ptr=thermal_occupancy_grid[aircraft_xcoord+xloop][aircraft_ycoord+yloop];
            // Distance of the position in question and the thermal
            distance_from_core=sqrt(((X_cg-thermal_ptr->center_x_position)*(X_cg-thermal_ptr->center_x_position))
                                    +((Y_cg-thermal_ptr->center_y_position)*(Y_cg-thermal_ptr->center_y_position)));

            // If the positon is lower than 1000 feet, accumulate thermal_wind_x and thermal_wind_y.
            if (Z_cg > -1000)
            {
              v_in_max=thermal_ptr->strength*thermal_ptr->radius/100;
              if (distance_from_core > thermal_ptr->radius)
              {
                v_in_max/=pow(distance_from_core/thermal_ptr->radius,2);
              }
              else
              {
                v_in_max*=distance_from_core/thermal_ptr->radius;
              }
              angle_in=atan2((thermal_ptr->center_y_position-Y_cg),(thermal_ptr->center_x_position-X_cg));
              if (Z_cg > -50)
              {
                thermal_wind_x+=v_in_max*cos(angle_in);
                thermal_wind_y+=v_in_max*sin(angle_in);
              }
              else if (Z_cg > -1000)
              {
                thermal_wind_x+=(v_in_max*cos(angle_in))*((950-(-Z_cg-50))/950);
                thermal_wind_y+=(v_in_max*sin(angle_in))*((950-(-Z_cg-0))/950);
              }
            }

            if (distance_from_core < thermal_ptr->radius)
            {
              Vel_down  = -1*thermal_ptr->strength;
              in_thermal = TRUE;
            }
            else if (distance_from_core < thermal_ptr->radius+thermal_ptr->boundary_thickness)
            {
              Vel_down  = -1*thermal_ptr->strength
                + (thermal_ptr->strength+sink_strength)*((distance_from_core-thermal_ptr->radius)/thermal_ptr->boundary_thickness);
              in_thermal = TRUE;
            }
          }
        }
      }
      // end of loop
      //
      // If this is not in a thermal, sink_strength is used. If this is in a
      // thermal, Vel_down has been set in the loop above.
      if (!in_thermal)
      {
        Vel_down= sink_strength;
      }

      // thermals grow stronger from the ground up
      if (-Z_cg < 50)
      {
        Vel_down *= (-Z_cg/50);
      }
#endif
      break;
    }

    Vel_down += z_wind_velocity;
	  Vel_north= x_wind_velocity + thermal_wind_x;
    Vel_east = y_wind_velocity + thermal_wind_y;

    return(0);
  }
  else
  {
    // The aircraft is out of the grid.
    return(1);
  }
}

// Description: see header file
void draw_thermals(CRRCMath::Vector3 pos)
{
  Thermal* thermal_ptr;

  double X_cg_rwy =  pos.r[0];
  double Y_cg_rwy =  pos.r[1];
  double H_cg_rwy = -pos.r[2];

  if (nDrawThermalsFromGrid)
  {
    // grid coordinates of aircraft
    int xa = absToGridCoor(X_cg_rwy);
    int ya = absToGridCoor(Y_cg_rwy);

    // part of the grid to look at
    int xmin = xa - nDrawThermalsFromGrid;
    int xmax = xa + nDrawThermalsFromGrid;
    int ymin = ya - nDrawThermalsFromGrid;
    int ymax = ya + nDrawThermalsFromGrid;

    if (xmin < 1)
      xmin = 1;
    if (xmax > occupancy_grid_size - 2)
      xmax = occupancy_grid_size - 2;
    if (ymin < 1)
      ymin = 1;
    if (ymax > occupancy_grid_size - 2)
      ymax = occupancy_grid_size - 2;

    for (int x=xmin; x<=xmax; x++)
      for (int y=ymin; y<=ymax; y++)
      {
        thermal_ptr = thermal_occupancy_grid[x][y];
        if (thermal_ptr != NULL)
        {
          thermal_ptr->draw(H_cg_rwy);
        }
      }
  }
  else
  {
    thermal_ptr = thermals;
    while(thermal_ptr !=NULL)
    {
      if (fabs(X_cg_rwy - thermal_ptr->center_x_position) < flThermalDistMax &&
          fabs(Y_cg_rwy - thermal_ptr->center_y_position) < flThermalDistMax)
      {
        thermal_ptr->draw(H_cg_rwy);
      }
      thermal_ptr = thermal_ptr->next_thermal;
    }
  }

}

void draw_wind(double direction_face)
{
  double length    = 0.8*dWindVelVar;
  double direction = (M_PI*cfg->wind->getDirection()/180) - direction_face;

  //
  int xsize, ysize;
  Video::getWindowSize(xsize, ysize);
  int h = ysize >> 3;
  int r = ysize >> 5;
  int dxA = (int)(floor(0.5*h*length*sin(direction-0.1)+0.5));
  int dyA = (int)(floor(0.5*h*length*cos(direction-0.1)+0.5));
  int dxB = (int)(floor(0.5*h*length*sin(direction+0.1)+0.5));
  int dyB = (int)(floor(0.5*h*length*cos(direction+0.1)+0.5));
  int dxC = (int)(floor(-0.5*h*length*sin(direction+0.1)+0.5));
  int dyC = (int)(floor(-0.5*h*length*cos(direction+0.1)+0.5));

#if 0
  glDisable(GL_LIGHTING);
  glMatrixMode (GL_PROJECTION);
  glPushMatrix();
  glLoadIdentity ();
  gluOrtho2D (0, xsize-1, 0, ysize);
#endif

  // Hintergrund
  glColor3f (0, 0, 0);
  glTranslatef(xsize-r-h/2, r+h/2, 0);
  gluDisk(therm_quadric, 0, h/2, 32, 1);
  glTranslatef(-(xsize-r-h/2),-(r+h/2),0.1);

  // Anzeiger
  glColor3f (0, 1, 0.);
  glBegin(GL_LINE_STRIP);
  glVertex2i(xsize - r - h/2 - dxC, r+h/2 - dyC);
  glVertex2i(xsize - r - h/2 - dxA, r+h/2 - dyA);
  glVertex2i(xsize - r - h/2 - dxB, r+h/2 - dyB);
  glVertex2i(xsize - r - h/2 - dxC, r+h/2 - dyC);
  glEnd();

#if 0
  glPopMatrix();
  glEnable(GL_LIGHTING);
#endif
}

#if DEBUG_THERMAL_SCRSHOT == 1

// 0: output for scilab
// 1: output for gnuplot (1)
// 2: output for octave
// 3: output for gnuplot (3)
# define DEBUG_THERMAL_SCRSHOT_FORMAT 3

void windfield_thermalScreenshot(CRRCMath::Vector3 pos)
{
  /**
   * using gnuplot (1):
   * ------------------
   * set contour surface
   * set cntrparam levels 20
   * splot "thermals.dat" with dots
   *
   * or
   *
   * unset contour
   * set zrange [-0.6:6]
   * splot "thermals.dat" with dots
   *
   * using gnuplot (3):
   * ------------------
   * splot "thermals.dat" matrix with line palette
   *
   * or
   *
   * set pm3d
   * splot "thermals.dat" matrix with pm3d
   *
   * or
   *
   * set hidden3d
   * splot "thermals.dat" matrix with lines
   *
   *
   * using scilab:
   * -------------
   * thfield = file("open", "thermalsb.dat", "old")
   * n= 2 * read(thfield, 1, 1) + 1
   * P = read(thfield, n, n);
   * x = linspace(0, n-1, n);
   * y = x;
   * plot3d(x,y,P);
   * file("close", thfield);
   *
   * using octave:
   * -------------
   * vdown = load -text thermals.dat * ;
   * vdl = length(vdown.vdown)
   * nums = linspace(0, vdl-1, vdl);
   * [xx, yy] = meshgrid(nums, nums) ;
   * mesh(xx, yy, vdown.vdown)
   *
   */

  const double dDist = 200;
  const double dStep = 4;
  const float  dirlen = 14.0;

  std::ofstream tf_up;
  std::ofstream tf_northeast;
  double        dVNorth;
  double        dVEast;
  double        dVDown;

  const int nSteps = (int)(dDist/dStep);

  tf_up.open("thermals.dat");
  tf_northeast.open("thermals_northeast.dat");

# if DEBUG_THERMAL_SCRSHOT_FORMAT == 0
  tf_up << nSteps << "\n";
# endif
# if DEBUG_THERMAL_SCRSHOT_FORMAT == 2
  tf_up << "# name: vdown\n";
  tf_up << "# type: matrix\n";
  tf_up << "# rows: " << 2*nSteps << "\n";
  tf_up << "# columns: " << 2*nSteps << "\n";
# endif

  for (int nX=-nSteps; nX<nSteps; nX++)
  {
    double x = pos.r[0] + nX*dStep;

    for (int nY=-nSteps; nY<nSteps; nY++)
    {
      double y = pos.r[1] + nY*dStep;

      calculate_wind(x, y, pos.r[2], dVNorth, dVEast, dVDown);

# if (DEBUG_THERMAL_SCRSHOT_FORMAT == 0) || (DEBUG_THERMAL_SCRSHOT_FORMAT == 2) || (DEBUG_THERMAL_SCRSHOT_FORMAT == 3)
      tf_up << -1*dVDown << " ";
# endif
# if DEBUG_THERMAL_SCRSHOT_FORMAT == 1
      tf_up  << x << " " << y << " " << -1*dVDown << "\n";
# endif

      // use:  plot 'thermals_northeast.dat' with vectors
      tf_northeast << x              << " " << y             << " ";
      tf_northeast << dirlen*dVNorth << " " << dirlen*dVEast << "\n";

    }
    tf_up << "\n";

  }
  tf_up.close();
  tf_northeast.close();

# if (THERMAL_NEWPOSLOG != 0)
  /*
   Using gnuplot:
   *
   set pm3d map
   splot  'thermalnewpos.dat' matrix with pm3d
   */
  {
    std::ofstream tlog;
    tlog.open("thermalnewpos.dat");
    for (unsigned int xc=0; xc<occupancy_grid_size; xc++)
    {
      for (unsigned int yc=0; yc<occupancy_grid_size; yc++)
      {
        tlog << (NewPosLogArray[xc][yc]) << " ";
      }
      tlog << "\n";
    }
    tlog.close();

    tlog.open("thermalpos.dat");
    for (unsigned int xc=0; xc<occupancy_grid_size; xc++)
    {
      for (unsigned int yc=0; yc<occupancy_grid_size; yc++)
      {
        tlog << PosLogArray[xc][yc] << " ";
      }
      tlog << "\n";
    }
    tlog.close();
  }
# endif
}
#endif

// ----- implementation of class Thermal --------------------
/**
 *  The constructor. Creates the object and initializes it
 *  with some random values.
 */
Thermal::Thermal()
{
  random_init();
  // to have a higher level of initial randomness:
  lifetime *= rand()/(RAND_MAX+1.0);
}

/**
 *  Formerly known as make_new_thermal(). This method
 *  initializes the object with some sensible random values.
 */
void Thermal::random_init()
{
  float xpos,ypos;
  int   xco,yco;

  // determine position of new thermal
  fInvisible = find_new_thermal_position(&xpos,&ypos,&xco,&yco);

#if (THERMAL_NEWPOSLOG != 0)
  if (!fInvisible)
  {
    NewPosLogArray[xco][yco]++;
  }
#endif

#if THERMAL_TEST != 0
# if THERMAL_TEST == 1
  xpos = -160;
  ypos = -57;
# endif
# if THERMAL_TEST == 2
  xpos = -170;
  ypos = -0;
# endif
  xcoord = absToGridCoor(xpos);
  ycoord = absToGridCoor(ypos);
#endif

  // Put it into the grid. If no valid position was found, this thermal stays invisible during
  // its current lifecycle.
  if (!fInvisible)
    thermal_occupancy_grid[xco][yco] = this;

  // Describe thermal
  center_x_position = xpos;
  center_y_position = ypos;
  xcoord            = xco;
  ycoord            = yco;
  radius=(gaussrand()*cfg->thermal->radius_sigma)+cfg->thermal->radius_mean;
#if (THERMAL_CODE == 0)
  // todo: is this boundary thickness correct? Until 2005-01-15 the initial thermals
  // have not been created using this code. It was radius/5 there.
  // 2005-01-20: gradient is very high -- using /5 now.
  boundary_thickness = radius/5;
#endif
  strength=(gaussrand()*cfg->thermal->strength_sigma)+
    cfg->thermal->strength_mean;
  lifetime=(gaussrand()*cfg->thermal->lifetime_sigma)+
    cfg->thermal->lifetime_mean;

#if THERMAL_TEST != 0
  radius   = 50;
  strength = 15;
  lifetime = 9999;
#endif

  switch (ThermalVersion)
  {
   case 3:
    {
      int nDist = (int)ceil((radius * thermalv3.get_r_max()/thermalv3.get_r_ref())/occupancy_grid_res);
      if (nDist > nInfluenceDist)
        nInfluenceDist = nDist;
    }
    break;

   default:
#if (THERMAL_CODE == 1)
    {
      int nDist = (int)ceil((radius/ThermalRadius)/occupancy_grid_res);
      if (nDist > nInfluenceDist)
        nInfluenceDist = nDist;
    }
#endif
    break;
  }
}

/**
 *  Remove a thermal from the grid
 */
void Thermal::remove_from_grid()
{
  thermal_occupancy_grid[xcoord][ycoord] = NULL;
}

/**
 *  Update the thermal. The thermal will move with the windfield
 *  and slowly die. The movement due to the windfield is calculated
 *  outside of this function because it is the same for all
 *  thermals.
 *
 *  \param flDeltaT elapsed time since last update
 *  \param x_motion x movement due to wind
 *  \param y_motion y movement due to wind
 */
void Thermal::update(float flDeltaT, float x_motion, float y_motion)
{
  // Move thermal
  center_x_position += x_motion;
  center_y_position += y_motion;
  // let it grow older
  lifetime -= flDeltaT;

  // This thermal has to replaced by a new one if its lifetime is over or if
  // it has moved out of the grid.
  if ((lifetime < 0) ||
      (center_x_position <  -1*(occupancy_grid_size/2) * occupancy_grid_res) ||
      (center_x_position >     (occupancy_grid_size/2) * occupancy_grid_res) ||
      (center_y_position <  -1*(occupancy_grid_size/2) * occupancy_grid_res) ||
      (center_y_position >     (occupancy_grid_size/2) * occupancy_grid_res))
  {
    // remove thermal from the grid
    remove_from_grid();

    // create a new thermal
    random_init();
  }
  else if (!fInvisible)
  {
    int   new_xcoord,new_ycoord;
    // new indices into grid
    new_xcoord = absToGridCoor(center_x_position);
    new_ycoord = absToGridCoor(center_y_position);

    // has it moved to a new square of the grid?
    if ((new_xcoord != xcoord) ||
        (new_ycoord != ycoord))
    {
      // Is this place in the grid occupied by another thermal?
      if (thermal_occupancy_grid[new_xcoord][new_ycoord]!=0)
      {
        // This should never happen with nGridDistMin>0. If it does,
        // there is work to be done.
        // It does! Why? I do NOT understand it...
        fprintf(stderr, "Error: multiple thermals in one location!\n");
      }
      else
      {
        // leave the old place in the grid
        thermal_occupancy_grid[xcoord][ycoord] = NULL;
        // enter the new place in the grid
        thermal_occupancy_grid[new_xcoord][new_ycoord] = this;
        xcoord = new_xcoord;
        ycoord = new_ycoord;
      }
    }
#if (THERMAL_NEWPOSLOG != 0)
    PosLogArray[xcoord][ycoord]++;
#endif
  }
}

#if (THERMAL_CODE == 1)
/**
 *  Calculate the influence of a thermal on the given
 *  location.
 *
 *  \param dX X location in world coordinates
 *  \param dY Y location in world coordinates
 *  \param dZ Z location in world coordinates
 *  \return vertical thermal velocity
 */
double Thermal::getVelocity(double dX, double dY, double dZ)
{
  // distance from aircraft to center of thermal
  double dDist   = sqrt((center_x_position - dX)*(center_x_position - dX)
                        +
                        (center_y_position - dY)*(center_y_position - dY));
  // radius of thermal, including downwind
  double dRadius = radius/ThermalRadius;

  if (dDist >= dRadius || dZ > -dAltitudeZeroStrength)
    return(0);
  else
  {
    int nIndex = (int)((1<<(ThermalProfile_bits+8)) * dDist/dRadius);

    // it gets weaker before it dies
    double current_strength;

    if (lifetime < dFadeOutTime)
      current_strength = strength * lifetime / dFadeOutTime;
    else
      current_strength = strength;

    if (dZ > -dAltitudeFullStrength)
      current_strength *= (-dZ - dAltitudeZeroStrength) / (dAltitudeFullStrength - dAltitudeZeroStrength);

    // interpolation of table values
    double dVal0 = ThermalProfile[ nIndex>>8   ];
    double dVal1 = ThermalProfile[(nIndex>>8)+1];

    return(-1*(dVal0 + (nIndex&0xFF)*(dVal1-dVal0)/256) * current_strength);
  }
}
#endif

/**
 *  Draws a thermal
 *
 *  \param H_cg_rwy height at which the thermal shall be drawn
 */
void Thermal::draw(double H_cg_rwy)
{
#if THERMAL_TEST != 0
  if (H_cg_rwy < 3*dAltitudeFullStrength)
    H_cg_rwy = 3*dAltitudeFullStrength;
#endif

  glPushMatrix();
  td_state_noblend->apply();

#if (THERMAL_CODE == 0)
  glColor4f(1,0,0,1);
  glTranslatef(center_y_position, H_cg_rwy, -center_x_position);
  gluSphere(therm_quadric,1,3,3);
  td_state_blend->apply();
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glRotatef(90,1,0,0);
  glColor4f(0.4,0,0,0.2);
  gluDisk(therm_quadric,0, radius + boundary_thickness,16,1);
#endif

#if (THERMAL_CODE == 1)
  if (H_cg_rwy > dAltitudeZeroStrength)
  {
    double strength_height;

    if (H_cg_rwy < dAltitudeFullStrength)
      strength_height = 0.2 * (H_cg_rwy - dAltitudeZeroStrength) / (dAltitudeFullStrength - dAltitudeZeroStrength);
    else
      strength_height = 0.2;

    glColor4f(1,0,0,1);
    glTranslatef(center_y_position, H_cg_rwy, -center_x_position);
    gluSphere(therm_quadric,1,3,3);
    td_state_blend->apply();
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glRotatef(90,1,0,0);
    glColor4f(0.4,0,0, strength_height);
    gluDisk(therm_quadric,0, radius, 16, 1);

    // The whole radius of the thermal is limited to not get annoying.
    double RadiusInnerPartRel = ThermalRadius;
    if (RadiusInnerPartRel < 0.4)
      RadiusInnerPartRel = 0.4;

    double dRadius = radius / RadiusInnerPartRel;
    glColor4f(0,0.4,0, strength_height);
    gluDisk(therm_quadric, radius, dRadius, 16,1);
  }
#endif
  glPopMatrix();
}

void Thermal::sumVelocity(double X_cg, double Y_cg, double Z_cg,
                          ThermikSchalen& thermalv3,
                          double& Vel_north, double& Vel_east, double& Vel_down)
{
  // it gets weaker before it dies
  double current_strength;

  if (lifetime < dFadeOutTime)
    current_strength = strength * lifetime / dFadeOutTime;
  else
    current_strength = strength;

  // distance from aircraft to center of thermal
  double dDist   = sqrt((center_x_position - X_cg)*(center_x_position - X_cg)
                        +
                        (center_y_position - Y_cg)*(center_y_position - Y_cg));

  //
  double localLenToThLen = thermalv3.get_r_ref()/radius;
  flttype dx, dy;

  thermalv3.vectorAt(dDist*localLenToThLen, -1*Z_cg*localLenToThLen,
                     dx, dy,
                     current_strength);

  Vel_down -= dy;

  // split up dx into vnorth and veast:
  float alpha  = atan2(Y_cg - center_y_position, X_cg - center_x_position);
  float vnorth = cos(alpha) * dx;
  float veast  = sin(alpha) * dx;
  Vel_north += vnorth;
  Vel_east  += veast;
}

double getMaxThermalDensity()
{
  return(1.0 / (
                (2*nGridDistMin+1) * occupancy_grid_res * (2*nGridDistMin+1) * occupancy_grid_res
                ));
}
