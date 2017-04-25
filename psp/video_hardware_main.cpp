/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2007 Peter Mackay and Chris Swindle.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
// r_main.c

extern "C"
{
#include "../quakedef.h"
}

#include <pspgu.h>
#include <pspgum.h>

#include "clipping.hpp"

using namespace quake;

entity_t	r_worldentity;

qboolean	r_cache_thrash;		// compatability

vec3_t		modelorg, r_entorigin;
entity_t	*currententity;

int			r_visframecount;	// bumped when going to a new PVS
int			r_framecount;		// used for dlight push checking

mplane_t	frustum[4];

int			c_brush_polys, c_alias_polys;

qboolean	envmap;				// qtrue during envmap command capture 

int			currenttexture = -1;		// to avoid unnecessary texture sets

int			cnttextures[2] = {-1, -1};     // cached

int			particletexture;	// little dot for particles
int			playertextures;		// up to 16 color translated skins

int			mirrortexturenum;	// quake texturenum, not gltexturenum
qboolean	mirror;
mplane_t	*mirror_plane;

//
// view origin
//
vec3_t	vup;
vec3_t	vpn;
vec3_t	vright;
vec3_t	r_origin;

ScePspFMatrix4	r_world_matrix;
ScePspFMatrix4	r_base_world_matrix;

//
// screen size info
//
refdef_t	r_refdef;

mleaf_t		*r_viewleaf, *r_oldviewleaf;

texture_t	*r_notexture_mip;

int		d_lightstylevalue[256];	// 8.8 fraction of base light value


void R_MarkLeaves (void);

cvar_t	r_norefresh        = {"r_norefresh",        "0"         };
cvar_t	r_drawentities     = {"r_drawentities",     "1"         };
cvar_t	r_drawviewmodel    = {"r_drawviewmodel",    "1"         };
cvar_t	r_speeds           = {"r_speeds",           "0"         };
cvar_t	r_fullbright       = {"r_fullbright",       "0"         };
cvar_t	r_lightmap         = {"r_lightmap",         "0"         };
cvar_t	r_shadows          = {"r_shadows",          "0"         };
cvar_t	r_mirroralpha      = {"r_mirroralpha",      "1"         };
cvar_t	r_wateralpha       = {"r_wateralpha",       "0.6", qtrue};
cvar_t	r_vsync            = {"r_vsync",            "0",   qtrue};
cvar_t	r_decals           = {"r_decals",           "4096",   qtrue};

cvar_t	r_mipmaps          = {"r_mipmaps",          "0",   qtrue};
cvar_t	r_mipmaps_func     = {"r_mipmaps_func",     "0",   qtrue};
cvar_t	r_mipmaps_bias     = {"r_mipmaps_bias",     "0",   qtrue};
cvar_t	r_dynamic          = {"r_dynamic",          "1"         };
cvar_t	r_novis            = {"r_novis",            "0"         };
cvar_t	r_tex_scale_down   = {"r_tex_scale_down",   "1",   qtrue};
cvar_t	r_tex_format       = {"r_tex_format",       "4",   qtrue};
cvar_t	r_tex_res          = {"r_tex_res",          "0",   qtrue};
cvar_t	r_particles_simple = {"r_particles_simple", "0",   qtrue};
cvar_t	gl_keeptjunctions  = {"gl_keeptjunctions",  "0"         };

	
/*
cvar_t	gl_finish = {"gl_finish","0"};
cvar_t	gl_clear = {"gl_clear","0"};
cvar_t	gl_cull = {"gl_cull","1"};
cvar_t	gl_texsort = {"gl_texsort","1"};
cvar_t	gl_smoothmodels = {"gl_smoothmodels","1"};
cvar_t	gl_affinemodels = {"gl_affinemodels","0"};
cvar_t	gl_polyblend = {"gl_polyblend","1"};
cvar_t	gl_flashblend = {"gl_flashblend","1"};
cvar_t	gl_playermip = {"gl_playermip","0"};
cvar_t	gl_nocolors = {"gl_nocolors","0"};
cvar_t	gl_reporttjunctions = {"gl_reporttjunctions","0"};
cvar_t	gl_doubleeyes = {"gl_doubleeys", "1"};

extern	cvar_t	gl_ztrick;*/

/*
=================
R_CullBox

Returns qtrue if the box is completely outside the frustom
=================
*/
qboolean R_CullBox (vec3_t mins, vec3_t maxs)
{
	int		i;

	for (i=0 ; i<4 ; i++)
		if (BoxOnPlaneSide (mins, maxs, &frustum[i]) == 2)
			return qtrue;
	return qfalse;
}

void R_RotateForEntity (entity_t *e)
{
	// Translate.
	const ScePspFVector3 translation = {
		e->origin[0], e->origin[1], e->origin[2]
	};
	sceGumTranslate(&translation);

	// Rotate.
	const ScePspFVector3 rotation = {
		e->angles[ROLL] * (GU_PI / 180.0f),
		-e->angles[PITCH] * (GU_PI / 180.0f),
		e->angles[YAW] * (GU_PI / 180.0f)
	};
	sceGumRotateZYX(&rotation);
	sceGumUpdateMatrix();
}

