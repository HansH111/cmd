/* ============================================================= *
 * Basic cmd interpreter with prepared hooks
 * as examples it checks for custom commands
 */
#pragma warning(disable: 4996 4748)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include "cmd_dll.h"

#define  PLC_YEAR		"2025"
#define  PLC_VERSION	"7.05"

char		*MODFILENAME=0;

/* custom command hook,   c=command   p=parameter string */
int	cmd_custom(char *c, char *p, char *mem, DWORD memsize)
{
	if (stricmp(c,"sayhello")==0) {
		while (*p==' ') p++;
		printf("hello %s!\n",p);
		return 0;
	}
	/* ignore commands */
	if (stricmp(c,"perlcall")==0) {
		printf("Error: %s command not supported !\n",c);
		return 0;
    }
    return -1;		// return cmd not found 
}

void showintro()
{
  printf("# Cmd Shell Executor                                     Copyright 2006 - %4s\n",PLC_YEAR);
  printf("# Version %4s                                                   www.atbas.org\n",PLC_VERSION);
}

void showsyntax(char *basename)
{
	showintro();
    printf("#\n");
	printf("# Syntax:  %s [/[C|K|I] command]\n",basename);
    printf("#\n");
	printf("# Option:  /C command  Runs the specified command and terminates.\n");
	printf("#          /K command  Runs the specified command and remains.\n");
	printf("#          /I params   Runs commands from STDIN   and terminates.\n");
    printf("#\n");
}

void Tgetmodinfo(char *basename, int maxlen)
{
	char	 pModulefn[300], *ptr;

	DWORD dwSize = GetModuleFileName(NULL,pModulefn,300);
	pModulefn[dwSize]=0;
	MODFILENAME=(char *)malloc(strlen(pModulefn)+1);
	if (MODFILENAME==NULL) exit(1);
	strcpy(MODFILENAME,pModulefn);
	if (basename) {
  	   ptr=strrchr(MODFILENAME,'\\');
	   if (ptr==NULL) ptr=strrchr(MODFILENAME,'/');
	   if (ptr) {
          strncpy(basename,&ptr[1],maxlen);
 		  basename[maxlen]=0;
	   }
	}
}

int main (int argc, char *argv[])
{
  char basename[41];
  int  retstat;

  Tgetmodinfo(basename,40);
  if (argc>1 && (argv[1][0]=='-' || (argv[1][0]=='/' && strchr("ASDCKRQIUV",toupper(argv[1][1]))==0)) ) {
     showsyntax(basename);
	 exit(0);
  }
  if (argc==1 && stricmp(basename,"Cmd.exe")==0) showintro();
  cmd_sethook(CMDHOOK_COMMAND, (void *)cmd_custom);

  retstat=cmd_runmain(MODFILENAME,argc,argv);
  fprintf(stdout,"\r\n");
  fflush(stdout);
  // cleanup
  if (MODFILENAME) free(MODFILENAME);
  return retstat;	
}
