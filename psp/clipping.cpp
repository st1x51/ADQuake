/*
Copyright (C) 2007 Peter Mackay and Chris Swindle.
Copyright (C) 2020 Sergey Galushko

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

#define CLIPPING_DEBUGGING	0
#define CLIP_LEFT			1
#define CLIP_RIGHT			1
#define CLIP_BOTTOM			1
#define CLIP_TOP			1
#define CLIP_NEAR			0
#define CLIP_FAR			0

#include "clipping.hpp"

#include <algorithm>
#include <pspgu.h>
#include <pspgum.h>

#include "math.hpp"

extern "C"
{
#include "../quakedef.h"
}

namespace quake
{
	namespace clipping
	{
		// Plane types are sorted, most likely to clip first.
		enum plane_index
		{
#if CLIP_BOTTOM
			plane_index_bottom,
#endif
#if CLIP_LEFT
			plane_index_left,
#endif
#if CLIP_RIGHT
			plane_index_right,
#endif
#if CLIP_TOP
			plane_index_top,
#endif
#if CLIP_NEAR
			plane_index_near,
#endif
#if CLIP_FAR
			plane_index_far,
#endif
			plane_count
		};

		// Types.
		typedef ScePspFVector4	plane_type;
		typedef plane_type		frustum_t[plane_count];

		// Transformed frustum.
		static ScePspFMatrix4		projection_view_matrix;
		static frustum_t			projection_view_frustum;
		static frustum_t			clipping_frustum;

		// The temporary working buffers.
		static const std::size_t	max_clipped_vertices	= 32;
		static glvert_t				work_buffer[2][max_clipped_vertices];

		static inline void calculate_frustum(const ScePspFMatrix4& clip, frustum_t* frustum)
		{
			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
		#if CLIP_NEAR_FAR
				"lv.q		C000,  0(%6)\n"			// C000 = matrix->x
				"lv.q		C010, 16(%6)\n"			// C010 = matrix->y
				"lv.q		C020, 32(%6)\n"			// C020 = matrix->z
				"lv.q		C030, 48(%6)\n"			// C030 = matrix->w
		#else
				"lv.q		C000,  0(%4)\n"			// C000 = matrix->x
				"lv.q		C010, 16(%4)\n"			// C010 = matrix->y
				"lv.q		C020, 32(%4)\n"			// C020 = matrix->z
				"lv.q		C030, 48(%4)\n"			// C030 = matrix->w
		#endif
				/* Extract the BOTTOM plane */
				"vadd.s		S100, S003, S001\n"		// S100 = matrix->x.w + matrix->x.y
				"vadd.s		S101, S013, S011\n"		// S101 = matrix->y.w + matrix->y.y
				"vadd.s		S102, S023, S021\n"		// S102 = matrix->z.w + matrix->z.y
				"vadd.s		S103, S033, S031\n"		// S103 = matrix->w.w + matrix->w.y
				"vdot.q		S110, C100, C100\n"		// S110 = S100*S100 + S101*S101 + S102*S102 + S103*S103
				"vzero.s	S111\n"					// S111 = 0
				"vcmp.s		EZ,   S110\n"			// CC[0] = ( S110 == 0.0f )
				"vrsq.s		S110, S110\n"			// S110 = 1.0 / sqrt( S110 )
				"vcmovt.s	S110, S111, 0\n"		// if ( CC[0] ) S110 = S111
				"vscl.q		C100[-1:1,-1:1,-1:1,-1:1], C100, S110\n"	// C100 = C100 * S110
				"sv.q		C100, %0\n"				// Store plane from register
				/* Extract the LEFT plane */
				"vadd.s		S100, S003, S000\n"		// S100 = matrix->x.w + matrix->x.x
				"vadd.s		S101, S013, S010\n"		// S101 = matrix->y.w + matrix->y.x
				"vadd.s		S102, S023, S020\n"		// S102 = matrix->z.w + matrix->z.x
				"vadd.s		S103, S033, S030\n"		// S103 = matrix->w.w + matrix->w.x
				"vdot.q		S110, C100, C100\n"		// S110 = S100*S100 + S101*S101 + S102*S102 + S103*S103
				"vzero.s	S111\n"					// S111 = 0
				"vcmp.s		EZ,   S110\n"			// CC[0] = ( S110 == 0.0f )
				"vrsq.s		S110, S110\n"			// S110 = 1.0 / sqrt( S110 )
				"vcmovt.s	S110, S111, 0\n"		// if ( CC[0] ) S110 = S111
				"vscl.q		C100[-1:1,-1:1,-1:1,-1:1], C100, S110\n"	// C100 = C100 * S110
				"sv.q		C100, %1\n"				// Store plane from register
				/* Extract the RIGHT plane */
				"vsub.s		S100, S003, S000\n"		// S100 = matrix->x.w - matrix->x.x
				"vsub.s		S101, S013, S010\n"		// S101 = matrix->y.w - matrix->y.x
				"vsub.s		S102, S023, S020\n"		// S102 = matrix->z.w - matrix->z.x
				"vsub.s		S103, S033, S030\n"		// S103 = matrix->w.w - matrix->w.x	
				"vdot.q		S110, C100, C100\n"		// S110 = S100*S100 + S101*S101 + S102*S102 + S103*S103
				"vzero.s	S111\n"					// S111 = 0
				"vcmp.s		EZ,   S110\n"			// CC[0] = ( S110 == 0.0f )
				"vrsq.s		S110, S110\n"			// S110 = 1.0 / sqrt( S110 )
				"vcmovt.s	S110, S111, 0\n"		// if ( CC[0] ) S110 = S111
				"vscl.q		C100[-1:1,-1:1,-1:1,-1:1], C100, S110\n"	// C100 = C100 * S110
				"sv.q		C100, %2\n"				// Store plane from register	
				/* Extract the TOP plane */
				"vsub.s		S100, S003, S001\n"		// S100 = matrix->x.w - matrix->x.y
				"vsub.s		S101, S013, S011\n"		// S101 = matrix->y.w - matrix->y.y
				"vsub.s		S102, S023, S021\n"		// S102 = matrix->z.w - matrix->z.y
				"vsub.s		S103, S033, S031\n"		// S103 = matrix->w.w - matrix->w.y	
				"vdot.q		S110, C100, C100\n"		// S110 = S100*S100 + S101*S101 + S102*S102 + S103*S103
				"vzero.s	S111\n"					// S111 = 0
				"vcmp.s		EZ,   S110\n"			// CC[0] = ( S110 == 0.0f )
				"vrsq.s		S110, S110\n"			// S110 = 1.0 / sqrt( S110 )
				"vcmovt.s	S110, S111, 0\n"		// if ( CC[0] ) S110 = S111
				"vscl.q		C100[-1:1,-1:1,-1:1,-1:1], C100, S110\n"	// C100 = C100 * S110
				"sv.q		C100, %3\n"				// Store plane from register	
		#if CLIP_NEAR_FAR
				/* Extract the NEAR plane */
				"vadd.s		S100, S003, S002\n"		// S100 = matrix->x.w + matrix->x.z
				"vadd.s		S101, S013, S012\n"		// S101 = matrix->y.w + matrix->y.z
				"vadd.s		S102, S023, S022\n"		// S102 = matrix->z.w + matrix->z.z
				"vadd.s		S103, S033, S032\n"		// S103 = matrix->w.w + matrix->w.z	
				"vdot.q		S110, C100, C100\n"		// S110 = S100*S100 + S101*S101 + S102*S102 + S103*S103
				"vzero.s	S111\n"					// S111 = 0
				"vcmp.s		EZ,   S110\n"			// CC[0] = ( S110 == 0.0f )
				"vrsq.s		S110, S110\n"			// S110 = 1.0 / sqrt( S110 )
				"vcmovt.s	S110, S111, 0\n"		// if ( CC[0] ) S110 = S111
				"vscl.q		C100[-1:1,-1:1,-1:1,-1:1], C100, S110\n"	// C100 = C100 * S110
				"sv.q		C100, %4\n"				// Store plane from register	
				/* Extract the FAR plane */
				"vsub.s		S100, S003, S002\n"		// S100 = clip->x.w - clip->x.z
				"vsub.s		S101, S013, S012\n"		// S101 = clip->y.w - clip->y.z
				"vsub.s		S102, S023, S022\n"		// S102 = clip->z.w - clip->z.z
				"vsub.s		S103, S033, S032\n"		// S103 = clip->w.w - clip->w.z	
				"vdot.q		S110, C100, C100\n"		// S110 = S100*S100 + S101*S101 + S102*S102 + S103*S103
				"vzero.s	S111\n"					// S111 = 0
				"vcmp.s		EZ,   S110\n"			// CC[0] = ( S110 == 0.0f )
				"vrsq.s		S110, S110\n"			// S110 = 1.0 / sqrt( S110 )
				"vcmovt.s	S110, S111, 0\n"		// if ( CC[0] ) S110 = S111
				"vscl.q		C100[-1:1,-1:1,-1:1,-1:1], C100, S110\n"	// C100 = C100 * S110
				"sv.q		C100, %5\n"				// Store plane from register
		#endif
				".set		pop\n"					// Restore assembler option
				:	"=m"( ( *frustum )[plane_index_bottom] ),
					"=m"( ( *frustum )[plane_index_left] ),
					"=m"( ( *frustum )[plane_index_right] ),
		#if CLIP_NEAR_FAR
					"=m"( ( *frustum )[plane_index_top] ),
					"=m"( ( *frustum )[plane_index_near] ),
					"=m"( ( *frustum )[plane_index_far] )
		#else
					"=m"( ( *frustum )[plane_index_top] )
		#endif
				:	"r"( &clip )
			);
		}

		void begin_frame()
		{
			// Get the projection matrix.
			sceGumMatrixMode(GU_PROJECTION);
			ScePspFMatrix4	proj;
			sceGumStoreMatrix(&proj);

			// Get the view matrix.
			sceGumMatrixMode(GU_VIEW);
			ScePspFMatrix4	view;
			sceGumStoreMatrix(&view);

			// Restore matrix mode.
			sceGumMatrixMode(GU_MODEL);

			// Combine the two matrices (multiply projection by view).
			math::multiply(view, proj, &projection_view_matrix);

			// Calculate and cache the clipping frustum.
			calculate_frustum(projection_view_matrix, &projection_view_frustum);

			__asm__ volatile (
				"ulv.q		C700, %4\n"				// Load plane into register
				"ulv.q		C710, %5\n"				// Load plane into register
				"ulv.q		C720, %6\n"				// Load plane into register
				"ulv.q		C730, %7\n"				// Load plane into register
				"sv.q		C700, %0\n"				// Store plane from register
				"sv.q		C710, %1\n"				// Store plane from register
				"sv.q		C720, %2\n"				// Store plane from register
				"sv.q		C730, %3\n"				// Store plane from register
				:	"=m"( clipping_frustum[plane_index_bottom] ),
					"=m"( clipping_frustum[plane_index_left] ),
					"=m"( clipping_frustum[plane_index_right] ),
					"=m"( clipping_frustum[plane_index_top] )
				:	"m"( projection_view_frustum[plane_index_bottom] ),
					"m"( projection_view_frustum[plane_index_left] ),
					"m"( projection_view_frustum[plane_index_right] ),
					"m"( projection_view_frustum[plane_index_top] )
			);
		}

		void begin_brush_model()
		{
			// Get the model matrix.
			ScePspFMatrix4	model_matrix;
			sceGumStoreMatrix(&model_matrix);

			// Combine the matrices (multiply projection-view by model).
			ScePspFMatrix4	projection_view_model_matrix;
			math::multiply(model_matrix, projection_view_matrix, &projection_view_model_matrix);

			// Calculate the clipping frustum.
			calculate_frustum(projection_view_model_matrix, &clipping_frustum);

			__asm__ volatile (
				"ulv.q	C700, %0\n"	// Load plane into register
				"ulv.q	C710, %1\n"	// Load plane into register
				"ulv.q	C720, %2\n"	// Load plane into register
				"ulv.q	C730, %3\n"	// Load plane into register
				:: "m"(clipping_frustum[plane_index_bottom]),
					"m"(clipping_frustum[plane_index_left]),
					"m"(clipping_frustum[plane_index_right]), 
					"m"(clipping_frustum[plane_index_top])
			);
		}

		void end_brush_model()
		{
			// Restore the clipping frustum.
			__asm__ volatile (
				"ulv.q		C700, %4\n"				// Load plane into register
				"ulv.q		C710, %5\n"				// Load plane into register
				"ulv.q		C720, %6\n"				// Load plane into register
				"ulv.q		C730, %7\n"				// Load plane into register
				"sv.q		C700, %0\n"				// Store plane from register
				"sv.q		C710, %1\n"				// Store plane from register
				"sv.q		C720, %2\n"				// Store plane from register
				"sv.q		C730, %3\n"				// Store plane from register
				:	"=m"( clipping_frustum[plane_index_bottom] ),
					"=m"( clipping_frustum[plane_index_left] ),
					"=m"( clipping_frustum[plane_index_right] ),
					"=m"( clipping_frustum[plane_index_top] )
				:	"m"( projection_view_frustum[plane_index_bottom] ),
					"m"( projection_view_frustum[plane_index_left] ),
					"m"( projection_view_frustum[plane_index_right] ),
					"m"( projection_view_frustum[plane_index_top] )
			);
		}

		// Is clipping required?
		bool is_clipping_required(const struct glvert_s* vertices, std::size_t vertex_count)
		{
			int res;
			__asm__ volatile (
				".set		push\n"					// save assembler option
				".set		noreorder\n"			// suppress reordering
				"move		$8,   %1\n"				// $8 = &vertices[0]
				"move		$9,   %2\n"				// $9 = vertex_count
				"li			$10,  20\n"				// $10 = 20( sizeof(glvert_t) )
				"mul		$10,  $10,   $9\n"		// $10 = $10 * $9
				"addu		$10,  $10,   $8\n"		// $10 = $10 + $8
			"0:\n"									// loop
				"ulv.q		C610, 8($8)\n"			// Load vertex into register skip tex cord 8 byte
				"vone.s		S613\n"					// Now set the 4th entry to be 1 as that is just random
				"vdot.q		S620, C700, C610\n"		// S620 = v[i] * frustrum[0]
				"vdot.q		S621, C710, C610\n"		// S621 = v[i] * frustrum[1]
				"vdot.q		S622, C720, C610\n"		// S622 = v[i] * frustrum[2]
				"vdot.q		S623, C730, C610\n"		// S623 = v[i] * frustrum[3]
				"vzero.q	C610\n"					// C610 = [0.0f, 0.0f, 0.0f. 0.0f]
				"addiu		%0,   $0, 1\n"			// res = 1
				"vcmp.q		LT,   C620, C610\n"		// S620 < 0.0f || S621 < 0.0f || S622 < 0.0f || S623 < 0.0f
				"bvt		4,    1f\n"				// if ( CC[4] == 1 ) jump to exit
				"addiu		$8,   $8,   20\n"		// $8 = $8 + 20( sizeof(glvert_t) )
				"bne		$10,  $8,   0b\n"		// if ( $9 != 0 ) jump to loop
				"move		%0,   $0\n"				// res = 0
			"1:\n"									// exit
				".set		pop\n"					// Restore assembler option
				:	"=r"(res)
				:	"r"(vertices), "r"(vertex_count)
				:	"$8", "$9", "$10"
			);
			return (res == 1) ? true : false;
		}

		// Clips a polygon against a plane.
		// http://hpcc.engin.umich.edu/CFD/users/charlton/Thesis/html/node90.html
		static void clip_to_plane(
			const plane_type& plane,
			const glvert_t* const unclipped_vertices,
			std::size_t const unclipped_vertex_count,
			glvert_t* const clipped_vertices,
			std::size_t* const clipped_vertex_count)
		{
			// Set up.
			const glvert_t* const	last_unclipped_vertex	= &unclipped_vertices[unclipped_vertex_count];

			// For each polygon edge...
			const glvert_t*	s				= unclipped_vertices;
			const glvert_t* p				= s + 1;
			glvert_t*		clipped_vertex	= clipped_vertices;
			do
			{
				// Check both nodal values, s and p. If the point values are:		
				const float s_val = (plane.x * s->xyz[0]) + (plane.y * s->xyz[1]) + (plane.z * s->xyz[2]) + plane.w;
				const float p_val = (plane.x * p->xyz[0]) + (plane.y * p->xyz[1]) + (plane.z * p->xyz[2]) + plane.w;
				
				if (s_val > 0.0f)
				{
					if (p_val > 0.0f)
					{
						// 1. Inside-inside, append the second node, p.
						*clipped_vertex++ = *p;
					}
					else
					{
						// 2. Inside-outside, compute and append the
						// intersection, i of edge sp with the clipping plane.
						clipped_vertex->st[0]  = s->st[0]  + ((s_val / (s_val - p_val)) * (p->st[0] - s->st[0]));
						clipped_vertex->st[1]  = s->st[1]  + ((s_val / (s_val - p_val)) * (p->st[1] - s->st[1]));
						clipped_vertex->xyz[0] = s->xyz[0] + ((s_val / (s_val - p_val)) * (p->xyz[0] - s->xyz[0]));
						clipped_vertex->xyz[1] = s->xyz[1] + ((s_val / (s_val - p_val)) * (p->xyz[1] - s->xyz[1]));
						clipped_vertex->xyz[2] = s->xyz[2] + ((s_val / (s_val - p_val)) * (p->xyz[2] - s->xyz[2]));
						clipped_vertex++;
					}
				}
				else
				{
					if (p_val > 0.0f)
					{
						// 4. Outside-inside, compute and append the
						// intersection i of edge sp with the clipping plane,
						// then append the second node p. 
						clipped_vertex->st[0]  = s->st[0]  + ((s_val / (s_val - p_val)) * (p->st[0] - s->st[0]));
						clipped_vertex->st[1]  = s->st[1]  + ((s_val / (s_val - p_val)) * (p->st[1] - s->st[1]));
						clipped_vertex->xyz[0] = s->xyz[0] + ((s_val / (s_val - p_val)) * (p->xyz[0] - s->xyz[0]));
						clipped_vertex->xyz[1] = s->xyz[1] + ((s_val / (s_val - p_val)) * (p->xyz[1] - s->xyz[1]));
						clipped_vertex->xyz[2] = s->xyz[2] + ((s_val / (s_val - p_val)) * (p->xyz[2] - s->xyz[2]));
						clipped_vertex++;

						*clipped_vertex++ = *p;
					}
					else
					{
						// 3. Outside-outside, no operation.
					}
				}

				// Next edge.
				s = p++;
				if (p == last_unclipped_vertex)
				{
					p = unclipped_vertices;
				}
			}
			while (s != unclipped_vertices);

			// Return the data.
			*clipped_vertex_count = clipped_vertex - clipped_vertices;
		}

		// Clips a polygon against the frustum.
		void clip(
			const struct glvert_s* unclipped_vertices,
			std::size_t unclipped_vertex_count,
			const struct glvert_s** clipped_vertices,
			std::size_t* clipped_vertex_count)
		{
			// No vertices to clip?
			if (!unclipped_vertex_count)
			{
				// Error.
				Sys_Error("Calling clip with zero vertices");
			}

			// Set up constants.
			const plane_type* const	last_plane		= &clipping_frustum[plane_count];

			// Set up the work buffer pointers.
			const glvert_t*			src				= unclipped_vertices;
			glvert_t*				dst				= work_buffer[0];
			std::size_t				vertex_count	= unclipped_vertex_count;

			// For each frustum plane...
			for (const plane_type* plane = &clipping_frustum[0]; plane != last_plane; ++plane)
			{
				// Clip the poly against this frustum plane.
				clip_to_plane(*plane, src, vertex_count, dst, &vertex_count);

				// No vertices left to clip?
				if (!vertex_count)
				{
					// Quit early.
					*clipped_vertex_count = 0;
					return;
				}

				// Use the next pair of buffers.
				src = dst;
				if (dst == work_buffer[0])
				{
					dst = work_buffer[1];
				}
				else
				{
					dst = work_buffer[0];
				}
			}

			// Fill in the return data.
			*clipped_vertices		= src;
			*clipped_vertex_count	= vertex_count;
		}
	}
}
