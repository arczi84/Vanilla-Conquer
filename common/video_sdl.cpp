//
// Copyright 2020 Electronic Arts Inc.
//
// TiberianDawn.DLL and RedAlert.dll and corresponding source code is free
// software: you can redistribute it and/or modify it under the terms of
// the GNU General Public License as published by the Free Software Foundation,
// either version 3 of the License, or (at your option) any later version.

// TiberianDawn.DLL and RedAlert.dll and corresponding source code is distributed
// in the hope that it will be useful, but with permitted additional restrictions
// under Section 7 of the GPL. See the GNU General Public License in LICENSE.TXT
// distributed with this program. You should have received a copy of the
// GNU General Public License along with permitted additional restrictions
// with this program. If not, see https://github.com/electronicarts/CnC_Remastered_Collection

/***************************************************************************
 **   C O N F I D E N T I A L --- W E S T W O O D   A S S O C I A T E S   **
 ***************************************************************************
 *                                                                         *
 *                 Project Name : Westwood Win32 Library                   *
 *                                                                         *
 *                    File Name : DDRAW.CPP                                *
 *                                                                         *
 *                   Programmer : Philip W. Gorrow                         *
 *                                                                         *
 *                   Start Date : October 10, 1995                         *
 *                                                                         *
 *                  Last Update : October 10, 1995   []                    *
 *                                                                         *
 *-------------------------------------------------------------------------*
 * Functions:                                                              *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*=========================================================================*/
/* The following PRIVATE functions are in this file:                       */
/*=========================================================================*/

/*= = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = = =*/

#include "gbuffer.h"
#include "palette.h"
#include "video.h"
#include "wwkeyboard.h"
#include "wwmouse.h"
#include "settings.h"
#include "debugstring.h"

#include <SDL.h>

#ifndef SDL2_BUILD

#include "video_sdl1.h"


#ifdef AMIGA
extern "C" 
{
extern unsigned char Apollo_AMMXon;
extern WWKeyboardClass* Keyboard;

#include <exec/types.h>
#include <exec/memory.h>
#include <dos/dos.h>
#include <hardware/custom.h>
#include <hardware/intbits.h>
#include <clib/exec_protos.h>
#include <proto/graphics.h>

#define GAME_SCREEN_WIDTH         (640)
#define GAME_SCREEN_HEIGHT        (400)
#define GAME_memsize              (GAME_SCREEN_WIDTH * GAME_SCREEN_HEIGHT * 2) + 128
#define GAME_SCREEN_DEPTH         (16)

#define SAGA_VIDEO_PLANEPTR       (0xDFF1EC)

static int curr_buffer = 0;

ULONG *x_MemAddr1;
ULONG *x_MemAddr2;
ULONG *x_MemAddr3;
ULONG *x_MemAddr0;

ULONG *x_FBAddr1;
ULONG *x_FBAddr2;
ULONG *x_FBAddr3;
ULONG *x_FBAddr0;

ULONG *x_Pixels  = NULL;

}
#else
unsigned char Apollo_AMMXon = 0;
#endif

extern bool enable_triple;
static SDL_Surface *logo;
extern SDL_Surface *back;
extern  float fps;

void InitBuffers(void)
{
#ifdef AMIGA
	// Aligned Drawing Buffers 1,2,3 */
	x_MemAddr1 = (ULONG*)AllocMem(GAME_memsize, MEMF_LOCAL | MEMF_FAST | MEMF_CLEAR);
	x_FBAddr1 = (ULONG*)((((ULONG)x_MemAddr1) + 31)& ~31);

	x_MemAddr2 = (ULONG*)AllocMem(GAME_memsize, MEMF_LOCAL | MEMF_FAST | MEMF_CLEAR);
	x_FBAddr2 = (ULONG*)((((ULONG)x_MemAddr2) + 31)& ~31);

	x_MemAddr3 = (ULONG*)AllocMem(GAME_memsize, MEMF_LOCAL | MEMF_FAST | MEMF_CLEAR);
	x_FBAddr3 = (ULONG*)((((ULONG)x_MemAddr3) + 31)& ~31);

    x_MemAddr0 = (ULONG*)AllocMem(GAME_memsize, MEMF_LOCAL | MEMF_FAST | MEMF_CLEAR);
	x_FBAddr0 = (ULONG*)((((ULONG)x_MemAddr0) + 31)& ~31);
#endif
} 

