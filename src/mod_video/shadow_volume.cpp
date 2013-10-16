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
  
/** \file shadow_volume.cpp
 * 
 *  Implementation of shadow algorithm.
 *
 */
 
/* references  : 
-Improving Shadows and Reflections via the Stencil Buffer
Mark J. Kilgard (NVIDIA Corporation) : http://www.opengl.org/resources/code/samples/mjktips/rts/index.html
-Practical and Robust Stenciled Shadow Volumes for Hardware-Accelerated Rendering
Authors: Cass Everitt, Mark J. Kilgard : http://arxiv.org/abs/cs/0301002

Objectif :
 -The algorithm used initially by CRRCSIM works correctly only if the ground is plan. Otherwise, certain parts of the shadow are not drawn because they are under the ground, others are in the air.

Choices:

*shadow volumes/shadow-map :
  -shadow volume to avoid the drawbacks of shado-map (quantification, not supported opengl-extension)

*simplifications : 
  -The silhouette is flat and calculated only once .
    -> Advantage: speed, the transformations of this silhouette are then made by the graphic accelerator.
    -> Drawback: not exact. The defects see each other in the strong angles and especially for planes with very dihedral or big fuselage. And indeed on with the biplane.
      
*possible improvements  TODO:
  -Preliminary calculation of several silhouettes, with several angles. Used in switching or in overlapping.
  -Calculation of the silhouette in background task for an update less frequent than the display.
  -Mixed algorithm ( Shadow Volume Reconstruction from Depth Maps - Michael D. McCool)
 

*TODO Visible bug: on the table, the shadow is above and also down. It would be necessary to handle the shadow of the table, all the bottom would be one shadow => OK


*TODO If several planes (with robots) the shadows are more and more black. It would be necessary to make the final plan of the shadows only once. Or use one 2eme bit of the stencil.
*/


#include <list>
//#include <sys/time.h>
#include "crrc_ssgutils.h"
#include "crrc_graphics.h"
#include "shadow.h"
#if (SHADOW_TYPE==SHADOW_VOLUME)
#define BOTTOM -10000 // " #infinity"   

#define TOP_MARGIN  .5 /*The being volume closes upward by a polygon plan while the model is not totally plan. It is then prolonged upward slightly higher that model to avoid lacks of shade(shadow). Do not put too much, otherwise risk of shadow on objects situated above. */

#define SHADOW_VOLUME_VISIBLE 0 // 1 to see the shadowVolume (TEST)

namespace Video
{
extern sgVec3     lightposn;

void APIENTRY gluTess_vertexCallback(GLdouble *v,ShadowVolume * sh);
void APIENTRY gluTess_beginCallback(GLenum which,ShadowVolume * sh);
void APIENTRY gluTess_endCallback(ShadowVolume * sh);
void APIENTRY gluTess_errorCallback(GLenum errorCode, ShadowVolume * sh);
void APIENTRY gluTess_combineCallback(GLdouble coor[3], void *v_d[4], GLfloat w[4], GLdouble **dOut, ShadowVolume* sh);
    
/*************************************/
//static int PredrawCallback1_compte=0;//TODO comment faire propre ? est-ce vraiment nécessaire ? voir si evitable autrment (fichier terrain mal construits (start altitude faux)
int PredrawCallback1(ssgState* state)
{
  glStencilMask(1);
#if (!SHADOW_VOLUME_VISIBLE)
  glColorMask(0,0,0,0);
#endif
  glEnable(GL_STENCIL_TEST);
  glDepthMask(0);
  glStencilFunc(GL_ALWAYS, 0, 0);
  glStencilOp(GL_KEEP, GL_KEEP, GL_INVERT);
	glDisable( GL_CULL_FACE );
	if(((ShadowVolume::ext_ssgState*)state)->shadowvolume_xform_pushed == 0)
	{
    ((ShadowVolume::ext_ssgState*)state)->shadowvolume_xform_pushed = 1;
	  glPushMatrix() ;
    glMultMatrixf( (float*) (((ShadowVolume::ext_ssgState*)state)->shadowvolume_xform) );
  }
  return 0;
}

int PostdrawCallback1(ssgState* state)
{
	if(((ShadowVolume::ext_ssgState*)state)->shadowvolume_xform_pushed == 1)
	{
    ((ShadowVolume::ext_ssgState*)state)->shadowvolume_xform_pushed = 0;
    glPopMatrix ();
  }
  glDisable(GL_STENCIL_TEST);
  glColorMask(1,1,1,1);
  glDepthMask(1);

  return 0;
}
/*************************************/
/*************************************/
int shadowVolumePredrawCallback2(ssgState*)
{
  glStencilMask(1);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, 0x1, 0x1);
  glStencilOp(GL_KEEP, GL_INVERT, GL_INVERT);
  glDisable( GL_CULL_FACE );///
  return 0;
}

