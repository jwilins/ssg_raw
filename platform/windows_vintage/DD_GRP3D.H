/*                                                                           */
/*   DD_GRP3D.h   ３Ｄ時の図形描画関数                                       */
/*                                                                           */
/*                                                                           */

#ifndef PBGWIN_DD_GRP3D_H
#define PBGWIN_DD_GRP3D_H		"DD_GRP3D : Version 0.03 : Update 2000/02/22"


// 更新履歴 //

// 2000/02/22 : GrpGrdPolygon() を追加
// 1999/12/04 : レーザー用特殊α関数を追加
// 1999/12/02 : やっと一部の関数が動作し始めた。16Bit-αはやっぱり綺麗だな...


// ヘッダファイル //
#include "platform/graphics_backend.h"

class GRAPHICS_GEOMETRY_D3D : public GRAPHICS_GEOMETRY {
public:
	// Generic methods
	// ---------------

	void Lock(void) final;
	void Unlock(void) final;
	void SetColor(RGB216 col) final;
	void SetAlphaNorm(uint8_t a) final;
	void SetAlphaOne(void) final;
	void DrawLine(int x1, int y1, int x2, int y2) final;
	void DrawBox(int x1, int y1, int x2, int y2) final;
	void DrawBoxA(int x1, int y1, int x2, int y2) final;
	void DrawTriangleFan(VERTEX_XY_SPAN<>) final;
	// ----------

	// Poly methods
	// ------------

	void DrawLineStrip(VERTEX_XY_SPAN<>);
	void DrawTriangles(
		TRIANGLE_PRIMITIVE, VERTEX_XY_SPAN<>, VERTEX_RGBA_SPAN<> colors = {}
	);
	void DrawTrianglesA(
		TRIANGLE_PRIMITIVE, VERTEX_XY_SPAN<>, VERTEX_RGBA_SPAN<> colors = {}
	);
	void DrawGrdLineEx(int x, int y1, RGB c1, int y2, RGB c2);
	// ------------
};

// Returns the instance of the channeled geometry interface if active, or a
// nullptr otherwise.
GRAPHICS_GEOMETRY_D3D *GrpGeom_Poly(void);

#endif
