#include <precomp.h>
#include <winternl.h> 

char *pCmdffn=0;	   	/* full filename */
char *pCmdDir=0;


int DoInitialize(char *ffn)
{
    char *ptr;
	pCmdffn=(char *)malloc(strlen(ffn)+1);
	if (pCmdffn==NULL) return(1);
    strcpy(pCmdffn, ffn);

	pCmdDir=(char *)malloc(strlen(ffn)+1);
	if (pCmdDir==NULL) return(1);
    strcpy(pCmdDir,ffn);
	ptr=strrchr(pCmdDir,'\\');   // BaseDIR	
	if (ptr==NULL)	ptr=strrchr(pCmdDir,'/');   // BaseDIR	
    if (ptr) ptr[0]=0;
	return(0);
}

__declspec(dllexport) int WINAPI cmd_sethook(int id, void *pFunc)
{
	switch (id) {
	   case CMDHOOK_LOADFILE : pCustomLoadFile=(tCMDHOOK_LoadFile)pFunc;  break;
	   case CMDHOOK_COMMAND  : pCustomCommand =(tCMDHOOK_Command )pFunc;  break;
	   case CMDHOOK_EXECUTE  : pCustomExecute =(tCMDHOOK_Execute )pFunc;  break;
	   default               : return 0;
	}
    return 1;
}

__declspec(dllexport) int WINAPI cmd_runmain(char *ffn, int argc, char *argv[])
{
	int ret;
	ret=DoInitialize(ffn);
	if (ret==0) {
		ret=cmd_main(argc,argv);
		if (pCmdffn) free(pCmdffn);
		if (pCmdDir) free(pCmdDir);
	}
	return ret;
}

INT WINAPI DllMain( IN PVOID hInstanceDll, IN ULONG dwReason, IN PVOID reserved)
{
	return TRUE;
}