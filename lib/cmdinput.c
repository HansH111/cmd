/*
 *  CMDINPUT.C - handles command input (tab completion, history, etc.).
 *
 *
 *  History:
 *
 *    01/14/95 (Tim Norman)
 *        started.
 *
 *    08/08/95 (Matt Rains)
 *        i have cleaned up the source code. changes now bring this source
 *        into guidelines for recommended programming practice.
 *        i have added some constants to help making changes easier.
 *
 *    12/12/95 (Tim Norman)
 *        added findxy() function to get max x/y coordinates to display
 *        correctly on larger screens
 *
 *    12/14/95 (Tim Norman)
 *        fixed the Tab completion code that Matt Rains broke by moving local
 *        variables to a more global scope and forgetting to initialize them
 *        when needed
 *
 *    8/1/96 (Tim Norman)
 *        fixed a bug in tab completion that caused filenames at the beginning
 *        of the command-line to have their first letter truncated
 *
 *    9/1/96 (Tim Norman)
 *        fixed a silly bug using printf instead of fputs, where typing "%i"
 *        confused printf :)
 *
 *    6/14/97 (Steffan Kaiser)
 *        ctrl-break checking
 *
 *    6/7/97 (Marc Desrochers)
 *        recoded everything! now properly adjusts when text font is changed.
 *        removed findxy(), reposition(), and reprint(), as these functions
 *        were inefficient. added goxy() function as gotoxy() was buggy when
 *        the screen font was changed. the printf() problem with %i on the
 *        command line was fixed by doing printf("%s",str) instead of
 *        printf(str). Don't ask how I find em just be glad I do :)
 *
 *    7/12/97 (Tim Norman)
 *        Note: above changes pre-empted Steffan's ctrl-break checking.
 *
 *    7/7/97 (Marc Desrochers)
 *        rewrote a new findxy() because the new dir() used it.  This
 *        findxy() simply returns the values of *maxx *maxy.  In the
 *        future, please use the pointers, they will always be correct
 *        since they point to BIOS values.
 *
 *    7/8/97 (Marc Desrochers)
 *        once again removed findxy(), moved the *maxx, *maxy pointers
 *        global and included them as externs in command.h.  Also added
 *        insert/overstrike capability
 *
 *    7/13/97 (Tim Norman)
 *        added different cursor appearance for insert/overstrike mode
 *
 *    7/13/97 (Tim Norman)
 *        changed my code to use _setcursortype until I can figure out why
 *        my code is crashing on some machines.  It doesn't crash on mine :)
 *
 *    27-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        added config.h include
 *
 *    28-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        put ifdef's around filename completion code.
 *
 *    30-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        moved filename completion code to filecomp.c
 *        made second TAB display list of filename matches
 *
 *    31-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        Fixed bug where if you typed something, then hit HOME, then tried
 *        to type something else in insert mode, it crashed.
 *
 *    07-Aug-1998 (John P Price <linux-guru@gcfl.net>)
 *        Fixed carrage return output to better match MSDOS with echo
 *        on or off.(marked with "JPP 19980708")
 *
 *    13-Dec-1998 (Eric Kohl)
 *        Added insert/overwrite cursor.
 *
 *    25-Jan-1998 (Eric Kohl)
 *        Replaced CRT io functions by Win32 console io functions.
 *        This can handle <Shift>-<Tab> for 4NT filename completion.
 *        Unicode and redirection safe!
 *
 *    04-Feb-1999 (Eric Kohl)
 *        Fixed input bug. A "line feed" character remained in the keyboard
 *        input queue when you pressed <RETURN>. This sometimes caused
 *        some very strange effects.
 *        Fixed some command line editing annoyances.
 *
 *    30-Apr-2004 (Filip Navara <xnavara@volny.cz>)
 *        Fixed problems when the screen was scrolled away.
 *
 *    28-September-2007 (Hervé Poussineau)
 *        Added history possibilities to right key.
 */

#include "precomp.h"
SHORT maxx;
SHORT maxy;

/*
 * global command line insert/overwrite flag
 */
static BOOL bInsert = TRUE;


