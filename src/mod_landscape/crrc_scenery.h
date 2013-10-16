/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 *   Copyright (C) 2000, 2001 Jan Kansky (original author)
 *   Copyright (C) 2004-2010 Jan Reucker
 *   Copyright (C) 2005, 2008 Jens Wilhelm Wulf
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

#ifndef CRRC_SCENERY_H
#define CRRC_SCENERY_H

#include <crrc_config.h>

#include "../mod_math/vector3.h"
#include "../mod_misc/SimpleXMLTransfer.h"
#include "../global_video.h"
#include <vector>
#include "heightdata.h"

#define HEIGHTMAP_SIZE_X  (64)
#define HEIGHTMAP_SIZE_Z  (64)


typedef struct
{
  float north;
  float east;
  float height;
  std::string name;
} T_Position;

typedef std::vector<T_Position> T_PosnArray;


/** \brief Abstract base class for scenery classes
 *
 *  This base class defines the public interface common to
 *  all scenery-drawing classes. It also reads common
 *  information from the scenery file (sky initialization,
 *  player and start positions, ...)
 */
class Scenery
{
  public:
    /**
     *  Initialization from XML description
     */
    Scenery(SimpleXMLTransfer *xml, int sky_variant = 0);

    /**
     *  Destructor
     */
    virtual ~Scenery();

     /**
     * Get pointeur on XML description section named "name"
     *
     */
    SimpleXMLTransfer *getXMLsection(const char * name);
    
    /**
     *  Draw the scenery
     *
     *  \param current_time current time in ms (for animation effects)
     */
    virtual void draw(double current_time) = 0;

    /**
     *  Get player position
     */
    virtual CRRCMath::Vector3 getPlayerPosition(int num = 0);
    virtual CRRCMath::Vector3 getStartPosition(int num = 0);
    virtual CRRCMath::Vector3 getStartPosition(std::string);
    virtual int getNumStartPosition();
    virtual std::string *const getStartPositionName(int num = 0);
  
    /**
     *  Get the height at a distinct point.
     *  \param x x coordinate
     *  \param z z coordinate
     *  \return terrain height at this point in ft
     */
    virtual float getHeight(float x, float z) = 0;
    
    /**
     *  Get the default wind direction specified in the scenery file.
     */
    inline float getDefaultWindDirection(void) {return flDefaultWindDirection;};

    /**
     *  Get the default wind speed specified in the scenery file.
     */
    inline float getDefaultWindSpeed(void) {return flDefaultWindSpeed;};

    /**
     *  Get the ImposeWindDirection value specified in the scenery file.
     */
    inline int  getImposeWindDirection(void) {return ImposeWindDirection;};

    /**
     * Get the real altitude of scenery origin point 
     */
    inline float  getOriginAltitude(void) {return OriginAltitude;};

    /**
     *  get height and plane equation at x|z
     *  \param x x coordinate
     *  \param z z coordinate
     *  \param tplane this is where the plane equation will be stored
     *  \return terrain height at this point in ft
     */
    virtual float getHeightAndPlane(float x, float z, float tplane[4]) = 0;
    
    /**
     * get  wind on  directions  at position  X_cg, Y_cg,Z_cg
     */
    virtual void getWindComponents(double X_cg,double  Y_cg,double  Z_cg,
                                   float  *x_wind_velocity, float  *y_wind_velocity,
                                   float  *z_wind_velocity)=0;;

    /**
     *  Get an ID code for this location or scenery type
     */
    virtual int getID() = 0;
    
    /**
     *  Get the name of this location as specified in the
     *  XML scenery description file.
     *
     *  \return scenery name
     */
    inline const char *getName()  {return name.c_str();};

    /**
     *  Get the index of the sky definition in the
     *  XML scenery description file that is currently loaded.
     *
     *  \return sky variant index
     */
    inline int getSkyVariant()  {return nSkyVariant;};

     /**
     *  locations:
     *  DAVIS          CRRC Davis flying field in Sudbry, Mass
     *  MEDFIELD       CRRC Medfield flying site in Medfield, Mass
     *  CAPE_COD       Slope Soaring at Cape Cod, Massachusetts 
     *  XML_HMAP       read from scenery file (old height-map format)
     *  NULL_RENDERER  empty renderer
     *
     *  \todo Everything that depends on these constants should
     *  get its information from the scenery file instead of
     *  using hard-coded stuff. In the end these constants should
     *  only be used as an argument to the ctor of BuiltinScenery.
     */
    enum { DAVIS=1, MEDFIELD=2, CAPE_COD=3, XML_HMAP=4, NULL_RENDERER=5,
           MODEL_BASED=6, PHOTO=7 };

  protected:
    float flDefaultWindSpeed;       ///< default wind speed from scenery file in ft/s
    float flDefaultWindDirection;   ///< default wind direction from scenery file in degrees
    int ImposeWindDirection;        ///< if true,  WindDirection is not modifiable
    std::string name;               ///< name of this location, from XML description
    float OriginAltitude;           ///< real altitude of scenery in ft (for air density)

  private:
    /**
     *  Read a set of positions into a position array
     */
    int parsePositions(SimpleXMLTransfer *tag, T_PosnArray& pa, bool default_on_empty = true);
  
    SimpleXMLTransfer *xml_description;
    T_PosnArray views;
    T_PosnArray starts;
    int nSkyVariant;                ///< Index of the currently loaded sky variant
};



/**
 *  Load a scenery from a file
 */
Scenery* loadScenery(const char *fname, int sky_variant = 0);


/** \brief initial NULL renderer scenery 
 *
 */
class SceneryNull : public Scenery
{
  public:
    SceneryNull(int sky_variant = 0) : Scenery(NULL, sky_variant) {}
  
    float getHeight(float x, float z){return 0;}
    float getHeightAndPlane(float x, float z, float tplane[4]){return 0;}
    int getID() {return 0;}
    void getWindComponents(double X_cg,double  Y_cg,double  Z_cg,
    float  *x_wind_velocity, float  *y_wind_velocity, float  *z_wind_velocity){}
    void draw(double current_time){};
    
    CRRCMath::Vector3 getPlayerPosition(int num){ return CRRCMath::Vector3(0.,0., 0.);}
    CRRCMath::Vector3 getStartPosition(int num){ return CRRCMath::Vector3(0.,0., 0.);}
    int getNumStartPosition(){return 0;}

};

#endif  // CRRC_SCENERY_H
