// BUZER SHADOWS
// Shadows by buzer
// NON-Opengl32.dll fix by BlueNightHawk


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
#include "studio.h"
#include "Rendering/StudioModelRenderer.h"
#include "dlight.h"
#include "r_efx.h"

#include "segno_com.h"

#include "r_studioint.h"
extern engine_studio_api_t IEngineStudio;

extern cvar_s *g_cvShadows;

#include "com_model.h"
#include "studio.h"



#include "Rendering/GameStudioModelRenderer.h"
extern CGameStudioModelRenderer g_StudioRenderer;

#define	SURF_PLANEBACK		2
#define	SURF_DRAWSKY		4
#define SURF_DRAWSPRITE		8
#define SURF_DRAWTURB		0x10
#define SURF_DRAWTILED		0x20
#define SURF_DRAWBACKGROUND	0x40
#define SURF_UNDERWATER		0x80
#define SURF_DONTWARP		0x100
#define BACKFACE_EPSILON	0.01

// 0-2 are axial planes
#define	PLANE_X			0
#define	PLANE_Y			1
#define	PLANE_Z			2

// buz end

#pragma warning(disable:4018)


// buz start

int g_shadowpolycounter;

template <typename FuncType>
inline void LoadProcEXT( FuncType &pfn, const char* name )
{
	pfn = (FuncType)wglGetProcAddress( name );
}

#define LOAD_PROC_EXT(x) LoadProcEXT( x, #x );


class CArraysLocker
{
public:
	CArraysLocker( void )
	{
		const GLubyte *str = glGetString(GL_RENDERER);
		if (str)
		{
			// we're in opengl mode
			LOAD_PROC_EXT( glLockArraysEXT );
			LOAD_PROC_EXT( glUnlockArraysEXT );
		}
	}

	void Lock( GLsizei count )
	{
		glLockArraysEXT(0, count);
	}

	void Unlock( void )
	{
		glUnlockArraysEXT();
	}

private:
	PFNGLLOCKARRAYSEXTPROC		glLockArraysEXT;
	PFNGLUNLOCKARRAYSEXTPROC	glUnlockArraysEXT;
};



CArraysLocker	g_Lock;

myvec3_t	vertexdata[MaxShadowFaceCount * 5];
GLushort	indexdata[MaxShadowFaceCount * 3 * 5];



bool	g_bShadows;

void SetupBuffer( void )
{
	glVertexPointer( 3, GL_FLOAT, sizeof(vec3_t), vertexdata );
	glEnableClientState(GL_VERTEX_ARRAY);
	g_shadowpolycounter = 0;
	g_bShadows = true;
}

void ClearBuffer( void )
{
	if (!g_bShadows)
		return;

	glDisableClientState(GL_VERTEX_ARRAY);
}

void Shadows_Init(CStudioModelRenderer *renderer)
{
	// buz
	renderer->sv_skyvec_x = gEngfuncs.pfnGetCvarPointer("sv_skyvec_x");
	renderer->sv_skyvec_y = gEngfuncs.pfnGetCvarPointer("sv_skyvec_y");
	renderer->sv_skyvec_z = gEngfuncs.pfnGetCvarPointer("sv_skyvec_z");

	g_cvShadows = gEngfuncs.pfnRegisterVariable("sgn_gl_shadows", "1", FCVAR_ARCHIVE);
}

// buz end

//
// =========== buz start =============
//

