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

/* $Header:   F:\projects\c&c\vcs\code\gscreen.cpv   2.17   16 Oct 1995 16:51:34   JOE_BOSTIC  $ */
/***********************************************************************************************
 ***             C O N F I D E N T I A L  ---  W E S T W O O D   S T U D I O S               ***
 ***********************************************************************************************
 *                                                                                             *
 *                 Project Name : Command & Conquer                                            *
 *                                                                                             *
 *                    File Name : GSCREEN.CPP                                                  *
 *                                                                                             *
 *                   Programmer : Joe L. Bostic                                                *
 *                                                                                             *
 *                   Start Date : 12/15/94                                                     *
 *                                                                                             *
 *                  Last Update : January 19, 1995 [JLB]                                       *
 *                                                                                             *
 *---------------------------------------------------------------------------------------------*
 * Functions:                                                                                  *
 *   GScreenClass::GScreenClass -- Default constructor for GScreenClass.                       *
 *   GScreenClass::One_Time -- Handles one time class setups.                                  *
 *   GScreenClass::Init -- Init's the entire display hierarchy by calling all Init routines.   *
 *   GScreenClass::Init_Clear -- Sets the map to a known state.                                *
 *   GScreenClass::Init_Theater -- Performs theater-specific initializations.                  *
 *   GScreenClass::Init_IO -- Initializes the Button list ('Buttons').                         *
 *   GScreenClass::Flag_To_Redraw -- Flags the display to be redrawn.                          *
 *   GScreenClass::Blit_Display -- Redraw the display from the hidpage to the seenpage.        *
 *   GScreenClass::Render -- General drawing dispatcher an display update function.            *
 *   GScreenClass::Input -- Fetches input and processes gadgets.                               *
 *   GScreenClass::Add_A_Button -- Add a gadget to the game input system.                      *
 *   GScreenClass::Remove_A_Button -- Removes a gadget from the game input system.             *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#include "function.h"

#include "common/filepcx.h"

#ifdef AMIGA
#include <chrono>
#include "mssleep.h"
#include "SDL.h"


#define CUSTOM_REGBASE          0xdff000
#define CUSTOM_DMACON           (CUSTOM_REGBASE + 0x096)
#define CUSTOM_DMACON2          (CUSTOM_DMACON  + 0x200)
unsigned short* DMACON2_KI = (unsigned short*)CUSTOM_DMACON2;
unsigned short* DMACONR1_KI = (unsigned short*)0xDFF002;  //<=- this should be DMA_CONR2    
unsigned short* DMACONR2_KI = (unsigned short*)0xDFF202;  //<=- this should be DMA_CONR2    

#endif

GadgetClass* GScreenClass::Buttons = 0;

GraphicBufferClass* GScreenClass::ShadowPage = 0;

/***********************************************************************************************
 * GScreenClass::GScreenClass -- Default constructor for GScreenClass.                         *
 *                                                                                             *
 *    This constructor merely sets the display system, so that it will redraw the first time   *
 *    the render function is called.                                                           *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
GScreenClass::GScreenClass(void)
{
    IsToUpdate = true;
    IsToRedraw = true;
}

/***********************************************************************************************
 * GScreenClass::One_Time -- Handles one time class setups.                                    *
 *                                                                                             *
 * This routine (and all those that overload it) must perform truly one-time initialization.   *
 * Such init's would normally be done in the constructor, but other aspects of the game may    *
 * not have been initialized at the time the constructors are called (such as the file system, *
 * the display, or other WWLIB subsystems), so many initializations should be deferred to the  *
 * One_Time init's.                                                                            *
 *                                                                                             *
 * Any variables set in this routine should be declared as static, so they won't be modified   *
 * by the load/save process.  Non-static variables will be over-written by a loaded game.      *
 *                                                                                             *
 * This function allocates the shadow buffer that is used for quick screen updates. If         *
 * there were any data files to load, they would be loaded at this time as well.               *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   Call this routine only ONCE at the beginning of the game.                       *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void GScreenClass::One_Time(void)
{
    /*
    **	Allocate the screen shadow page. This page is used to reduce access to the
    **	actual screen memory. It contains a duplicate of what the SEENPAGE is.
    */
    Buttons = 0;
    ShadowPage = new GraphicBufferClass(320, 200);
    if (ShadowPage) {
        ShadowPage->Clear();
        HiddenPage.Clear();
    }
}

/***********************************************************************************************
 * GScreenClass::Init -- Init's the entire display hierarchy by calling all Init routines.     *
 *                                                                                             *
 * This routine shouldn't be overloaded.  It's the main map initialization routine, and will   *
 * perform a complete map initialization, from mixfiles to clearing the buffers.  Calling this *
 * routine results in calling every initialization routine in the entire map hierarchy.        *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      theater      theater to initialize to                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/28/1994 BR : Created.                                                                  *
 *=============================================================================================*/
