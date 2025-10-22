/* vim: set tabstop=3 expandtab:
**
** This file is in the public domain.
**
** osd.c
**
** $Id: osd.c,v 1.2 2001/04/27 14:37:11 neil Exp $
**
*/
#ifdef NES_UNIX
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
       
#include <noftypes.h>
#include <nofconfig.h>
#include <log.h>
#include <osd.h>
#include <nofrendo.h>
#include <log.h>

#include <version.h>

/* path must be a valid path, and therefore no longer than PATH_MAX */
static void addSlash(char *path)
{
   int len = strlen(path);
   if(path[len - 1] != '/' && len + 1 <= PATH_MAX)
   {
      path[len] = '/';
      path[++len] = 0;
   }
}

/* filename must be a valid filename, and therefore no longer than PATH_MAX */
static void removePath(char *filename)
{
   char temp[PATH_MAX + 1];
   int i;

   i = strlen(filename);
   while(filename[--i] != '/' && i > 0);

   if(filename[i] == '/')
   {
      strncpy(temp, (filename + i + 1), PATH_MAX);
      strncpy(filename, temp, PATH_MAX);
   }
}

/* if filename ends in extension, cut it off of filename */
static void removeExtension(char *filename, const char *extension)
{
   int i = strlen(filename);
   int j = strlen(extension);

   if(i <= j) return;

   while(i && j)
   {
      i--;
      j--;
      if(filename[i] != extension[j]) return;
   }
   filename[i] = 0;
}

/* this determines where to store our data files */
static const char *dataDirectory(void)
{
   static char dataPath[PATH_MAX + 1];
   static bool checked = false;
   char cwd[PATH_MAX + 1];

   if(!checked)
   {
      checked = true;

      /* fall back to using cwd */
      getcwd(cwd, PATH_MAX);
      strncpy(dataPath, cwd, PATH_MAX);

      /* but default to using ~/.nofrendo/ if possible */
      if(getenv("HOME"))
      {
         char temp[PATH_MAX + 1];
         strncpy(temp, getenv("HOME"), PATH_MAX);
         addSlash(temp);
         strncat(temp, ".nofrendo", PATH_MAX - strlen(temp));

         if(!mkdir(temp, 0755) || errno == EEXIST)
         {
            nofrendo_log_printf("Succeeded in choosing HOME-based homeDirectory.\n");
            strncpy(dataPath, temp, PATH_MAX);
         }
      }
      
      /* make sure either path ends in a slash */
      addSlash(dataPath);
      nofrendo_log_printf("Storing data in %s\n", dataPath);
   }

   return dataPath;
}

/* This is os-specific part of main() */
int osd_main(int argc, char *argv[])
{
   static char configfilename[PATH_MAX + 1];

   /* command-line parameters */
   if(argc < 2)
   {
      printf("usage: %s IMAGE\n\n", argv[0]);
      printf("WHAP!\n");
      printf("WHAP!\n");
      printf("WHAP!\n");
      printf("Bad user!\n");
      printf("Run me correctly or you'll get more of the same!\n");
      return -1;
   }

   /* config file */
   strncpy(configfilename, dataDirectory(), PATH_MAX);
   strncat(configfilename, "config", PATH_MAX - strlen(configfilename));
   config.filename = configfilename;

   /* all done */
   return main_loop(argv[1], system_autodetect);
}

/* File system interface */
void osd_fullname(char *fullname, const char *shortname)
{
   strncpy(fullname, shortname, PATH_MAX);
}

/* This gives filenames for storage of saves */
char *osd_newextension(char *string, char *ext)
{
   char name[PATH_MAX + 1];

   removePath(string);
   removeExtension(string, ".gz");
   removeExtension(string, ".bz2");

   strncpy(name, dataDirectory(), PATH_MAX);
   strncat(name, string, PATH_MAX - strlen(name));
   strncat(name, ext, PATH_MAX - strlen(name));

   strcpy(string, name);

   nofrendo_log_printf("osd_newextension: %s\n", string);
   return string;
}

/* This gives filenames for storage of PCX snapshots */
int osd_makesnapname(char *filename, int len)
{
   char fullpath[PATH_MAX + 1];
   struct stat stat_data;
   int pcx_num = -1;

   /* TODO: check if fullpath has %, \, or anything else special in it */
   strncpy(fullpath, dataDirectory(), PATH_MAX);
   strncat(fullpath, "snap%04d.pcx", PATH_MAX - strlen(fullpath));

   while (++pcx_num < 10000)
   {
      snprintf(filename, len, fullpath, pcx_num);

      if(stat(filename, &stat_data))
         return pcx_num;
   }

   return -1;
}

