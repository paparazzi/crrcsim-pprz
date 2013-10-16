/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 *   Copyright (C) 2000, 2001 Jan Kansky (original author)
 *   Copyright (C) 2004-2010 Jan Reucker
 *   Copyright (C) 2004, 2005, 2006, 2008 Jens Wilhelm Wulf
 *   Copyright (C) 2005 Chris Bayley
 *   Copyright (C) 2005 Lionel Cailler
 *   Copyright (C) 2006 Todd Templeton
 *   Copyright (C) 2009 Joel Lienard
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

/** \file model_based_scenery.cpp
 *  This file defines a "scenery" class which contains all data
 *  and methods to construct and draw the landscape.
 */

#include <crrc_config.h>

#include <iostream>
#include <iomanip>

#include "model_based_scenery.h"
#include "../crrc_main.h"
#include "../mod_misc/SimpleXMLTransfer.h"
#include "../mod_misc/filesystools.h"
#include "hd_tilingterrain.h"
#include "wind_from_terrain.h"

#if WINDDATA3D != 1
#include "../GUI/crrc_msgbox.h"
#endif

// This module uses some internal SSG stuff from the video module!
#include "../mod_video/crrc_ssgutils.h"

const float FEET2METERS=0.3048;


/****************************************************************************/
/* Model based scenery                                                      */
/****************************************************************************/
ModelBasedScenery::ModelBasedScenery(SimpleXMLTransfer *xml, int sky_variant)
    : Scenery(xml, sky_variant), location(Scenery::MODEL_BASED)
{
  ssgEntity *model = NULL;
  SimpleXMLTransfer *scene = xml->getChild("scene", true);
  getHeight_mode = scene->attributeAsInt("getHeight_mode", 2);
  //std::cout << "----getHeight_mode : " <<  getHeight_mode <<std::endl;
  SceneGraph = new ssgRoot();

  // Create an "invisible" state. This state actually makes a node
  // visible in a predefined way. This is used to visualize invisible
  // objects (e.g. collision boxes).
  invisible_state = new ssgSimpleState();
  invisible_state->disable(GL_COLOR_MATERIAL);
  invisible_state->disable(GL_TEXTURE_2D);
  invisible_state->enable(GL_LIGHTING);
  invisible_state->enable(GL_BLEND);
  //invisible_state->setShadeModel(GL_SMOOTH);
  //invisible_state->setShininess(0.0f);
  invisible_state->setMaterial(GL_EMISSION, 0.0, 0.0, 0.0, 0.0);
  invisible_state->setMaterial(GL_AMBIENT, 1.0, 0.0, 0.0, 0.5);
  invisible_state->setMaterial(GL_DIFFUSE, 1.0, 0.0, 0.0, 0.5);
  invisible_state->setMaterial(GL_SPECULAR, 1.0, 0.0, 0.0, 0.5);

  // transform everything from SSG coordinates to CRRCsim coordinates
  initial_trans = new ssgTransform();
  SceneGraph->addKid(initial_trans);
//10.76
  sgMat4 it = {  {1,  0.0,  0.0,   0},
    {0.0,  0.0, -1,   0},
    {0.0,  1,  0.0,   0},
    {0.0,  0.0,  0.0, 1.0}
  };

  initial_trans->setTransform(it);

  // find all "objects" defined in the file
  int num_children = scene->getChildCount();

  for (int cur_child = 0; cur_child < num_children; cur_child++)
  {
    SimpleXMLTransfer *kid = scene->getChildAt(cur_child);
    // only use "object" tags
    if (kid->getName() == "object")
    {
      std::string filename = kid->attribute("filename", "not_specified");
      bool is_terrain = (kid->attributeAsInt("terrain", 1) != 0);
      bool is_visible = (kid->attributeAsInt("visible", 1) != 0);

      // PLIB automatically loads the texture file,
      // but it does not know which directory to use.
      // Where is the object file?
      std::string    of  = FileSysTools::getDataPath("objects/" + filename, TRUE);
      // compile and set relative texture path
      std::string    tp  = of.substr(0, of.length()-filename.length()-1-7) + "textures";
      ssgTexturePath(tp.c_str());

      // load model
      std::cout << "Loading 3D object \"" << of.c_str() << "\"";
      if (is_terrain)
      {
        std::cout << " (part of terrain)";
      }
      if (!is_visible)
      {
        std::cout << " (invisible)";
      }
      std::cout << std::endl;
      model = ssgLoad(of.c_str());
      if (model != NULL)
      {
        if (!is_visible)
        {
          setToInvisibleState(model);
        }
        
        // The model may contain internal node attributes (e.g. for
        // integrated collision boxes). Parse these attributes now.
        evaluateNodeAttributes(model);
        
        
        // now parse the instances and place the model in the SceneGraph
        for (int cur_instance = 0; cur_instance < kid->getChildCount(); cur_instance++)
        {
          SimpleXMLTransfer *instance = kid->getChildAt(cur_instance);
          if (instance->getName() == "instance")
          {
            sgCoord coord;
            
            // try north/east/height first, then fallback to x/y/z
            try
            {
              coord.xyz[SG_X] = instance->attributeAsDouble("east");
            }
            catch (XMLException &e)
            {
              coord.xyz[SG_X] = instance->attributeAsDouble("y", 0.0);
            }
            try
            {
              coord.xyz[SG_Y] = instance->attributeAsDouble("north");
            }
            catch (XMLException &e)
            {
              coord.xyz[SG_Y] = instance->attributeAsDouble("x", 0.0);
            }
            try
            {
              coord.xyz[SG_Z] = instance->attributeAsDouble("height");
            }
            catch (XMLException &e)
            {
              coord.xyz[SG_Z] = instance->attributeAsDouble("z", 0.0);
            }
            coord.hpr[0] = 180 - instance->attributeAsDouble("h", 0.0);
            coord.hpr[1] = -instance->attributeAsDouble("p", 0.0);
            coord.hpr[2] = -instance->attributeAsDouble("r", 0.0);

            std::cout << std::setprecision(1);
            std::cout << "  Placing instance at " << coord.xyz[SG_X] << ";" << coord.xyz[SG_Y] << ";" << coord.xyz[SG_Z];
            std::cout << ", orientation " << (180-coord.hpr[0]) << ";" << -coord.hpr[1] << ";" << -coord.hpr[2] << std::endl;
            std::cout << std::setprecision(6);
            ssgTransform *trans = new ssgTransform();
            trans->setTransform(&coord);
            
            // In PLIB::SSG, intersection testing is done by a tree-walking
            // function. This can be influenced by the tree traversal mask
            // bits. The HOT and LOS flags are cleared for objects that are
            // not part of the terrain, so that the height-of-terrain and
            // line-of-sight algorithms ignore this branch of the tree.
            if (!is_terrain)
            {
              trans->clrTraversalMaskBits(SSGTRAV_HOT | SSGTRAV_LOS);
            }
            // Objects are made invisible by clearing the CULL traversal flag.
            // This means that ssgCullAndDraw will ignore this branch.
            if (!is_visible)
            {
              trans->clrTraversalMaskBits(SSGTRAV_CULL);
            }
            initial_trans->addKid(trans);
            trans->addKid(model);
          }
        }
      }
    }
  }
  /*memorise H of Terrain */
  if ( getHeight_mode==1)
  {
    make_tab_HeightAndPlane();
  }
  
  if ( getHeight_mode==2)
  {
    heightdata = new HD_TilingTerrain(SceneGraph);
  }
  else
  {
    heightdata = NULL;
  }

  //wind
  SimpleXMLTransfer *wind = xml->getChild("wind", true);
  std::string wind_filename = wind->attribute("filename","");  
#if WINDDATA3D == 1
  wind_data = 0;//default : no wind_data
  std::string wind_position_unit = wind->attribute("unit","");
  try {
    flDefaultWindDirection = wind->attributeAsInt("direction");
    ImposeWindDirection = true;
    }
  catch (XMLException)
    {
    // if not attribut "direction", normal mode
    }
  
  if (wind_position_unit.compare("m")==0)
  {
    wind_position_coef = FEET2METERS;
  }
  else
  {
    wind_position_coef = 1;
  }
  std::cout << "wind file name :  " << wind_filename.c_str()<< std::endl;
  if (wind_filename.length() > 0)
  {
    std::cout << "init wind ---------";
    int n = init_wind_data((wind_filename.c_str()));
    std::cout << n << "  points processed" << std::endl;
  }
#else
  if (wind_filename.length() > 0)
  {
    new CGUIMsgBox("Insufficient configuration to read windfields.");
  }
#endif
}


