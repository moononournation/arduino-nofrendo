/* vim: set tabstop=3 expandtab:
**
** Nofrendo (c) 1998-2000 Matthew Conte (matt@conte.com)
**
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of version 2 of the GNU Library General 
** Public License as published by the Free Software Foundation.
**
** This program is distributed in the hope that it will be useful, 
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU 
** Library General Public License for more details.  To obtain a 
** copy of the GNU Library General Public License, write to the Free 
** Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
**
** Any permitted reproduction of these routines, in whole or in part,
** must bear this legend.
**
**
** sdl.c
**
** $Id: sdl.c,v 1.2 2001/04/27 14:37:11 neil Exp $
**
*/
#ifdef NES_SDL
#include <SDL/SDL.h>
#include <SDL/SDL_timer.h>
#include <SDL/SDL_video.h>
#include <math.h>
#include <string.h>
#include <stdbool.h>
#include <osd.h>
#include <noftypes.h>
#include <bitmap.h>
#include <nofconfig.h>
#include <event.h>
#include <gui.h>
#include <log.h>
#include <nes/nes.h>
#include <nes/nes_pal.h>
#include <nes/nesinput.h>
#include <osd.h>
#include <nofrendo.h>
#define  DEFAULT_SAMPLERATE   22050
#define  DEFAULT_BPS          8
#define  DEFAULT_FRAGSIZE     1024

#define  DEFAULT_WIDTH        256
#define  NES_VISIBLE_HEIGHT   NES_SCREEN_HEIGHT
#define  DEFAULT_HEIGHT       NES_VISIBLE_HEIGHT

/*
** Timer
*/

static void (*timer_callback)(void) = NULL;
static int tick_ideal = 0;
static int tick_interval = 0;

Uint32 mySDLTimer(Uint32 i)
{
   static int tickDiff = 0;
   Uint32 tickLast;

   tickLast = SDL_GetTicks();

   if (timer_callback)
      timer_callback();
   
   tickDiff += tick_interval - tick_ideal + tickLast - SDL_GetTicks();
   
   if (tickDiff >= 10)
   {
      tickDiff -= 10;
      return tick_interval - 10;
   }
   else
   {
      return tick_interval;
   }
}
/*
static int round(double value)
{
   int upper, lower;
   upper = (int) ceil(value);
   lower = (int) floor(value);

   if (upper - value > value - lower)
      return lower;
   else
      return upper;
}
*/
int osd_installtimer(int frequency, void *func, int funcsize, void *counter, int countersize)
{
   double ideal = 1000 / frequency;

   /* these arguments are used only for djgpp, which needs to lock code/data */
   UNUSED(counter);
   UNUSED(countersize);
   UNUSED(funcsize);

   tick_ideal = round(ideal);
   tick_interval = round(ideal / 10) * 10;

   SDL_SetTimer(tick_interval, mySDLTimer);

   timer_callback = func;

   return 0;
}
void *mem_alloc( int size, bool prefer_fast_memory){
   return malloc(size);
}
int main(int argc, char **argv){
  return nofrendo_main(argc, argv);
}

/*
** Audio
*/
static int sound_bps = DEFAULT_BPS;
static int sound_samplerate = DEFAULT_SAMPLERATE;
static int sound_fragsize = DEFAULT_FRAGSIZE;
static unsigned char *audioBuffer = NULL;
static void (*audio_callback)(void *buffer, int length) = NULL;

/* this is the callback that SDL calls to obtain more audio data */
static void sdl_audio_player(void *udata, unsigned char *stream, int len)
{
   /* SDL requests buffer fills in terms of bytes, not samples */
   if (16 == sound_bps)
      len /= 2;

   if (audio_callback)
      audio_callback(stream, len);
}

void osd_setsound(void (*playfunc)(void *buffer, int length))
{
   audio_callback = playfunc;
}

static void osd_stopsound(void)
{
   audio_callback = NULL;

   SDL_CloseAudio();
   if (NULL != audioBuffer)
      free(audioBuffer);
}