/*
====================
BuildFaces

====================
*/
/*
====================
BuildFaces

====================
*/
void CStudioModelRenderer::BuildFaces(SubModelData& dst, mstudiomodel_t* src)
{
	// get number of triangles in all meshes
	int i, n = 0;
	mstudiomesh_t* pmeshes = (mstudiomesh_t*)((byte*)m_pStudioHeader + src->meshindex);
	for (i = 0; i < src->nummesh; i++)
	{
		int j;
		short* ptricmds = (short*)((byte*)m_pStudioHeader + pmeshes[i].triindex);
		while (j = *(ptricmds++))
		{
			if (j < 0) j *= -1;
			n += (j - 2);
			ptricmds += 4 * j;
		}
	}

	if (n == 0)  return;

	dst.faces.reserve(n);

	for (i = 0; i < src->nummesh; i++)
	{
		short* ptricmds = (short*)((byte*)m_pStudioHeader + pmeshes[i].triindex);

		int j;
		while (j = *(ptricmds++))
		{
			if (j > 0)
			{
				// convert triangle strip
				j -= 3;
				assert(j >= 0);

				short indices[3];
				indices[0] = ptricmds[0]; ptricmds += 4;
				indices[1] = ptricmds[0]; ptricmds += 4;
				indices[2] = ptricmds[0]; ptricmds += 4;
				dst.faces.push_back(Face(indices[0], indices[1], indices[2]));

				bool reverse = false;
				for (; j > 0; j--, ptricmds += 4)
				{
					indices[0] = indices[1];
					indices[1] = indices[2];
					indices[2] = ptricmds[0];

					if (!reverse)
						dst.faces.push_back(Face(indices[2], indices[1], indices[0]));
					else
						dst.faces.push_back(Face(indices[0], indices[1], indices[2]));
					reverse = !reverse;
				}
			}
			else
			{
				// convert triangle fan
				j = -j - 3;
				assert(j >= 0);

				short indices[3];
				indices[0] = ptricmds[0]; ptricmds += 4;
				indices[1] = ptricmds[0]; ptricmds += 4;
				indices[2] = ptricmds[0]; ptricmds += 4;
				dst.faces.push_back(Face(indices[0], indices[1], indices[2]));

				for (; j > 0; j--, ptricmds += 4)
				{
					indices[1] = indices[2];
					indices[2] = ptricmds[0];
					dst.faces.push_back(Face(indices[0], indices[1], indices[2]));
				}
			}
		}
	}
}

/*
====================
BuildEdges

====================
*/
void CStudioModelRenderer::BuildEdges(SubModelData& dst, mstudiomodel_t* src)
{
	if (dst.faces.size() == 0) return;

	dst.edges.reserve(dst.faces.size() * 3); // this is maximum
	for (int i = 0; i < dst.faces.size(); i++)
	{
		Face& f = dst.faces[i];
		AddEdge(dst, i, f.vertex0, f.vertex1);
		AddEdge(dst, i, f.vertex1, f.vertex2);
		AddEdge(dst, i, f.vertex2, f.vertex0);
	}

	dst.edges.resize(dst.edges.size()); // can i free unused memory by doing this?
}

/*
====================
AddEdge

====================
*/
void CStudioModelRenderer::AddEdge(SubModelData& dst, int face, int v0, int v1)
{
	// first look for face's neighbour
	for (int i = 0; i < dst.edges.size(); i++)
	{
		Edge& e = dst.edges[i];
		if ((e.vertex0 == v1) && (e.vertex1 == v0) && (e.face1 == -1))
		{
			e.face1 = face;
			return;
		}
	}

	// add new edge to list
	Edge e;
	e.face0 = face;
	e.face1 = -1;
	e.vertex0 = v0;
	e.vertex1 = v1;
	dst.edges.push_back(e);
}


void SpecialProcess(SubModelData& dst)
{
	// все индексы вершин умножаютс€ на 2, так как в массиве вершин кажда€ вершина
	// будет в двух экземпл€рах - оригинальна€, и смещЄнна€ на размер теневого объема

	int i;
	for (i = 0; i < dst.faces.size(); i++)
	{
		Face& f = dst.faces[i];
		f.vertex0 *= 2;
		f.vertex1 *= 2;
		f.vertex2 *= 2;
	}

	for (i = 0; i < dst.edges.size(); i++)
	{
		Edge& e = dst.edges[i];
		e.vertex0 *= 2;
		e.vertex1 *= 2;
	}
}

