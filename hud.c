/*
Copyright (C) 1996-1997 Id Software, Inc.

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
#include "quakedef.h"
#include <pspgu.h>
int      	hudpic;	
int			sb_updates;
cvar_t hud_alpha = {"hud_alpha", "255"};

void Hud_LoadPics(void)
{
	hudpic = loadtextureimage_hud("textures/hud");
}
void Hud_Init (void)
{
	Hud_LoadPics(); //we are loading our precached pictures
	Cvar_RegisterVariable (&hud_alpha);
}

void Hud_Changed (void)
{
	sb_updates = 0;	
}

void DrawNumber(int x, int y, int number)
{
	int a,b,c,d,e;
	int numpoints[] ={0,25,45,70,95,120,145,170,190,215,0}; 
	a = number / 10000;
	b = (number/1000)%10;
	c = (number/100)%10;
	d = (number/10)%10;
	e = number%10;
	if(number >= 100)
		showimgpart (x, y, numpoints[c], 0, 25, 25, hudpic, 1,GU_RGBA(180, 124, 41, (int)hud_alpha.value));
	if(number >= 10)
		showimgpart (x+20, y, numpoints[d], 0, 25, 25, hudpic, 1,GU_RGBA(180, 124, 41, (int)hud_alpha.value));
	showimgpart (x+40, y, numpoints[e], 0, 25, 25, hudpic, 1,GU_RGBA(180, 124, 41, (int)hud_alpha.value));
}
void Hud_Draw (void)
{
	if (scr_con_current == vid.height)
		return;		
    if (scr_viewsize.value == 130)
        return;
	sb_updates++;
	
	showimgpart (0, 240, 50, 25, 20, 22, hudpic, 1,GU_RGBA(180, 124, 41,(int)hud_alpha.value));
	if(cl.stats[STAT_HEALTH] <= 0)
		DrawNumber(30,240,0);
	else
		DrawNumber(30,240,cl.stats[STAT_HEALTH]);
	DrawNumber(390,240,cl.stats[STAT_SHELLS]);
	DrawNumber(300,240,cl.stats[STAT_AMMO]); 
}