int shadowVolumePostdrawCallback2(ssgState*)
{
  glEnable(GL_DEPTH_TEST);
  glDisable(GL_STENCIL_TEST);
  return 0;
}
/**************************************/
/******************************/
ssgBranch *ShadowVolume::makeShadowVolumeDraw()
//pour mettre de l'ombre la ou le volume d'ombre à mis des marques dans le stencil
//il suffirait de dessiner un rectangle sur tout l'écran
// on met un cube pour ne pas changer la vue (TODO ??)
{
	ssgBranch *branch;
	branch = new ssgBranch();
	ssgSimpleState *state2 = new ssgSimpleState ;
  state2->disable( GL_CULL_FACE );
  state2->disable(GL_COLOR_MATERIAL);
  state2->disable(GL_TEXTURE_2D);
  state2->enable(GL_LIGHTING);
  state2->enable(GL_BLEND);
  //state2->disable(GL_DEPTH_TEST);fonctionne pas ->mis dans callback
  state2->setMaterial(GL_AMBIENT, 0.0, 0.0, 0.0, 0.);
  state2->setMaterial(GL_DIFFUSE, 0.0, 0.0, 0.0, 0.6);
  state2->setMaterial(GL_SPECULAR, 0.0, 0.0, 0.0, 0.);
	state2->setStateCallback(SSG_CALLBACK_PREDRAW, shadowVolumePredrawCallback2);
  state2->setStateCallback(SSG_CALLBACK_POSTDRAW, shadowVolumePostdrawCallback2);
  ssgVertexArray *vertices = new ssgVertexArray( 8 );
  float q=1000;
  sgVec3 nn1 = { -q,  q, -q };
  sgVec3 nn2 = { -q,  q,  q };
  sgVec3 nn3 = {  q,  q,  q };
  sgVec3 nn4 = {  q,  q, -q };
  
  sgVec3 nn5 = { -q, -q, -q };
  sgVec3 nn6 = { -q, -q,  q };
  sgVec3 nn7 = {  q, -q,  q };
  sgVec3 nn8 = {  q, -q, -q };
  vertices->add( nn1 );vertices->add( nn2 );vertices->add( nn3 );vertices->add( nn4 );
  vertices->add( nn5 );vertices->add( nn6 );vertices->add( nn7 );vertices->add( nn8 );
  vertices->add( nn1 );vertices->add( nn5 );vertices->add( nn8 );vertices->add( nn4 );
  vertices->add( nn2 );vertices->add( nn1 );vertices->add( nn5 );vertices->add( nn6 );
  vertices->add( nn2 );vertices->add( nn6 );vertices->add( nn7 );vertices->add( nn3 );
  vertices->add( nn4 );vertices->add( nn8 );vertices->add( nn7 );vertices->add( nn3 );
	ssgLeaf *l = new ssgVtxTable( GL_QUADS, vertices, NULL, NULL, NULL);

	l->setState ( state2 );
	branch->addKid( l );
	return branch;		
}