/*
=============================================================

  SPRITE MODELS

=============================================================
*/

/*
================
R_GetSpriteFrame
================
*/
mspriteframe_t *R_GetSpriteFrame (entity_t *currententity)
{
	msprite_t		*psprite;
	mspritegroup_t	*pspritegroup;
	mspriteframe_t	*pspriteframe;
	int				i, numframes, frame;
	float			*pintervals, fullinterval, targettime, time;

	psprite = static_cast<msprite_t*>(currententity->model->cache.data);
	frame = currententity->frame;

	if ((frame >= psprite->numframes) || (frame < 0))
	{
		Con_Printf ("R_DrawSprite: no such frame %d\n", frame);
		frame = 0;
	}

	if (psprite->frames[frame].type == SPR_SINGLE)
	{
		pspriteframe = psprite->frames[frame].frameptr;
	}
	else
	{
		pspritegroup = (mspritegroup_t *)psprite->frames[frame].frameptr;
		pintervals = pspritegroup->intervals;
		numframes = pspritegroup->numframes;
		fullinterval = pintervals[numframes-1];

		time = cl.time + currententity->syncbase;

	// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
	// are positive, so we don't have to worry about division by 0
		targettime = time - ((int)(time / fullinterval)) * fullinterval;

		for (i=0 ; i<(numframes-1) ; i++)
		{
			if (pintervals[i] > targettime)
				break;
		}

		pspriteframe = pspritegroup->frames[i];
	}

	return pspriteframe;
}

qboolean R_SpriteGlow(vec3_t org, float *alpha, float *scale)
{
	float	dist, dist2;
	vec3_t	glowDist;

	trace_t	trace;

	VectorSubtract( org, r_refdef.vieworg, glowDist );
	dist = VectorLength( glowDist );

	memset (&trace, 0, sizeof(trace));
	SV_RecursiveHullCheck (cl.worldmodel->hulls, 0, 0, 1, r_refdef.vieworg, org, &trace);

	dist2 = VectorLength2( r_refdef.vieworg ,trace.endpos );
	if(( 1.0f - dist2 ) * dist > 8 )
	{
		Con_Printf("dist fuck out %f\n", dist2);
		return qtrue;
    }

	*alpha = 19000.0 / ( dist * dist );
	*alpha = bound( 0.01f, *alpha, 1.0f );

	if( *alpha <= 0.01f )
	{
        Con_Printf("Alpha fuck out\n");
		return qtrue;
    }
	// make the glow fixed size in screen space, taking into consideration the scale setting.
	if( *scale == 0.0f )
	    *scale = 1.0f;

	*scale *= dist * ( 1.0f / bound( 100.0f, /*r_flaresize->value*/200, 300.0f ));

	return qfalse;
}

