/*                                                                           */
/*   DD_GRP2D.cpp   ２Ｄ＆８Ｂｉｔ時の図形描画関数                           */
/*                                                                           */
/*                                                                           */

#include "DD_UTY.H"
#include "constants.h"
#include "DD_GRP2D.H"
#include "DD_CLIP2D.H"		// 図形クリッピング用関数群
#include "game/cast.h"
#include <ddraw.h>

#pragma message(PBGWIN_DD_GRP2D_H)

#define ATABLE_MAX	8
#define ALPHA_ONE	0x00	// 一種の加算α
#define ALPHA_NORM	0x01	// ノーマルなSrc-α

typedef struct tagAlphaInfo {
	DWORD	CheckValue;
	DWORD	RefCount;
} AlphaInfo;


// ２Ｄ用変数 //
BYTE				*Grp_Vram;				// これは公開しない
long				Grp_Pitch = 0;			// これも公開しない
uint32_t	Grp_AddrY[GRP_RES.h];	// Ｙ座標テーブル
uint8_t	Col2D;	// 現在の色
BYTE				Col2D_R;				// 赤(α用)
BYTE				Col2D_G;				// 緑(α用)
BYTE				Col2D_B;				// 青(α用)
BYTE				Col2D_A;				// α値
BYTE				Col2D_Mode;				// αモード
BOOL				AlphaFlag = TRUE;		// αテーブルが更新されたかどうか
BYTE				AlphaBuf[ATABLE_MAX][256];		// αテーブル
BYTE				*AlphaTable = AlphaBuf[0];		// 現在の参照先
AlphaInfo			AInfo[ATABLE_MAX];
BYTE				ATableRef = 0;					// αテーブル格納先


// ２Ｄ用関数(補助) //
static void _2DGrpHline(int x1, int x2_inclusive, int y);	// 横ラインを引く
static void _2DGrpCHline(int x1, int x2, int y);	// 横ラインを引く(ｸﾘｯﾋﾟﾝｸﾞ有り)
static void _2DGrpHlineA(int x1, int x2_exclusive, int y);	// α横ラインを引く
static void _2DUpdateAlphaTable(void);	// αテーブルを更新する


// 描画系関数ポインタをセットする //
void GRAPHICS_GEOMETRY_DDRAW::Setup(void)
{
	// 固有の処理(この関数はSurfaceが存在するときに呼ばれるはず) //
	Lock();
	for(const auto i : std::views::iota(0, GRP_RES.h)) {
		Grp_AddrY[i] = (i * Grp_Pitch);
	}
	Unlock();

	// テーブル初期化要求を発行 //
	DisableAlphaTable();
}

// 画面描画の準備をする //
void GRAPHICS_GEOMETRY_DDRAW::Lock(void)
{
	DDSURFACEDESC	ddsd;

	memset(&ddsd,0,sizeof(DDSURFACEDESC));
	ddsd.dwSize = sizeof(ddsd);
	DxObj.Back->Lock(NULL,&ddsd,DDLOCK_WAIT,NULL);

	Grp_Vram  = static_cast<uint8_t *>(ddsd.lpSurface);
	Grp_Pitch = ddsd.lPitch;
}

// 画面描画を完了する //
void GRAPHICS_GEOMETRY_DDRAW::Unlock(void)
{
	DxObj.Back->Unlock(NULL);
	Grp_Vram  = NULL;
	Grp_Pitch = 0;		// これはいらないかな...
}

// 色セット //
void GRAPHICS_GEOMETRY_DDRAW::SetColor(RGB216 col)
{
	Col2D_R = col.r;
	Col2D_G = col.g;
	Col2D_B = col.b;
	Col2D = col.PaletteIndex();
	AlphaFlag = TRUE;
}

// αセット //
void GRAPHICS_GEOMETRY_DDRAW::SetAlphaNorm(uint8_t a)
{
	Col2D_A    = a;
	Col2D_Mode = ALPHA_NORM;
	AlphaFlag = TRUE;
}

void GRAPHICS_GEOMETRY_DDRAW::SetAlphaOne(void)
{
	Col2D_A    = 0xFF;
	Col2D_Mode = ALPHA_ONE;
	AlphaFlag = TRUE;
}

