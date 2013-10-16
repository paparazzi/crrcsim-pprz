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
  
/** \file airplane_vis.h
 * 
 *  This file is all about airplane visualization.
 */

#ifndef AIRPLANE_VISUALIZATION_H_
#define AIRPLANE_VISUALIZATION_H_

#include <plib/ssg.h>
#include <string>

#include "../mod_math/vector3.h"
#include "../mod_misc/SimpleXMLTransfer.h"
#include "crrc_animation.h"

namespace Video
{

/**
 * \brief A class to visualize an airplane
 *
 * This class encapsulates the visualization of an airplane.
 * 
 */
class AirplaneVisualization
{
  public:
    AirplaneVisualization(std::string const& model_name,
                          std::string const& texture_path,
                          CRRCMath::Vector3 const& pCG,
                          SimpleXMLTransfer *xml);
  
    ~AirplaneVisualization();
  
    void setPosition( CRRCMath::Vector3 const& pos,
                      double phi, double theta, double psi);
  
    friend long new_visualization(std::string const& model_name,
                                  std::string const& texture_path,
                                  CRRCMath::Vector3 const& pCG,
                                  SimpleXMLTransfer *xml);

    friend  void set_position(long id,
                              CRRCMath::Vector3 const &pos,
                              double phi,
                              double theta,
                              double psi);

    friend  void delete_visualization(long id);
    
  private:
    ssgTransform  *initial_trans;
    ssgTransform  *model_trans;
    ssgEntity     *model;
    ssgEntity     *shadow;
    ssgTransform  *shadow_trans;

    static std::vector<AirplaneVisualization*> ListOfVisualizations;
  
};

} // end namespace Video::

#endif // AIRPLANE_VISUALIZATION_H_
