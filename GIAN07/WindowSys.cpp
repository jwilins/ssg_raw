/*                                                                           */
/*   WINDOWSYS.cpp   コマンドウィンドウ処理                                  */
/*                                                                           */
/*                                                                           */

#include "WindowSys.h"
#include "LOADER.H"
#include "FONTUTY.H"
#include "platform/text_backend.h"
#include "game/enum_flags.h"
#include "game/snd.h"
#include "game/ut_math.h"


///// [構造体] /////

// メッセージウィンドウ管理用構造体 //
typedef struct tagMSG_WINDOW{
	WINDOW_LTRB	MaxSize;	// ウィンドウの最終的な大きさ
	WINDOW_LTRB	NowSize;	// ウィンドウの現在のサイズ
	PIXEL_POINT	TextTopleft;

	MSG_WINDOW_FLAGS	Flags;
	FONT_ID	FontID;	// 使用するフォント
	uint8_t	FontDy;	// フォントのＹ増量値
	uint8_t	State;	// 状態
	uint8_t	MaxLine;	// 最大表示可能行数
	uint8_t	Line;	// 次に挿入する行

	uint8_t	FaceID;	// 使用する顔番号
	uint8_t	NextFace;	// 次に表示する顔番号
	uint8_t	FaceState;	// 顔の状態
	uint8_t	FaceTime;	// 顔表示用カウンタ

	Narrow::string_view	Msg[MSG_HEIGHT];	// 表示するメッセージへのポインタ

	// Contains all text from [Msg], concatenated with '\n'.
	Narrow::string	Text;

	std::optional<TEXTRENDER_RECT_ID>	TRR;

	void MsgBlank() {
		Line = 0;
		for(auto& msg : Msg) {
			msg = {};
		}
		Text.clear();
	}

} MSG_WINDOW;



///// [非公開関数] /////

static void CWinKeyEvent(WINDOW_SYSTEM *ws);				// キーボード入力を処理する

static void DrawWindowFrame(int x,int y,int w,int h);		// ウィンドウ枠を描画する
static void GrpBoxA2(int x1,int y1,int x2,int y2);			// 平行四辺形ＢＯＸ描画



///// [グローバル変数] /////

MSG_WINDOW		MsgWindow;		// メッセージウィンドウ



uint8_t WINDOW_MENU::MaxItems() const
{
	uint8_t ret = NumItems;
	for(auto i = 0; i < NumItems; i++) {
		if(ItemPtr[i]->Submenu) {
			ret = (std::max)(ret, ItemPtr[i]->Submenu->MaxItems());
		}
	}
	return ret;
}

void WINDOW_CHOICE::SetActive(bool active)
{
	EnumFlagSet(Flags, WINDOW_FLAGS::DISABLED, !active);
}

void WINDOW_SYSTEM::Init(PIXEL_COORD w)
{
	W = w;
	Parent.SetItems(true);

	// Don't forget the header.
	const auto max_items = (1 + Parent.MaxItems());

	for(auto i = 0; i < max_items; i++) {
		TRRs[i] = TextObj.Register({ W, CWIN_ITEM_H });
	}
}

void WINDOW_SYSTEM::Open(WINDOW_POINT topleft, int select)
{
	x = topleft.x;
	y = topleft.y;

	Count       = 0;
	Select[0]   = select;
	SelectDepth = 0;
	State       = CWIN_INIT;

	OldKey   = Key_Data;
	KeyCount = CWIN_KEYWAIT;

	FirstWait = true;
}

void WINDOW_SYSTEM::OpenCentered(PIXEL_COORD w, int select)
{
	// Shifting it down by 9 pixels avoids the clash with the background image
	// gradient.
	const WINDOW_POINT topleft = {
		(320 - (w / 2)),
		(73 + (CWIN_MAX_H / 2) - (((Parent.NumItems + 1) * CWIN_ITEM_H) / 2))
	};
	return Open(topleft, select);
}