/*
=================
R_DrawSpriteModel

=================
*/
extern "C" void TraceLine (vec3_t start, vec3_t end, vec3_t impact);
void R_DrawSpriteModel (entity_t *e)
{
	int             i;
	mspriteframe_t  *frame;
	float		    angle, dot, sr, cr, scale=1.0f, alpha=1.0f;
	vec3_t		    v_forward, v_right, v_up, point;
	msprite_t	    *psprite;

	// don't even bother culling, because it's just a single
	// polygon without a surface cache
	frame = R_GetSpriteFrame (e);
	psprite = static_cast<msprite_t*>(currententity->model->cache.data);
	
	//alpha = e->renderamt * (1.0f / 255.0f);
	//if(alpha <= 0)
	//  alpha  = 1.0f;

	if(ISGLOW(e))
	   if(R_SpriteGlow(currententity->origin, &alpha, &scale))
	      return;

	alpha *= 255.0f;

#if 0
	if (psprite->type == SPR_ORIENTED)
	{	// bullet marks on walls
		AngleVectors (currententity->angles, v_forward, v_right, v_up);
		up = v_up;
		right = v_right;
	}
	else
	{	// normal sprite
		up = vup;
		right = vright;
	}
#else
    int type = psprite->type;

	// automatically roll parallel sprites if requested
	if( e->angles[ROLL] != 0.0f && type == SPR_VP_PARALLEL )
		type = SPR_VP_PARALLEL_ORIENTED;

	switch( type )
	{
	case SPR_ORIENTED:
		AngleVectors( currententity->angles, v_forward, v_right, v_up );
		VectorScale( v_forward, 0.01f, v_forward );	// to avoid z-fighting
		VectorSubtract( currententity->origin, v_forward, currententity->origin );
		break;
	case SPR_FACING_UPRIGHT:
		VectorSet( v_right, currententity->origin[1] - r_origin[1], -(currententity->origin[0] - r_origin[0]), 0.0f );
		VectorSet( v_up, 0.0f, 0.0f, 1.0f );
		VectorNormalize( v_right );
		break;
	case SPR_VP_PARALLEL_UPRIGHT:
		dot = vpn[2];
		if(( dot > 0.999848f ) || ( dot < -0.999848f ))	// cos(1 degree) = 0.999848
			return; // invisible
		VectorSet( v_up, 0.0f, 0.0f, 1.0f );
		VectorSet( v_right, vpn[1], -vpn[0], 0.0f );
		VectorNormalize( v_right );
		break;
	case SPR_VP_PARALLEL_ORIENTED:
		angle = currententity->angles[ROLL] * (M_PI * 2.0f / 360.0f);
		SinCos( angle, &sr, &cr );
		for( i = 0; i < 3; i++ )
		{
			v_right[i] = (vright[i] * cr + vup[i] * sr);
			v_up[i] = vright[i] * -sr + vup[i] * cr;
		}
		break;
	case SPR_VP_PARALLEL: // normal sprite
	default:
		VectorCopy( vright, v_right );
		VectorCopy( vup, v_up );
		break;
	}
#endif
	// Bind the texture.
	GL_Bind(frame->gl_texturenum);

	sceGuEnable(GU_BLEND);
	sceGuEnable(GU_ALPHA_TEST);
	sceGuAlphaFunc(GU_GREATER, 0, 0xff); 

	if(ISCOLOR(e) || ISADDITIVE(e) || ISTEXTURE(e))
	{
        sceGuDepthMask(GU_TRUE);
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
		sceGuColor(GU_RGBA(int(e->rendercolor[0]), int(e->rendercolor[1]), int(e->rendercolor[2]), int(alpha)));
	}
	else if(ISSOLID(e))
	{
        sceGuDepthMask(GU_TRUE);
		sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
		sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_DST_ALPHA, 0, 0);
		sceGuColor(GU_RGBA(255, 255, 255, int(alpha)));
	}
	else if(ISGLOW(e) )
	{
        sceGuDepthMask(GU_TRUE);
        sceGuTexFunc(GU_TFX_MODULATE , GU_TCC_RGBA);
		sceGuColor(GU_RGBA(255, 255, 255, int(alpha)));
	}
    else
    {
        sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGBA);
	}

	// Allocate memory for this polygon.
	glvert_t* const	vertices =
		static_cast<glvert_t*>(sceGuGetMemory(sizeof(glvert_t) * 4));

	VectorMA (e->origin, frame->down * scale, v_up,    point);
	VectorMA (    point, frame->left * scale, v_right, point);

	vertices[0].st[0]	= 0.0f;
	vertices[0].st[1]	= 1.0f;
	vertices[0].xyz[0]	= point[0];
	vertices[0].xyz[1]	= point[1];
	vertices[0].xyz[2]	= point[2];

	VectorMA (e->origin, frame->up   * scale, v_up,    point);
	VectorMA (    point, frame->left * scale, v_right, point);

	vertices[1].st[0]	= 0.0f;
	vertices[1].st[1]	= 0.0f;
	vertices[1].xyz[0]	= point[0];
	vertices[1].xyz[1]	= point[1];
	vertices[1].xyz[2]	= point[2];

	VectorMA (e->origin, frame->up    * scale, v_up,    point);
	VectorMA (    point, frame->right * scale, v_right, point);

	vertices[2].st[0]	= 1.0f;
	vertices[2].st[1]	= 0.0f;
	vertices[2].xyz[0]	= point[0];
	vertices[2].xyz[1]	= point[1];
	vertices[2].xyz[2]	= point[2];

	VectorMA (e->origin, frame->down  * scale, v_up,    point);
	VectorMA (    point, frame->right * scale, v_right, point);


	vertices[3].st[0]	= 1.0f;
	vertices[3].st[1]	= 1.0f;
	vertices[3].xyz[0]	= point[0];
	vertices[3].xyz[1]	= point[1];
	vertices[3].xyz[2]	= point[2];


	// Draw the clipped vertices.
	sceGuDrawArray(
		GU_TRIANGLE_FAN,
		GU_TEXTURE_32BITF | GU_VERTEX_32BITF,
		4, 0, vertices);

    if(ISCOLOR(e) || ISADDITIVE(e) || ISTEXTURE(e) || ISGLOW(e))
    {
	   sceGuDepthMask(GU_FALSE);
	   sceGuColor(0xffffffff);
	}
    else if(ISSOLID(e))
    {
	   sceGuDepthMask(GU_FALSE);
	   sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	   sceGuColor(0xffffffff);
    }

	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGuDisable(GU_BLEND);
	sceGuDisable(GU_ALPHA_TEST);
}

/*
=============================================================

  ALIAS MODELS

=============================================================
*/


#define NUMVERTEXNORMALS	162

extern "C" float	r_avertexnormals[NUMVERTEXNORMALS][3];
float r_avertexnormals[NUMVERTEXNORMALS][3] = {
#include "../anorms.h"
};

vec3_t	shadevector;
/*
float	shadelight, ambientlight;
*/

// precalculated dot products for quantized angles
#define SHADEDOT_QUANT 16
float	r_avertexnormal_dots[SHADEDOT_QUANT][256] =
#include "../anorm_dots.h"
;

float	*shadedots = r_avertexnormal_dots[0];

int	lastposenum;

