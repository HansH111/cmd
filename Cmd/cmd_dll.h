/*
 *  cmd_dll.h - Function definitions for calling the Cmd.dll
 *
 *  History:
 *
 *    16 April 2025 -  Hans Harder
 *        started.
 *
 */

/*
 *   definitions for cmd_sethook function  id values
 */
#define CMDHOOK_LOADFILE	1
#define CMDHOOK_COMMAND		2
#define CMDHOOK_EXECUTE		3

/*
 * dll functions for setting hook and to start main cmd
 */
int WINAPI cmd_sethook(int id, void *pFunc);
int WINAPI cmd_runmain(char *ffn, int argc, char *argv[]);