void ModelBasedScenery::setToInvisibleState(ssgEntity* ent)
{
  if (ent->isAKindOf(ssgTypeLeaf()))
  {
    ssgLeaf *leaf = (ssgLeaf*)ent;

    if (leaf->hasState())
    {
      leaf->setState(invisible_state);
    }
  }
  else if (ent->isAKindOf(ssgTypeBranch()))
  {
    ssgBranch *branch = (ssgBranch*)ent;

    // continue down the hierarchy
    int kids = branch->getNumKids();
    for (int i = 0; i < kids; i++)
    {
      ssgEntity* currKid = branch->getKid(i);
      setToInvisibleState(currKid);
    }
  }
}


/**
 * Recursively walk a scene graph and evaluate any node
 * attributes placed inside the name strings of leaves.
 *
 */
void ModelBasedScenery::evaluateNodeAttributes(ssgEntity* ent)
{
  if (ent->isAKindOf(ssgTypeLeaf()))
  {
    SSGUtil::NodeAttributes attr = SSGUtil::getNodeAttributes(ent);
    std::string node_name = SSGUtil::getPureNodeName(ent);
    
    if (attr.checkAttribute("terrain") == -1)
    {
      ent->clrTraversalMaskBits(SSGTRAV_HOT | SSGTRAV_LOS);
      std::cout << "  leaf \"" << node_name 
                << "\" is excluded from terrain calculations (found -terrain attribute)"
                << std::endl;
    }
    if (attr.checkAttribute("visible") == -1)
    {
      std::cout << "  leaf \"" << node_name 
                << "\" is invisible (found -visible attribute)"
                << std::endl;
      SSGUtil::spliceBranch(new ssgInvisible(), ent);
    }
    
  }
  else if (ent->isAKindOf(ssgTypeBranch()))
  {
    ssgBranch *branch = (ssgBranch*)ent;

    // continue down the hierarchy
    int kids = branch->getNumKids();
    for (int i = 0; i < kids; i++)
    {
      ssgEntity* currKid = branch->getKid(i);
      evaluateNodeAttributes(currKid);
    }
  }
}