/********************************************************************/
int ShadowVolume::update(float p0, float p1, float p2, float phi, float theta, float psi)
{
//Deformation of the shadow-volume so as to tilt the top as the plane and direct it as the light.


  //rotation and translation (same as aiplane)
  sgMat4 m, temp;
  sgVec3 rvec;
  sgMakeIdentMat4(m);
  
  sgSetVec3(rvec, 0.0, 1.0, 0.0);
  sgMakeRotMat4(temp, 180.0f - (float)psi * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m, temp);
  
  sgSetVec3(rvec, -1.0, 0.0, 0.0);
  sgMakeRotMat4(temp, (float)theta * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m, temp);
  
  sgSetVec3(rvec, 0.0, 0.0, 1.0);
  sgMakeRotMat4(temp, (float)phi * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m, temp);

  sgMakeIdentMat4(temp);
  temp[3][0] = p1;
  temp[3][1] = -1 * p2;
  temp[3][2] = -1 * p0;
  sgPostMultMat4(m, temp);

  vshadow_trans->setTransform(m);
  
  /*correction of Point of top of a the shadow "infinite" inversed pyramid in the data of plib : 
    we make an inverse rotation so that it is always upright of the plane after rotation.
    inverse rotation : inverse angles and order */
  sgMat4 m0;
  sgMakeIdentMat4(m0);
  
  sgSetVec3(rvec, 0.0, 0.0, 1.0);
  sgMakeRotMat4(temp, -(float)phi * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m0, temp);
  
  sgSetVec3(rvec, -1.0, 0.0, 0.0);
  sgMakeRotMat4(temp, -(float)theta * SG_RADIANS_TO_DEGREES, rvec);
  sgPreMultMat4(m0, temp);
      
  sgSetVec3(rvec, 0.0, BOTTOM, 0.0);
  sgXformVec3(rvec, m0);
  ext_ssgVertexArray *v = vertices;
  while( v )
  {
    float * bottom= v->get(0);
    sgCopyVec3(bottom, rvec);
    v = v->prev;
  }
  
  /* The transformations of type not ortho are badly supported by Plib.
  We make them directly with opengl in the callback */
 
  //direct shadow as the light
  sgMakeIdentMat4(m);
  sgMakeIdentMat4(temp);
  temp[1][0] = -lightposn[0]/lightposn[1];
  temp[1][2] = lightposn[2]/lightposn[1];
  sgPreMultMat4(m, temp);

  sgCopyMat4(state1->shadowvolume_xform , m);
  state1->shadowvolume_xform_pushed = 0;

  return 1;
}
/************************************************/
ShadowVolume::ShadowVolume(ssgEntity *model)
  :  vertices(0), vshadow_draw(NULL), vshadow_trans(NULL)
{

	vshadow_trans = new ssgTransform();
  this->addKid(vshadow_trans);
  volume = new ssgBranch();
  vshadow_trans->addKid(volume);
  vshadow_draw = (ssgEntity*)makeShadowVolumeDraw();
  this->addKid(vshadow_draw);

    
  state1 = new ext_ssgState ;
  state1->enable(GL_COLOR_MATERIAL);
  state1->disable(GL_TEXTURE_2D);
	state1->disable( GL_LIGHTING );

	state1->setStateCallback(SSG_CALLBACK_PREDRAW, PredrawCallback1);
  state1->setStateCallback(SSG_CALLBACK_POSTDRAW, PostdrawCallback1);

//Calculation of the model silhouette
  //init tesselation
GLUtesselator* tobj = gluNewTess();
  //tessellation Property
gluTessProperty(tobj, GLU_TESS_BOUNDARY_ONLY,TRUE);
gluTessProperty(tobj, GLU_TESS_WINDING_RULE,GLU_TESS_WINDING_NONZERO);
gluTessProperty(tobj,GLU_TESS_TOLERANCE, 0.01);//useful ?
gluTessNormal(tobj,  0, 1, 0);//useful ?
  //callback registration
gluTessCallback(tobj, GLU_TESS_VERTEX_DATA,  (void (APIENTRY*) ()) gluTess_vertexCallback);
gluTessCallback(tobj, GLU_TESS_COMBINE_DATA, (void (APIENTRY*) ()) gluTess_combineCallback);
gluTessCallback(tobj, GLU_TESS_BEGIN_DATA, (void (APIENTRY*) ()) gluTess_beginCallback);
gluTessCallback(tobj, GLU_TESS_END_DATA, (void (APIENTRY*) ()) gluTess_endCallback);
gluTessCallback(tobj, GLU_TESS_ERROR_DATA, (void (APIENTRY*) ()) gluTess_errorCallback);
  //call
gluTessBeginPolygon(tobj, this);
sgMat4 xform = { {1.0,  0.0,  0.0,  0}, 
                 {0.0,  0.0, -1.0,  0},
                 {0.0,  1.0,  0.0,  0},
                 {0.0,  0.0,  0.0,  0} };
    /*for timing :
    struct timeval start, end;
    long mtime, seconds, useconds;    
    gettimeofday(&start, NULL);
    */ 
makeSilhouette(model, xform, tobj, this);// it call gluTessVertex() 
gluTessEndPolygon(tobj);

    /*gettimeofday(&end, NULL);
    seconds  = end.tv_sec  - start.tv_sec;
    useconds = end.tv_usec - start.tv_usec;
    mtime = ((seconds) * 1000 + useconds/1000.0) + 0.5;
    printf("Tesselation time: %ld milliseconds\n", mtime);
    */

gluDeleteTess(tobj);
}