/*
=============
GL_DrawAliasFrame
=============
*/
extern vec3_t lightcolor; // LordHavoc: .lit support
void GL_DrawAliasFrame (aliashdr_t *paliashdr, int posenum)
{
	float 	l;
	trivertx_t	*verts;
	int		*order;
	int		count;

lastposenum = posenum;

	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		int prim;
		if (count < 0)
		{
			count = -count;
			prim = GU_TRIANGLE_FAN;
		}
		else
		{
			prim = GU_TRIANGLE_STRIP;
		}

		// Allocate the vertices.
		struct vertex
		{
			float u, v;
			unsigned int color; 
			float x, y, z;
		};

		vertex* const out = static_cast<vertex*>(sceGuGetMemory(sizeof(vertex) * count));

		for (int vertex_index = 0; vertex_index < count; ++vertex_index)
		{
			// texture coordinates come from the draw list
			out[vertex_index].u = ((float *)order)[0];
			out[vertex_index].v = ((float *)order)[1];
			order += 2;

			// normals and vertexes come from the frame list
            l = shadedots[verts->lightnormalindex];
            float       r,g,b;

			r = l * lightcolor[0];
            g = l * lightcolor[1];
            b = l * lightcolor[2];

			 if(r > 1)
				r = 1;
			 if(g > 1)
				g = 1;
			 if(b > 1)
				b = 1;

			out[vertex_index].x = verts->v[0];
			out[vertex_index].y = verts->v[1];
			out[vertex_index].z = verts->v[2];
			out[vertex_index].color = GU_COLOR(r, g, b, 1.0f);
			++verts;
		}
		sceGuDrawArray(prim, GU_TEXTURE_32BITF | GU_VERTEX_32BITF | GU_COLOR_8888, count, 0, out);
	}
	sceGuColor(0xffffffff);
}


/*
=============
GL_DrawAliasShadow
=============
*/
extern	vec3_t			lightspot;

void GL_DrawAliasShadow (aliashdr_t *paliashdr, int posenum)
{/*
	float	s, t, l;
	int		i, j;
	int		index;
	trivertx_t	*v, *verts;
	int		list;
	int		*order;
	vec3_t	point;
	float	*normal;
	float	height, lheight;
	int		count;

	lheight = currententity->origin[2] - lightspot[2];

	height = 0;
	verts = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *)((byte *)paliashdr + paliashdr->commands);

	height = -lheight + 1.0;

	while (1)
	{
		// get the vertex count and primitive type
		count = *order++;
		if (!count)
			break;		// done
		if (count < 0)
		{
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		}
		else
			glBegin (GL_TRIANGLE_STRIP);

		do
		{
			// texture coordinates come from the draw list
			// (skipped for shadows) glTexCoord2fv ((float *)order);
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0]*(point[2]+lheight);
			point[1] -= shadevector[1]*(point[2]+lheight);
			point[2] = height;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	*/
}



/*
=================
R_SetupAliasFrame

=================
*/
void R_SetupAliasFrame (int frame, aliashdr_t *paliashdr)
{
	int				pose, numposes;
	float			interval;

	if ((frame >= paliashdr->numframes) || (frame < 0))
	{
		Con_DPrintf ("R_AliasSetupFrame: no such frame %d\n", frame);
		frame = 0;
	}

	pose = paliashdr->frames[frame].firstpose;
	numposes = paliashdr->frames[frame].numposes;

	if (numposes > 1)
	{
		interval = paliashdr->frames[frame].interval;
		pose += (int)(cl.time / interval) % numposes;
	}

	GL_DrawAliasFrame (paliashdr, pose);
}



