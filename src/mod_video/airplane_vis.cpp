/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 * Copyright (C) 2009 Jan Reucker (original author)
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
  
/** \file airplane_vis.cpp
 * 
 *  This file is all about airplane visualization.
 */
#include "../i18n.h"
#include "airplane_vis.h"
#include "crrc_ssgutils.h"
#include "crrc_graphics.h"
#include "shadow.h"
#include <list>
#include <string>
#include <stdexcept>
#include <iostream>
#include <sstream>

#include "../global.h"  // only for LOG()


namespace Video
{

// Define this to 1 to use the stencil buffer for shadowing
// (if available).
#define EXPERIMENTAL_STENCIL_SHADOW 1
  

/// \todo there should be only one #define. Currently there are two
///       (inside and outside the namespace)
#define INVALID_AIRPLANE_VISUALIZATION -1

  
std::vector<AirplaneVisualization*> AirplaneVisualization::ListOfVisualizations;


#if EXPERIMENTAL_STENCIL_SHADOW == 1
int shadowPredrawCallback(ssgState*)
{
  glStencilMask(1);
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_GREATER, 1, 1);
  glStencilOp(GL_KEEP, GL_REPLACE, GL_REPLACE);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  return 0;
}

int shadowPostdrawCallback(ssgState*)
{
  glDisable(GL_STENCIL_TEST);
  return 0;
}
#endif

/** \brief Create a "shadow" instance of a model
 *
 *  This function operates on a clone of a model and sets all associated
 *  states to a "shadowy" look. If it is called for an entity that has the
 *  "-shadow" attribute set, return true to signal the caller that he has to
 *  remove this entity from the graph.
 *  
 *
 *  \param ent  A clone of the "real" model
 *  \retval true The current entity has to be removed from the graph
 *  \retval false No action required
 */
bool makeShadow(ssgEntity *ent)
{
  bool boRemoveCurrentEntity = false;
  
  if (ent->isAKindOf(ssgTypeLeaf()))
  {
    ssgLeaf *leaf = (ssgLeaf*)ent;

    SSGUtil::NodeAttributes attr = SSGUtil::getNodeAttributes(leaf);
    if (attr.checkAttribute("shadow") == -1)
    {
      // no shadow --> remove this entity from all parents
      boRemoveCurrentEntity = true;
    }
    else if (leaf->hasState())
    {
      ssgSimpleState *state = (ssgSimpleState*)leaf->getState();
      state->disable(GL_COLOR_MATERIAL);
      state->disable(GL_TEXTURE_2D);
      state->enable(GL_LIGHTING);
      state->enable(GL_BLEND);
      state->setShadeModel(GL_SMOOTH);
      state->setShininess(0.0f);
      state->setMaterial(GL_EMISSION, 0.0, 0.0, 0.0, 0.0);
      
      #if EXPERIMENTAL_STENCIL_SHADOW == 1
      if (vidbits.stencil)
      {
        state->setMaterial(GL_AMBIENT, 0.0, 0.0, 0.0, 0.);
        state->setMaterial(GL_DIFFUSE, 0.0, 0.0, 0.0, 0.6);
        state->setMaterial(GL_SPECULAR, 0.0, 0.0, 0.0, 0.);
        state->setStateCallback(SSG_CALLBACK_PREDRAW, shadowPredrawCallback);
        state->setStateCallback(SSG_CALLBACK_POSTDRAW, shadowPostdrawCallback);
      }
      else
      #endif
      {
        state->setMaterial(GL_AMBIENT, 0.0, 0.0, 0.0, 1.0);
        state->setMaterial(GL_DIFFUSE, 0.0, 0.0, 0.0, 1.0);
        state->setMaterial(GL_SPECULAR, 0.0, 0.0, 0.0, 1.0);
        state->setStateCallback(SSG_CALLBACK_PREDRAW, NULL);
        state->setStateCallback(SSG_CALLBACK_POSTDRAW, NULL);
      }
    }
  }
  else if (ent->isAKindOf(ssgTypeBranch()))
  {
    ssgBranch *branch = (ssgBranch*)ent;

    // continue down the hierarchy
    std::list<ssgLeaf*> ToBeRemoved;
    std::list<ssgLeaf*>::iterator it;
    int kids = branch->getNumKids();
    for (int i = 0; i < kids; i++)
    {
      ssgEntity* currKid = branch->getKid(i);
      if (makeShadow(currKid))
      {
        ToBeRemoved.push_back(static_cast<ssgLeaf*>(currKid));
      }
    }
    for (it = ToBeRemoved.begin(); it != ToBeRemoved.end(); it++)
    {
      SSGUtil::removeLeafFromGraph(*it);
    }
    

  }
  return boRemoveCurrentEntity;
}



AirplaneVisualization::AirplaneVisualization( std::string const& model_name,
                                              std::string const& texture_path,
                                              CRRCMath::Vector3 const& pCG,
                                              SimpleXMLTransfer *xml)
 :  initial_trans(NULL), 
    model_trans(NULL), model(NULL),
    shadow(NULL), shadow_trans(NULL)
{
  ssgTexturePath(texture_path.c_str());
  // load model
  model = ssgLoad(model_name.c_str());

  if (model != NULL)
  {
#if (SHADOW_TYPE==SHADOW_VOLUME)
    shadow = (ssgEntity*)new ShadowVolume(model);
    scene->addKid(shadow);
#endif
    // transform model from SSG coordinates to CRRCsim coordinates
    initial_trans = new ssgTransform();
    model_trans = new ssgTransform();
    scene->addKid(model_trans);
    model_trans->addKid(initial_trans);
    initial_trans->addKid(model);
    
    sgMat4 it = {  {1.0,  0.0,  0.0,  0},
                   {0.0,  0.0, -1.0,  0},
                   {0.0,  1.0,  0.0,  0},
                   {pCG.r[1],  pCG.r[2],  -pCG.r[0],  1.0} };
    
    initial_trans->setTransform(it);

    // add a simple shadow
#if (SHADOW_TYPE==SHADOW_PROJECTION)
    shadow = (ssgEntity*)initial_trans->clone(SSG_CLONE_RECURSIVE | SSG_CLONE_GEOMETRY | SSG_CLONE_STATE);
    makeShadow(shadow);
    shadow_trans = new ssgTransform();
    scene->addKid(shadow_trans);
    shadow_trans->addKid(shadow);
#endif
    
    /// \todo add animations ("real" model only, without shadow)
    initAnimations(xml, model);
  }
  else
  {
    std::string msg = "Error opening model ";
    msg += model_name;
    msg += " (texture path ";
    msg += texture_path;
    msg += ")";
    throw std::runtime_error(msg);
  }
}
  
