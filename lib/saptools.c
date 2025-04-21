#include "saptools.h"

char *EXEFILENAME = NULL;	// full filename including dirnm
char *EXEFILEDIR  = NULL;	// dirname only
char *EXEBASEFILE = NULL;	// filename only


char *STstristr(char *arg1, char *arg2)
{                  
   const char *a, *b;
                   
   for(;*arg1;*arg1++) {
     a = arg1;
     b = arg2;
     while((*a++ | 32) == (*b++ | 32))  if(!*b) return (arg1); 
   } 
   return(NULL);
}

void STsetenv(char *nm, char *sval,int val)
{
  if (sval==NULL) {
	  char vals[21];
	  sprintf(vals,"%d",val);
	  SetEnvironmentVariable(nm,vals);
  } else
	  SetEnvironmentVariable(nm,sval);
}


// -- return 1 if exists, 0 if not found
int STexists(char *fn, int tdir)
{
    DWORD attr;
	attr = GetFileAttributes (fn);
	if (tdir) {
  	  if (attr==INVALID_FILE_ATTRIBUTES)  return 0;
	  if (attr&FILE_ATTRIBUTE_DIRECTORY)  return 1;
	} else {
	  if (attr != 0xFFFFFFFF && (! (attr & FILE_ATTRIBUTE_DIRECTORY)) ) return 1;
	}
	return 0;
}
// -- return 0 if access, 1 if not found
int STaccess(char *fn, int tdir)
{
	if (STexists(fn,tdir)) return 0;
	return 1;
}

char *STgetenv(char *nm,char *dest,int maxsiz,int *pval)
{
  static char *tmp=NULL;
  char  *ptr;
  int    len;
 
  if (dest) *dest='\0';
  if (pval) *pval=0;
  if (tmp==NULL) {
	  tmp=(char *)malloc(512);
	  if (tmp==NULL)  return tmp;
  }
  if (maxsiz==0) maxsiz=512;
  ptr=dest?dest:tmp;
  len=GetEnvironmentVariable(nm,ptr,maxsiz-1);
  if (len > 0) {
     ptr[len]='\0';
     if (pval) *pval=atoi(ptr);
     return ptr;
  }
  return NULL;
}


// find out dirnms and exe names
// fills: EXEFILEDIR,EXEFILENAME,
void	STgetexeinfo(void)
{
	char	 pModuleDir[300];
	char	*ptr;

	DWORD dwSize = GetModuleFileName(NULL,pModuleDir,300);
	pModuleDir[dwSize]=0;
	
	EXEFILENAME=(char *)malloc(strlen(pModuleDir)+1);
	strcpy(EXEFILENAME,pModuleDir);
	
	ptr=strrchr(pModuleDir,'\\');
    if (!ptr) ptr=strrchr(pModuleDir,'/');
    if (ptr) *ptr='\0';
	EXEFILEDIR=(char *)malloc(strlen(pModuleDir)+1);
	strcpy(EXEFILEDIR,pModuleDir);

	EXEBASEFILE=(char *)malloc(strlen(&ptr[1])+1);
	strcpy(EXEBASEFILE,&ptr[1]);
}

void STgetparentdir(char *dirnm,char *dst)
{
	char *ptr;
	strcpy(dst,dirnm);
	ptr=strrchr(dst,'\\');
    if (!ptr) ptr=strrchr(dst,'/');
	if (ptr) {
		*ptr = '\0';
	} else {
		dst[0]= '\0';
	}
}

void STsplitfilename(char *fn, char *basenm, char *ext) 
{
	char *ptr;
	strcpy(basenm,fn);
	if (ext) strcpy(ext,"");
	ptr=strrchr(basenm,'.');
	if (ptr) {
	   *ptr='\0';
       if (ext) strcpy(ext,&ptr[1]);
	}
}