void FreeBuffers(void)
{
#ifdef AMIGA
    FreeMem(x_MemAddr1, GAME_memsize);
    FreeMem(x_MemAddr2, GAME_memsize);
    FreeMem(x_MemAddr3, GAME_memsize);
    FreeMem(x_MemAddr0, GAME_memsize);
#endif
}

static SDL_Surface* window;
static SDL_Surface* windowSurface;
#else
static SDL_Window* window;
static SDL_Renderer* renderer;
#endif
static SDL_Palette* palette;
static Uint32 pixel_format;
static SDL_Rect render_dst;

static struct
{
    int GameW;
    int GameH;
    bool Clip;
    float ScaleX{1.0f};
    float ScaleY{1.0f};
    void* Raw;
    int W;
    int H;
    int HotX;
    int HotY;
    float X;
    float Y;
    SDL_Cursor* Pending;
    SDL_Cursor* Current;
    SDL_Surface* Surface;
} hwcursor;

#define ARRAY_SIZE(x) int(sizeof(x) / sizeof(x[0]))
#ifdef SDL2_BUILD
#define MAKEFORMAT(f)                                                                                                  \
    {                                                                                                                  \
        SDL_PIXELFORMAT_##f, #f                                                                                        \
    }
Uint32 SettingsPixelFormat()
{
    /*
    ** Known good RGB formats for both the surface and texture.
    */
    static struct
    {
        Uint32 format;
        std::string name;
    } formats[] = {
        MAKEFORMAT(ARGB8888),
        MAKEFORMAT(RGBA8888),
        MAKEFORMAT(ABGR8888),
        MAKEFORMAT(BGRA8888),
        MAKEFORMAT(RGB24),
        MAKEFORMAT(BGR24),
        MAKEFORMAT(RGB888),
        MAKEFORMAT(BGR888),
        MAKEFORMAT(RGB555),
        MAKEFORMAT(BGR555),
        MAKEFORMAT(RGB565),
        MAKEFORMAT(BGR565),
    };

    std::string str = Settings.Video.PixelFormat;

    for (auto& c : str) {
        c = toupper(c);
    }

    for (int i = 0; i < ARRAY_SIZE(formats); i++) {
        if (str.compare(formats[i].name) == 0) {
            return formats[i].format;
        }
    }

    return SDL_PIXELFORMAT_UNKNOWN;
}
#endif
static void Update_HWCursor();

static void Update_HWCursor_Settings()
{
#ifdef AMIGA
    return;
#endif
    /*
    ** Update mouse scaling settings.
    */
    int win_w, win_h;
#ifdef SDL2_BUILD
    SDL_GetRendererOutputSize(renderer, &win_w, &win_h);
#else
    win_w = Settings.Video.Width;
    win_h = Settings.Video.Height;
#endif
    hwcursor.ScaleX = win_w / (float)hwcursor.GameW;
    hwcursor.ScaleY = win_h / (float)hwcursor.GameH;

    /*
    ** Update screen boxing settings.
    */
    float ar = (float)hwcursor.GameW / hwcursor.GameH;
    if (Settings.Video.Boxing) {
        render_dst.w = win_w;
        render_dst.h = render_dst.w / ar;
        if (render_dst.h > win_h) {
            render_dst.h = win_h;
            render_dst.w = render_dst.h * ar;
        }
        render_dst.x = (win_w - render_dst.w) / 2;
        render_dst.y = (win_h - render_dst.h) / 2;
    } else {
        render_dst.w = win_w;
        render_dst.h = win_h;
        render_dst.x = 0;
        render_dst.y = 0;
    }

    /*
    ** Ensure cursor clip is in the desired state.
    */
    Set_Video_Cursor_Clip(hwcursor.Clip);

    /*
    ** Update visible cursor scaling.
    */
    Update_HWCursor();
}

class SurfaceMonitorClassDummy : public SurfaceMonitorClass
{

public:
    SurfaceMonitorClassDummy()
    {
    }

    virtual void Restore_Surfaces()
    {
    }

    virtual void Set_Surface_Focus(bool in_focus)
    {
    }

    virtual void Release()
    {
    }
};

SurfaceMonitorClassDummy AllSurfacesDummy;           // List of all direct draw surfaces
SurfaceMonitorClass& AllSurfaces = AllSurfacesDummy; // List of all direct draw surfaces

