/*
 *  PAUSE.C - pause internal command.
 *
 *
 *  History:
 *
 *    16 Jul 1998 (Hans B Pufal)
 *        started.
 *
 *    16 Jul 1998 (John P Price)
 *        Seperated commands into individual files.
 *
 *    27-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        added config.h include
 *
 *    18-Jan-1999 (Eric Kohl)
 *        Unicode ready!
 */

#include "precomp.h"

#ifdef INCLUDE_CMD_PAUSE

/*
 * Perform PAUSE command.
 *
 * FREEDOS extension : If parameter is specified use that as the pause
 *   message.
 *
 * ?? Extend to include functionality of CHOICE if switch chars
 *     specified.
 *
 *    30-Apr-2005 (Magnus Olsen) <magnus@greatlord.com>
 *        Remove all hardcoded strings in En.rc
 */

INT cmd_pause (LPTSTR param)
{
    char *mode;
	TRACE ("cmd_pause: \'%s\')\n", debugstr_aw(param));

    if (!_tcsncmp (param, _T("/?"), 2))
    {
        ConOutResPaging(TRUE,STRING_PAUSE_HELP1);
        return 0;
    }

    if (*param)
        ConOutPrintf (param);
    else
        msg_pause ();

    mode=getenv("SSH_termmode");
	if (mode && strcmp(mode,"0")==0) {		// raw mode
		char value[12];
		int c;
		do {
			value[0]=0;
			c=tty_getinput("pause press return: ",value,10);
		} while (c!=0x0d);
	} else {
	    cgetchar ();
	}
    return 0;
}

#endif

/* EOF */