void	STgetdirs(char *etcdir, char *datadir, char *logdir, char *statdir)
{
	char	basedir[300];
	
	STgetexeinfo();
    STgetparentdir(EXEFILEDIR,basedir);

	if (etcdir) {
  	   sprintf(etcdir,"%s\\etc",basedir);
       if (STexists(etcdir,1)==0) strcpy(etcdir,basedir);
	}
	if (datadir) {
	   sprintf(datadir,"%s\\data",basedir);
		if (STexists(datadir,1)==0) strcpy(datadir,"C:\\ProgramData\\SapTooling\\data");
        if (STexists(datadir,1)==0) strcpy(datadir,basedir);
	}
	if (logdir) {
		sprintf(logdir,"%s\\log",basedir);
		if (STexists(logdir,1)==0) strcpy(logdir,"C:\\ProgramData\\SapTooling\\log");
        if (STexists(logdir,1)==0) strcpy(logdir,basedir);
	}
	if (statdir) {
		sprintf(statdir,"%s\\stat",basedir);
		if (STexists(statdir,1)==0) strcpy(statdir,"C:\\ProgramData\\SapTooling\\stat");
        if (STexists(statdir,1)==0) strcpy(statdir,basedir);
    }
	return;
}

void STfindbasedir(char *basedir,int siz)
{
  char *drv, *root;
  /* --- find out orimon home directory if it is not defined */
  root=NULL;
  root=STgetenv("SapToolsDir",basedir,siz,0);
  if (!root) root=STgetenv("STbasedir",basedir,siz,0);
  if (!root) {
      drv=STgetenv("OriMonDrv",basedir,80,0);
      if (drv) {
		char inidir[81];
		drv[1]=':';
		drv[2]='\0';
		strcat(basedir,"\\");
		root=STgetenv("OriMonRoot",0,80,0);
		if (root) {
  		   while (*root=='\\') root++;
	       strcat(basedir,root);
		   sprintf(inidir,"%s\\ini",basedir);
  	       if (STexists(inidir,1)==0)  drv=NULL;
		} else {
		   drv=NULL;
		}
	  }
	  if (drv == NULL) {
	  	int      i;
		/* search C D E F disks */
		strcpy(basedir,"C:\\apps\\SapTools\\ini");
 		for (i=0; (i<4 && STexists(basedir,1)==0); i++)  basedir[0]++;
		if (!STexists(basedir,1) ) {
			strcpy(basedir,"C:\\appl\\sap\\ini");
	 		for (i=0; (i<4 && STexists(basedir,1)==0); i++)  basedir[0]++;
		}
		if (!STexists(basedir,1) ) {
  		   strcpy(basedir,"C:\\apps\\OriMon\\ini");
	  	   for (i=0; (i<4 && STexists(basedir,1)==0); i++)  basedir[0]++;
		   if (!STexists(basedir,1) ) basedir[0]='C';
		}
		basedir[strlen(basedir)-4]=0;  // cut of \ini
	  }
  }
  SetEnvironmentVariable("SapToolsDir",basedir);
  SetEnvironmentVariable("STbasedir"  ,basedir);
  SetEnvironmentVariable("OriMonDir"  ,basedir);
  basedir[2]=0;			
  SetEnvironmentVariable("OriMonDrv"  ,basedir);
  SetEnvironmentVariable("OriMonRoot" ,&basedir[3]);
  basedir[2]='\\';
  if (STstristr(basedir,"orimon")) {
	  char tmp[256], typ[3];
	  strcpy(typ,"ST");
      sprintf(tmp,"%s\\cmd\\ST-status.exe",basedir);
	  if (STexists(tmp,0) == 0) strcpy(typ,"OM");
      SetEnvironmentVariable("STexetype",typ);
  } else
      SetEnvironmentVariable("STexetype","ST");
}

void STcheckpath(char *basedir)
{
  char *curpath;
  curpath=STgetenv("PATH",0,0,0);
  if (curpath  && STstristr(curpath,"util")==0) {
	  char newpath[512];
	  int len;
	  strcpy(newpath,basedir);
	  strcat(newpath,"\\util;");
	  strcat(newpath,basedir);
	  strcat(newpath,"\\cmd;");
	  if (STstristr(curpath,"c:\\windows;")==0)       strcat(newpath,"C:\\Windows;");  
	  if (STstristr(curpath,"\\system32;" )==0)       strcat(newpath,"C:\\Windows\\system32;");  
	  if (STstristr(curpath,"\\system32\\wbem;")==0)  strcat(newpath,"C:\\Windows\\system32\\wbem;");  
      len=strlen(newpath) + strlen(curpath);
	  if (len<500) {
		  strcat(newpath,curpath);
	  }
	  SetEnvironmentVariable("PATH" ,newpath);
  }
}