/***********************************************************************************************
 * Set_Video_Mode -- Initializes Direct Draw and sets the required Video Mode                  *
 *                                                                                             *
 * INPUT:           int width           - the width of the video mode in pixels                *
 *                  int height          - the height of the video mode in pixels               *
 *                  int bits_per_pixel  - the number of bits per pixel the video mode supports *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/26/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <winstub.h>

#ifdef AMIGA

extern bool logo_end;
extern unsigned char* BlackPalette;
extern void * threadFunc(void * arg);

void * threadFunc(void * arg)
{
	pthread_detach(pthread_self());
    char *file[] = {"screen1-8bit.bmp","screen2-8bit.bmp","screen3-8bit.bmp"};
    SDL_Surface* black = SDL_CreateRGBSurface(0, 640, 400, 2, 0, 0, 0, 0);
    SDL_Flip(SDL_GetVideoSurface());
    for (int i = 0; i < 3 ; i++)
    {
        if (logo_end)
           break;
        logo = SDL_LoadBMP( file[i] );

        SDL_BlitSurface( logo, NULL, window, NULL );
        SDL_Flip(window);
       
        if ((!logo_end) && (i < 2))
            sleep(8);
        else {
            while (!logo_end)
                sleep(1);
            }
    }

    if (Apollo_AMMXon) {
        if (logo) {
            SDL_BlitSurface( black, NULL, window, NULL );
            SDL_Flip(window);
            *(volatile ULONG*)SAGA_VIDEO_PLANEPTR = (ULONG) black->pixels;
        }
        //*(volatile unsigned short int*)0xDFF1F4 = (unsigned short int)0x0401;
    }
   
    if (logo) {
        SDL_FreeSurface(logo);
        logo = nullptr;
    }
    if (black) {
        SDL_FreeSurface(black);
        black = nullptr;
    }
    pthread_exit(NULL);

    return NULL;
}
#endif
bool Set_Video_Mode(int w, int h, int bits_per_pixel)
{
    SDL_Init(SDL_INIT_VIDEO/*|SDL_INIT_AUDIO*/);

    if (Apollo_AMMXon) {
        InitBuffers();
    }

    SDL_ShowCursor(SDL_DISABLE);

    int win_w = w;
    int win_h = h;
    int win_flags = 0;
#ifdef SDL2_BUILD
    Uint32 requested_pixel_format = SettingsPixelFormat();
#else

//#ifdef AMIGA
	bits_per_pixel = 16;
//#endif
    win_flags = SDL_HWSURFACE;


#endif
    if (!Settings.Video.Windowed) {
        /*
        ** Native fullscreen if no proper width and height set.
        */
        if (Settings.Video.Width < w || Settings.Video.Height < h) {
            win_w = Settings.Video.Width = 0;
            win_h = Settings.Video.Height = 0;
            win_flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
        } else {
            win_w = Settings.Video.Width;
            win_h = Settings.Video.Height;
            win_flags |= SDL_WINDOW_FULLSCREEN;
        }
    } else if (Settings.Video.WindowWidth > w || Settings.Video.WindowHeight > h) {
        win_w = Settings.Video.WindowWidth;
        win_h = Settings.Video.WindowHeight;
    } else {
        Settings.Video.WindowWidth = win_w;
        Settings.Video.WindowHeight = win_h;
    }

#ifndef SDL2_BUILD
    const SDL_VideoInfo* video_info = SDL_GetVideoInfo();
    window = SDL_SetVideoMode(win_w, win_h, bits_per_pixel, win_flags);
#ifdef AMIGA
    // Thread id
    pthread_t threadId;
    // Create a thread that will function threadFunc()
    int err = pthread_create(&threadId, NULL, &threadFunc, NULL);
    // Check if thread is created sucessfuly
    if (err)
    {
        std::cout << "Thread creation failed : " << strerror(err);
    }
#endif
    SDL_WM_SetCaption("Vanilla Conquer", NULL);
