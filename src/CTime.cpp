/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 * Copyright (C) 2005 Olivier Bordes (original author)
 * Copyright (C) 2005 Jan Reucker
 * Copyright (C) 2008 Jens Wilhelm Wulf
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
  

// handle the game timing to use CPU as appropriate
//

#include "CTime.h"
#include <iostream>

CTime::CTime (SimpleXMLTransfer *cfg)
{
  cyclesToCalculate = 0;
  
  try
  {
    int speed;
    SimpleXMLTransfer *video = cfg->getChild("video", true);
    speed = video->attributeAsInt("fps", DEFAULT_GAME_SPEED);
    setGameSpeed(speed);
  }
  catch (XMLException e)
  {
    std::cerr << "CTime::CTime (SimpleXMLTransfer *cfg)" << std::endl;
    std::cerr << "XMLException: " << e.what() << std::endl;
    std::cerr << "Falling back to desired framerate of ";
    std::cerr << DEFAULT_GAME_SPEED << " FPS" << std::endl;
    setGameSpeed (DEFAULT_GAME_SPEED);
  }
}

CTime::~CTime ()
{
}

void CTime::setGameSpeed (Uint16 speed)
{
  if (speed == 0)
  {
    speed = 1;
  }
  gameSpeed = speed;
  cycleLength = 1000/gameSpeed;
  timer1 = SDL_GetTicks();
}

void CTime::update ()
{
  // ensure we are not going too fast
  while (1)
  {
    timer2 = SDL_GetTicks() - timer1;
    if (timer2 >= cycleLength)
      break;
    else
      SDL_Delay (3);   // delay 3 milliseconds
  }

  // update timing
  timer1= SDL_GetTicks() - (timer2 % cycleLength);
  cyclesToCalculate= timer2 / cycleLength;
  if (cyclesToCalculate > MAX_SKIPPED_FRAME)
    cyclesToCalculate = MAX_SKIPPED_FRAME;
}

void CTime::putBackIntoCfg(SimpleXMLTransfer *cfg)
{
    SimpleXMLTransfer *video = cfg->getChild("video");
    video->setAttributeOverwrite("fps", gameSpeed);
}