/*****************************/
ShadowVolume::~ShadowVolume()
{
//Nothing to make
}

/****************************************************/
/*callback routines registered by gluTessCallback() */
/****************************************************/
void gluTess_vertexCallback(GLdouble vd[3], ShadowVolume *sh)
{
  sgVec3  v;
  sgSetVec3 ( v, vd ) ;//convert double to float
  sh->vertices_top->add( v );
  sh->vertices->add( v );
}
/*****************************************************/
void gluTess_beginCallback(GLenum which, ShadowVolume * sh)
{
  sh->vertices_top = new ssgVertexArray(  );//vertices of the cap of the top
  sh->vertices = new ShadowVolume::ext_ssgVertexArray( sh->vertices );//vertices of volume sides
  sgVec3 vbottom = { 0, BOTTOM, 0 };
  sh->vertices->add( vbottom );//Point of top of a "infinite" inversed pyramid. 
}
/*****************************************************/
void gluTess_endCallback(ShadowVolume* sh)
{
#if (SHADOW_VOLUME_VISIBLE)
  ssgColourArray *colors=new ssgColourArray();
  sgVec4 color={1,.0,0,.5};
  colors->add(color);
#else
 ssgColourArray *colors=NULL;
#endif
	ssgVtxTable *l_top = new ssgVtxTable( GL_POLYGON, sh->vertices_top, NULL, NULL, colors);
	l_top->setState ( sh->state1 );
	l_top->setCullFace(false);
	sh->volume->addKid( l_top );
	float* vertice1 = (sh->vertices->get(1));
	//printf("close poly1 %.1f %.1f %.1f \n",vertice1[0],vertice1[1],vertice1[2]);
  if(vertice1) sh->vertices->add( vertice1 );//close polygone
  else    printf (" *** Empty Polygone !\n");
	ssgVtxTable *l = new ssgVtxTable( GL_TRIANGLE_FAN,sh->vertices, NULL, NULL, NULL);
	l->setState ( sh->state1 );
	l->setCullFace(false);
	sh->volume->addKid( l );
  printf("## Shadow polygon %d + %d vertices \n",
              sh->vertices->getNum(), sh->vertices_top->getNum());
}
/*****************************************************/
void gluTess_errorCallback(GLenum errorCode, ShadowVolume* sh)
{
  const GLubyte *estring;
  estring = gluErrorString(errorCode);
  fprintf (stderr, "Tessellation Error: %s\n", estring);
  exit (0);
}
/*****************************************************/
void gluTess_combineCallback(GLdouble coords[3],
      void *vertex_data[4], GLfloat weight[4], GLdouble **dataOut,
      ShadowVolume* sh)
{
  csgdVec3 vertex; 
  vertex.v[0] = coords[0];
  vertex.v[1] = coords[1];
  vertex.v[2] = coords[2];
  sh->vectTess.push_front(vertex);
  *dataOut = sh->vectTess.front().v;
} 
/*******end routines registered by gluTessCallback()************/