// コマンドウィンドウを１フレーム動作させる //
void CWinMove(WINDOW_SYSTEM *ws)
{
	switch(ws->State){
		case(CWIN_DEAD):	// 使用されていない
		return;

		case(CWIN_FREE):	// 入力待ち状態
			CWinKeyEvent(ws);
			ws->Count = 0;
		return;

		case(CWIN_OPEN):	// 項目移動中(進む)
		break;

		case(CWIN_CLOSE):	// 項目移動中(戻る)
		break;

		case(CWIN_NEXT):	// 次のウィンドウに移行中
		break;

		case(CWIN_BEFORE):	// 前のウィンドウに移行中
		break;

		case(CWIN_INIT):	// 初期化処理中
			ws->State = CWIN_FREE;
		break;
	}

	ws->Count++;
}

void CWinDrawLabel(
	TEXTRENDER_SESSION& s, const WINDOW_LABEL& label, bool is_title
)
{
	struct COLOR_PAIR {
		RGB shadow;
		RGB text;
	};

	static constexpr COLOR_PAIR COL[2][2] = {
		COLOR_PAIR{ { 128, 128, 128 }, { 255, 255, 255 } }, // Active, regular
		COLOR_PAIR{ { 128, 128, 128 }, { 255, 255,  70 } }, // Active, HL
		COLOR_PAIR{ {  96,  96,  96 }, { 192, 192, 192 } }, // Disabled, regular
		COLOR_PAIR{ {  96,  96,  96 }, { 192, 192,  70 } }, // Disabled, HL
	};

	const auto disabled = !!(label.Flags & WINDOW_FLAGS::DISABLED);
	const auto highlight = !!(label.Flags & WINDOW_FLAGS::HIGHLIGHT);
	const auto& col = COL[disabled][highlight];
	s.SetFont(CWIN_FONT);

	// Adding CWIN_ITEM_LEFT to centered text would throw it off-center,
	// obviously. Also, non-centered titles that don't start with spaces
	// shouldn't be dedented relative to the menu items.
	const auto starts_with_space = (label.Title.ptr && (*label.Title.ptr == ' '));
	const auto left = (!!(label.Flags & WINDOW_FLAGS::CENTER)
		? TextLayoutXCenter(s, label.Title)
		: ((is_title && starts_with_space) ? 0 : CWIN_ITEM_LEFT)
	);
	s.Put({ (left + 1), 0 }, label.Title, col.shadow);
	s.Put({ (left + 0), 0 }, label.Title, col.text);
}

// コマンドウィンドウの描画 //
void CWinDraw(WINDOW_SYSTEM *ws)
{
	int				i;
	WINDOW_COORD	top = ws->y;

	// アクティブな項目を検索する //
	auto* p = CWinSearchActive(ws);

	// 半透明ＢＯＸの描画 //
	const uint8_t alpha = (GrpGeom_FB() ? (64 + 32) : 128);

	GrpGeom->Lock();
	GrpGeom->SetAlphaNorm(alpha);

	GrpGeom->SetColor({ 0, 0, 0 });
	GrpGeom->DrawBoxA(ws->x, top, (ws->x + ws->W), (top + CWIN_ITEM_H));
	top += CWIN_ITEM_H;

	GrpGeom->SetColor({ 0, 0, 2 });
	for(i=0;i<p->NumItems;i++){
		if(i==ws->Select[ws->SelectDepth]){
			GrpGeom->SetAlphaNorm(128);
			GrpGeom->SetColor({ 5, 0, 0 });
		}
		GrpGeom->DrawBoxA(ws->x, top, (ws->x + ws->W), (top + CWIN_ITEM_H));
		top += CWIN_ITEM_H;
		if(i==ws->Select[ws->SelectDepth]){
			GrpGeom->SetAlphaNorm(alpha);
			GrpGeom->SetColor({ 0, 0, 2 });
		}
	}
	GrpGeom->Unlock();

	// 文字列の描画 //
	WINDOW_POINT topleft = { ws->x, ws->y };
	const auto trr = ws->TRRs[0];
	const Narrow::string_view str = p->Title->Title;
	TextObj.Render(topleft, trr, str, [=](TEXTRENDER_SESSION& s) {
		CWinDrawLabel(s, *p->Title, true);
	});
	topleft.y += (CWIN_ITEM_H + 1); // ???

	for(i = 0; i < p->NumItems; i++) {
		const auto trr = ws->TRRs[1 + i];
		auto* item = p->ItemPtr[i];
		const Narrow::string_view c = ((item->Flags == item->FlagsPrev)
			? item->Title
			: ""
		);
		TextObj.Render(topleft, trr, c, [=](TEXTRENDER_SESSION& s) {
			CWinDrawLabel(s, *item, false);
		});
		item->FlagsPrev = item->Flags;
		topleft.y += CWIN_ITEM_H;
	}
}