static VOID
ClearCommandLine(LPTSTR str, INT maxlen, SHORT orgx, SHORT orgy)
{
    INT count;

    SetCursorXY (orgx, orgy);
    for (count = 0; count < (INT)_tcslen (str); count++)
        ConOutChar (_T(' '));
    _tcsnset (str, _T('\0'), maxlen);
    SetCursorXY (orgx, orgy);
}

VOID GetPromptString(char *str);

enum KEY_ACTION{
        CONTROL_A = 1,         /* Ctrl-a */
        CONTROL_C = 3,         /* Ctrl-c */
        CONTROL_D = 4,         /* Ctrl-d */
        CONTROL_F = 6,         /* Ctrl-f */
        CONTROL_G = 7,         /* Ctrl-h */
        CONTROL_H = 8,         /* Ctrl-h */
        CONTROL_I = 9,         /* Ctrl-h */
        CONTROL_S = 19,        /* Ctrl-s */
        CONTROL_T = 20,        /* Ctrl-s */
        CONTROL_V = 22,        /* Ctrl-u */
        CONTROL_W = 23,        /* Ctrl-u */
        CONTROL_X = 24,        /* Ctrl-u */
        CONTROL_Y = 25,        /* Ctrl-u */
        ESC = 27,           /* Escape */
        BACKSPACE =  127,   /* Backspace */
        /* The following are just soft codes, not really reported by the
         * terminal directly. */
        TTY_LEFT = 1000,
        TTY_RIGHT,
        TTY_UP,
        TTY_DOWN,
        TTY_DEL,
        TTY_INS,
        TTY_HOME,
        TTY_END,
        TTY_PGUP,
        TTY_PGDOWN,
};

/* Read a key from the terminal put in raw mode, trying to handle
 * escape sequences. */
int tty_readkey()
{
    char c, seq[3];

    while(1) {
		if (fread(&c,1,1,stdin)==0) {
			return 0;
		}
        switch(c) {
        case ESC:    /* escape sequence */
            /* ESC [ sequences. */
			fread(&seq[0],1,1,stdin);
			fread(&seq[1],1,1,stdin);
            if (seq[0] == '[') {
                if (seq[1] >= '0' && seq[1] <= '9') {
                    /* Extended escape, read additional byte. */
					fread(&seq[2],1,1,stdin);
					if (seq[2] == '~') {
						switch (seq[1]) {
		                   case '1':  return TTY_HOME;
		                   case '2':  return TTY_INS;
		                   case '3':  return TTY_DEL;
		                   case '4':  return TTY_END;
		                   case '5':  return TTY_PGUP;
		                   case '6':  return TTY_PGDOWN;
						}
					}
					fprintf(stdout,"key esc [ %02x %02x\r\n",seq[1],seq[2]);
					fflush(stdout);
                } else {
                    switch(seq[1]) {
                    case 'A': return TTY_UP;
                    case 'B': return TTY_DOWN;
                    case 'C': return TTY_RIGHT;
                    case 'D': return TTY_LEFT;
                    case 'H': return TTY_HOME;
                    case 'F': return TTY_END;
                    }
					fprintf(stdout,"key esc [ %02x\r\n",seq[1]);
					fflush(stdout);
                }
            }
            break;
        default:
            return c;
        }
    }
}

int tty_getCursorPosition(int *rows, int *cols) {
	char buf[32];
	unsigned int i = 0;

	fwrite("\x1b[6n", 1, 4, stdout);
	fflush(stdout);
	while (i < sizeof(buf) - 1) {
		if (fread(&buf[i], 1, 1, stdin) != 1) break;
		if (buf[i] == 'R') break;
		i++;
	}
	buf[i] = '\0';

	if (buf[0] != '\x1b' || buf[1] != '[') return -1;
	if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return -1;

	return 0;
}

int tty_getWindowSize(int *rows, int *cols) {
	fwrite("\x1b[999C",1, 6,stdout);
	fflush(stdout);
	return tty_getCursorPosition(rows, cols);
}