/*
====================
SetupModelExtraData

====================
*/
void CStudioModelRenderer::SetupModelExtraData(void)
{
	m_pCurretExtraData = &m_ExtraData[m_pRenderModel->name];

	if (m_pCurretExtraData->submodels.size() > 0)
		return;

	// generate extra data for this model
	gEngfuncs.Con_Printf("Generating extra data for model %s\n", m_pRenderModel->name);

	// get number of submodels
	int i, n = 0;
	mstudiobodyparts_t* bp = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex);
	for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
		n += bp[i].nummodels;

	if (n == 0)
	{
		gEngfuncs.Con_Printf("Error: model %s has 0 submodels\n", m_pRenderModel->name);
		return;
	}

	m_pCurretExtraData->submodels.resize(n);

	// convert strips and fans to triangles, generate adjacency info
	n = 0;
	int facecounter = 0, edgecounter = 0;
	for (i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		mstudiomodel_t* sm = (mstudiomodel_t*)((byte*)m_pStudioHeader + bp[i].modelindex);
		for (int j = 0; j < bp[i].nummodels; j++)
		{
			if (n >= m_pCurretExtraData->submodels.size())
			{
				gEngfuncs.Con_Printf("Warning: in model %s submodel index %d is out of range\n", m_pRenderModel->name, n);
				return;
			}

			BuildFaces(m_pCurretExtraData->submodels[n], &sm[j]);
			BuildEdges(m_pCurretExtraData->submodels[n], &sm[j]);
			SpecialProcess(m_pCurretExtraData->submodels[n]);

			facecounter += m_pCurretExtraData->submodels[n].faces.size();
			edgecounter += m_pCurretExtraData->submodels[n].edges.size();

			n++;
		}
	}

	gEngfuncs.Con_Printf("Done (%d polys, %d edges)\n", facecounter, edgecounter);
}

/*
====================
DrawShadowsForEnt

====================
*/
void CStudioModelRenderer::DrawShadowsForEnt(void)
{
	/*	myvec3_t tmp;
		VectorSubtract(m_pCurrentEntity->origin, m_vRenderOrigin, tmp);
		if (DotProduct(tmp, tmp) > (700 * 700))
			return;*/

	SetupModelExtraData();

	if (!m_pCurretExtraData)
		return;

	glDepthMask(GL_FALSE);
	glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE); // disable writes to color buffer
	glEnable(GL_STENCIL_TEST);
	glStencilFunc(GL_ALWAYS, 0, ~0);

	// i expected IEngineStudio.StudioSetupModel to return valid pointers to current
	// bodypart and submodel, but looks like it doesn't. Make it myself.		

	mstudiobodyparts_t* bp = (mstudiobodyparts_t*)((byte*)m_pStudioHeader + m_pStudioHeader->bodypartindex);

	int baseindex = 0;
	for (int i = 0; i < m_pStudioHeader->numbodyparts; i++)
	{
		int index = m_pCurrentEntity->curstate.body / bp[i].base;
		index = index % bp[i].nummodels;

		mstudiomodel_t* sm = (mstudiomodel_t*)((byte*)m_pStudioHeader + bp[i].modelindex) + index;
		DrawShadowVolume(m_pCurretExtraData->submodels[index + baseindex], sm);
		baseindex += bp[i].nummodels;
	}

	glDepthMask(GL_TRUE);
	glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
	glDisable(GL_STENCIL_TEST);
}

/*
====================
DrawShadowVolume

====================
*/

bool facelight[MaxShadowFaceCount];