#else
    window =
        SDL_CreateWindow("Vanilla Conquer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, win_w, win_h, win_flags);
    if (window == nullptr) {
        DBG_ERROR("SDL_CreateWindow failed: %s", SDL_GetError());
        Reset_Video_Mode();
        return false;
    }

    DBG_INFO("Created SDL2 %s window in %dx%d", (win_flags ? "fullscreen" : "windowed"), win_w, win_h);

    pixel_format = SDL_GetWindowPixelFormat(window);
    if (pixel_format == SDL_PIXELFORMAT_UNKNOWN || SDL_BITSPERPIXEL(pixel_format) < 16) {
        DBG_ERROR("SDL2 window pixel format unsupported: %s (%d bpp)",
                  SDL_GetPixelFormatName(pixel_format),
                  SDL_BITSPERPIXEL(pixel_format));
        Reset_Video_Mode();
        return false;
    }

    DBG_INFO("  pixel format: %s (%d bpp)", SDL_GetPixelFormatName(pixel_format), SDL_BITSPERPIXEL(pixel_format));

    DBG_INFO("SDL2 drivers available: (user preference '%s')", Settings.Video.Driver.c_str());
    int renderer_index = -1;
    for (int i = 0; i < SDL_GetNumRenderDrivers(); i++) {
        SDL_RendererInfo info;
        if (SDL_GetRenderDriverInfo(i, &info) == 0) {
            if (Settings.Video.Driver.compare(info.name) == 0) {
                renderer_index = i;
            }

            DBG_INFO(" %s%s", info.name, (i == renderer_index ? " (selected)" : ""));
        }
    }

    renderer = SDL_CreateRenderer(window, renderer_index, SDL_RENDERER_TARGETTEXTURE);
    if (renderer == nullptr) {
        DBG_ERROR("SDL_CreateRenderer failed: %s", SDL_GetError());
        Reset_Video_Mode();
        return false;
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer, &info) != 0) {
        DBG_ERROR("SDL_GetRendererInfo failed: %s", SDL_GetError());
        Reset_Video_Mode();
        return false;
    }

    DBG_INFO("Initialized SDL2 driver '%s'", info.name);
    DBG_INFO("  flags:");
    if (info.flags & SDL_RENDERER_SOFTWARE) {
        DBG_INFO("    SDL_RENDERER_SOFTWARE");
    }
    if (info.flags & SDL_RENDERER_ACCELERATED) {
        DBG_INFO("    SDL_RENDERER_ACCELERATED");
    }
    if (info.flags & SDL_RENDERER_PRESENTVSYNC) {
        DBG_INFO("    SDL_RENDERER_PRESENT_VSYNC");
    }
    if (info.flags & SDL_RENDERER_TARGETTEXTURE) {
        DBG_INFO("    SDL_RENDERER_TARGETTEXTURE");
    }

    DBG_INFO("  max texture size: %dx%d", info.max_texture_width, info.max_texture_height);

    DBG_INFO("  %d texture formats supported: (user preference '%s')",
             info.num_texture_formats,
             SDL_GetPixelFormatName(requested_pixel_format));

    /*
    ** Pick the first pixel format or the user requested one. It better be RGB.
    */
    pixel_format = SDL_PIXELFORMAT_UNKNOWN;
    for (int i = 0; i < info.num_texture_formats; i++) {
        if ((pixel_format == SDL_PIXELFORMAT_UNKNOWN && i == 0) || info.texture_formats[i] == requested_pixel_format) {
            pixel_format = info.texture_formats[i];
        }
    }

    for (int i = 0; i < info.num_texture_formats; i++) {
        DBG_INFO("    %s%s",
                 SDL_GetPixelFormatName(info.texture_formats[i]),
                 (pixel_format == info.texture_formats[i] ? " (selected)" : ""));
    }

    /*
    ** Set requested scaling algorithm.
    */
    if (!SDL_SetHintWithPriority(SDL_HINT_RENDER_SCALE_QUALITY, Settings.Video.Scaler.c_str(), SDL_HINT_OVERRIDE)) {
        DBG_WARN("  scaler '%s' is unsupported");
    } else {
        DBG_INFO("  scaler set to '%s'", Settings.Video.Scaler.c_str());
    }
#endif
    if (palette == nullptr) {
        palette = SDL_AllocPalette(256);
    }

    /*
    ** Set mouse scaling options.
    */
    hwcursor.GameW = w;
    hwcursor.GameH = h;
    hwcursor.X = w / 2;
    hwcursor.Y = h / 2;
    Update_HWCursor_Settings();

    return true;
}