void ModelBasedScenery::make_tab_HeightAndPlane()
{
  int i,j;
  float x,y,h;
  for (i=0; i < SIZE_GRID_PLANES; i++)
  {
    for (j=0; j < SIZE_GRID_PLANES; j++)
    {
      x = (i- SIZE_GRID_PLANES/2)*SIZE_CELL_GRID_PLANES;
      y = (j- SIZE_GRID_PLANES/2)*SIZE_CELL_GRID_PLANES;
      h=getHeightAndPlane_(x,  y, tab_HeightAndPlane[i][j]);
      tab_HOT [i][j] =h;
    }
  }

}

ModelBasedScenery::~ModelBasedScenery()
{
  delete SceneGraph;
  delete heightdata;
#if WINDDATA3D == 1
  if (wind_data)
    delete wind_data;
#endif
}

void ModelBasedScenery::draw(double current_time)
{
  ssgCullAndDraw(SceneGraph);
}

float ModelBasedScenery::getHeight(float x_north, float y_east)
{
  return getHeightAndPlane(x_north,  y_east, NULL);
}


float ModelBasedScenery::getHeightAndPlane(float x, float y, float tplane[4])
{
  if ( getHeight_mode==1)
  {
    int i,j;
    float tplane_loc[4];
    float dx, dy, hot;;
    i = (int)(x/SIZE_CELL_GRID_PLANES);
    dx = (x - i*SIZE_CELL_GRID_PLANES)/SIZE_CELL_GRID_PLANES;
    i  += (SIZE_GRID_PLANES/2);
    j = (int)(y/SIZE_CELL_GRID_PLANES);
    dy = (y - j*SIZE_CELL_GRID_PLANES)/SIZE_CELL_GRID_PLANES;
    j  += (SIZE_GRID_PLANES/2);
    if (i<0)
    {
      i=0;
      dx=0;
    }
    if (i >= SIZE_GRID_PLANES)
    {
      i = SIZE_GRID_PLANES-1;
      dx=0;
    }
    if (j<0)
    {
      j=0;
      dy=0;
    }
    if (j >= SIZE_GRID_PLANES)
    {
      j = SIZE_GRID_PLANES-1;
      dy=0;
    }
    sgCopyVec4 (tplane_loc,tab_HeightAndPlane[i][j]);
    if (tplane) sgCopyVec4 (tplane, tplane_loc);
    hot =
      (1-dy)* ( (1-dx)*tab_HOT[i][j]  + dx*tab_HOT[i+1][j]  )
      +  dy * ( (1-dx)*tab_HOT[i][j+1]+ dx*tab_HOT[i+1][j+1]);
    return hot;
  }
  else
  {
    float hot;
    if (getHeight_mode==2)
      hot  = heightdata->getHeightAndPlane(x,  y,  tplane);
    else
      hot = getHeightAndPlane_(x,  y,  tplane);
    return hot;
  }
}