void CStudioModelRenderer::DrawShadowVolume(SubModelData& data, mstudiomodel_t* model)
{
	if ((data.faces.size() == 0) || (data.faces.size() > MaxShadowFaceCount))
		return;

	GetShadowVector(m_ShadowDir);

	myvec3_t d;
	VectorScale(m_ShadowDir, 256, d);

	// transform vertices by bone matrices

	// get pointer to untransformed vertices
	myvec3_t* pstudioverts = (myvec3_t*)((byte*)m_pStudioHeader + model->vertindex);

	// get pointer to bone index for each vertex
	byte* pvertbone = ((byte*)m_pStudioHeader + model->vertinfoindex);


	int i, j;
	for (i = 0, j = 0; i < model->numverts; i++, j += 2)
	{
		VectorTransform(pstudioverts[i], (*m_pbonetransform)[pvertbone[i]], vertexdata[j]);
		VectorSubtract(vertexdata[j], d, vertexdata[j + 1]);
	}

	g_Lock.Lock(model->numverts * 2);

	int facecount = 0;
	GLushort* inddata = indexdata;

	std::vector<Face>::iterator f;
	for (f = data.faces.begin(), i = 0; f < data.faces.end(); ++f, ++i)
	{
		myvec3_t v1, v2, norm;
		VectorSubtract(vertexdata[f->vertex1], vertexdata[f->vertex0], v1);
		VectorSubtract(vertexdata[f->vertex2], vertexdata[f->vertex1], v2);
		CrossProduct(v2, v1, norm);
		facelight[i] = (DotProduct(norm, m_ShadowDir) >= 0);

		// comment this 'if' block if you want to use z-pass method
		if (facelight[i])
		{
			inddata[0] = f->vertex0;
			inddata[1] = f->vertex2;
			inddata[2] = f->vertex1;
			inddata[3] = f->vertex0 + 1;
			inddata[4] = f->vertex1 + 1;
			inddata[5] = f->vertex2 + 1;
			inddata += 6;
			facecount += 2;
		}
	}

	std::vector<Edge>::iterator e;
	for (e = data.edges.begin(); e < data.edges.end(); ++e)
	{
		if (facelight[e->face0])
		{
			if ((e->face1 != -1) && facelight[e->face1])
				continue;

			inddata[0] = e->vertex0;
			inddata[1] = e->vertex1;
		}
		else
		{
			if ((e->face1 == -1) || !facelight[e->face1])
				continue;

			inddata[0] = e->vertex1;
			inddata[1] = e->vertex0;
		}

		inddata[2] = inddata[0] + 1;
		inddata[3] = inddata[2];
		inddata[4] = inddata[1];
		inddata[5] = inddata[1] + 1;

		inddata += 6;

		facecount += 2;
	}

	//	assert((facecount * 3) <= (MaxShadowFaceCount * 5));

		// use this block for z-pass method

		// draw front faces incrementing stencil values
	/*	glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
		glCullFace(GL_BACK);
		glDrawElements(GL_TRIANGLES, facecount * 3, GL_UNSIGNED_SHORT, indexdata);

		// draw back faces decrementing stencil values
		glStencilOp(GL_KEEP, GL_KEEP, GL_DECR);
		glCullFace(GL_FRONT);
		glDrawElements(GL_TRIANGLES, facecount * 3, GL_UNSIGNED_SHORT, indexdata);
	*/

	// use this block for z-fail method

	// draw back faces incrementing stencil values when z fails
	glStencilOp(GL_KEEP, GL_INCR, GL_KEEP);
	glCullFace(GL_FRONT);
	glDrawElements(GL_TRIANGLES, facecount * 3, GL_UNSIGNED_SHORT, indexdata);

	// draw front faces decrementing stencil values when z fails
	glStencilOp(GL_KEEP, GL_DECR, GL_KEEP);
	glCullFace(GL_BACK);
	glDrawElements(GL_TRIANGLES, facecount * 3, GL_UNSIGNED_SHORT, indexdata);


	g_Lock.Unlock();

	g_shadowpolycounter += facecount * 2;
}

/*
====================
GetShadowVector

====================
*/
void CStudioModelRenderer::GetShadowVector(myvec3_t& vecOut)
{
	if ((sv_skyvec_x->value != 0) || (sv_skyvec_y->value != 0) || (sv_skyvec_z->value != 0))
	{
		vecOut[0] = -sv_skyvec_x->value;
		vecOut[1] = -sv_skyvec_y->value;
		vecOut[2] = -sv_skyvec_z->value;
	}
	else
	{
		vecOut[0] = 0.5;
		vecOut[1] = 1;
		vecOut[2] = 1.5;
	}

	VectorNormalize(vecOut);
}


mleaf_t* Mod_PointInLeaf(vec3_t p, model_t* model) // quake's func
{
	mnode_t* node = model->nodes;
	while (1)
	{
		if (node->contents < 0)
			return (mleaf_t*)node;
		mplane_t* plane = node->plane;
		float d = DotProduct(p, plane->normal) - plane->dist;
		if (d > 0)
			node = node->children[0];
		else
			node = node->children[1];
	}

	return NULL;	// never reached
}

model_t* g_pworld;
int		g_visframe;
int		g_framecount;
myvec3_t	g_lightvec;