AirplaneVisualization::~AirplaneVisualization()
{
  ssgBranch *parent = model_trans->getParent(0);
  parent->removeKid(model_trans);
	
#if (SHADOW_TYPE==SHADOW_PROJECTION)
  parent->removeKid(shadow_trans);
#endif
#if (SHADOW_TYPE==SHADOW_VOLUME)
  parent->removeKid(shadow);
  //parent->removeKid(shadow_draw);
#endif
}
  

/** \brief Create a rotation matrix
 *
 *  This function creates a rotation matrix from the original
 *  OpenGL glRotatef commands in CRRCAirplaneV2::draw().
 *
 *  \param m The matrix to be rotated
 *  \param phi Euler angle phi
 *  \param theta Euler angle theta
 *  \param psi Euler angle psi
 */
inline void makeOGLRotMat4(sgMat4 m, double phi, double theta, double psi)
{
  sgMat4 temp;
  sgVec3 rvec;
  
  sgSetVec3(rvec, 0.0, 1.0, 0.0);
  sgMakeRotMat4(temp, 180.0f - (float)psi * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m, temp);
  
  sgSetVec3(rvec, -1.0, 0.0, 0.0);
  sgMakeRotMat4(temp, (float)theta * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m, temp);
  
  sgSetVec3(rvec, 0.0, 0.0, 1.0);
  sgMakeRotMat4(temp, (float)phi * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m, temp);
}


void makeShadowMatrix(sgMat4 sm, float x, float y);


void AirplaneVisualization::setPosition(CRRCMath::Vector3 const& pos,
                                        double phi, double theta, double psi)
{
  sgMat4 m;
  sgMakeIdentMat4(m);

  m[3][0] = pos.r[1];
  m[3][1] = -1 * pos.r[2];
  m[3][2] = -1 * pos.r[0];
  makeOGLRotMat4(m, phi, theta, psi);
  model_trans->setTransform(m);

#if (SHADOW_TYPE==SHADOW_PROJECTION)
  sgMat4  sm;
  makeShadowMatrix(sm, pos.r[0], pos.r[1]);
  sgPostMultMat4(m, sm);
//jwtodoshadow  m[3][1] += 0.1;
  m[3][1] += .001;//JL
  shadow_trans->setTransform(m);
#endif
#if (SHADOW_TYPE==SHADOW_VOLUME)
  ((ShadowVolume*)shadow)->update(pos.r[0], pos.r[1], pos.r[2], phi, theta, psi);
#endif
}


/**
 * Create a new airplane visualization
 */
long new_visualization( std::string const& model_name,
                        std::string const& texture_path,
                        CRRCMath::Vector3 const& pCG,
                        SimpleXMLTransfer *xml)
{
  AirplaneVisualization* vis = NULL;
  long id = INVALID_AIRPLANE_VISUALIZATION;
  
  try
  {
    vis = new AirplaneVisualization(model_name, texture_path, pCG, xml);
    
    // add the new visualization to the list of all visualizations
    // first search for an empty entry
    std::vector<AirplaneVisualization*>::size_type pos;
    for ( pos = 0;
          pos < AirplaneVisualization::ListOfVisualizations.size();
          pos++)
    {
      if (AirplaneVisualization::ListOfVisualizations[pos] == NULL)
      {
        AirplaneVisualization::ListOfVisualizations[pos] = vis;
        id = (long)pos;
        break;
      }
    }
    
    // if no empty entry was found, just add it to the end of the list
    if (id == INVALID_AIRPLANE_VISUALIZATION)
    {
      AirplaneVisualization::ListOfVisualizations.push_back(vis);
      id = (long)(AirplaneVisualization::ListOfVisualizations.size() - 1);
    }
    
    std::ostringstream log;
    log << _("Loaded model ") << model_name << " (ID " << id << ")";
    LOG(log.str());
  }
  catch (std::runtime_error &e)
  {
    std::cerr << e.what() << std::endl;
    delete vis;
    vis = NULL;
    id = INVALID_AIRPLANE_VISUALIZATION;
  }
  
  return id;
}


/**
 * Deallocate an airplane visualization
 */
void delete_visualization(long id)
{
  if ((id >= 0) && (id < (long)AirplaneVisualization::ListOfVisualizations.size()))
  {
    delete AirplaneVisualization::ListOfVisualizations[id];
    AirplaneVisualization::ListOfVisualizations[id] = NULL;
  }
}


/**
 * Update the position of a visualization
 */
void set_position(long id,
                  CRRCMath::Vector3 const &pos,
                  double phi,
                  double theta,
                  double psi)
{
  if ((id >= 0) && (id < (long)AirplaneVisualization::ListOfVisualizations.size()))
  {
    AirplaneVisualization::ListOfVisualizations[id]->setPosition(pos, phi, theta, psi);
  }
}

} // end namespace Video::