// コマンド [Exit] のデフォルト処理関数 //
bool CWinExitFn(INPUT_BITS key)
{
	if(Input_IsOK(key) || Input_IsCancel(key)) {
		return false;
	}
	return true;
}

PIXEL_SIZE CWinTextExtent(Narrow::string_view str)
{
	return TextObj.TextExtent(FONT_ID::SMALL, str);
}

PIXEL_SIZE CWinItemExtent(Narrow::string_view str)
{
	auto ret = CWinTextExtent(str);
	ret.w += CWIN_ITEM_LEFT;
	ret.h = CWIN_ITEM_H;
	return ret;
}

void MWinInit(const WINDOW_LTRB& rc, MSG_WINDOW_FLAGS flags)
{
	MsgWindow.MaxSize = rc;
	MsgWindow.Flags = flags;
	MsgWindow.TextTopleft = {
		.x = (!!(flags & MSG_WINDOW_FLAGS::WITH_FACE) ? FACE_W : 8),
		.y = 8,
	};
	MsgWindow.TRR = TextObj.Register(rc.Size() - MsgWindow.TextTopleft);
}

void MWinOpen(void)
{
	if(MsgWindow.State != MWIN_DEAD) return;

	// 状態および、最終値のセット //
	MsgWindow.State     = MWIN_OPEN;
	MsgWindow.FaceState = MFACE_NONE;			// 何も表示しない
	MsgWindow.FaceID    = 0;
	MsgWindow.FaceTime  = 0;

	// 矩形の初期値をセットする //
	const auto y_mid = ((MsgWindow.MaxSize.bottom + MsgWindow.MaxSize.top) / 2);
	MsgWindow.NowSize.left   = MsgWindow.MaxSize.left;
	MsgWindow.NowSize.right  = MsgWindow.MaxSize.right;
	MsgWindow.NowSize.top    = y_mid - 4;
	MsgWindow.NowSize.bottom = y_mid + 4;

	MWinCmd(MWCMD_NORMALFONT);					// ノーマルフォント

	// ウィンドウ内に表示するものの初期化 //
	MsgWindow.MsgBlank();
}

// メッセージウィンドウをクローズする //
void MWinClose(void)
{
//	if(MsgWindow.State != MWIN_FREE) return;

	MsgWindow.FaceState = MFACE_CLOSE;
	MsgWindow.State     = MWIN_CLOSE;
}

// メッセージウィンドウを強制クローズする //
void MWinForceClose(void)
{
	MsgWindow.FaceState = MFACE_NONE;
	MsgWindow.State     = MWIN_DEAD;
}