void Toggle_Video_Fullscreen()
{
    Settings.Video.Windowed = !Settings.Video.Windowed;
#ifdef SDL2_BUILD
    if (!Settings.Video.Windowed) {
        if (Settings.Video.Width == 0 || Settings.Video.Height == 0) {
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        } else {
            SDL_SetWindowSize(window, Settings.Video.Width, Settings.Video.Height);
            SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
        }
    } else {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, Settings.Video.WindowWidth, Settings.Video.WindowHeight);
    }
#endif
    Update_HWCursor_Settings();
}

void Get_Video_Scale(float& x, float& y)
{
    x = hwcursor.ScaleX;
    y = hwcursor.ScaleY;
}

void Set_Video_Cursor_Clip(bool clipped)
{
    hwcursor.Clip = clipped;

    if (window) {
        int relative;
#ifdef SDL2_BUILD
        if (Settings.Video.Windowed) {
            SDL_SetWindowGrab(window, hwcursor.Clip ? SDL_TRUE : SDL_FALSE);
            relative = SDL_SetRelativeMouseMode(Settings.Mouse.RawInput && hwcursor.Clip ? SDL_TRUE : SDL_FALSE);

            /*
            ** When grabbing with raw input, move in-game cursor where the real cursor was and vice versa.
            */
            if (Settings.Mouse.RawInput) {
                if (hwcursor.Clip) {
                    int x, y;
                    SDL_GetMouseState(&x, &y);
                    hwcursor.X = x / hwcursor.ScaleX;
                    hwcursor.Y = y / hwcursor.ScaleY;
                } else {
                    SDL_WarpMouseInWindow(window, hwcursor.X * hwcursor.ScaleX, hwcursor.Y * hwcursor.ScaleY);
                }
            }
        } else {
            SDL_SetWindowGrab(window, SDL_TRUE);
            relative = SDL_SetRelativeMouseMode(Settings.Mouse.RawInput ? SDL_TRUE : SDL_FALSE);
        }
#else
        relative = -1;
#endif
        if (relative < 0) {
            //DBG_ERROR("Raw input not supported, disabling.");
            Settings.Mouse.RawInput = false;
        }
    }
}

void Move_Video_Mouse(float xrel, float yrel)
{
    if (hwcursor.Clip || !Settings.Video.Windowed) {
        hwcursor.X += xrel * (Settings.Mouse.Sensitivity / 100.0f);
        hwcursor.Y += yrel * (Settings.Mouse.Sensitivity / 100.0f);
    }

    if (hwcursor.X >= hwcursor.GameW) {
        hwcursor.X = hwcursor.GameW - 1;
    } else if (hwcursor.X < 0) {
        hwcursor.X = 0;
    }

    if (hwcursor.Y >= hwcursor.GameH) {
        hwcursor.Y = hwcursor.GameH - 1;
    } else if (hwcursor.Y < 0) {
        hwcursor.Y = 0;
    }
}

void Get_Video_Mouse(int& x, int& y)
{
    if (Settings.Mouse.RawInput && (hwcursor.Clip || !Settings.Video.Windowed)) {
        x = hwcursor.X;
        y = hwcursor.Y;
    } else {
        float scale_x, scale_y;
        Get_Video_Scale(scale_x, scale_y);
        SDL_GetMouseState(&x, &y);
        x /= scale_x;
        y /= scale_y;
    }
}

/***********************************************************************************************
 * Reset_Video_Mode -- Resets video mode and deletes Direct Draw Object                        *
 *                                                                                             *
 * INPUT:		none                                                                            *
 *                                                                                             *
 * OUTPUT:     none                                                                            *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   09/26/1995 PWG : Created.                                                                 *
 *=============================================================================================*/
void Reset_Video_Mode(void)
{
    if (hwcursor.Pending) {
        SDL_FreeCursor(hwcursor.Pending);
        hwcursor.Pending = nullptr;
    }

    if (hwcursor.Current) {
        SDL_FreeCursor(hwcursor.Current);
        hwcursor.Current = nullptr;
    }

    if (hwcursor.Surface) {
        SDL_FreeSurface(hwcursor.Surface);
        hwcursor.Surface = nullptr;
    }
#ifdef SDL2_BUILD
    SDL_DestroyRenderer(renderer);
    renderer = nullptr;

    SDL_DestroyWindow(window);
    window = nullptr;
#else
    if (window) {
        SDL_FreeSurface(window);
        window = nullptr;
    }
#endif
    if (palette) {
        SDL_FreePalette(palette);
        palette = nullptr;
    }

    FreeBuffers();
}