float ModelBasedScenery::getHeightAndPlane_(float x_north, float y_east, float tplane[4])
{
  ssgHit *results ;
  int num_hits ;
  float hot ;   /* H.O.T == Height Of Terrain */
  sgVec3   s;
  sgSetVec3(s, 0.0, 1.0, 0.0);
  sgMat4 m = {  {  1,   0,   0,   0},
                {  0,   1,   0,   0},
                {  0,   0,   1,   0},
                {-y_east , 0, x_north, 1.0}
              };
  num_hits = ssgLOS(SceneGraph, s, m, &results ) ;
#define DEEPEST_HELL  -9999.0
  hot = DEEPEST_HELL ;
  int numero=-1;
  for ( int i = 0 ; i < num_hits ; i++ )
  {
    ssgHit *h = &results [ i ] ;
    float hgt = - h->plane[3] / h->plane[1] ;
    if ( hgt >= hot )
    {
      hot = hgt ;
      numero = i;
    }
  }
  if ( tplane )
  {
    if ( numero >= 0)
    {
      sgCopyVec4 ( tplane, results [ numero ].plane ) ;
      tplane[3] = tplane[3] - tplane[0]*y_east + tplane[2]*x_north;
      if (tplane[1]<0) /*  ??? revoir :  preferable d'orienter correctement les facettes */
        sgNegateVec4 ( tplane , tplane );
      //std::cout << "----plane***" <<  tplane[0] <<"  "<<  tplane[1] <<"  "<<  tplane[2]<<"  "<<  tplane[3] <<std::endl;
    }
    else
    {
      tplane[0] = .0;
      tplane[1] = 1.0;
      tplane[2] = 0.0;
      tplane[3] = -hot;
    }
  }
  return hot;
}
/****/
void ModelBasedScenery::getWindComponents(double X,double  Y,double  Z,
    float  *x_wind_velocity, float  *y_wind_velocity, float  *z_wind_velocity)
{
#if WINDDATA3D == 1
  float  flWindVel = cfg->wind->getVelocity();
  //import wind data from file
  float x,y,z,vx,vy,vz;
  if (wind_data)
  {
    x = X * wind_position_coef;
    y  = Y * wind_position_coef;
    z  = -Z * wind_position_coef;
    int ret = find_wind_data(x,y,z,&vx,&vy,&vz);
    if (ret)
    {
      *x_wind_velocity = vx * flWindVel;
      *y_wind_velocity = vy * flWindVel;
      *z_wind_velocity = vz * flWindVel;
    }
    else
    {
      *x_wind_velocity = 0.0;
      *y_wind_velocity = 0.0;
      *z_wind_velocity = 0.0;
    }
    //std::cout << "----at"<< x<<"  "<<y<<"  "<< z<<"wind components***" << *x_wind_velocity <<"  "<< *y_wind_velocity <<"  "<<  *z_wind_velocity<<std::endl;
  }
  else
#endif
  {
  //default mode
    wind_from_terrain( X,  Y,  Z, x_wind_velocity, y_wind_velocity, z_wind_velocity);
  }
}