void GScreenClass::Init(TheaterType theater)
{
    Init_Clear();
    Init_IO();
    Init_Theater(theater);
}

/***********************************************************************************************
 * GScreenClass::Init_Clear -- Sets the map to a known state.                                  *
 *                                                                                             *
 * This routine (and those that overload it) clears any buffers and variables to a known       *
 * state.  It assumes that all buffers are allocated & valid.  The map should be displayable   *
 * after calling this function, and should draw basically an empty display.                    *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/28/1994 BR : Created.                                                                  *
 *=============================================================================================*/
void GScreenClass::Init_Clear(void)
{
    /*
    ** Clear the ShadowPage & HidPage to force a complete shadow blit.
    */
    if (ShadowPage) {
        ShadowPage->Clear();
        HiddenPage.Clear();
    }

    IsToRedraw = true;
}

/***********************************************************************************************
 * GScreenClass::Init_Theater -- Performs theater-specific initializations.                    *
 *                                                                                             *
 * This routine (and those that overload it) performs any theater-specific initializations     *
 * needed.  This will include setting the palette, setting up remap tables, etc.  This routine *
 * only needs to be called when the theater has changed.                                       *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/28/1994 BR : Created.                                                                  *
 *=============================================================================================*/
void GScreenClass::Init_Theater(TheaterType)
{
}

/***********************************************************************************************
 * GScreenClass::Init_IO -- Initializes the Button list ('Buttons').                           *
 *                                                                                             *
 * INPUT:                                                                                      *
 *      none.                                                                                  *
 *                                                                                             *
 * OUTPUT:                                                                                     *
 *      none.                                                                                  *
 *                                                                                             *
 * WARNINGS:                                                                                   *
 *      none.                                                                                  *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/28/1994 BR : Created.                                                                  *
 *=============================================================================================*/
void GScreenClass::Init_IO(void)
{
    /*
    ** Reset the button list.  This means that any other elements of the map that need
    ** buttons must attach them after this routine is called!
    */
    Buttons = 0;
}

/***********************************************************************************************
 * GScreenClass::Flag_To_Redraw -- Flags the display to be redrawn.                            *
 *                                                                                             *
 *    This function is used to flag the display system whether any rendering is needed. The    *
 *    parameter tells the system either to redraw EVERYTHING, or just that something somewhere *
 *    has changed and the individual Draw_It functions must be called. When a sub system       *
 *    determines that it needs to render something local to itself, it would call this routine *
 *    with a false parameter. If the entire screen gets trashed or needs to be rebuilt, then   *
 *    this routine will be called with a true parameter.                                       *
 *                                                                                             *
 * INPUT:   complete -- bool; Should the ENTIRE screen be redrawn?                             *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   This doesn't actually draw the screen, it merely sets flags so that when the    *
 *             Render() function is called, the appropriate drawing steps will be performed.   *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void GScreenClass::Flag_To_Redraw(bool complete)
{
    IsToUpdate = true;
    if (complete) {
        IsToRedraw = true;
    }
}

/***********************************************************************************************
 * GScreenClass::Input -- Fetches input and processes gadgets.                                 *
 *                                                                                             *
 *    This routine will fetch the keyboard/mouse input and dispatch this through the gadget    *
 *    system.                                                                                  *
 *                                                                                             *
 * INPUT:   key      -- Reference to the key code (for future examination).                    *
 *                                                                                             *
 *          x,y      -- Reference to mouse coordinates (for future examination).               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/19/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void GScreenClass::Input(KeyNumType& key, int& x, int& y)
{
    key = Keyboard->Check();

    x = Get_Mouse_X();
    y = Get_Mouse_Y();

    if (Buttons) {

        /*
        ** If any buttons need redrawing, they will do so in the Input routine, and
        ** they should draw themselves to the HidPage.  So, flag ourselves for a Blit
        ** to show the newly drawn buttons.
        */
        if (Buttons->Is_List_To_Redraw()) {
            Flag_To_Redraw(false);
        }

        GraphicViewPortClass* oldpage = Set_Logic_Page(HidPage);

        key = Buttons->Input();

        Set_Logic_Page(oldpage);

    } else {
        if (key) {
            key = Keyboard->Get();
        }
    }
    AI(key, x, y);
}