static void Update_HWCursor()
{
#ifdef AMIGA
    return;
#endif
    float scale_x = 1.0f;
    float scale_y = 1.0f;
    int scaled_w = hwcursor.W;
    int scaled_h = hwcursor.H;

    /*
    ** Pre-scale cursor *only* if we are not emulating a hw cursor.
    */
    if (Settings.Video.HardwareCursor) {
        scale_x = hwcursor.ScaleX;
        scale_y = hwcursor.ScaleY;
        scaled_w *= scale_x;
        scaled_h *= scale_y;
    }

    /*
    ** Allocate or reallocate surface if it has the wrong size.
    */
    if (hwcursor.Surface == nullptr || hwcursor.Surface->w != scaled_w || hwcursor.Surface->h != scaled_h) {
        if (hwcursor.Surface) {
            SDL_FreeSurface(hwcursor.Surface);
        }

        /*
        ** Real HW cursor needs to be scaled up. Emulated can use original cursor data.
        */
#ifdef SDL2_BUILD
        if (Settings.Video.HardwareCursor) {
            hwcursor.Surface = SDL_CreateRGBSurfaceWithFormat(0, scaled_w, scaled_h, 8, SDL_PIXELFORMAT_INDEX8);
        } else
#endif
        {
            hwcursor.Surface =
                SDL_CreateRGBSurfaceFrom(hwcursor.Raw, hwcursor.W, hwcursor.H, 16, hwcursor.W, 0, 0, 0, 0);
        }
#ifdef SDL2_BUILD
        SDL_SetSurfacePalette(hwcursor.Surface, palette);
        SDL_SetColorKey(hwcursor.Surface, SDL_TRUE, 0);
#else
        SDL_SetColorKey(hwcursor.Surface, SDL_SRCCOLORKEY, 0);
#endif
    }

    /*
    ** Prepare HW cursor by scaling up and creating the SDL version.
    */
    if (Settings.Video.HardwareCursor) {
        uint8_t* src = (uint8_t*)hwcursor.Raw;
        uint8_t* dst = (uint8_t*)hwcursor.Surface->pixels;
        int src_pitch = hwcursor.W;
        int dst_pitch = hwcursor.Surface->pitch;

        for (int y = 0; y < scaled_h; y++) {
            for (int x = 0; x < scaled_w; x++) {
                dst[dst_pitch * y + x] = src[src_pitch * (int)(y / scale_y) + (int)(x / scale_x)];
            }
        }

        if (hwcursor.Pending) {
            SDL_FreeCursor(hwcursor.Pending);
        }

        /*
        ** Queue new cursor to be set during frame flip.
        */
#ifdef SDL2_BUILD
        hwcursor.Pending =
            SDL_CreateColorCursor(hwcursor.Surface, hwcursor.HotX * hwcursor.ScaleX, hwcursor.HotY * hwcursor.ScaleY);
#endif
    }
}

void Set_Video_Cursor(void* cursor, int w, int h, int hotx, int hoty)
{
    hwcursor.Raw = cursor;
    hwcursor.W = w;
    hwcursor.H = h;
    hwcursor.HotX = hotx;
    hwcursor.HotY = hoty;

    Update_HWCursor();
}

/***********************************************************************************************
 * Get_Free_Video_Memory -- returns amount of free video memory                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   bytes of available video RAM                                                      *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    11/29/95 12:52PM ST : Created                                                            *
 *=============================================================================================*/
unsigned int Get_Free_Video_Memory(void)
{
    return 1000000000;
}

/***********************************************************************************************
 * Get_Video_Hardware_Caps -- returns bitmask of direct draw video hardware support            *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   hardware flags                                                                    *
 *                                                                                             *
 * WARNINGS: Must call Set_Video_Mode 1st to create the direct draw object                     *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    1/12/96 9:14AM ST : Created                                                              *
 *=============================================================================================*/
unsigned Get_Video_Hardware_Capabilities(void)
{
    return VIDEO_BLITTER;
}

/***********************************************************************************************
 * Wait_Vert_Blank -- Waits for the start (leading edge) of a vertical blank                   *
 *                                                                                             *
 * INPUT:                                                                                      *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *=============================================================================================*/
void Wait_Vert_Blank(void)
{
}