static int osd_init_sound(void)
{
   SDL_AudioSpec wanted, obtained;
   unsigned int bufferSize;

   sound_bps = config.read_int("sdlaudio", "sound_bps", DEFAULT_BPS);
   sound_samplerate = config.read_int("sdlaudio", "sound_samplerate", DEFAULT_SAMPLERATE);
   sound_fragsize = config.read_int("sdlaudio", "sound_fragsize", DEFAULT_FRAGSIZE);

   if (sound_bps != 8 && sound_bps != 16)
      sound_bps = 8;

   if (sound_samplerate < 5000)
      sound_samplerate = 5000;
   else if (sound_samplerate > 48000)
      sound_samplerate = 48000;

   if (sound_fragsize < 128)
      sound_fragsize = 128;
   else if (sound_fragsize > 32768)
      sound_fragsize = 32768;

   audio_callback = NULL;

   /* set the audio format */
   wanted.freq = sound_samplerate;
   wanted.format = (sound_bps == 8) ? AUDIO_U8 : AUDIO_S16;
   wanted.channels = 1;   /* 1 = mono, 2 = stereo */
   wanted.samples = sound_fragsize;
   wanted.callback = sdl_audio_player;
   wanted.userdata = NULL;

   if (SDL_OpenAudio (&wanted, &obtained) < 0)
   {
      nofrendo_log_printf("Couldn't open audio: %s\n", SDL_GetError());
      return -1;
   }

   /* ensure we get U8 or S16 */
   if (AUDIO_U8 != obtained.format && AUDIO_S16 != obtained.format)
   {
      nofrendo_log_printf("Could not get correct audio output format\n");
      return -1;
   }

   sound_bps = (obtained.format == AUDIO_U8) ? 8 : 16;
   sound_samplerate = obtained.freq;
   /* twice as big, to be on the safe side */
   bufferSize = (sound_bps / 8) * obtained.samples * 2;

   audioBuffer = malloc(bufferSize);
   if (NULL == audioBuffer)
   {
      nofrendo_log_printf("error allocating audio buffer\n");
      return -1;
   }

   SDL_PauseAudio(0);
   return 0;
}

void osd_getsoundinfo(sndinfo_t *info)
{
   info->sample_rate = sound_samplerate;
   info->bps = sound_bps;
}

/*
** Video
*/

static int init(int width, int height);
static void shutdown(void);
static int set_mode(int width, int height);
static void set_palette(rgb_t *pal);
static void clear(uint8 color);
static bitmap_t *lock_write(void);
static void free_write(int num_dirties, rect_t *dirty_rects);

viddriver_t sdlDriver =
{
   "Simple DirectMedia Layer",         /* name */
   init,          /* init */
   shutdown,      /* shutdown */
   set_mode,      /* set_mode */
   set_palette,   /* set_palette */
   clear,         /* clear */
   lock_write,    /* lock_write */
   free_write,    /* free_write */
   NULL,          /* custom_blit */
   false          /* invalidate flag */
};

void osd_getvideoinfo(vidinfo_t *info)
{
   info->default_width = DEFAULT_WIDTH;
   info->default_height = DEFAULT_HEIGHT;
   info->driver = &sdlDriver;
}

/* Now that the driver declaration is out of the way, on to the SDL stuff */
static SDL_Surface *mySurface = NULL;
static SDL_Color myPalette[256];
static bitmap_t *myBitmap = NULL;
static bool fullscreen = false;

/* flip between full screen and windowed */
void osd_togglefullscreen(int code)
{
   bool pause;
   nes_t *nes = nes_getcontextptr();

   if (INP_STATE_MAKE != code)
      return;

   ASSERT(nes);

   pause = nes->pause;
   nes->pause = true;

   fullscreen ^= true;

   if (set_mode(mySurface->w, mySurface->h))
      ASSERT(0);

   sdlDriver.invalidate = true;

   nes->pause = pause;
}

/* initialise SDL video */
static int init(int width, int height)
{
   return set_mode(width, height);
}

/* squash memory leaks */
static void shutdown(void)
{
   if (NULL != mySurface)
   {
      SDL_FreeSurface(mySurface);
      mySurface = NULL;
   }

   if (NULL != myBitmap)
      bmp_destroy(&myBitmap);
}