// メッセージウィンドウを動作させる //
void MWinMove(void)
{
	switch(MsgWindow.FaceState){
		case(MFACE_OPEN):		// 顔を表示しようとしている
			MsgWindow.FaceTime+=16;
			if(MsgWindow.FaceTime==0) MsgWindow.FaceState = MFACE_WAIT;
		break;

		case(MFACE_NEXT):
			MsgWindow.FaceTime+=16;
			if(MsgWindow.FaceTime==0){
				MsgWindow.FaceState = MFACE_OPEN;
				MsgWindow.FaceID    = MsgWindow.NextFace;
				GrpBackend_PaletteSet(
					FaceData[MsgWindow.FaceID / FACE_NUMX].pal
				);
			}
		break;

		case(MFACE_CLOSE):		// 顔を消そうとしている
			MsgWindow.FaceTime+=16;
			if(MsgWindow.FaceTime==0) MsgWindow.FaceState = MFACE_NONE;
		break;

		default:
		break;
	}

	switch(MsgWindow.State){
		case(MWIN_OPEN):
			MsgWindow.NowSize.top    -= 2;
			MsgWindow.NowSize.bottom += 2;

			// 完全にオープンできた場合 //
			if(MsgWindow.NowSize.top <= MsgWindow.MaxSize.top){
				MsgWindow.NowSize = MsgWindow.MaxSize;
				MsgWindow.State   = MWIN_FREE;
			}
		break;

		case(MWIN_CLOSE):
			MsgWindow.NowSize.top    += 3;
			MsgWindow.NowSize.bottom -= 3;
			MsgWindow.NowSize.right  += 6;
			MsgWindow.NowSize.left   -= 6;

			// 完全にクローズできた場合 //
			if(MsgWindow.NowSize.top >= MsgWindow.NowSize.bottom){
				MsgWindow.State   = MWIN_DEAD;
			}
		break;

		case(MWIN_DEAD):	case(MWIN_FREE):
		break;
	}
}

// メッセージウィンドウを描画する(上に同じ) //
void MWinDraw(void)
{
	PIXEL_LTRB	src;

	const auto x = MsgWindow.NowSize.left;	// ウィンドウ左上Ｘ
	const auto y = MsgWindow.NowSize.top;	// ウィンドウ左上Ｙ
	const auto w = (MsgWindow.NowSize.right - x); 	// ウィンドウ幅
	const auto h = (MsgWindow.NowSize.bottom - y);	// ウィンドウ高さ
	int		len,time,oy;

	// メッセージウィンドウが死んでいたら何もしない //
	if(MsgWindow.State == MWIN_DEAD) return;

	// 半透明部の描画 //
	GrpGeom->Lock();
	GrpGeom->SetAlphaNorm(GrpGeom_FB() ? (64 + 32) : 110);
	GrpGeom->SetColor({ 0, 0, 3 });
	GrpGeom->DrawBoxA((x + 4), (y + 4), (x + w - 4), (y + h - 4));
	GrpGeom->Unlock();

	// 文字列を表示するのはウィンドウが[FREE]である場合だけ        //
	// -> こうしないと文字列用 Surface を作成することになるので... //
	if((MsgWindow.State == MWIN_FREE) && MsgWindow.TRR) {
		const auto topleft = (WINDOW_POINT{ x, y } + MsgWindow.TextTopleft);
		const auto trr = MsgWindow.TRR.value();
		const auto& text = MsgWindow.Text;
		TextObj.Render(topleft, trr, text, [](TEXTRENDER_SESSION& s) {
			// セットされたフォントで描画
			s.SetFont(MsgWindow.FontID);
			for(auto i = 0; i < MsgWindow.Line; i++) {
				const auto line = MsgWindow.Msg[i];

				// 一応安全対策
				if(line.empty()) {
					continue;
				}
				const PIXEL_COORD top = (i * MsgWindow.FontDy);
				const auto left = (
					!!(MsgWindow.Flags & MSG_WINDOW_FLAGS::CENTER)
						? TextLayoutXCenter(s, line)
						: 0
				);

				// 灰色で１どっとずらして描画
				s.Put({ (left + 1), top }, line, RGB{ 128, 128, 128 });
				// 白で表示すべき位置に表示
				s.Put({ (left + 0), top }, line, RGB{ 255, 255, 255 });
			}
		});
	}

	DrawWindowFrame(x,y,w,h);

	// お顔をかきましょう(表示を要請されている場合にだけ) //
	const auto sid = (SURFACE_ID::FACE + (MsgWindow.FaceID / FACE_NUMX));
	switch(MsgWindow.FaceState){
		case(MFACE_WAIT):
			oy = MsgWindow.MaxSize.bottom - 100;
			src = PIXEL_LTWH{
				((MsgWindow.FaceID % FACE_NUMX) * FACE_W), 0, FACE_W, FACE_H
			};
			GrpSurface_Blit({ (x + 2), oy }, sid, src);
		break;

		case(MFACE_OPEN):
			time = MsgWindow.FaceTime>>2;
			oy = MsgWindow.MaxSize.bottom - 100;
			for(auto i = 0; i < FACE_H; i++){
				len = cosl(time+i*153,(64-time)/2);
				//len = cosl(time+i*4,64-time);
				src = PIXEL_LTWH{
					((MsgWindow.FaceID % FACE_NUMX) * FACE_W), i, FACE_W, 1
				};
				GrpSurface_Blit({ (x + len + 2), (oy + i) }, sid, src);
				//if(i & 1)	GrpSurface_Blit({ (x + len + 2), (oy + i) }, sid, src);
				//else     	GrpSurface_Blit({ (x - len + 2), (oy + i) }, sid, src);
			}
		break;

		case(MFACE_NEXT):
			time = (255-MsgWindow.FaceTime)>>2;
			oy = MsgWindow.MaxSize.bottom - 100;
			for(auto i = 0; i < FACE_H; i++){
				len = cosl(time+i*153,(64-time)/2);
				//len = cosl(time+i*4,64-time);
				src = PIXEL_LTWH{
					((MsgWindow.FaceID % FACE_NUMX) * FACE_W), i, FACE_W, 1
				};
				GrpSurface_Blit({ (x + len + 2), (oy + i) }, sid, src);
				//if(i & 1)	GrpSurface_Blit({ (x + len + 2), (oy + i) }, sid, src);
				//else     	GrpSurface_Blit({ (x - len + 2), (oy + i) }, sid, src);
			}
		break;

		case(MFACE_CLOSE):
			time = MsgWindow.FaceTime>>1;
			oy = MsgWindow.MaxSize.bottom - 100;
			for(auto i = 0; i < FACE_H; i++){
				len = cosl(time+i*4,time);
				src = PIXEL_LTWH{
					((MsgWindow.FaceID % FACE_NUMX) * FACE_W), i, FACE_W, 1
				};
				if(i & 1) {
					GrpSurface_Blit({ (x - len + 2), (oy + i) }, sid, src);
				} else {
					GrpSurface_Blit({ (x + len + 2), (oy + i) }, sid, src);
				}
			}
		break;
	}
}