/*
=================
R_DrawAliasModel

=================
*/
void R_DrawAliasModel (entity_t *e)
{
	int			i, j;
	int			lnum;
	vec3_t		dist;
	float		add;
	model_t		*clmodel;
	vec3_t		mins, maxs;
	aliashdr_t	*paliashdr;
	trivertx_t	*verts, *v;
	int			index;
	float		s, t, an;
	int			anim;
	bool 		force_fullbright;

	force_fullbright = false;
	clmodel = currententity->model;

	VectorAdd (currententity->origin, clmodel->mins, mins);
	VectorAdd (currententity->origin, clmodel->maxs, maxs);

	if (R_CullBox (mins, maxs))
		return;


	VectorCopy (currententity->origin, r_entorigin);
	VectorSubtract (r_origin, r_entorigin, modelorg);

	//
	// get lighting information
	//
	// LordHavoc: .lit support begin
	//ambientlight = shadelight = R_LightPoint (currententity->origin); // LordHavoc: original code, removed shadelight and ambientlight
	R_LightPoint(currententity->origin); // LordHavoc: lightcolor is all that matters from this
	// LordHavoc: .lit support end
	// allways give the gun some light
	// LordHavoc: .lit support begin
	//if (e == &cl.viewent && ambientlight < 24) // LordHavoc: original code
	//	ambientlight = shadelight = 24; // LordHavoc: original code
	if (e == &cl.viewent)
	{
		if (lightcolor[0] < 24)
			lightcolor[0] = 24;
		if (lightcolor[1] < 24)
			lightcolor[1] = 24;
		if (lightcolor[2] < 24)
			lightcolor[2] = 24;
	}
	// LordHavoc: .lit support end

	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		if (cl_dlights[lnum].die >= cl.time)
		{
			VectorSubtract (currententity->origin,
							cl_dlights[lnum].origin,
							dist);
			add = cl_dlights[lnum].radius - Length(dist);

		    // LordHavoc: .lit support begin
			/* LordHavoc: original code
			if (add > 0)
			{
				ambientlight += add;
				//ZOID models should be affected by dlights as well
				shadelight += add;
			}
			*/
			if (add > 0)
	    	{
				lightcolor[0] += add * cl_dlights[lnum].color[0];
				lightcolor[1] += add * cl_dlights[lnum].color[1];
				lightcolor[2] += add * cl_dlights[lnum].color[2];
			}
			// LordHavoc: .lit support end
		}
	}

	// clamp lighting so it doesn't overbright as much
    // LordHavoc: .lit support begin
	/* LordHavoc: original code removed
	if (ambientlight > 128)
     	ambientlight = 128;
	if (ambientlight + shadelight > 192)
		shadelight = 192 - ambientlight;
	*/
	// LordHavoc: .lit support end


	// ZOID: never allow players to go totally black
	i = currententity - cl_entities;
	if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
    // LordHavoc: .lit support begin
	//	if (ambientlight < 8) // LordHavoc: original code
	//		ambientlight = shadelight = 8; // LordHavoc: original code
	{
		if (lightcolor[0] < 8)
			lightcolor[0] = 8;
		if (lightcolor[1] < 8)
			lightcolor[1] = 8;
		if (lightcolor[2] < 8)
			lightcolor[2] = 8;
	}
	// LordHavoc: .lit support end

	// HACK HACK HACK -- no fullbright colors, so make torches and projectiles full light
	if (!strcmp (clmodel->name, "progs/flame2.mdl") ||
	    !strcmp (clmodel->name, "progs/flame.mdl") ||
	    !strcmp (clmodel->name, "progs/lavaball.mdl") ||
	    !strcmp (clmodel->name, "progs/bolt.mdl") ||
	    !strcmp (clmodel->name, "progs/bolt2.mdl") ||
	    !strcmp (clmodel->name, "progs/bolt3.mdl") ||
	    !strcmp (clmodel->name, "progs/s_light.mdl") ||
	    !strcmp (clmodel->name, "progs/eyes.mdl") ||
	    !strcmp (clmodel->name, "progs/k_spike.mdl") ||
	    !strcmp (clmodel->name, "progs/s_spike.mdl") ||
	    !strcmp (clmodel->name, "progs/spike.mdl") ||
	    !strcmp (clmodel->name, "progs/laser.mdl")) 
	{
	// LordHavoc: .lit support begin
	//	ambientlight = shadelight = 256; // LordHavoc: original code
		lightcolor[0] = lightcolor[1] = lightcolor[2] = 256;
	// LordHavoc: .lit support end
		force_fullbright = true;
	}

	shadedots = r_avertexnormal_dots[((int)(e->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	// LordHavoc: .lit support begin
	//shadelight = shadelight / 200.0; // LordHavoc: original code
	VectorScale(lightcolor, 1.0f / 200.0f, lightcolor);
	// LordHavoc: .lit support end
	
	an = e->angles[1]/180*M_PI;
	shadevector[0] = cosf(-an);
	shadevector[1] = sinf(-an);
	shadevector[2] = 1;
	VectorNormalize (shadevector);


	//
	// locate the proper data
	//
	paliashdr = (aliashdr_t *)Mod_Extradata (currententity->model);

	c_alias_polys += paliashdr->numtris;

	//
	// draw all the triangles
	//
	sceGumPushMatrix();

	R_RotateForEntity (e);

	const ScePspFVector3 translation =
	{
		paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]
	};
	sceGumTranslate(&translation);

	const ScePspFVector3 scaling =
	{
		paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]
	};
	sceGumScale(&scaling);

	anim = (int)(cl.time*10) & 3;
    GL_Bind(paliashdr->gl_texturenum[currententity->skinnum][anim]);

	// we can't dynamically colormap textures, so they are cached
	// seperately for the players.  Heads are just uncolored.
	if (currententity->colormap != vid.colormap && 0 /* && !gl_nocolors.value*/)
	{
		i = currententity - cl_entities;
		if (i >= 1 && i<=cl.maxclients /* && !strcmp (currententity->model->name, "progs/player.mdl") */)
		{
		    GL_Bind(playertextures - 1 + i);
		}

	}
	if (force_fullbright)
		sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	else
		sceGuTexFunc(GU_TFX_MODULATE, GU_TCC_RGB); 

	sceGuShadeModel(GU_SMOOTH);
	
	sceGumUpdateMatrix();

	R_SetupAliasFrame (currententity->frame, paliashdr);
	
	sceGuShadeModel(GU_FLAT);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
	sceGumPopMatrix();
	sceGumUpdateMatrix();

	if (r_shadows.value)
	{
		sceGumPushMatrix();
		R_RotateForEntity (e);
		
		GL_DrawAliasShadow (paliashdr, lastposenum);
	
		sceGumPopMatrix();
		sceGumUpdateMatrix();
	}

}