/***********************************************************************************************
 * GScreenClass::Add_A_Button -- Add a gadget to the game input system.                        *
 *                                                                                             *
 *    This will add a gadget to the game input system. The gadget will be processed in         *
 *    subsiquent calls to the GScreenClass::Input() function.                                  *
 *                                                                                             *
 * INPUT:   gadget   -- Reference to the gadget that will be added to the input system.        *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/19/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void GScreenClass::Add_A_Button(GadgetClass& gadget)
{
    /*------------------------------------------------------------------------
    If this gadget is already in the list, remove it before adding it in:
    - If 1st gadget in list, use Remove_A_Button to remove it, to reset the
      value of 'Buttons' appropriately
    - Otherwise, just call the Remove function for that gadget to remove it
      from any list it may be in
    ------------------------------------------------------------------------*/
    if (Buttons == &gadget) {
        Remove_A_Button(gadget);
    } else {
        gadget.Remove();
    }

    /*------------------------------------------------------------------------
    Now add the gadget to our list:
    - If there are not buttons, start the list with this one
    - Otherwise, add it to the tail of the existing list
    ------------------------------------------------------------------------*/
    if (Buttons) {
        gadget.Add_Tail(*Buttons);
    } else {
        Buttons = &gadget;
    }
}

/***********************************************************************************************
 * GScreenClass::Remove_A_Button -- Removes a gadget from the game input system.               *
 *                                                                                             *
 * INPUT:   gadget   -- Reference to the gadget that will be removed from the input system.    *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   'gadget' MUST be already a part of 'Buttons', or the new value of 'Buttons'     *
 *               will be invalid!                                                              *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   01/19/1995 JLB : Created.                                                                 *
 *=============================================================================================*/
void GScreenClass::Remove_A_Button(GadgetClass& gadget)
{
    Buttons = gadget.Remove();
}

/***********************************************************************************************
 * GScreenClass::Render -- General drawing dispatcher an display update function.              *
 *                                                                                             *
 *    This routine should be called in the main game loop (once every game frame). It will     *
 *    call the Draw_It() function if necessary. All rendering is performed to the LogicPage    *
 *    which is set to the HIDPAGE. After rendering has been performed, the HIDPAGE is          *
 *    copied to the visible page.                                                              *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   This actually updates the graphic display. As a result it can take quite a      *
 *             while to perform.                                                               *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   12/15/1994 JLB : Created.                                                                 *
 *=============================================================================================*/
void GScreenClass::Render(void)
{
    // if (Buttons && Buttons->Is_List_To_Redraw()) {
    //	IsToRedraw = true;
    //}

    if (IsToUpdate || IsToRedraw) {

        // WWMouse->Erase_Mouse(&HidPage, TRUE);
        GraphicViewPortClass* oldpage = Set_Logic_Page(HidPage);

        // if (IsToRedraw) {
        //	Hide_Mouse();
        //	SeenBuff.To_Buffer(0, 0, 320, 200, ShadowPage);
        //	Show_Mouse();
        //}
        Draw_It(IsToRedraw);

        if (Buttons)
            Buttons->Draw_All(false);

#ifdef SCENARIO_EDITOR
        /*
        ** Draw the Editor's buttons
        */
        if (Debug_Map) {
            if (Buttons) {
                Buttons->Draw_All();
            }
        }
#endif
        /*
        ** Draw the multiplayer message system to the Hidpage at this point.
        ** This way, they'll Blit along with the rest of the map.
        */
        if (Messages.Num_Messages() > 0) {
            Messages.Set_Width(Lepton_To_Cell(Map.TacLeptonWidth) * ICON_PIXEL_W);
        }
        Messages.Draw();

#ifndef REMASTER_BUILD
        Blit_Display();
#endif
        IsToUpdate = false;
        IsToRedraw = false;

        Set_Logic_Page(oldpage);
    }
}

#ifdef CHEAT_KEYS

#define MAX_SCREENS_SAVED 30 * 15 // Enough for 30 seconds @ 15 fps

GraphicBufferClass* ScreenList[MAX_SCREENS_SAVED];
int CurrentScreen = 0;
bool ScreenRecording = false;

void Add_Current_Screen(void)
{
#if (0) // ST - 1/2/2019 5:51PM
    if (ScreenRecording) {
        ScreenList[CurrentScreen] = new GraphicBufferClass;
        ScreenList[CurrentScreen]->Init(SeenBuff.Get_Width(), SeenBuff.Get_Height(), NULL, 0, (GBC_Enum)0);
        SeenBuff.Blit(*ScreenList[CurrentScreen]);

        CurrentScreen++;

        if (CurrentScreen == MAX_SCREENS_SAVED) {

            char filename[20];
            for (int i = 0; i < MAX_SCREENS_SAVED; i++) {
                sprintf(filename, "SCRN%04d.PCX", i);
                Write_PCX_File(filename, *ScreenList[i], (unsigned char*)CurrentPalette);
                delete ScreenList[i];
            }

            CurrentScreen = 0;
            ScreenRecording = 0;
        }
    }
#endif
}

#endif // CHEAT_KEYS

