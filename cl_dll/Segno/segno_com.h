#pragma once

#include "com_model.h"
#include "windows.h"
#include "../GL/GL.H"
#include "../GL/GLEXT.H"
#include "Rendering/StudioUtil.h"

extern bool g_bShadows;

// similar to server side traceresult
typedef struct trace_s {
	int		fAllSolid;			// if true, plane is not valid
	int		fStartSolid;		// if true, the initial point was in a solid area
	int		fInOpen;
	int		fInWater;
	float	flFraction;			// time completed, 1.0 = didn't hit anything
	vec3_t	vecEndPos;			// final position
	float	flPlaneDist;
	vec3_t	vecPlaneNormal;		// surface normal at impact
	struct physent_s* pHit;				// entity the surface is on
	int		iHitgroup;			// 0 == generic, non zero is specific body part
} TRACE;


typedef struct viewparams_s {
	vec3_t vieworg; // view origin
	vec3_t viewangles; // view angles
	vec3_t attachmentvecs[4][3]; // forward, right, up
	vec3_t attachmentangles[4]; // pitch, yaw, roll
	vec3_t camangles[4];

	int ducking; // is local player ducking?
	int iCamAttchment; // camera will follow attachment number
	int iConstCamAttchment; // put the attachment for this a little behind the iCamAttchment attachment
	

	// TODO : add more stuff here
} viewparams_t;

typedef struct util_s {
	vec3_t VecToAngles(vec3_t forward);
	vec3_t VecToAngles(vec3_t forward, vec3_t up);

	vec3_t FloatToVec(float* in);

	// returns true if flFraction != 1.0
	bool TraceLine(vec3_t vecStart, vec3_t vecEnd, bool ignore_glass = false, int ent_ignore = -1, TRACE* ptr = NULL, int flags = -1 /*optional pmtrace flags*/);

	int PointContents(vec3_t origin);



	// TODO : add more stuff here
} util_t;



typedef struct ref_com_s {
	cl_entity_s* player();
	cl_entity_s* view(); // return viewmodel pointer
	float frametime;
	double time( void );

	// TODO : add more stuff here
} ref_com_t;

extern ref_com_t com;
extern util_t util;
extern viewparams_t viewparams;