/* set a video mode */
static int set_mode(int width, int height)
{
   int flags;
   bool restorePalette;

   if (NULL != mySurface)
   {
      SDL_FreeSurface(mySurface);
      mySurface = NULL;
      restorePalette = true;
   }

   if (fullscreen)
   {
      flags = SDL_HWSURFACE | SDL_HWPALETTE | SDL_FULLSCREEN;
      mySurface = SDL_SetVideoMode(width, height, 8, flags);
      if (NULL == mySurface)
         nofrendo_log_printf("Fullscreeen failed: %s\n", SDL_GetError());
   }

   if (NULL == mySurface)
   {
      fullscreen = false;
      flags = SDL_HWSURFACE | SDL_HWPALETTE;
      mySurface = SDL_SetVideoMode(width, height, 8, flags);
   }

   if (NULL == mySurface)
   {
      nofrendo_log_printf("SDL Video failed: %s\n", SDL_GetError());
      return -1;
   }

   if (restorePalette)
   {
      SDL_SetColors(mySurface, myPalette, 0, 256);
   }

   SDL_ShowCursor(0);
   return 0;
}

/* copy nes palette over to hardware */
static void set_palette(rgb_t *pal)
{
   int i;

   for (i = 0; i < 256; i++)
   {
      myPalette[i].r = pal[i].r;
      myPalette[i].g = pal[i].g;
      myPalette[i].b = pal[i].b;
   }

   SDL_SetColors(mySurface, myPalette, 0, 256);
}

/* clear all frames to a particular color */
static void clear(uint8 color)
{
   SDL_FillRect(mySurface, 0, color);
}

/* acquire the directbuffer for writing */
static bitmap_t *lock_write(void)
{
   SDL_LockSurface(mySurface);
   myBitmap = bmp_createhw(mySurface->pixels, mySurface->w, 
                           mySurface->h, mySurface->pitch);
   return myBitmap;
}

/* release the resource */
static void free_write(int num_dirties, rect_t *dirty_rects)
{
   bmp_destroy(&myBitmap);
   SDL_UnlockSurface(mySurface);

   if (-1 == num_dirties)
   {
      SDL_UpdateRect(mySurface, 0, 0, 0, 0);
   }
   else if (num_dirties > 0)
   {
      /* loop through and modify the rects to be in terms of the screen */
      if (NES_SCREEN_WIDTH < mySurface->w || NES_VISIBLE_HEIGHT < mySurface->h)
      {
         int i, x_offset, y_offset;

         x_offset = (mySurface->w - NES_SCREEN_WIDTH) >> 1;
         y_offset = (mySurface->h - NES_VISIBLE_HEIGHT) >> 1;

         for (i = 0; i < num_dirties; i++)
         {
            dirty_rects[i].x += x_offset;
            dirty_rects[i].y += y_offset;
         }
      }
         
      SDL_UpdateRects(mySurface, num_dirties, (SDL_Rect *) dirty_rects);
   }
}

/*
** Input
*/

typedef struct joystick_s
{
   SDL_Joystick *js;
   int *button_array;
   int *axis_array;
} joystick_t;

static joystick_t **joystick_array = 0;
static int joystick_count = 0;

