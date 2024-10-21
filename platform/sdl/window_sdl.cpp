/*
 *   SDL window creation
 *
 */

#include "platform/window_backend.h"
#include "platform/sdl/log_sdl.h"
#include "platform/graphics_backend.h"
#include "platform/snd_backend.h"
#include "platform/input.h"
#include "constants.h"
#include "game/bgm.h"
#include "game/frame.h"
#include "game/graphics.h"
#include <SDL_events.h>
#include <SDL_hints.h>
#include <SDL_mouse.h>
#include <SDL_render.h>
#include <SDL_timer.h>
#include <SDL_video.h>

// We only need the Win32 functions for the vintage build, but why compile this
// translation unit twice just to add two small functions?
#ifdef WIN32
	#include <SDL_syswm.h>
#endif

constexpr auto LOG_CAT = SDL_LOG_CATEGORY_VIDEO;

static SDL_Window *Window;

std::u8string_view WndBackend_SDLRendererName(int8_t id)
{
	if(id < 0) {
		const auto ret = SDL_GetHint(SDL_HINT_RENDER_DRIVER);
		if(!ret) {
			return {};
		};
		return reinterpret_cast<const char8_t *>(ret);
	}
	SDL_RendererInfo info;
	if(SDL_GetRenderDriverInfo(id, &info) != 0) {
		return {};
	}
	return reinterpret_cast<const char8_t *>(info.name);
}

SDL_Window *WndBackend_SDLCreate(const GRAPHICS_PARAMS& params)
{
	assert(Window == nullptr);

	uint32_t flags = 0;

	// Set the necessary window flags for certain APIs to avoid
	// SDL_CreateRenderer()'s janky closing and reopening of the window
	// with the correct flags.
	const auto name = WndBackend_SDLRendererName(params.api);

	if(name.starts_with(u8"opengl")) {
		flags |= SDL_WINDOW_OPENGL;
		SDL_GL_ResetAttributes();

		// SDL_GL_ResetAttributes() also resets the essential profile mask and
		// version selection attributes, but chooses the target OpenGL version
		// via a hardcoded #ifdef priority list that prefers regular OpenGL
		// over ES 2 over ES 1. If the user requested any of the ES versions,
		// SDL_CreateRenderer() would still recreate the window because the
		// `SDL_GL_CONTEXT_PROFILE_MASK` got set to regular/non-ES OpenGL.
		// So, we're forced to specify the correct flags ourselves after all.
		const auto [maj, min] = ([name]() -> std::pair<int, int> {
			if(name.starts_with(u8"opengles")) {
				SDL_GL_SetAttribute(
					SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES
				);
				if(name == u8"opengles2") {
					return { 2, OPENGL_TARGET_ES2_MIN };
				}
				return { 1, OPENGL_TARGET_ES1_MIN };
			}
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, 0);
			return { OPENGL_TARGET_CORE_MAJ, OPENGL_TARGET_CORE_MIN };
		})();
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, maj);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, min);
	}

	constexpr auto res = GRP_RES;
	constexpr auto left = SDL_WINDOWPOS_CENTERED;
	constexpr auto top = SDL_WINDOWPOS_CENTERED;
	Window = SDL_CreateWindow(GAME_TITLE, left, top, res.w, res.h, flags);
	if(!Window) {
		Log_Fail(LOG_CAT, "Error creating SDL window");
		return nullptr;
	}
	SDL_ShowCursor(false);

	return Window;
}

#ifdef WIN32
HWND WndBackend_Win32(void)
{
	if(!Window) {
		return nullptr;
	}
	SDL_SysWMinfo wminfo;
	SDL_VERSION(&wminfo.version);
	if(!SDL_GetWindowWMInfo(Window, &wminfo)) {
		Log_Fail(LOG_CAT, "Error retrieving window handle");
		return nullptr;
	}
	return wminfo.info.win.window;
}

HWND WndBackend_Win32Create(const GRAPHICS_PARAMS& params)
{
	WndBackend_SDLCreate(params);
	return WndBackend_Win32();
}
#endif

void WndBackend_Cleanup(void)
{
	if(Window) {
		SDL_DestroyWindow(Window);
		Window = nullptr;
	}
}

int WndBackend_Run(void)
{
	bool quit = false;
	bool active = true;
	uint64_t ticks_last = 0;

	while(!quit) {
		// Read input events first to remove them from the queue
		SDL_PumpEvents();
		Key_Read();

		[[gsl::suppress(type.5)]] SDL_Event event;
		while(SDL_PeepEvents(
			&event, 1, SDL_GETEVENT, SDL_FIRSTEVENT, SDL_LASTEVENT
		) == 1) {
			switch(event.type) {
			case SDL_QUIT:
				return 0;

			case SDL_WINDOWEVENT: {
				switch(event.window.event) {
				case SDL_WINDOWEVENT_FOCUS_LOST:
					BGM_Pause();
					SndBackend_PauseAll();
					active = false;
					break;
				case SDL_WINDOWEVENT_FOCUS_GAINED:
					BGM_Resume();
					SndBackend_ResumeAll();
					active = true;
					break;
				default:
					break;
				}
				break;
			}

			default:
				break;
			}
		}

		if(active) {
			const auto ticks_start = SDL_GetTicks64();
			if(
				(Grp_FPSDivisor == 0) ||
				((ticks_start - ticks_last) >= FRAME_TIME_TARGET)
			) {
				quit = !GameFrame();
				if(Grp_FPSDivisor != 0) {
					// Since SDL_Delay() works at not-even-exact millisecond
					// granularity, we subtract 1 and spin for the last
					// millisecond to ensure that we hit the exact frame
					// boundary.
					const auto ticks_frame = (SDL_GetTicks64() - ticks_start);
					if(ticks_frame < (FRAME_TIME_TARGET - 1)) {
						SDL_Delay((FRAME_TIME_TARGET - 1) - ticks_frame);
					}
					ticks_last = ticks_start;
				}
			}
		} else {
			SDL_WaitEvent(nullptr);
		}
	}
	return 0;
}