/*
** $Log: osd.c,v $
** Revision 1.2  2001/04/27 14:37:11  neil
** wheeee
**
** Revision 1.1  2001/04/27 12:54:40  neil
** blah
**
** Revision 1.1.1.1  2001/04/27 07:03:54  neil
** initial
**
** Revision 1.38  2000/10/10 14:04:27  neil
** what's TRUE is not true
**
** Revision 1.37  2000/10/10 13:58:14  matt
** stroustrup squeezing his way in the door
**
** Revision 1.36  2000/10/10 13:05:20  matt
** Mr. Clean makes a guest appearance
**
** Revision 1.35  2000/07/31 04:29:04  matt
** one million cleanups
**
** Revision 1.34  2000/07/27 01:37:30  neil
** change of case in system enum
**
** Revision 1.33  2000/07/26 21:36:14  neil
** Big honkin' change -- see the mailing list
**
** Revision 1.32  2000/07/23 16:47:15  neil
** no more static charp[ in osd_newextension
**
** Revision 1.31  2000/07/21 18:08:43  neil
** Now we store our data files in ~/.nofrendo
**
** Revision 1.30  2000/07/21 14:35:58  neil
** Off by one on extension stripping
**
** Revision 1.29  2000/07/21 14:06:16  neil
** little convenience fixes
**
** Revision 1.28  2000/07/21 13:54:42  neil
** Fixed snapshot file generation
**
** Revision 1.27  2000/07/21 13:39:17  neil
** New main structure and new event structure
**
** Revision 1.26  2000/07/20 15:09:26  neil
** moving to SDL
**
** Revision 1.25  2000/07/19 15:59:40  neil
** PATH_MAX, strncpy, snprintf, and strncat are our friends
**
** Revision 1.24  2000/07/19 12:32:37  neil
** warning-free days are here again with PATH_MAX
**
** Revision 1.23  2000/07/18 09:24:07  neil
** Sound update... SDL works reasonably, but latent.  OSS direct buffer access split out from OSS write() access.
**
** Revision 1.22  2000/07/16 06:47:23  neil
** Madly hacking at the sound, Desprately grabbing at whatever might work
**
** Revision 1.21  2000/07/14 05:40:31  neil
** Exploring options for making sound work
**
** Revision 1.20  2000/07/11 08:04:59  neil
** Moved the rom info display to osd.c from input.c
**
** Revision 1.19  2000/07/11 07:48:49  neil
** Replaced config.h with version.h, thanks to the config.h API
**
** Revision 1.18  2000/07/11 07:39:25  neil
** split input into input.c and ggiinput.c; swallowed bogoggi.h into linux.h
**
** Revision 1.17  2000/07/11 07:14:45  neil
** Headers merged
**
** Revision 1.16  2000/07/11 06:34:40  neil
** moved some files from input.c to (better?) places in osd.c and sound.c
**
** Revision 1.15  2000/07/09 19:39:13  bsittler
** Fixed timer interval (it was 17 us, not 17 ms) and added page-flipping.
**
** Revision 1.14  2000/07/09 18:36:38  neil
** bogoggi eliminates the uintx already defined warnings
**
** Revision 1.13  2000/07/09 06:42:06  neil
** Non-working attempt at GGI keyboard input
**
** Revision 1.12  2000/07/07 03:23:24  neil
** You can't see anything, but input and sound work impeccably
**
** Revision 1.11  2000/07/06 18:55:35  neil
** Pub-lic Do-main here I come... I don't need no GPL
**
** Revision 1.10  2000/07/06 16:41:42  matt
** removed dpp flag
**
** Revision 1.9  2000/07/06 16:40:14  neil
** copyright and/or log added
**
** Revision 1.8  2000/07/06 16:31:50  neil
** Now works, albeit lacking speed and input, on my desktop
**
** Revision 1.7  2000/07/06 05:38:38  neil
** read it and weep (I'm not sure if it's tears of joy
**
** Revision 1.6  2000/07/06 04:27:14  neil
** It's close enough to working that I can taste it
**
** Revision 1.5  2000/07/05 23:37:46  neil
** Muahahaha
**
** Revision 1.3  2000/07/05 19:57:36  neil
** __GNUC -> __DJGPP in nes.c
**
** Revision 1.2  2000/07/05 17:26:16  neil
** Moved the externs in nofrendo.c to osd.h
**
** Revision 1.1  2000/07/05 17:14:34  neil
** Linux: Act Two, Scene One
**
**
*/
#endif