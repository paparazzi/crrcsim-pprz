/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 * Copyright (C) 2006 - 2009 Jan Reucker (original author)
 * Copyright (C) 2006 Todd Templeton
 * Copyright (C) 2007, 2008, 2010 Jens Wilhelm Wulf
 * Copyright (C) 2008 Olivier Bordes
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
  

/** \file global.cpp
 *
 *  Initialization of the global variables.
 */
 
#include "global.h"
#include <stdlib.h>

SimStateHandler*  Global::Simulation = NULL;
int               Global::training_mode = 0;
int               Global::nVerbosity = 0;
int               Global::HUDCompass = 0;
Scenery*          Global::scenery = NULL;
int               Global::wind_mode = 2;
CGUIMain*         Global::gui = NULL;
CRRCAudioServer*  Global::soundserver = NULL;
T_GameHandler*    Global::gameHandler = NULL;
TSimInputs        Global::inputs;
float             Global::dt;
int               Global::nFPS;
std::string       Global::verboseString;
TestModeData      Global::testmode;
T_TX_Interface*   Global::TXInterface; 
TInputDev*        Global::inputDev;
Aircraft*         Global::aircraft;
FlightRecorder*   Global::recorder;
Robots*           Global::robots;