/***********************************************************************************************
 * Set_Palette -- set a direct draw palette                                                    *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    ptr to 768 rgb palette bytes                                                      *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    10/11/95 3:33PM ST : Created                                                             *
 *=============================================================================================*/
void Set_DD_Palette(void* rpalette)
{
    SDL_Color colors[256];

    unsigned char* rcolors = (unsigned char*)rpalette;
    for (int i = 0; i < 256; i++) {
        colors[i].r = (unsigned char)rcolors[i * 3] << 2;
        colors[i].g = (unsigned char)rcolors[i * 3 + 1] << 2;
        colors[i].b = (unsigned char)rcolors[i * 3 + 2] << 2;
#ifdef SDL2_BUILD
        colors[i].a = 0xFF;
#endif
    }

    /*
    ** First color is transparent. This needs to be set so that hardware cursor has transparent
    ** surroundings when converting from 8-bit to 32-bit.
    */
#ifdef SDL2_BUILD
    colors[0].a = 0;
#endif
    if (!Apollo_AMMXon)	
        SDL_SetPaletteColors(palette, colors, 0, 256);

    /*
    ** Cursor needs to be updated when palette changes.
    */
    Update_HWCursor();
    if (Apollo_AMMXon)	
	{
	    unsigned long col_data;
	    for (int i = 0; i < 256; i++)
	    {
	        col_data = (i << 24) + (((unsigned char)rcolors[i * 3]) << 18) + (((unsigned char)rcolors[i * 3+1]) <<10) + (((unsigned char)rcolors[i * 3+2])<<2);
	        *(volatile unsigned long*)0xDFF388 = (unsigned long)col_data;
	    }
	}
}

/***********************************************************************************************
 * Wait_Blit -- waits for the DirectDraw blitter to become idle                                *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   07-25-95 03:53pm ST : Created                                                             *
 *=============================================================================================*/

void Wait_Blit(void)
{
}

/***********************************************************************************************
 * SMC::SurfaceMonitorClass -- constructor for surface monitor class                           *
 *                                                                                             *
 *                                                                                             *
 *                                                                                             *
 * INPUT:    Nothing                                                                           *
 *                                                                                             *
 * OUTPUT:   Nothing                                                                           *
 *                                                                                             *
 * WARNINGS: None                                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *    11/3/95 3:23PM ST : Created                                                              *
 *=============================================================================================*/

SurfaceMonitorClass::SurfaceMonitorClass()
{
    SurfacesRestored = false;
}

/*
** VideoSurfaceDDraw
*/
class VideoSurfaceSDL;
static VideoSurfaceSDL* frontSurface = nullptr;

class VideoSurfaceSDL : public VideoSurface
{
public:
    VideoSurfaceSDL(int w, int h, GBC_Enum flags)
        : flags(flags)
        , windowSurface(nullptr)
#ifdef SDL2_BUILD
        , texture(nullptr)
#endif
    {
        surface = SDL_CreateRGBSurface(0, w, h, 8, 0, 0, 0, 0);
        if (!Apollo_AMMXon)
            SDL_SetSurfacePalette(surface, palette);

        if (flags & GBC_VISIBLE) {
#ifdef SDL2_BUILD
            windowSurface = SDL_CreateRGBSurfaceWithFormat(0, w, h, SDL_BITSPERPIXEL(pixel_format), pixel_format);
            texture = SDL_CreateTexture(renderer, windowSurface->format->format, SDL_TEXTUREACCESS_STREAMING, w, h);
#else
            windowSurface = SDL_CreateRGBSurface(0, w, h, 16, 0, 0, 0, 0);
#endif
            frontSurface = this;
        }
    }

    virtual ~VideoSurfaceSDL()
    {
        if (frontSurface == this) {
            frontSurface = nullptr;
        }
        if (surface) {
            SDL_FreeSurface(surface);
        }
#ifdef SDL2_BUILD
        if (texture) {
            SDL_DestroyTexture(texture);
        }
#endif
        if (windowSurface) {
            SDL_FreeSurface(windowSurface);
        }
    }

    virtual void* GetData() const
    {
        return surface->pixels;
    }
    virtual long GetPitch() const
    {
        return surface->pitch;
    }
    virtual bool IsAllocated() const
    {
        return false;
    }

    virtual void AddAttachedSurface(VideoSurface* surface)
    {
    }

    virtual bool IsReadyToBlit()
    {
        return true;
    }