extern bool CanVblankSync;

/***********************************************************************************************
 * GScreenClass::Blit_Display -- Redraw the display from the hidpage to the seenpage.          *
 *                                                                                             *
 *    This routine is used to copy the correct display from the HIDPAGE                        *
 *    to the SEENPAGE.                                                                         *
 *                                                                                             *
 * INPUT:   none                                                                               *
 *                                                                                             *
 * OUTPUT:  none                                                                               *
 *                                                                                             *
 * WARNINGS:   none                                                                            *
 *                                                                                             *
 * HISTORY:                                                                                    *
 *   02/14/1994 JLB : Created.                                                                 *
 *   05/01/1994 JLB : Converted to member function.                                            *
 *=============================================================================================*/
float fps;
bool ShowFPS = false;

#ifdef AMIGA
    static char KI_fps[200]="<>";
    static char KI_fps2[200] = "<>";
    //static float fps;
    static int frame_counter = 0;
#endif

void GScreenClass::Blit_Display(void)
{
#ifdef AMIGA
    static unsigned int time_frame_start1, time_frame_end, render_time;

    if(frame_counter==0)
        time_frame_start1 = SDL_GetTicks();
    if (frame_counter == 60)
    {
        time_frame_end = SDL_GetTicks();

        render_time = time_frame_end - time_frame_start1;

        fps = 1000.0 / ((float)render_time / 60.0);
        frame_counter = 0;
        //sprintf(KI_fps, "%ld %.2f", render_time, fps);

        if(fps<10)
            sprintf(KI_fps, "00%05.1f  \n ",fps);
        else if (fps<100)
            sprintf(KI_fps, "0%05.1f   \n ", fps);
        else
            sprintf(KI_fps, "%05.1f    \n ", fps);
    }
    else
        frame_counter++;

    //HidPage.Print(KI_fps, 110, 3, YELLOW, BLACK);
    if (ShowFPS)
    	Simple_Text_Print(KI_fps, 110, 2, YELLOW, BLACK, TPF_8POINT);
    //else
     	//Simple_Text_Print(KI_fps, 110, 2, BLACK, BLACK, TPF_8POINT);   

#if 0
    // show DMACON
    uint16_t the_CON = *DMACONR2_KI;

    int dma_audio_vals[4];

    dma_audio_vals[0] = the_CON & 0x1;
    dma_audio_vals[1] = the_CON & 0x2;
    dma_audio_vals[2] = the_CON & 0x4;
    dma_audio_vals[3] = the_CON & 0x8;

    //sprintf(KI_fps2, "DMA2: %d%d%d%d \n ", dma_audio_vals[0], dma_audio_vals[1]/2, dma_audio_vals[2]/4, dma_audio_vals[3]/8);
    //Simple_Text_Print(KI_fps2, 110, 30, YELLOW, BLACK, TPF_8POINT);

    the_CON = *DMACONR1_KI;

    dma_audio_vals[0] = the_CON & 0x1;
    dma_audio_vals[1] = the_CON & 0x2;
    dma_audio_vals[2] = the_CON & 0x4;
    dma_audio_vals[3] = the_CON & 0x8;

    //sprintf(KI_fps2, "DMA1: %d%d%d%d \n ", dma_audio_vals[0], dma_audio_vals[1]/2, dma_audio_vals[2]/4, dma_audio_vals[3]/8);
    //Simple_Text_Print(KI_fps2, 110, 20, YELLOW, BLACK, TPF_8POINT);
#endif
#endif


#if (0)
    if (HidPage.Get_IsDirectDraw() && (Options.GameSpeed > 1 || Options.ScrollRate == 6 && CanVblankSync)) {
        WWMouse->Draw_Mouse(&HidPage);
        SeenBuff.Get_Graphic_Buffer()->Get_DD_Surface()->Flip(NULL, DDFLIP_WAIT);
        SeenBuff.Blit(HidPage, 0, 0, 0, 0, SeenBuff.Get_Width(), SeenBuff.Get_Height(), false);
#ifdef CHEAT_KEYS
        Add_Current_Screen();
#endif
        // HidPage.Blit ( SeenBuff , 0 , 0 , 0 , 0 , HidPage.Get_Width() , HidPage.Get_Height() , (BOOL) FALSE );
        WWMouse->Erase_Mouse(&HidPage, false);
    } else {
#else //(0)
    WWMouse->Draw_Mouse(&HidPage);
    HidPage.Blit(SeenBuff, 0, 0, 0, 0, HidPage.Get_Width(), HidPage.Get_Height(), false);
#ifdef CHEAT_KEYS
    Add_Current_Screen();
#endif
    WWMouse->Erase_Mouse(&HidPage, false);
#endif //(0)
#if (0)
    }
#endif //(0)
}