//==================================================================================

/*
=============
R_DrawEntitiesOnList
=============
*/
void R_DrawEntitiesOnList (void)
{
	int		i;

	if (!r_drawentities.value)
		return;

	// draw sprites seperately, because of alpha blending
	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_alias:
			R_DrawAliasModel (currententity);
			break;

		case mod_brush:
			R_DrawBrushModel (currententity);
			break;

		default:
			break;
		}
	}

	for (i=0 ; i<cl_numvisedicts ; i++)
	{
		currententity = cl_visedicts[i];

		switch (currententity->model->type)
		{
		case mod_sprite:
			R_DrawSpriteModel (currententity);
			break;
		}
	}
}

/*
=============
R_DrawViewModel
=============
*/
void R_DrawViewModel (void)
{
/*
	float		ambient[4], diffuse[4];
	int			j;
	int			lnum;
	vec3_t		dist;
	float		add;
	dlight_t	*dl;
	int			ambientlight, shadelight;
*/
	if (!r_drawviewmodel.value)
		return;

	if (chase_active.value)
		return;

	if (envmap)
		return;

	if (!r_drawentities.value)
		return;

	if (cl.items & IT_INVISIBILITY)
		return;

	if (cl.stats[STAT_HEALTH] <= 0)
		return;

	currententity = &cl.viewent;
	if (!currententity->model)
		return;
//New vars
	currententity->renderamt = cl_entities[cl.viewentity].renderamt;
    currententity->rendermode = cl_entities[cl.viewentity].rendermode;
    VectorCopy (cl_entities[cl.viewentity].rendercolor, currententity->rendercolor);
//New vars
#if 0
	j = R_LightPoint (currententity->origin);

	if (j < 24)
		j = 24;		// allways give some light on gun
	ambientlight = j;
	shadelight = j;

// add dynamic lights		
	for (lnum=0 ; lnum<MAX_DLIGHTS ; lnum++)
	{
		dl = &cl_dlights[lnum];
		if (!dl->radius)
			continue;
		if (!dl->radius)
			continue;
		if (dl->die < cl.time)
			continue;

		VectorSubtract (currententity->origin, dl->origin, dist);
		add = dl->radius - Length(dist);
		if (add > 0)
			ambientlight += add;
	}

	ambient[0] = ambient[1] = ambient[2] = ambient[3] = (float)ambientlight / 128;
	diffuse[0] = diffuse[1] = diffuse[2] = diffuse[3] = (float)shadelight / 128;
#endif
	// hack the depth range to prevent view model from poking into walls
	sceGuDepthRange(0, 19660);
	R_DrawAliasModel (currententity);
	sceGuDepthRange(0, 65535);
}


/*
============
R_PolyBlend
============
*/
void R_PolyBlend (void)
{
/*	if (!gl_polyblend.value)
		return;*/
	if (!v_blend[3])
		return;
/*
	GL_DisableMultitexture();

	glDisable (GL_ALPHA_TEST);
	glEnable (GL_BLEND);
	glDisable (GL_DEPTH_TEST);
	glDisable (GL_TEXTURE_2D);

    glLoadIdentity ();

    glRotatef (-90,  1, 0, 0);	    // put Z going up
    glRotatef (90,  0, 0, 1);	    // put Z going up

	glColor4fv (v_blend);

	glBegin (GL_QUADS);

	glVertex3f (10, 100, 100);
	glVertex3f (10, -100, 100);
	glVertex3f (10, -100, -100);
	glVertex3f (10, 100, -100);
	glEnd ();

	glDisable (GL_BLEND);
	glEnable (GL_TEXTURE_2D);
	glEnable (GL_ALPHA_TEST);*/
}


static int SignbitsForPlane (mplane_t *out)
{
	int	bits, j;

	// for fast box on planeside test

	bits = 0;
	for (j=0 ; j<3 ; j++)
	{
		if (out->normal[j] < 0)
			bits |= 1<<j;
	}
	return bits;
}


void R_SetFrustum (void)
{
	int		i;

	if (r_refdef.fov_x == 90) 
	{
		// front side is visible

		VectorAdd (vpn, vright, frustum[0].normal);
		VectorSubtract (vpn, vright, frustum[1].normal);

		VectorAdd (vpn, vup, frustum[2].normal);
		VectorSubtract (vpn, vup, frustum[3].normal);
	}
	else
	{
		// rotate VPN right by FOV_X/2 degrees
		RotatePointAroundVector( frustum[0].normal, vup, vpn, -(90-r_refdef.fov_x / 2 ) );
		// rotate VPN left by FOV_X/2 degrees
		RotatePointAroundVector( frustum[1].normal, vup, vpn, 90-r_refdef.fov_x / 2 );
		// rotate VPN up by FOV_X/2 degrees
		RotatePointAroundVector( frustum[2].normal, vright, vpn, 90-r_refdef.fov_y / 2 );
		// rotate VPN down by FOV_X/2 degrees
		RotatePointAroundVector( frustum[3].normal, vright, vpn, -( 90 - r_refdef.fov_y / 2 ) );
	}

	for (i=0 ; i<4 ; i++)
	{
		frustum[i].type = PLANE_ANYZ;
		frustum[i].dist = DotProduct (r_origin, frustum[i].normal);
		frustum[i].signbits = SignbitsForPlane (&frustum[i]);
	}
}

