#ifndef SAPTOOLS_H
#define SAPTOOLS_H

#pragma warning(disable: 4996 4748 4103)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
/*** #include <shellapi.h>  only for elevatenow ***/
#include <stdio.h>
#include <stdlib.h>

char *STstristr(char *arg1, char *arg2);
void  STsetenv (char *nm, char *sval,int val);
char *STgetenv (char *nm,char *dest,int maxsiz,int *pval);
int   STexists (char *fn, int tdir);
int   STaccess (char *fn, int tdir);
void  STfindbasedir(char *basedir,int siz);
void  STcheckpath  (char *basedir);

void  STgetexeinfo(void);
void  STgetparentdir(char *dirnm,char *dst);
void  STsplitfilename(char *fn, char *basenm, char *ext);
void  STgetdirs(char *etcdir, char *datadir, char *logdir, char *statdir);

extern char *EXEFILENAME;	// full filename including dirnm
extern char *EXEFILEDIR;	// dirname only
extern char *EXEBASEFILE;	// filename only

#endif