// 直線 //
void GRAPHICS_GEOMETRY_DDRAW::DrawLine(int x1, int y1, int x2, int y2)
{
	int sx,sy,dx,dy,s,i;
	const auto c = Col2D;

	if(Grp_Vram==NULL) return;

	//if(!Grp_LClip(&x1,&y1,&x2,&y2)) return;
	if(!GrpLineClip(&x1,&y1,&x2,&y2)) return;

	if(x1>x2)	sx = -1, dx = x1-x2;
	else		sx =  1, dx = x2-x1;

	if(y1>y2)	sy = -1, dy = y1-y2;
	else		sy =  1, dy = y2-y1;

	if(dx>dy){
		s = dx>>1;
		for(i=0;i<=dx;i++){
			*(Grp_Vram+x1+Grp_AddrY[y1]) = c;
			s+=dy;
			if(s>dx){
				s-=dx;
				y1+=sy;
			}
			x1+=sx;
		}
	}
	else{
		s = dy>>1;
		for(i=0;i<=dy;i++){
			*(Grp_Vram+x1+Grp_AddrY[y1]) = c;
			s+=dx;
			if(s>dy){
				s-=dy;
				x1+=sx;
			}
			y1+=sy;
		}
	}
}

// 長方形 //
void GRAPHICS_GEOMETRY_DDRAW::DrawBox(int x1, int y1, int x2, int y2)
{
	int i;

	if(_2DClipRect(&x1,&y1,&x2,&y2)){
		for(i=y1;i<y2;i++)
			_2DGrpHline(x1, (x2 - 1), i);
	}
}

void GRAPHICS_GEOMETRY_DDRAW::DrawPoint(WINDOW_POINT p)
{
	if(!Grp_Vram) {
		return;
	}
	if(_2DChkPoint(p.x, p.y)) {
		Grp_Vram[Grp_AddrY[p.y] + p.x] = Col2D;
	}
}

// α長方形 //
void GRAPHICS_GEOMETRY_DDRAW::DrawBoxA(int x1, int y1, int x2, int y2)
{
	int i;

	// 工事中につき、少々お待ち下さい... //
	if(AlphaFlag) _2DUpdateAlphaTable();

	if(_2DClipRect(&x1,&y1,&x2,&y2)){
		for(i=y1;i<y2;i++)
			_2DGrpHlineA(x1,x2,i);
	}

}

// 横ラインを引く //
static void _2DGrpHline(int x1, int x2_inclusive, int y)
{
	memset((Grp_Vram + Grp_AddrY[y] + x1), Col2D, ((x2_inclusive - x1) + 1));
}

// 横ラインを引く(ｸﾘｯﾋﾟﾝｸﾞ有り) //
void GRAPHICS_GEOMETRY_DDRAW::DrawHLine(int x1, int x2, int y)
{
	extern int CLIP_X_MIN,CLIP_X_MAX,CLIP_Y_MIN,CLIP_Y_MAX;
	int		temp;

	if(!Grp_Vram) {
		return;
	}

	// 完全判定 //
	if(
		(x1 > CLIP_X_MAX) ||
		(x2 < CLIP_X_MIN) ||
		(y > CLIP_Y_MAX) ||
		(y < CLIP_Y_MIN)
	 ) {
		return;
	}

	// Ｘクリッピング //
	if(x1<CLIP_X_MIN) x1  = CLIP_X_MIN;
	if(x2 > CLIP_X_MAX) {
		x2 = CLIP_X_MAX;
	}

	if(x1>x2){
		temp = x1;
		x1   = x2;
		x2   = temp;
	}

	_2DGrpHline(x1,x2,y);
}

// α横ラインを引く //
static void _2DGrpHlineA(int x1, int x2_exclusive, int y)
{
	union{
		DWORDLONG	dq;
		struct{
			BYTE	db0;
			BYTE	db1;
			BYTE	db2;
			BYTE	db3;
			BYTE	db4;
			BYTE	db5;
			BYTE	db6;
			BYTE	db7;
		};
	} Dat64;

	int		i,temp;
	BYTE	*write;

	if(Grp_Vram==NULL) return;
	write = Grp_Vram + x1 + Grp_AddrY[y];
	x2_exclusive -= x1;
	temp = (x2_exclusive >> 3);
	for(i=0;i<temp;i++){
		Dat64.dq  = *reinterpret_cast<DWORDLONG *>(write);
		Dat64.db0 = AlphaTable[Dat64.db0];
		Dat64.db1 = AlphaTable[Dat64.db1];
		Dat64.db2 = AlphaTable[Dat64.db2];
		Dat64.db3 = AlphaTable[Dat64.db3];
		Dat64.db4 = AlphaTable[Dat64.db4];
		Dat64.db5 = AlphaTable[Dat64.db5];
		Dat64.db6 = AlphaTable[Dat64.db6];
		Dat64.db7 = AlphaTable[Dat64.db7];
		*reinterpret_cast<DWORDLONG *>(write) = Dat64.dq;
		write+=8;
	}

	Dat64.dq  = *reinterpret_cast<DWORDLONG *>(write);
	switch(x2_exclusive & 7) {
		case 7:	Dat64.db6 = AlphaTable[Dat64.db6];	[[fallthrough]];
		case 6:	Dat64.db5 = AlphaTable[Dat64.db5];	[[fallthrough]];
		case 5:	Dat64.db4 = AlphaTable[Dat64.db4];	[[fallthrough]];
		case 4:	Dat64.db3 = AlphaTable[Dat64.db3];	[[fallthrough]];
		case 3:	Dat64.db2 = AlphaTable[Dat64.db2];	[[fallthrough]];
		case 2:	Dat64.db1 = AlphaTable[Dat64.db1];	[[fallthrough]];
		case 1:	Dat64.db0 = AlphaTable[Dat64.db0];
			*reinterpret_cast<DWORDLONG *>(write) = Dat64.dq;
			[[fallthrough]];
		case 0:	return;
	}
}