static int key_array[SDLK_LAST];
#define LOAD_KEY(key, def_key) \
key_array[key] = config.read_int("sdlkeys", #key, def_key)

static void osd_initinput()
{
   int i, j;
   SDL_Joystick *js;
   char group[255];
   char key[255];

   /* joystick */

   joystick_count = SDL_NumJoysticks();
   joystick_array = malloc(joystick_count * sizeof(joystick_t *));

   if (NULL == joystick_array)
   {
      nofrendo_log_printf("error allocating space for joystick array\n");
      joystick_count = 0;
   }
 
   nofrendo_log_printf("joystick_count == %i\n", joystick_count);

   for (i = 0; i < joystick_count; i++)
   {
      sprintf(group, "sdljoystick%i", i);
      js = SDL_JoystickOpen(i);

      if (js)
      {
         joystick_array[i] = malloc(sizeof(joystick_t));
         joystick_array[i]->js = js;
 
         nofrendo_log_printf("joystick %i is a %s\n", i, SDL_JoystickName(i));

         /* load buttons */
         j = SDL_JoystickNumButtons(joystick_array[i]->js);
         joystick_array[i]->button_array = malloc(j * sizeof(int));
         for (j--; j >= 0; j--)
         {
            sprintf(key, "button%i", j);
            joystick_array[i]->button_array[j] = config.read_int(group, key, event_none);
         }

         /* load axes */
         j = SDL_JoystickNumAxes(joystick_array[i]->js);
         joystick_array[i]->axis_array = malloc(j * sizeof(int) * 2);
         for (j--; j >= 0; j--)
         {
            sprintf(key, "positiveaxis%i", j);
            joystick_array[i]->axis_array[(j << 1) + 1] = config.read_int(group, key, event_none);

            sprintf(key, "negativeaxis%i", j);
            joystick_array[i]->axis_array[j << 1] = config.read_int(group, key, event_none);
         }
      }
      else
      {
         joystick_array[i] = 0;
      }
   }

   SDL_JoystickEventState(SDL_ENABLE);

   /* keyboard */

   LOAD_KEY(SDLK_ESCAPE, event_quit);
   
   LOAD_KEY(SDLK_F1, event_soft_reset);
   LOAD_KEY(SDLK_F2, event_hard_reset);
   LOAD_KEY(SDLK_F3, event_gui_toggle_fps);
   LOAD_KEY(SDLK_F4, event_snapshot);
   LOAD_KEY(SDLK_F5, event_state_save);
   LOAD_KEY(SDLK_F6, event_toggle_sprites);
   LOAD_KEY(SDLK_F7, event_state_load);
   LOAD_KEY(SDLK_F8, event_none);
   LOAD_KEY(SDLK_F9, event_none);
   LOAD_KEY(SDLK_F10, event_osd_1);
   LOAD_KEY(SDLK_F11, event_none);
   LOAD_KEY(SDLK_F12, event_none);

   LOAD_KEY(SDLK_BACKQUOTE, event_none);

   LOAD_KEY(SDLK_1, event_state_slot_1);
   LOAD_KEY(SDLK_EXCLAIM, event_state_slot_1);
   LOAD_KEY(SDLK_2, event_state_slot_2);
   LOAD_KEY(SDLK_AT, event_state_slot_2);
   LOAD_KEY(SDLK_3, event_state_slot_3);
   LOAD_KEY(SDLK_HASH, event_state_slot_3);
   LOAD_KEY(SDLK_4, event_state_slot_4);
   LOAD_KEY(SDLK_DOLLAR, event_state_slot_4);
   LOAD_KEY(SDLK_5, event_state_slot_5);
/*   LOAD_KEY(SDLK_PERCENT, event_state_slot_5);*/
   LOAD_KEY(SDLK_6, event_state_slot_6);
   LOAD_KEY(SDLK_CARET, event_state_slot_6);
   LOAD_KEY(SDLK_7, event_state_slot_7);
   LOAD_KEY(SDLK_AMPERSAND, event_state_slot_7);
   LOAD_KEY(SDLK_8, event_state_slot_8);
   LOAD_KEY(SDLK_ASTERISK, event_state_slot_8);
   LOAD_KEY(SDLK_9, event_state_slot_9);
   LOAD_KEY(SDLK_LEFTPAREN, event_state_slot_9);
   LOAD_KEY(SDLK_0, event_state_slot_0);
   LOAD_KEY(SDLK_RIGHTPAREN, event_state_slot_0);
   
   LOAD_KEY(SDLK_MINUS, event_gui_pattern_color_down);
   LOAD_KEY(SDLK_UNDERSCORE, event_gui_pattern_color_down);
   LOAD_KEY(SDLK_EQUALS, event_gui_pattern_color_up);
   LOAD_KEY(SDLK_PLUS, event_gui_pattern_color_up);

   LOAD_KEY(SDLK_BACKSPACE, event_gui_display_info);

   LOAD_KEY(SDLK_TAB, event_joypad1_select);

   LOAD_KEY(SDLK_q, event_toggle_channel_0);
   LOAD_KEY(SDLK_w, event_toggle_channel_1);
   LOAD_KEY(SDLK_e, event_toggle_channel_2);
   LOAD_KEY(SDLK_r, event_toggle_channel_3);
   LOAD_KEY(SDLK_t, event_toggle_channel_4);
   LOAD_KEY(SDLK_y, event_toggle_channel_5);
   LOAD_KEY(SDLK_u, event_palette_hue_down);
   LOAD_KEY(SDLK_i, event_palette_hue_up);
   LOAD_KEY(SDLK_o, event_gui_toggle_oam);
   LOAD_KEY(SDLK_p, event_gui_toggle_pattern);

/*   LOAD_KEY(SDLK_LEFTBRACE, event_none);
   LOAD_KEY(SDLK_RIGHTBRACE, event_none);*/
   LOAD_KEY(SDLK_LEFTBRACKET, event_none);
   LOAD_KEY(SDLK_RIGHTBRACKET, event_none);
   LOAD_KEY(SDLK_BACKSLASH, event_toggle_frameskip);
/*   LOAD_KEY(SDLK_Bar, event_toggle_frameskip);*/

   LOAD_KEY(SDLK_a, event_gui_toggle_wave);
   LOAD_KEY(SDLK_s, event_set_filter_0);
   LOAD_KEY(SDLK_d, event_set_filter_1);
   LOAD_KEY(SDLK_f, event_set_filter_2);
   LOAD_KEY(SDLK_g, event_none);
   LOAD_KEY(SDLK_h, event_none);
   LOAD_KEY(SDLK_j, event_palette_tint_down);
   LOAD_KEY(SDLK_k, event_palette_tint_up);
   LOAD_KEY(SDLK_l, event_palette_set_shady);
   LOAD_KEY(SDLK_COLON, event_palette_set_default);
   LOAD_KEY(SDLK_SEMICOLON, event_palette_set_default);
   LOAD_KEY(SDLK_QUOTEDBL, event_none);
   LOAD_KEY(SDLK_RETURN, event_joypad1_start);
   LOAD_KEY(SDLK_PAUSE, event_togglepause);

   LOAD_KEY(SDLK_z, event_joypad1_b);
   LOAD_KEY(SDLK_x, event_joypad1_a);
   LOAD_KEY(SDLK_c, event_joypad1_select);
   LOAD_KEY(SDLK_v, event_joypad1_start);
   LOAD_KEY(SDLK_b, event_joypad2_b);
   LOAD_KEY(SDLK_n, event_joypad2_a);
   LOAD_KEY(SDLK_m, event_joypad2_select);
   LOAD_KEY(SDLK_COMMA, event_joypad2_start);
   LOAD_KEY(SDLK_LESS, event_none);
   LOAD_KEY(SDLK_PERIOD, event_none);
   LOAD_KEY(SDLK_GREATER, event_none);
   LOAD_KEY(SDLK_QUESTION, event_none);
   LOAD_KEY(SDLK_SLASH, event_none);

   LOAD_KEY(SDLK_SPACE, event_gui_toggle);

   LOAD_KEY(SDLK_LCTRL, event_joypad1_b);
   LOAD_KEY(SDLK_RCTRL, event_joypad1_b);
   LOAD_KEY(SDLK_LALT, event_joypad1_a);
   LOAD_KEY(SDLK_RALT, event_joypad1_a);
   LOAD_KEY(SDLK_LSHIFT, event_joypad1_a);
   LOAD_KEY(SDLK_RSHIFT, event_joypad1_a);

   LOAD_KEY(SDLK_KP1, event_none);
   LOAD_KEY(SDLK_KP2, event_joypad1_down);
   LOAD_KEY(SDLK_KP3, event_none);
   LOAD_KEY(SDLK_KP4, event_joypad1_left);
   LOAD_KEY(SDLK_KP5, event_startsong);
   LOAD_KEY(SDLK_KP6, event_joypad1_right);
   LOAD_KEY(SDLK_KP7, event_songdown);
   LOAD_KEY(SDLK_KP8, event_joypad1_up);
   LOAD_KEY(SDLK_KP9, event_songup);
   LOAD_KEY(SDLK_UP, event_joypad1_up);
   LOAD_KEY(SDLK_DOWN, event_joypad1_down);
   LOAD_KEY(SDLK_LEFT, event_joypad1_left);
   LOAD_KEY(SDLK_RIGHT, event_joypad1_right);
   LOAD_KEY(SDLK_HOME, event_none);
   LOAD_KEY(SDLK_END, event_none);
   LOAD_KEY(SDLK_PAGEUP, event_none);
   LOAD_KEY(SDLK_PAGEDOWN, event_none);
   LOAD_KEY(SDLK_KP_PLUS, event_toggle_frameskip);

   /* events */

   event_set(event_osd_1, osd_togglefullscreen);
}

void osd_getinput(void)
{
   int code, highval, lowval;
   SDL_Event myEvent;
   event_t func_event;

   while (SDL_PollEvent(&myEvent))
   {
      switch(myEvent.type)
      {
      case SDL_KEYDOWN:
      case SDL_KEYUP:
         code = (myEvent.key.state == SDL_PRESSED) ? INP_STATE_MAKE : INP_STATE_BREAK;

         func_event = event_get(key_array[myEvent.key.keysym.sym]);
         if (func_event)
            func_event(code);
         break;

      case SDL_QUIT:
         event_get(event_quit)(INP_STATE_MAKE);
         break;

      case SDL_JOYAXISMOTION:
         highval = (myEvent.jaxis.value > 0) ? INP_STATE_MAKE : INP_STATE_BREAK;
         lowval  = (myEvent.jaxis.value < 0) ? INP_STATE_MAKE : INP_STATE_BREAK;

         func_event = event_get(joystick_array[myEvent.jaxis.which]->axis_array[myEvent.jaxis.axis << 1]);
         if (func_event)
            func_event(lowval);

         func_event = event_get(joystick_array[myEvent.jaxis.which]->axis_array[(myEvent.jaxis.axis << 1) + 1]);
         if (func_event)
            func_event(highval);
         break;

      case SDL_JOYBUTTONDOWN:
      case SDL_JOYBUTTONUP:
         code = (myEvent.jbutton.state == SDL_PRESSED) ? INP_STATE_MAKE : INP_STATE_BREAK;

         func_event = event_get( joystick_array[myEvent.jbutton.which]->button_array[myEvent.jbutton.button] );
         if (func_event)
            func_event(code);
         break;

      default:
         break;
      }
   }
}

static void osd_freeinput(void)
{
   int i;

   for (i = 0; i < joystick_count; i++)
   {
      if (joystick_array[i])
      {
         SDL_JoystickClose(joystick_array[i]->js);
         free(joystick_array[i]->button_array);
         free(joystick_array[i]->axis_array);
         free(joystick_array[i]);
      }
   }

   free(joystick_array);
}

void osd_getmouse(int *x, int *y, int *button)
{
   *button = SDL_GetMouseState(x, y);
}

/*
** Shutdown
*/

/* this is at the bottom, to eliminate warnings */
void osd_shutdown()
{
   osd_stopsound();
   osd_freeinput();
   SDL_Quit();
}

static int logprint(const char *string)
{
   return fprintf(stderr, "%s", string);
}

/*
** Startup
*/

int osd_init()
{
   nofrendo_log_chain_logfunc(logprint);

   /* Initialize the SDL library */
   if (SDL_Init (SDL_INIT_AUDIO | SDL_INIT_TIMER | SDL_INIT_VIDEO
                 | SDL_INIT_JOYSTICK) < 0)
   {
      printf("Couldn't initialize SDL: %s\n", SDL_GetError());
      return -1;
   }

   SDL_WM_SetCaption("Nofrendo", 0);

   if (osd_init_sound())
      return -1;

   osd_initinput();

   return 0;
}

/*
** $Log: sdl.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.16  2000/12/11 13:31:41  neil
** caption
**
** Revision 1.15  2000/11/25 20:29:42  matt
** tiny mod
**
** Revision 1.14  2000/11/09 14:06:31  matt
** state load fixed, state save mostly fixed
**
** Revision 1.13  2000/11/06 02:20:00  matt
** vid_drv / log api changes
**
** Revision 1.12  2000/11/05 16:36:06  matt
** thinlib round 2
**
** Revision 1.11  2000/11/05 06:26:41  matt
** thinlib spawns changes
**
** Revision 1.10  2000/11/01 17:31:54  neil
** fixed some conflicting key assignments
**
** Revision 1.9  2000/11/01 14:17:16  matt
** multi-system event system, or whatever
**
** Revision 1.8  2000/10/23 15:54:15  matt
** suppressed warnings
**
** Revision 1.7  2000/10/22 20:37:33  neil
** restored proper timer correction, and added support for variable frequencies
**
** Revision 1.6  2000/10/22 19:17:06  matt
** more sane timer ISR / autoframeskip
**
** Revision 1.5  2000/10/21 19:34:03  matt
** many more cleanups
**
** Revision 1.4  2000/10/17 11:59:29  matt
** let me see why i can't go window->full->window
**
** Revision 1.3  2000/10/13 14:09:57  matt
** sound is configurable from config file
**
** Revision 1.2  2000/10/13 13:19:15  matt
** fixed a few minor bugs and 16-bit sound
**
** Revision 1.1  2000/10/10 14:24:25  matt
** initial revision
**
*/
#endif