void RecursiveDrawWorld(mnode_t* node)
{
	if (node->contents == CONTENTS_SOLID)
		return;

	if (node->visframe != g_visframe)
		return;

	if (node->contents < 0)
		return;		// faces already marked by engine

	// recurse down the children, Order doesn't matter
	RecursiveDrawWorld(node->children[0]);
	RecursiveDrawWorld(node->children[1]);

	// draw stuff
	int c = node->numsurfaces;
	if (c)
	{
		msurface_t* surf = g_pworld->surfaces + node->firstsurface;

		for (; c; c--, surf++)
		{
			if (surf->visframe != g_framecount)
				continue;

			if (surf->flags & (SURF_DRAWSKY | SURF_DRAWTURB | SURF_UNDERWATER))
				continue;

			// cull from light vector

			float dot;
			mplane_t* plane = surf->plane;

			switch (plane->type)
			{
			case PLANE_X:
				dot = g_lightvec[0];	break;
			case PLANE_Y:
				dot = g_lightvec[1];	break;
			case PLANE_Z:
				dot = g_lightvec[2];	break;
			default:
				dot = DotProduct(g_lightvec, plane->normal); break;
			}

			if ((dot > 0) && (surf->flags & SURF_PLANEBACK))
				continue;

			if ((dot < 0) && !(surf->flags & SURF_PLANEBACK))
				continue;

			glpoly_t* p = surf->polys;
			float* v = p->verts[0];

			glBegin(GL_POLYGON);
			for (int i = 0; i < p->numverts; i++, v += VERTEXSIZE)
			{
				glTexCoord2f(v[3], v[4]);
				glVertex3fv(v);
			}
			glEnd();
		}
	}
}

PFNGLACTIVETEXTUREARBPROC glActiveTextureARB = NULL;

void DrawShadows(void)
{
	ClearBuffer();

	// buz start
	// тут можно бюло просто нарисовать серый квадрат на весь экран, как это часто делают
	// в стенсильных тен€х такого рода, однако вместо этого € пробегаюсь по полигонам world'а,
	// и рисую серым только те, которые обращены к "солнышку", чтобы тень не рисовалась
	// на "обратных" стенках.
	if (g_bShadows && IEngineStudio.IsHardware())
	{
		if (NULL == glActiveTextureARB)
			glActiveTextureARB = (PFNGLACTIVETEXTUREARBPROC)wglGetProcAddress("glActiveTextureARB");

		glPushAttrib(GL_ALL_ATTRIB_BITS);

		// buz: workaround half-life's bug, when multitexturing left enabled after
		// rendering brush entities
		glActiveTextureARB(GL_TEXTURE1_ARB);
		glDisable(GL_TEXTURE_2D);
		glActiveTextureARB(GL_TEXTURE0_ARB);

		glDepthMask(GL_FALSE);
		glDisable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glBlendFunc(GL_DST_COLOR, GL_ZERO);
		glColor4f(0.5, 0.5, 0.5, 1);

		glStencilFunc(GL_NOTEQUAL, 0, ~0);
		glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
		glEnable(GL_STENCIL_TEST);

		// get current visframe number
		g_pworld = gEngfuncs.GetEntityByIndex(0)->model;
		mleaf_t* leaf = Mod_PointInLeaf(g_StudioRenderer.m_vRenderOrigin, g_pworld);
		g_visframe = leaf->visframe;

		// get current frame number
		g_framecount = g_StudioRenderer.m_nFrameCount;

		// get light vector
		g_StudioRenderer.GetShadowVector(g_lightvec);

		// draw world
		RecursiveDrawWorld(g_pworld->nodes);

		glPopAttrib();
	}

	g_bShadows = false;
	// buz end
}

// buz end

// blue start
void H_ClearBuffer(void)
{
	// taken from the custom opengl32.dll. Called from createentities in entity.cpp.
	// works exactly like the modified opengl32.dll
	int hasStencil;
	glGetIntegerv(GL_STENCIL_BITS, &hasStencil);
	if (hasStencil)
	{
		glClear(GL_STENCIL_BUFFER_BIT);
		glClear(GL_STENCIL_BITS);
		glClearStencil(0);
	}
}
// blue end