#pragma once

/*
 *  cmdhooks.h - Plugins for cmd.exe
 *  Add extra hooks for more plugins
 *
 *
 *  History:
 *
 *    16 April 2025 -  Hans Harder
 *        started.
 *
 */
#define CMDHOOK_LOADFILE	1
#define CMDHOOK_COMMAND		2
#define CMDHOOK_EXECUTE		3

typedef PCHAR    (__cdecl *tCMDHOOK_LoadFile)(char *fn   , DWORD *siz);
typedef INT      (__cdecl *tCMDHOOK_Command )(char *First, char  *Rest, char *mem, DWORD memsize);
typedef INT      (__cdecl *tCMDHOOK_Execute )(char *First, char  *Rest);

extern tCMDHOOK_LoadFile	pCustomLoadFile;
extern tCMDHOOK_Command		pCustomCommand;
extern tCMDHOOK_Execute		pCustomExecute;