void MWinMsg(Narrow::string_view s)
{
	int			Line,i;

	Line = MsgWindow.Line;

	if(Line==MsgWindow.MaxLine){
		// すでに表示最大行数を超えていた場合 //
		for(i=1;i<MsgWindow.MaxLine-1;i++) MsgWindow.Msg[i] = MsgWindow.Msg[i+1];
		MsgWindow.Msg[Line-1] = s;
	} else {
		// ポインタセット＆行数更新 //
		MsgWindow.Msg[Line] = s;
		MsgWindow.Line = Line+1;
	}

	MsgWindow.Text.clear();
	for(decltype(MsgWindow.Line) i = 0; i < MsgWindow.Line; i++) {
		MsgWindow.Text += MsgWindow.Msg[Line];
		MsgWindow.Text += '\n';
	}
}

// 顔をセットする //
void MWinFace(uint8_t faceID)
{
	if(MsgWindow.State==MWIN_DEAD) return;		// 表示不可
	if(faceID/FACE_NUMX>=FACE_MAX) return;		// あり得ない数字

	assert(MsgWindow.TextTopleft.x == FACE_W);

	if(MsgWindow.FaceState==MFACE_NONE){
		MsgWindow.FaceState = MFACE_OPEN;
		MsgWindow.FaceID = faceID;
		GrpBackend_PaletteSet(FaceData[faceID / FACE_NUMX].pal);
	}
	else{
		MsgWindow.FaceState = MFACE_NEXT;
		MsgWindow.NextFace  = faceID;
	}

	MsgWindow.FaceTime  = 0;
}