// αテーブルを更新する //
static RGB216 RGB216FromPalette(uint8_t index)
{
	return RGB216{
		((Cast::up<uint16_t>(DxObj.Palette[index].r) * RGB216::MAX) / 255u),
		((Cast::up<uint16_t>(DxObj.Palette[index].g) * RGB216::MAX) / 255u),
		((Cast::up<uint16_t>(DxObj.Palette[index].b) * RGB216::MAX) / 255u),
	};
}

static uint8_t _2DBlendOne(const RGB216& col)
{
	return RGB216{
		(((Col2D_R + col.r) > RGB216::MAX) ? RGB216::MAX : Col2D_R),
		(((Col2D_G + col.g) > RGB216::MAX) ? RGB216::MAX : Col2D_G),
		(((Col2D_B + col.b) > RGB216::MAX) ? RGB216::MAX : Col2D_B),
	}.PaletteIndex();
}

// Result := Src*Alpha + Dest*(1-Alpha) //
static uint8_t _2DBlendNorm(const RGB216& col)
{
	return RGB216::Clamped(
		(((Col2D_R * Col2D_A) + (col.r * (255 - Col2D_A))) >> 8),
		(((Col2D_G * Col2D_A) + (col.g * (255 - Col2D_A))) >> 8),
		(((Col2D_B * Col2D_A) + (col.b * (255 - Col2D_A))) >> 8)
	).PaletteIndex();
}

template <uint8_t (*Func)(const RGB216&)> static void _2DBlend(uint8_t *table)
{
	// 固定パレットに対する処理 //
	RGB216::ForEach([&](const RGB216& col) {
		table[col.PaletteIndex()] = Func(col);
	});
	// 可変パレットに対する処理 //
	for(int pal = 0; pal < 20; pal++) {
		table[  0 + pal] = Func(RGB216FromPalette(  0 + pal));
		table[236 + pal] = Func(RGB216FromPalette(236 + pal));
	}
}

static void _2DUpdateAlphaTable(void)
{
	int		i;
	BYTE	ref;

	AlphaFlag = FALSE;

	// 参照テーブルから検索を行う //
	const auto chk = (
		Col2D +
		(static_cast<uint32_t>(Col2D_A) << 8) +
		(static_cast<uint32_t>(Col2D_Mode) << 16)
	);
	for(i=0; i<ATABLE_MAX; i++){
		// すでにキャッシュが効いている場合 //
		if(chk == AInfo[i].CheckValue){
			AInfo[i].RefCount++;
			AlphaTable = AlphaBuf[i];
			// DebugOut(u8"αテーブルキャッシュＯＫ");
			return;
		}
	}

	// 参考：後で参照カウントの最小値をループ内で検索し、
	//       それに対して変更を行うようにするのだ

	ref        = ATableRef;
	ATableRef  = (ATableRef+1)%ATABLE_MAX;
	AlphaTable = AlphaBuf[ref];
	AInfo[ref].CheckValue = chk;
	AInfo[ref].RefCount   = 0;

	switch(Col2D_Mode) {
	case(ALPHA_ONE): 	_2DBlend<_2DBlendOne>(AlphaTable); 	break;
	case(ALPHA_NORM):	_2DBlend<_2DBlendNorm>(AlphaTable);	break;
	}
}

void DisableAlphaTable(void)
{
	for(auto& it : AInfo) {
		it.CheckValue = 0xffffffff; // 無効化する
	}

	ATableRef = 0;
	AlphaFlag = TRUE;
}
