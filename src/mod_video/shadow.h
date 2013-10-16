/*
 * CRRCsim - the Charles River Radio Control Club Flight Simulator Project
 *
 * Copyright (C) 2011 JOel Lienard (original author)
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
  
/** \file shadow.h
 * 
 *  -choice of shadow algorithm
 *  -class definition of shadow volume algorithm
 */

#ifndef SHADOW_H_
#define SHADOW_H_

//define shadow algorithm
#define SHADOW_PROJECTION 1 // shadow projection algorithm (initial CRRCSIM algo)
#define SHADOW_VOLUME 2     // shadow volume algorithm (new)
#define SHADOW_TYPE  SHADOW_VOLUME

#if (SHADOW_TYPE==SHADOW_VOLUME)

namespace Video
{

class csgdVec3 //encapsulation of sgdVec3 for use in list
{
  public:sgdVec3 v;
};



class ShadowVolume : public ssgBranch
{
  public: 
    ShadowVolume(ssgEntity *model);
    ~ShadowVolume();
    int update(float x, float y, float z, float phi, float theta, float psi);
    
  friend APIENTRY void gluTess_vertexCallback(GLdouble *v,ShadowVolume * sh);
  friend APIENTRY void gluTess_beginCallback(GLenum which,ShadowVolume * sh);
  friend APIENTRY void gluTess_endCallback(ShadowVolume * sh);
  friend APIENTRY void gluTess_errorCallback(GLenum errorCode, ShadowVolume * sh);
  friend APIENTRY void gluTess_combineCallback(GLdouble coor[3], void *v_d[4], GLfloat w[4], GLdouble **dOut, ShadowVolume* sh);
  friend int PredrawCallback1(ssgState* state);
  friend int PostdrawCallback1(ssgState* state);
    
  private:
    class ext_ssgVertexArray : public  ssgVertexArray
    {
      public: 
        ext_ssgVertexArray(ext_ssgVertexArray *p){ prev=p; };
        ~ext_ssgVertexArray(){};
        ext_ssgVertexArray *prev;
    };
    class ext_ssgState : public ssgSimpleState
    {
      public :
        sgMat4 shadowvolume_xform;
        int shadowvolume_xform_pushed;
    };

    ssgVertexArray *vertices_top;
    ext_ssgVertexArray *vertices;
    ext_ssgState *state1;
    ssgBranch *volume;
    ssgEntity     *vshadow_draw;
    ssgTransform  *vshadow_trans;
    ssgBranch *makeShadowVolumeDraw();
    void makeSilhouette(ssgEntity * e, sgMat4 xform, GLUtesselator* tobj, ShadowVolume* sh);
    std::list<csgdVec3> vectTess;
    
};
}// end namespace Video::

#endif //(SHADOW_TYPE==SHADOW_VOLUME)
#endif // SHADOW_H_