/*
===============
R_SetupFrame
===============
*/
void R_SetupFrame (void)
{
// don't allow cheats in multiplayer
	if (cl.maxclients > 1)
		Cvar_Set ("r_fullbright", "0");

	R_AnimateLight ();

	r_framecount++;

// build the transformation matrix for the given view angles
	VectorCopy (r_refdef.vieworg, r_origin);

	AngleVectors (r_refdef.viewangles, vpn, vright, vup);

// current viewleaf
	r_oldviewleaf = r_viewleaf;
	r_viewleaf = Mod_PointInLeaf (r_origin, cl.worldmodel);

	V_SetContentsColor (r_viewleaf->contents);
	/*
	V_CalcBlend ();
	*/

	r_cache_thrash = qfalse;

	c_brush_polys = 0;
	c_alias_polys = 0;

}


/*
=============
R_SetupGL
=============
*/
void R_SetupGL (void)
{
	float	screenaspect;
	extern	int glwidth, glheight;
	int		x, x2, y2, y, w, h;

	//
	// set up viewpoint
	//
	/*
	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	*/
	sceGumMatrixMode(GU_PROJECTION);
	sceGumLoadIdentity();

	x = r_refdef.vrect.x * glwidth/vid.width;
	x2 = (r_refdef.vrect.x + r_refdef.vrect.width) * glwidth/vid.width;
	y = (vid.height-r_refdef.vrect.y) * glheight/vid.height;
	y2 = (vid.height - (r_refdef.vrect.y + r_refdef.vrect.height)) * glheight/vid.height;

	// fudge around because of frac screen scale
	if (x > 0)
		x--;
	if (x2 < glwidth)
		x2++;
	if (y2 < 0)
		y2--;
	if (y < glheight)
		y++;

	w = x2 - x;
	h = y - y2;

	if (envmap)
	{
		x = y2 = 0;
		w = h = 256;
	}

	sceGuViewport(
		glx,
		gly + (glheight / 2) - y2 - (h / 2),
		w,
		h);
	sceGuScissor(x, glheight - y2 - h, x + w, glheight - y2);

    screenaspect = (float)r_refdef.vrect.width/r_refdef.vrect.height;
	sceGumPerspective(r_refdef.fov_y, screenaspect, 4, 4096);

	if (mirror)
	{
		if (mirror_plane->normal[2])
		{
			/*glScalef (1, -1, 1);*/
		}
		else
		{
			/*glScalef (-1, 1, 1);*/
		}
		/*glCullFace(GL_BACK);*/
	}
	else
	{
		/*glCullFace(GL_FRONT);*/
	}
	sceGumUpdateMatrix();

	/*glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();*/
	sceGumMatrixMode(GU_VIEW);
	sceGumLoadIdentity();

    /*glRotatef (-90,  1, 0, 0);	    // put Z going up*/
	sceGumRotateX(-90 * (GU_PI / 180.0f));

    /*glRotatef (90,  0, 0, 1);	    // put Z going up*/
	sceGumRotateZ(90 * (GU_PI / 180.0f));

    /*glRotatef (-r_refdef.viewangles[2],  1, 0, 0);*/
	sceGumRotateX(-r_refdef.viewangles[2] * (GU_PI / 180.0f));

    /*glRotatef (-r_refdef.viewangles[0],  0, 1, 0);*/
	sceGumRotateY(-r_refdef.viewangles[0] * (GU_PI / 180.0f));

    /*glRotatef (-r_refdef.viewangles[1],  0, 0, 1);*/
	sceGumRotateZ(-r_refdef.viewangles[1] * (GU_PI / 180.0f));

    /*glTranslatef (-r_refdef.vieworg[0],  -r_refdef.vieworg[1],  -r_refdef.vieworg[2]);*/
	const ScePspFVector3 translation = {
		-r_refdef.vieworg[0],
		-r_refdef.vieworg[1],
		-r_refdef.vieworg[2]
	};
	sceGumTranslate(&translation);

	/*glGetFloatv (GL_MODELVIEW_MATRIX, r_world_matrix);*/
	sceGumStoreMatrix(&r_world_matrix);
	sceGumUpdateMatrix();

	sceGumMatrixMode(GU_MODEL);

	clipping::begin_frame();

	//
	// set drawing parms
	//
	/*
	if (gl_cull.value)
	{
		glEnable(GL_CULL_FACE);
	}
	else
	{
		glDisable(GL_CULL_FACE);
	}

	glDisable(GL_BLEND);
	glDisable(GL_ALPHA_TEST);
	glEnable(GL_DEPTH_TEST);*/
	sceGuDisable(GU_BLEND);
	sceGuTexFunc(GU_TFX_REPLACE, GU_TCC_RGB);
}