int tty_getinput(char *prompt,char *s, int maxlen)
{
  int 				c, plen,len, pos, terminate = 0;
  char		 		insert_mode = 1;   /* default insert mode */
  int		 		xstart,viewlen;
  static int		cols=0,rows=0;
  char              ts[513], debug[80];

  if (cols==0) {
	  if (tty_getWindowSize(&rows,&cols)<0) cols=80;
  }
  plen=strlen(prompt);
  xstart=len=0;
  pos=len=strlen(s);
  viewlen=cols-plen-2;
  if (maxlen>500) maxlen=500;
//  fprintf(stdout,"\r----------->         1         2         3         4         5         6\n");
//  fprintf(stdout,"\r----------->1234567890123456789012345678901234567890123456789012345678901234567890\n");
  do
  {
   if ((pos+1)>maxlen) pos = maxlen;
   if (xstart>pos)  xstart=pos;
   if ((pos-xstart+1)>viewlen)  xstart=pos-viewlen;
   // --- ts contains viewport string
   if ((xstart+viewlen)>=len) {
      sprintf(ts,"%s",&s[xstart]);
      c=len-pos;
   } else {
      sprintf(ts,"%*.*s",viewlen,viewlen,&s[xstart]);
      c=viewlen-(pos-xstart);
   }
//   sprintf(debug,"%03d-%03d %03d>",xstart,pos,c);
   fprintf(stdout,"\r%s%s%c[K",prompt,ts,27);
   if (c>0) fprintf(stdout,"%c[%dD",27,c);
   fflush(stdout);
   c=tty_readkey();
   if (c==0) return 0;
   switch( c )
   {
   case CONTROL_A :  
   case TTY_HOME  :  pos = 0; xstart=0;
                     break;
   case TTY_END   :  
   case CONTROL_F :  pos = len;
                     if (len>viewlen) xstart = len - viewlen;
                     break;
   case CONTROL_V :
   case TTY_INS   :  insert_mode = (!insert_mode);
                     break;
   case TTY_LEFT  :  
   case CONTROL_S :  if (pos > 0) pos--;
                     break;
   case TTY_RIGHT :  
   case CONTROL_D :  if (pos < len) pos++;
                     break;
   case BACKSPACE :
   case CONTROL_H :  if (pos+xstart > 0)
                     {
						 memmove(&s[pos - 1], &s[pos], len - pos + 1);
                         if (pos > 0)  pos--;
                         len--;
                     }
                     break;
   case CONTROL_G :
   case TTY_DEL   :  if (pos < len)
                     {
						 memmove(&s[pos], &s[pos + 1], len - pos);
                         len--;
                     }
                     break;
   case CONTROL_X :  
   case CONTROL_Y :  len = 0;
                     pos = 0;
                     xstart = 0;
                     break;
   case CONTROL_T :  len = pos;
                     break;
   default        :
    if (c==TTY_UP || c==TTY_DOWN || c==0x0d)
       terminate = c;
    else if ( pos<maxlen  && c>=32 && c<=126 ) {
      if (insert_mode)
      {
          if (len==maxlen)
		      memmove(&s[pos + 1], &s[pos], len - pos );
          else
          {
		      memmove(&s[pos + 1], &s[pos], len - pos + 1);
              len++;
          }
          s[pos] = c;
          pos++;
      }
      else
      {
          if (pos >= len)  len++;
          s[pos] = c;
          pos++;
      }
    }
    break;
  } /* switch */
  if (pos>maxlen) pos = maxlen;
  s[len] = 0;
 }
 while (!terminate);
 return c;
}


