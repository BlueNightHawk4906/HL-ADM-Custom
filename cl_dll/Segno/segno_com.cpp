#include "HUD/HUD.h"
#include "cl_util.h"
#include "cvardef.h"
#include "usercmd.h"
#include "const.h"

#include "entity_state.h"
#include "cl_entity.h"
#include "ref_params.h"
#include "Input/IN_Defs.h" // PITCH YAW ROLL
#include "pm_movevars.h"
#include "pm_shared.h"
#include "pm_defs.h"
#include "event_api.h"
#include "pmtrace.h"
#include "screenfade.h"
#include "shake.h"
#include "hltv.h"

#include "buz_com_model.h"
#include "studio.h"
#include "Rendering/StudioModelRenderer.h"
#include "dlight.h"
#include "r_efx.h"

#include "segno_com.h"
cvar_t	*g_cvShadows; // buz
CStudioModelRenderer* pRenderer = NULL;

ref_com_t com;
util_t util;
viewparams_t viewparams;

extern void UpdateViewAttachments(viewparams_s* ptr);

#define TEST_DLIGHT 0

void Dlight_Test()
{
	dlight_t* dl = gEngfuncs.pEfxAPI->CL_AllocDlight(0);
	TRACE tr;
	util.TraceLine(viewparams.vieworg, viewparams.vieworg + viewparams.attachmentvecs[0][0] * 8192, false, -1, &tr);
	dl->origin = tr.vecEndPos;
	dl->die = gEngfuncs.GetClientTime();	// die at next frame
	dl->color.r = 100;
	dl->color.g = 100;
	dl->color.b = 100;
	dl->radius = 200;
}

// VIEWPARAMS
void UpdateViewParams(struct ref_params_s* pparams)
{
	viewparams.vieworg = pparams->vieworg;
	viewparams.viewangles = pparams->viewangles;
	UpdateViewAttachments(&viewparams);

	// test to see if attachment angles are working 
#if TEST_DLIGHT == 1
	Dlight_Test();
#endif
}


// COMMON STUFF
cl_entity_s* ref_com_s::view()
{
	return gEngfuncs.GetViewModel();
}
cl_entity_s* ref_com_s::player()
{
	return gEngfuncs.GetLocalPlayer();
}

double ref_com_s::time(void)
{
	return gEngfuncs.GetClientTime();
}


// UTILITIES
int util_s::PointContents(vec3_t origin)
{
	return gEngfuncs.PM_PointContents(origin, NULL);
}

bool util_s::TraceLine(vec3_t vecStart, vec3_t vecEnd, bool ignore_glass, int ent_ignore, TRACE* ptr, int flags)
{
	pmtrace_t pmtrace;
	physent_t* pe = nullptr;

	gEngfuncs.pEventAPI->EV_SetTraceHull(2);

	if (flags == -1)
	{
		if (ignore_glass)
			flags = PM_STUDIO_BOX | PM_GLASS_IGNORE;
		else
			flags = PM_STUDIO_BOX;
	}

	gEngfuncs.pEventAPI->EV_PlayerTrace(vecStart, vecEnd, flags, ent_ignore, &pmtrace);
	if (pmtrace.fraction != 1)
	{
		pe = gEngfuncs.pEventAPI->EV_GetPhysent(pmtrace.ent);
	}
	if (ptr)
	{
		ptr->fAllSolid = pmtrace.allsolid;
		ptr->fStartSolid = pmtrace.startsolid;
		ptr->fInOpen = pmtrace.inopen;
		ptr->fInWater = (PointContents(pmtrace.endpos) == CONTENTS_WATER) ? 1 : 0;
		ptr->flFraction = pmtrace.fraction;
		ptr->vecEndPos = pmtrace.endpos;
		ptr->vecPlaneNormal = pmtrace.plane.normal;
		ptr->flPlaneDist = pmtrace.plane.dist;
		if (pe)
		{
			ptr->pHit = pe;
		}
		ptr->iHitgroup = pmtrace.hitgroup;
	}

	if (pmtrace.fraction != 1)
	{
		return true;
	}
	return false;


}

#define M_PI		3.14159265358979323846	// matches value in gcc v2 math.h
vec3_t util_s::VecToAngles(vec3_t forward)
{
	vec3_t angles;

	float	tmp, yaw, pitch;

	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	// Convert radians to degrees.
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = 0;
	return angles;
}
// returns roll value
vec3_t util_s::VecToAngles(vec3_t forward, vec3_t up)
{
	vec3_t angles;

	float	tmp, yaw, pitch, tempyaw;

	if (forward[1] == 0 && forward[0] == 0)
	{
		yaw = tempyaw = 0;
		if (forward[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		tempyaw = atan2(forward[1], forward[0]);
		yaw = (atan2(forward[1], forward[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		tmp = sqrt(forward[0] * forward[0] + forward[1] * forward[1]);
		pitch = (atan2(forward[2], tmp) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	// Find the vector in the xy plane 90 degrees to the right of our bearing.
	float planeRightX = sin(tempyaw);
	float planeRightY = -cos(tempyaw);

	// Roll is the rightward lean of our up vector, computed here using a dot product.
	float roll = asin(up[0] * planeRightX + up[1] * planeRightY);
	// If we're twisted upside-down, return a roll in the range +-(pi/2, pi)
	if (up.z < 0)
		roll = ((0.0f <= roll) - (roll < 0.0f)) * M_PI - roll;


	// Convert radians to degrees.
	angles[0] = pitch;
	angles[1] = yaw;
	angles[2] = roll * 180 / M_PI;
	return angles;
}