/*
================
R_RenderScene

r_refdef must be set before the first call
================
*/
void R_RenderScene (void)
{
	R_SetupFrame ();

	R_SetFrustum ();

	R_SetupGL ();

	R_MarkLeaves ();	// done here so we know if we're in water

	R_DrawWorld ();		// adds static entities to the list

	S_ExtraUpdate ();	// don't let sound get messed up if going slow

	R_DrawEntitiesOnList ();

	/*GL_DisableMultitexture();*/

	R_RenderDlights ();

	R_DrawParticles ();

#ifdef GLTEST
	Test_Draw ();
#endif

}


/*
=============
R_Clear
=============
*/
void R_Clear (void)
{
#if 0
	sceGuClear(GU_COLOR_BUFFER_BIT | GU_DEPTH_BUFFER_BIT);
#else
	sceGuClear(GU_DEPTH_BUFFER_BIT);
#endif

	if (r_mirroralpha.value != 1.0)
	{
		/*
		gldepthmin = 0;
		gldepthmax = 0.5;
		glDepthFunc (GL_LEQUAL);
		*/
	}
	else
	{
		/*
		gldepthmin = 0;
		gldepthmax = 1;
		glDepthFunc (GL_LEQUAL);*/
	}

	/*glDepthRange (gldepthmin, gldepthmax);*/
}

/*
=============
R_Mirror
=============
*/
void R_Mirror (void)
{
	float		d;
	msurface_t	*s;
	entity_t	*ent;

	if (!mirror)
		return;

	r_base_world_matrix = r_world_matrix;

	d = DotProduct (r_refdef.vieworg, mirror_plane->normal) - mirror_plane->dist;
	VectorMA (r_refdef.vieworg, -2*d, mirror_plane->normal, r_refdef.vieworg);

	d = DotProduct (vpn, mirror_plane->normal);
	VectorMA (vpn, -2*d, mirror_plane->normal, vpn);

	r_refdef.viewangles[0] = -asinf(vpn[2])/M_PI*180;
	r_refdef.viewangles[1] = atan2f(vpn[1], vpn[0])/M_PI*180;
	r_refdef.viewangles[2] = -r_refdef.viewangles[2];

	ent = &cl_entities[cl.viewentity];
	if (cl_numvisedicts < MAX_VISEDICTS)
	{
		cl_visedicts[cl_numvisedicts] = ent;
		cl_numvisedicts++;
	}
/*
	gldepthmin = 0.5;
	gldepthmax = 1;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);
*/
	R_RenderScene ();
	R_DrawWaterSurfaces ();
/*
	gldepthmin = 0;
	gldepthmax = 0.5;
	glDepthRange (gldepthmin, gldepthmax);
	glDepthFunc (GL_LEQUAL);
*/
	// blend on top
/*	glEnable (GL_BLEND);
	glMatrixMode(GL_PROJECTION);*/
	if (mirror_plane->normal[2])
	{
		/*glScalef (1,-1,1);*/
	}
	else
	{
		/*glScalef (-1,1,1);*/
	}
	/*glCullFace(GL_FRONT);
	glMatrixMode(GL_MODELVIEW);

	glLoadMatrixf (r_base_world_matrix);

	glColor4f (1,1,1,r_mirroralpha.value);
	*/
	s = cl.worldmodel->textures[mirrortexturenum]->texturechain;
	for ( ; s ; s=s->texturechain)
		R_RenderBrushPoly (s);
	cl.worldmodel->textures[mirrortexturenum]->texturechain = NULL;
	/*glDisable (GL_BLEND);
	glColor4f (1,1,1,1);*/
}

/*
================
R_RenderView

r_refdef must be set before the first call
================
*/
void R_RenderView (void)
{
	double	time1, time2;

	if (r_norefresh.value)
		return;

	if (!r_worldentity.model || !cl.worldmodel)
		Sys_Error ("R_RenderView: NULL worldmodel");

	if (r_speeds.value)
	{
		/*glFinish ();*/
		time1 = Sys_FloatTime ();
		c_brush_polys = 0;
		c_alias_polys = 0;
	}

	mirror = qfalse;
/*
	if (gl_finish.value)
		glFinish ();*/

	R_Clear ();

	// render normal view

	R_RenderScene ();

/*
	sceGumMatrixMode(GU_VIEW);
	ScePspFMatrix4 view_matrix;
	sceGumStoreMatrix(&view_matrix);

	sceGumLoadIdentity();

	ScePspFVector3 translation =
	{
		view_matrix.x.w, view_matrix.y.w, view_matrix.z.w
	};
	sceGumTranslate(&translation);
	sceGumUpdateMatrix();

	sceGumMatrixMode(GU_MODEL);

	sceGumLoadIdentity();
	sceGumUpdateMatrix();
*/

    R_DrawViewModel ();

	R_DrawWaterSurfaces ();

	// render mirror view
	R_Mirror ();

	R_PolyBlend ();

	if (r_speeds.value)
	{
		time2 = Sys_FloatTime ();
		Con_Printf ("%3i ms  %4i wpoly %4i epoly\n", (int)((time2-time1)*1000), c_brush_polys, c_alias_polys); 
	}
}