/* read in a command line */
BOOL ReadCommand(LPTSTR str, INT maxlen)
{
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    SHORT orgx;     /* origin x/y */
    SHORT orgy;
    SHORT curx;     /*current x/y cursor position*/
    SHORT cury;
    SHORT tempscreen;
    INT   count;    /*used in some for loops*/
    INT   current = 0;  /*the position of the cursor in the string (str)*/
    INT   charcount = 0;/*chars in the string (str)*/
    INPUT_RECORD ir;
    DWORD dwControlKeyState;
#ifdef FEATURE_UNIX_FILENAME_COMPLETION
    WORD   wLastKey = 0;
#endif
    TCHAR  ch;
    BOOL bReturn = FALSE;
    BOOL bCharInput;
#ifdef FEATURE_4NT_FILENAME_COMPLETION
    TCHAR szPath[MAX_PATH];
#endif
#ifdef FEATURE_HISTORY
    //BOOL bContinue=FALSE;/*is TRUE the second case will not be executed*/
    TCHAR PreviousChar;
#endif

    if (!GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        /* No console */
        HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
        DWORD dwRead;
        CHAR  chr, esc=0, *mode=NULL;
		int	   c;
		char  szPrompt[256];
		GetPromptString(szPrompt);

		str[0]=0;
        mode=getenv("SSH_termmode");
		if (mode == NULL) {
		   mode=(char *)malloc(2);
		   if (mode) strcpy(mode,"9");
		}
		if (mode && strcmp(mode,"0")==0) {		// raw mode
			str[0]=0;
			do {
				c=tty_getinput(szPrompt,str,maxlen);
				if (c==0) return FALSE;
				if (c!= 0x0d) {
   					str[0]=0;
					if (c==TTY_UP)   History (-1, str);
				    if (c==TTY_DOWN) History ( 1, str);
				}
			} while (c!=0x0d);
	 	    fprintf(stdout,"\r%s%s",szPrompt,str);
			fflush(stdout);
		} else {
        charcount=0;
		do
        {
			if (fread(&chr,1,1,stdin)==0) {
				return FALSE;
			}
#ifdef _UNICODE
            MultiByteToWideChar(InputCodePage, 0, &chr, 1, &str[charcount++], 1);
#endif
			if (chr==8 || chr==127) {
				if (charcount>0) {
					charcount--;
			        str[charcount] = _T('\0');
					fprintf(stdout,"\b \b");
				} else {
					str[0]=_T('\0');
				}
			} else if (chr>=32) {	
				str[charcount++]=chr;
				fprintf(stdout,"%c",chr);
			}
	        str[charcount] = _T('\0');
//			fprintf(stdout,"ch=%02x %c line=[%s] esc=%d\n",chr,(chr>=32)?chr:'.',str,esc);
			fflush(stdout);
        } while (chr != 0x0d && charcount < maxlen);
	}
#ifdef FEATURE_HISTORY
        /* add to the history */
        if (str[0]) History (0, str);
#endif
		strcat(str,"\n");
		ConOutChar (_T('\n'));						/* go to beginning of line and reprint prompt and input */
//		if (charcount > 1) {
//  	  	   PrintPrompt ();
//		   ConOutPrintf (_T("%s"), str);
//		}
        return TRUE;
    }

    /* get screen size */
    maxx = csbi.dwSize.X;
    maxy = csbi.dwSize.Y;

    curx = orgx = csbi.dwCursorPosition.X;
    cury = orgy = csbi.dwCursorPosition.Y;

    memset (str, 0, maxlen * sizeof (TCHAR));

    SetCursorType (bInsert, TRUE);

    do
    {
        bReturn = FALSE;
        ConInKey (&ir);

        dwControlKeyState = ir.Event.KeyEvent.dwControlKeyState;

        if (dwControlKeyState &
            (RIGHT_ALT_PRESSED |LEFT_ALT_PRESSED|
             RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED) )
        {
            switch (ir.Event.KeyEvent.wVirtualKeyCode)
            {
#ifdef FEATURE_HISTORY
                case 'K':
                    /*add the current command line to the history*/
                    if (dwControlKeyState &
                        (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))
                    {
                        if (str[0])
                            History(0,str);

                        ClearCommandLine (str, maxlen, orgx, orgy);
                        current = charcount = 0;
                        curx = orgx;
                        cury = orgy;
                        //bContinue=TRUE;
                        break;
                    }

                case 'D':
                    /*delete current history entry*/
                    if (dwControlKeyState &
                        (LEFT_CTRL_PRESSED|RIGHT_CTRL_PRESSED))
                    {
                        ClearCommandLine (str, maxlen, orgx, orgy);
                        History_del_current_entry(str);
                        current = charcount = _tcslen (str);
                        ConOutPrintf (_T("%s"), str);
                        GetCursorXY (&curx, &cury);
                        //bContinue=TRUE;
                        break;
                    }

#endif /*FEATURE_HISTORY*/
            }
        }

        bCharInput = FALSE;

        switch (ir.Event.KeyEvent.wVirtualKeyCode)
        {
            case VK_BACK:
                /* <BACKSPACE> - delete character to left of cursor */
                if (current > 0 && charcount > 0)
                {
                    if (current == charcount)
                    {
                        /* if at end of line */
                        str[current - 1] = _T('\0');
                        if (GetCursorX () != 0)
                        {
                            ConOutPrintf (_T("\b \b"));
                            curx--;
                        }
                        else
                        {
                            SetCursorXY ((SHORT)(maxx - 1), (SHORT)(GetCursorY () - 1));
                            ConOutChar (_T(' '));
                            SetCursorXY ((SHORT)(maxx - 1), (SHORT)(GetCursorY () - 1));
                            cury--;
                            curx = maxx - 1;
                        }
                    }
                    else
                    {
                        for (count = current - 1; count < charcount; count++)
                            str[count] = str[count + 1];
                        if (GetCursorX () != 0)
                        {
                            SetCursorXY ((SHORT)(GetCursorX () - 1), GetCursorY ());
                            curx--;
                        }
                        else
                        {
                            SetCursorXY ((SHORT)(maxx - 1), (SHORT)(GetCursorY () - 1));
                            cury--;
                            curx = maxx - 1;
                        }
                        GetCursorXY (&curx, &cury);
                        ConOutPrintf (_T("%s "), &str[current - 1]);
                        SetCursorXY (curx, cury);
                    }
                    charcount--;
                    current--;
                }
                break;

            case VK_INSERT:
                /* toggle insert/overstrike mode */
                bInsert ^= TRUE;
                SetCursorType (bInsert, TRUE);
                break;

            case VK_DELETE:
                /* delete character under cursor */
                if (current != charcount && charcount > 0)
                {
                    for (count = current; count < charcount; count++)
                        str[count] = str[count + 1];
                    charcount--;
                    GetCursorXY (&curx, &cury);
                    ConOutPrintf (_T("%s "), &str[current]);
                    SetCursorXY (curx, cury);
                }
                break;

            case VK_HOME:
                /* goto beginning of string */
                if (current != 0)
                {
                    SetCursorXY (orgx, orgy);
                    curx = orgx;
                    cury = orgy;
                    current = 0;
                }
                break;

            case VK_END:
                /* goto end of string */
                if (current != charcount)
                {
                    SetCursorXY (orgx, orgy);
                    ConOutPrintf (_T("%s"), str);
                    GetCursorXY (&curx, &cury);
                    current = charcount;
                }
                break;

            case VK_TAB:
#ifdef FEATURE_UNIX_FILENAME_COMPLETION
                /* expand current file name */
                if ((current == charcount) ||
                    (current == charcount - 1 &&
                     str[current] == _T('"'))) /* only works at end of line*/
                {
                    if (wLastKey != VK_TAB)
                    {
                        /* if first TAB, complete filename*/
                        tempscreen = charcount;
                        CompleteFilename (str, charcount);
                        charcount = _tcslen (str);
                        current = charcount;

                        SetCursorXY (orgx, orgy);
                        ConOutPrintf (_T("%s"), str);

                        if (tempscreen > charcount)
                        {
                            GetCursorXY (&curx, &cury);
                            for (count = tempscreen - charcount; count--; )
                                ConOutChar (_T(' '));
                            SetCursorXY (curx, cury);
                        }
                        else
                        {
                            if (((charcount + orgx) / maxx) + orgy > maxy - 1)
                                orgy += maxy - ((charcount + orgx) / maxx + orgy + 1);
                        }

                        /* set cursor position */
                        SetCursorXY ((orgx + current) % maxx,
                                 orgy + (orgx + current) / maxx);
                        GetCursorXY (&curx, &cury);
                    }
                    else
                    {
                        /*if second TAB, list matches*/
                        if (ShowCompletionMatches (str, charcount))
                        {
                            PrintPrompt();
                            GetCursorXY(&orgx, &orgy);
                            ConOutPrintf(_T("%s"), str);

                            /* set cursor position */
                            SetCursorXY((orgx + current) % maxx,
                                         orgy + (orgx + current) / maxx);
                            GetCursorXY(&curx, &cury);
                        }

                    }
                }
                else
                {
                    MessageBeep(-1);
                }
#endif
#ifdef FEATURE_4NT_FILENAME_COMPLETION
                /* used to later see if we went down to the next line */
                tempscreen = charcount;
                szPath[0]=_T('\0');

                /* str is the whole things that is on the current line
                   that is and and out.  arg 2 is weather it goes back
                    one file or forward one file */
                CompleteFilename(str, !(ir.Event.KeyEvent.dwControlKeyState & SHIFT_PRESSED), szPath, current);
                /* Attempt to clear the line */
                ClearCommandLine (str, maxlen, orgx, orgy);
                curx = orgx;
                cury = orgy;
                current = charcount = 0;

                /* Everything is deleted, lets add it back in */
                _tcscpy(str,szPath);

                /* Figure out where cusor is going to be after we print it */
                charcount = _tcslen(str);
                current = charcount;

                SetCursorXY(orgx, orgy);
                /* Print out what we have now */
                ConOutPrintf(_T("%s"), str);

                /* Move cursor accordingly */
                if (tempscreen > charcount)
                {
                    GetCursorXY(&curx, &cury);
                    for(count = tempscreen - charcount; count--; )
                        ConOutChar(_T(' '));
                    SetCursorXY(curx, cury);
                }
                else
                {
                    if (((charcount + orgx) / maxx) + orgy > maxy - 1)
                        orgy += maxy - ((charcount + orgx) / maxx + orgy + 1);
                }
                SetCursorXY((short)(((int)orgx + current) % maxx), (short)((int)orgy + ((int)orgx + current) / maxx));
                GetCursorXY(&curx, &cury);
#endif
                break;

            case _T('M'):
            case _T('C'):
                /* ^M does the same as return */
                bCharInput = TRUE;
                if (!(ir.Event.KeyEvent.dwControlKeyState &
                    (RIGHT_CTRL_PRESSED|LEFT_CTRL_PRESSED)))
                {
                    break;
                }

            case VK_RETURN:
                /* end input, return to main */
#ifdef FEATURE_HISTORY
                /* add to the history */
                if (str[0])
                    History (0, str);
#endif
                str[charcount++] = _T('\n');
                str[charcount] = _T('\0');
                ConOutChar(_T('\n'));
            bReturn = TRUE;
                break;

            case VK_ESCAPE:
                /* clear str  Make this callable! */
                ClearCommandLine (str, maxlen, orgx, orgy);
                curx = orgx;
                cury = orgy;
                current = charcount = 0;
                break;

#ifdef FEATURE_HISTORY
            case VK_F3:
                History_move_to_bottom();
#endif
            case VK_UP:
#ifdef FEATURE_HISTORY
                /* get previous command from buffer */
                ClearCommandLine (str, maxlen, orgx, orgy);
                History (-1, str);
                current = charcount = _tcslen (str);
                if (((charcount + orgx) / maxx) + orgy > maxy - 1)
                    orgy += maxy - ((charcount + orgx) / maxx + orgy + 1);
                ConOutPrintf (_T("%s"), str);
                GetCursorXY (&curx, &cury);
#endif
                break;

            case VK_DOWN:
#ifdef FEATURE_HISTORY
                /* get next command from buffer */
                ClearCommandLine (str, maxlen, orgx, orgy);
                History (1, str);
                current = charcount = _tcslen (str);
                if (((charcount + orgx) / maxx) + orgy > maxy - 1)
                    orgy += maxy - ((charcount + orgx) / maxx + orgy + 1);
                ConOutPrintf (_T("%s"), str);
                GetCursorXY (&curx, &cury);
#endif
                break;

            case VK_LEFT:
                if (dwControlKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
                {
                    /* move cursor to the previous word */
                    if (current > 0)
                    {
                        while (current > 0 && str[current - 1] == _T(' '))
                        {
                            current--;
                            if (curx == 0)
                            {
                                cury--;
                                curx = maxx -1;
                            }
                            else
                            {
                                curx--;
                            }
                        }

                        while (current > 0 && str[current -1] != _T(' '))
                        {
                            current--;
                            if (curx == 0)
                            {
                                cury--;
                                curx = maxx -1;
                            }
                            else
                            {
                                curx--;
                            }
                        }

                        SetCursorXY(curx, cury);
                    }
                }
                else
                {
                    /* move cursor left */
                    if (current > 0)
                    {
                        current--;
                        if (GetCursorX () == 0)
                        {
                            SetCursorXY ((SHORT)(maxx - 1), (SHORT)(GetCursorY () - 1));
                            curx = maxx - 1;
                            cury--;
                        }
                        else
                        {
                            SetCursorXY ((SHORT)(GetCursorX () - 1), GetCursorY ());
                            curx--;
                        }
                    }
                    else
                    {
                        MessageBeep (-1);
                    }
                }
                break;

            case VK_RIGHT:
                if (dwControlKeyState & (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
                {
                    /* move cursor to the next word */
                    if (current != charcount)
                    {
                        while (current != charcount && str[current] != _T(' '))
                        {
                            current++;
                            if (curx == maxx - 1)
                            {
                                cury++;
                                curx = 0;
                            }
                            else
                            {
                                curx++;
                            }
                        }

                        while (current != charcount && str[current] == _T(' '))
                        {
                            current++;
                            if (curx == maxx - 1)
                            {
                                cury++;
                                curx = 0;
                            }
                            else
                            {
                                curx++;
                            }
                        }

                        SetCursorXY(curx, cury);
                    }
                }
                else
                {
                    /* move cursor right */
                    if (current != charcount)
                    {
                        current++;
                        if (GetCursorX () == maxx - 1)
                        {
                            SetCursorXY (0, (SHORT)(GetCursorY () + 1));
                            curx = 0;
                            cury++;
                        }
                        else
                        {
                            SetCursorXY ((SHORT)(GetCursorX () + 1), GetCursorY ());
                            curx++;
                        }
                    }
#ifdef FEATURE_HISTORY
                    else
                    {
                        LPCTSTR last = PeekHistory(-1);
                        if (last && charcount < (INT)_tcslen (last))
                        {
                            PreviousChar = last[current];
                            ConOutChar(PreviousChar);
                            GetCursorXY(&curx, &cury);
                            str[current++] = PreviousChar;
                            charcount++;
                        }
                    }
#endif
                }
                break;

            default:
                /* This input is just a normal char */
                bCharInput = TRUE;

            }
#ifdef _UNICODE
            ch = ir.Event.KeyEvent.uChar.UnicodeChar;
            if (ch >= 32 && (charcount != (maxlen - 2)) && bCharInput)
#else
            ch = ir.Event.KeyEvent.uChar.AsciiChar;
            if ((UCHAR)ch >= 32 && (charcount != (maxlen - 2)) && bCharInput)
#endif /* _UNICODE */
            {
                /* insert character into string... */
                if (bInsert && current != charcount)
                {
                    /* If this character insertion will cause screen scrolling,
                     * adjust the saved origin of the command prompt. */
                    tempscreen = _tcslen(str + current) + curx;
                    if ((tempscreen % maxx) == (maxx - 1) &&
                        (tempscreen / maxx) + cury == (maxy - 1))
                    {
                        orgy--;
                        cury--;
                    }

                    for (count = charcount; count > current; count--)
                        str[count] = str[count - 1];
                    str[current++] = ch;
                    if (curx == maxx - 1)
                        curx = 0, cury++;
                    else
                        curx++;
                    ConOutPrintf (_T("%s"), &str[current - 1]);
                    SetCursorXY (curx, cury);
                    charcount++;
                }
                else
                {
                    if (current == charcount)
                        charcount++;
                    str[current++] = ch;
                    if (GetCursorX () == maxx - 1 && GetCursorY () == maxy - 1)
                        orgy--, cury--;
                    if (GetCursorX () == maxx - 1)
                        curx = 0, cury++;
                    else
                        curx++;
                    ConOutChar (ch);
                }
            }

        //wLastKey = ir.Event.KeyEvent.wVirtualKeyCode;
    }
    while (!bReturn);

    SetCursorType (bInsert, TRUE);

#ifdef FEATURE_ALIASES
    /* expand all aliases */
    ExpandAlias (str, maxlen);
#endif /* FEATURE_ALIAS */
    return TRUE;
}
