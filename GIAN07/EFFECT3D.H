/*                                                                           */
/*   Effect3D.h   ３Ｄエフェクトの処理                                       */
/*                                                                           */
/*                                                                           */

#ifndef PBGWIN_EFFECT3D_H
#define PBGWIN_EFFECT3D_H	"EFFECT3D : Ver 0.01 : Update 2000/05/31"
//#pragma message(PBGWIN_EFFECT3D_H)



///// [更新履歴] /////
// 2000/05/31 : 開発開始



///// [ヘッダファイル] /////
import std.compat;
#include "game/coords.h"



///// [ 定数 ] /////
#define STG4ROCK_STDMOVE	0		// 普通のスクロールね
#define STG4ROCK_ACCMOVE1	1		// 加速有りスクロール(1)
#define STG4ROCK_ACCMOVE2	2		// 加速有りスクロール(2)
#define STG4ROCK_3DMOVE		3		// ３Ｄ回転
#define STG4ROCK_LEAVE		4		// 一時的に岩を消去する
#define STG4ROCK_END		5		// エフェクト終了



///// [ 構造体 ] /////
typedef struct tagPoint3D{
	WORLD_COORD x, y, z;
} Point3D;

typedef struct tagLineList3D{
	PIXEL_POINT	center;	/* 頂点の座標の補正用 */
	std::span<WORLD_POINT>	p;	/* 頂点の座標         */

	uint8_t	DegX, DegY, DegZ;	/* 各軸に対する回転角 */
} LineList3D;

typedef struct tagCircle3D{
	int		ox,oy;
	int		r;
	uint8_t	deg;
	uint8_t	n;
} Circle3D;

typedef struct tagDeg3D{
	int		dx;
	int		dy;
	int		dz;
} Deg3D;

typedef struct tagCube3D{
	Point3D		p;
	Deg3D		d;
	int			l;
} Cube3D;

typedef struct tagStar2D {
	int		x,y;
	int		vy;
} Star2D;

// 雲管理用構造体 //
typedef struct tagCloud2D {
	int		x, y;		// ｘ６４座標
	int		vy;			// 速度のｙ成分(ｙしかないけど)
	uint8_t	type;	// 雲の種類
} Cloud2D;

// 非・汎用２Ｄ正方形ワイヤーフレーム //
typedef struct tagWFLine2D {
	int		ox,oy;		// 中心座標
	int		w;			// 正方形の一辺の長さ
	uint8_t	d;	// 正方形の傾き角度
} WFLine2D;

// 偽ＥＣＬ羅列管理用構造体 //
typedef struct tagFakeECLString {
	int		SrcX, SrcY;		// 元画像の基準となるＸ＆Ｙ座標
	int		x,y;			// 現在の座標x64
	int		vx,vy;			// 現在の速度成分x64
} FakeECLString;

// 岩管理用構造体 //
typedef struct tagRock3D {
	int		x, y, z;		// 現在の座標
	int		vx, vy;			// 速度XY成分(2D-平面上)

	uint32_t	count;	// カウンタ
	int		v;				// 速度

	char	a;				// 加速度
	uint8_t	d;	// 角度(2D-平面上)
	uint8_t	GrpID;	// グラフィックＩＤ
	uint8_t	State;	// 現在の状態
} Rock3D;


///// [ 関数 ] /////
void InitLineList3D(std::span<LineList3D> w);
void DrawLineList3D(std::span<const LineList3D> w);

void InitWarning(void);
void DrawWarning(void);
void MoveWarning(uint8_t count);
void MoveWarningR(char count);

void Init3DCube(void);
void Draw3DCube(void);
void Move3DCube(void);
/*
void InitStg3Cloud(void);	// ３面の雲の初期化を行う
void MoveStg3Cloud(void);	// ３面の雲を動作させる
void DrawStg3Cloud(void);	// ３面の雲を描画する
*/
void InitEffectFakeECL(void);
void MoveEffectFakeECL(void);
void DrawEffectFakeECL(void);

void InitStg4Rock(void);	// ステージ４の背景となる岩の集団
void MoveStg4Rock(void);	//
void DrawStg4Rock(void);	//
void SendCmdStg4Rock(uint8_t Cmd, uint8_t Param);	// コマンド送信

void InitStg6Raster();	// ６面ラスター初期化
void MoveStg6Raster();	// ６面ラスター動作
void DrawStg6Raster();	// ６面ラスター描画

void InitStg3Star();	// ３面高速星初期化
void MoveStg3Star();	// ３面高速星動作
void DrawStg3Star();	// ３面高速星描画

#endif