// コマンドを送る //
void MWinCmd(uint8_t cmd)
{
	int temp;
	int		Ysize = 0;

	switch(cmd){
		case(MWCMD_LARGEFONT):		// ラージフォントを使用する
			Ysize +=  8; [[fallthrough]];
		case(MWCMD_NORMALFONT):		// ノーマルフォントを使用する
			Ysize +=  2; [[fallthrough]];
		case(MWCMD_SMALLFONT):		// スモールフォントを使用する
			Ysize += 14;
			temp = MsgWindow.MaxSize.bottom - MsgWindow.MaxSize.top - 16;
			MsgWindow.MaxLine = temp / Ysize;							// 表示可能最大行数
			MsgWindow.FontDy  =(temp % Ysize)/(temp/Ysize)+Ysize + 1;	// Ｙ増量
			MsgWindow.FontID  = Cast::down_enum<FONT_ID>(cmd);	// 使用フォント
			[[fallthrough]];

		case(MWCMD_NEWPAGE):		// 改ページする
			// 文字列無効化, 最初の行へ
			MsgWindow.MsgBlank();
		break;

		default:		// ここに来たらバグね...
		break;
	}
}

// ウィンドウ枠を描画する //
static void DrawWindowFrame(int x,int y,int w,int h)
{
	PIXEL_LTRB	src;

	w = w >> 1;
	h = h >> 1;

	// 左上 //
	src = { 0, 0, w, h };
	GrpSurface_Blit({ x, y }, SURFACE_ID::SYSTEM, src);

	// 右上 //
	src = { (384 - w), 0, 384, h };
	GrpSurface_Blit({ (x + w), y }, SURFACE_ID::SYSTEM, src);

	// 左下 //
	src = { 0, (80 - h), w, 80 };
	GrpSurface_Blit({ x, (y + h) }, SURFACE_ID::SYSTEM, src);

	// 右下 //
	src = { (384 - w), (80 - h), 384, 80 };
	GrpSurface_Blit({ (x + w), (y + h) }, SURFACE_ID::SYSTEM, src);
}

// ヘルプ文字列を送る //
void MWinHelp(WINDOW_SYSTEM *ws)
{
	// アクティブなウィンドウを検索し、メッセージ領域をクリアする //
	const auto *p = CWinSearchActive(ws);
	MsgWindow.MsgBlank();

	// 一列だけ文字列を割り当てる //
	MWinMsg(p->ItemPtr[ws->Select[ws->SelectDepth]]->Help);
}

// アクティブなウィンドウを探す //
WINDOW_MENU *CWinSearchActive(WINDOW_SYSTEM *ws)
{
	int				i;

	// 現在アクティブな項目を探す //
	auto *p = &(ws->Parent);
	for(i=0;i<ws->SelectDepth;i++){
		p = p->ItemPtr[ws->Select[i]]->Submenu;
	}

	return p;
}