/**********************************************/
void ShadowVolume::makeSilhouette(ssgEntity * e, sgMat4 xform, GLUtesselator* tobj, ShadowVolume* sh)
  /*Investigate all the branches of the model
   and sends triangles to the tesselator*/
{
  if ( e->isAKindOf(ssgTypeBranch()) )
  {
    ssgBranch *br = (ssgBranch *) e ;
    if ( e -> isA ( ssgTypeTransform() ) )
    {
      sgMat4 xform1;
      ((ssgTransform *)e)->getTransform ( xform1 ) ;
      sgPreMultMat4  ( xform, xform1 ) ;
    }
    sgMat4 local_xform;
    sgCopyMat4(local_xform, xform);// save transformation matrix
    for ( int i = 0 ; i < br -> getNumKids () ; i++ )
    {
      makeSilhouette ( br -> getKid ( i ), xform, tobj, sh );
      // restore transformation matrix
      sgCopyMat4(xform, local_xform);
    }
  }
  else if ( e -> isAKindOf ( ssgTypeLeaf() ) )
  {
    ssgLeaf  *leaf = (ssgLeaf  *) e ;
    SSGUtil::NodeAttributes attr = SSGUtil::getNodeAttributes(leaf);
    if (attr.checkAttribute("shadow") != -1)
    {
      int nt = leaf->getNumTriangles();
      for ( int i = 0 ; i < nt ; i++ )//for  each triangle
      {
        short iv1,iv2,iv3;
        sgVec3 v1,v2,v3;
        leaf->getTriangle ( i, &iv1, &iv2,  &iv3 );
        //Apply the transform to each vertex
        sgXformPnt3( v1,  leaf->getVertex(iv1),xform);
        sgXformPnt3( v2,  leaf->getVertex(iv2),xform);
        sgXformPnt3( v3,  leaf->getVertex(iv3),xform);

        //project on horizontal plane
        v1[1]=TOP_MARGIN;
        v2[1]=TOP_MARGIN;
        v3[1]=TOP_MARGIN;
        
        //Correct (if need be) the orientation of triangle
        sgVec3 vn;
        sgMakeNormal ( vn, v1, v2, v3);
        if(vn[1]<0)
        {
          sgVec3 vtemp;
          sgCopyVec3(vtemp, v2);
          sgCopyVec3(v2, v1);
          sgCopyVec3(v1, vtemp);
        }
        //realloc vertex on double (plib vorks on float, glutess on double)
        double *vd1, *vd2, *vd3;
        csgdVec3 vertex;
        sh->vectTess.push_front(vertex); vd1 = sh->vectTess.front().v;
        sh->vectTess.push_front(vertex); vd2 = sh->vectTess.front().v;
        sh->vectTess.push_front(vertex); vd3 = sh->vectTess.front().v;
        sgdSetVec3(vd1, v1);
        sgdSetVec3(vd2, v2);
        sgdSetVec3(vd3, v3);
        //submit triangles to gluTess
        gluTessBeginContour(tobj);
          gluTessVertex(tobj, vd1, vd1);
          gluTessVertex(tobj, vd2, vd2);
          gluTessVertex(tobj, vd3, vd3);
        gluTessEndContour(tobj);
      }
    }
  }
}
} // end of namespace Video::
#endif //(SHADOW_TYPE==SHADOW_VOLUME)
///////////////