    virtual bool LockWait()
    {
		//printf("LockWait()\n");
        return true;//(SDL_LockSurface(surface) == 0);
    }

    virtual bool Unlock()
    {
        //SDL_UnlockSurface(surface);
        return true;
    }

    virtual void Blt(const Rect& destRect, VideoSurface* src, const Rect& srcRect, bool mask)
    {
		printf("Trying to blit\n");
        //SDL_BlitSurface(((VideoSurfaceSDL*)src)->surface, (SDL_Rect*)(&srcRect), surface, (SDL_Rect*)&destRect);
    }

    virtual void FillRect(const Rect& rect, unsigned char color)
    {
		printf("Trying to fill rect\n");
        //SDL_FillRect(surface, (SDL_Rect*)(&rect), color);
    }

    void RenderSurface()
    {
        void* pixels;
        int pitch;
        unsigned long col_data = 0;


		if (!Apollo_AMMXon)
            SDL_BlitSurface(surface, NULL, windowSurface, NULL);
#ifdef SDL2_BUILD
        if (Settings.Video.HardwareCursor) {
            /*
            ** Swap cursor before a frame is drawn. This reduces flickering when it's done only once per frame.
            */
            if (hwcursor.Pending) {
                SDL_SetCursor(hwcursor.Pending);

                if (hwcursor.Current) {
                    SDL_FreeCursor(hwcursor.Current);
                }

                hwcursor.Current = hwcursor.Pending;
                hwcursor.Pending = nullptr;
            }

            /*
            ** Update hardware cursor visibility.
            */
            SDL_ShowCursor(!Get_Mouse_State());
        } else if (!Get_Mouse_State() && hwcursor.Surface != nullptr) {
            /*
            ** Draw software emulated cursor.
            */
            int x, y;
            SDL_Rect dst;

            Get_Video_Mouse(x, y);

            dst.x = x - hwcursor.HotX;
            dst.y = y - hwcursor.HotY;
            dst.w = hwcursor.Surface->w;
            dst.h = hwcursor.Surface->h;

            SDL_BlitSurface(hwcursor.Surface, nullptr, windowSurface, &dst);
        }
//#ifdef SDL2_BUILD
        SDL_UpdateTexture(texture, NULL, windowSurface->pixels, windowSurface->pitch);
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, &render_dst);
        SDL_RenderPresent(renderer);
#else
        SDL_ShowCursor(!Get_Mouse_State());

        if (!Apollo_AMMXon)
		{
			SDL_BlitSurface(surface, NULL, window, NULL);
            SDL_Flip(window);
		}
#ifdef AMIGA		
        else  
        {
#if 1            
           if (enable_triple == true)
           {
            switch (curr_buffer)
                {
                case 0:
                    *(volatile ULONG*)SAGA_VIDEO_PLANEPTR = (ULONG) x_FBAddr1;
                    surface->pixels = (ushort*)x_FBAddr2;
                        break;
                case 1:
                    *(volatile ULONG*)SAGA_VIDEO_PLANEPTR = (ULONG) x_FBAddr2;
                    surface->pixels = (ushort*)x_FBAddr3;
                        break;
                case 2:
                    *(volatile ULONG*)SAGA_VIDEO_PLANEPTR = (ULONG) x_FBAddr3;
                    surface->pixels = (ushort*)x_FBAddr1;
                        break;
                }

                curr_buffer++;
                if (curr_buffer > 2)
                    curr_buffer = 0;
            }        
            else
//#else
            {
                *(volatile ULONG*)SAGA_VIDEO_PLANEPTR = (ULONG) x_FBAddr0;//(ULONG) surface->pixels; 
                surface->pixels = (ushort*)x_FBAddr0;
            }
#endif
        }
#endif


#endif
    }
private:
    SDL_Surface* surface;
    SDL_Surface* windowSurface;
#ifdef SDL2_BUILD
    SDL_Texture* texture;
#endif
    GBC_Enum flags;
};


/* Enable FPS counter here */

void Video_Render_Frame()
{  
    if (frontSurface) {
        frontSurface->RenderSurface();
    }
}

/*
** Video
*/

Video::Video()
{
}

Video::~Video()
{
}

Video& Video::Shared()
{
    static Video video;
    return video;
}

VideoSurface* Video::CreateSurface(int w, int h, GBC_Enum flags)
{
    return new VideoSurfaceSDL(w, h, flags);
}