// キーボード入力を処理する //
static void CWinKeyEvent(WINDOW_SYSTEM *ws)
{
	if(ws->FirstWait){
		if(Key_Data) {
			return;
		} else {
			ws->FirstWait = false;
		}
	}

	// 操作性が悪かった点を修正 //
	if(Key_Data==0 && ws->KeyCount){
		ws->OldKey   = 0;
		ws->KeyCount = 0;
		return;
	}

	// アクティブなウィンドウを検索する //
	const auto* p = CWinSearchActive(ws);
	auto Depth = ws->SelectDepth;

	// In case the item just disabled itself...
	while(!!(p->ItemPtr[ws->Select[Depth]]->Flags & WINDOW_FLAGS::DISABLED)) {
		ws->Select[Depth] = ((ws->Select[Depth] + 1) % p->NumItems);
	}

	// アクティブな項目をセットする //
	auto *p2 = p->ItemPtr[ws->Select[Depth]];

	// キーボードの過剰なリピート防止 //
	if(ws->KeyCount) {
		ws->KeyCount--;
		if(ws->KeyCount == 0) {
			ws->OldKey = 0;
		}
		return;
	} else if(
		!!(p2->Flags & WINDOW_FLAGS::FAST_REPEAT) &&
		Input_OptionKeyDelta(ws->OldKey)
	) {
		ws->KeyCount = ws->FastRepeatWait;
		ws->FastRepeatWait = (std::max)((ws->FastRepeatWait - 2), 0);
		if(ws->KeyCount == 0) {
			ws->OldKey = 0;
		}
		return;
	} else if(
		(ws->OldKey == KEY_UP) || (ws->OldKey == KEY_DOWN) ||
		(ws->OldKey == KEY_LEFT) || (ws->OldKey == KEY_RIGHT)
	) {
		ws->KeyCount = CWIN_KEYWAIT;
		return;
	} else if(Input_IsOK(ws->OldKey) || Input_IsCancel(ws->OldKey)) {
		// いかなる場合もリピートを許可しないキー //
		if(Key_Data == ws->OldKey) {
			return;
		}
	} else {
		ws->KeyCount = 0;
	}

	ws->OldKey = Key_Data;

	const auto next_active = [](
		const WINDOW_MENU& menu, auto cur, int_fast8_t direction
	) {
		do {
			cur = ((cur + menu.NumItems + direction) % menu.NumItems);
		} while(!!(menu.ItemPtr[cur]->Flags & WINDOW_FLAGS::DISABLED));
		return cur;
	};

	const auto move_to_previous_level = [](WINDOW_SYSTEM& ws) {
		if(ws.SelectDepth == 0) {
			return;
		}
		ws.SelectDepth--;
		if(const auto *new_p = CWinSearchActive(&ws)) {
			new_p->SetItems(false);
		}
	};

	// 一部のキーボード入力を処理する(KEY_UP/KEY_DOWN) //
	if((Key_Data == KEY_UP) || (Key_Data == KEY_DOWN)) {
		// 一つ上の項目へ / 一つ下の項目へ
		const auto delta = ((Key_Data == KEY_UP) ? -1 : +1);
		ws->Select[Depth] = next_active(*p, ws->Select[Depth], delta);
		Snd_SEPlay(SOUND_ID_SELECT);
	} else if(Input_IsCancel(Key_Data)) {
		Snd_SEPlay(SOUND_ID_CANCEL);
	} else if(Input_OptionKeyDelta(Key_Data)) {
		Snd_SEPlay(SOUND_ID_SELECT);
	} else if(Key_Data == 0) {
		ws->FastRepeatWait = CWIN_KEYWAIT;
	}

	if(p2->OptionFn || p2->CallBackFn) {
		// コールバック動作時の処理 //
		const auto ret = ([p, p2] {
			if(p2->OptionFn) {
				if(Input_IsCancel(Key_Data)) {
					return false;
				} else if(const auto delta = Input_OptionKeyDelta(Key_Data)) {
					p2->OptionFn(delta);
				}
				p->SetItems(true);
				return true;
			}

			return p2->CallBackFn(Key_Data);
		})();
		if(ret == false) {
			if(Depth==0){
				if(!Input_IsCancel(Key_Data)) {
					// 後で (CWIN_CLOSE) に変更すること//
					ws->State  = CWIN_DEAD;
					ws->OldKey = 0;			// ここは結構重要
				}
			}
			else{
				move_to_previous_level(*ws);
			}
		}
	} else {
		// Hotkeys might be pressed while the cursor is on an item with a menu,
		// where we wouldn't fall into the branch above...
		p->SetItems(false);

		// デフォルトのキーボード動作 //
		if(Input_IsOK(Key_Data)) {
			// 決定・選択
			if(p2->Submenu && (p2->Submenu->NumItems != 0)) {
				p2->Submenu->Title = p2;
				p2->Submenu->SetItems(false);

				// Jump to the first active item to avoid potentially drawing
				// the selection cursor on top of a disabled item on the next
				// frame.
				ws->Select[Depth + 1] = next_active(*p2->Submenu, -1, +1);
				ws->SelectDepth++;
			}
		} else if(Input_IsCancel(Key_Data)) {
			// キャンセル
			move_to_previous_level(*ws);
		}
	}
}

// 平行四辺形ＢＯＸ描画 //
static void GrpBoxA2(int x1,int y1,int x2,int y2)
{
	int i;

	for(i=0;i<=y2-y1;i++)
		GrpGeom->DrawBoxA(x1, (y1 + i), (x2 + (i * 2)), (y1 + i + 1));
}
