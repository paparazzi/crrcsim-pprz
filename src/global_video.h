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

/**
 * \file global_video.h
 *
 * Global interface for mod_video.
 */

#ifndef GLOBAL_VIDEO_H
#define GLOBAL_VIDEO_H

#include "mod_misc/SimpleXMLTransfer.h"
#include "mod_math/vector3.h"

// We need this header as long as pure OpenGL stuff is exposed
// through this interface
#include "include_gl.h"

namespace Video
{
/// \todo #define's should only be declared once. Right now all
///       global defines from mod_video are defined twice.

#define INVALID_AIRPLANE_VISUALIZATION -1

#if defined(__APPLE__) || defined(MACOSX)
#define DEFAULT_SKYBOX_TEXTURE_OFFSET (0.0009f)
#else
#define DEFAULT_SKYBOX_TEXTURE_OFFSET (0.0f)
#endif

void display();
void read_config(SimpleXMLTransfer* cf);
void initialize_scenegraph();
void initialize_window(bool boFlatShading);
void adjust_zoom(float field_of_view);
void cleanup();
int  setupScreen(int nX, int nY, int nFullscreen);
void setWindowTitleString();
void drawSolidCube(GLfloat size);
void resize_window(int w, int h);

/**
 * calculate looking direction
 */
void UpdateCamera(float flDeltaT);
  
/**
 * Get the size of the current window
 */
void getWindowSize(int& x, int& y);

/**
 * Read the "sloppy cam" setting from mod_video
 */
float getSloppyCam();

/**
 * Write the "sloppy cam" setting
 * \param flValue  New value for sloppy cam
 */
void  setSloppyCam(float flValue);

/**
 * Initialize the console
 */
void initConsole();

/**
 * Create a new airplane visualization
 */
long new_visualization( std::string const& model_name,
                        std::string const& texture_path,
                        CRRCMath::Vector3 const& pCG,
                        SimpleXMLTransfer *xml);

/**
 * Deallocate an airplane visualization
 */
void delete_visualization(long id);

/**
 * Update the position of a visualization
 */
void set_position(long id,
                  CRRCMath::Vector3 const &pos,
                  double phi,
                  double theta,
                  double psi);

/**
 * Configure the sky visualization
 */
bool setup_sky(SimpleXMLTransfer *xml);

} // end namespace Video::

#endif // GLOBAL_VIDEO_H
