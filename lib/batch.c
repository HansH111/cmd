/*
 *  BATCH.C - batch file processor for CMD.EXE.
 *
 *
 *  History:
 *
 *    ??/??/?? (Evan Jeffrey)
 *        started.
 *
 *    15 Jul 1995 (Tim Norman)
 *        modes and bugfixes.
 *
 *    08 Aug 1995 (Matt Rains)
 *        i have cleaned up the source code. changes now bring this
 *        source into guidelines for recommended programming practice.
 *
 *        i have added some constants to help making changes easier.
 *
 *    29 Jan 1996 (Steffan Kaiser)
 *        made a few cosmetic changes
 *
 *    05 Feb 1996 (Tim Norman)
 *        changed to comply with new first/rest calling scheme
 *
 *    14 Jun 1997 (Steffen Kaiser)
 *        bug fixes.  added error level expansion %?.  ctrl-break handling
 *
 *    16 Jul 1998 (Hans B Pufal)
 *        Totally reorganised in conjunction with COMMAND.C (cf) to
 *        implement proper BATCH file nesting and other improvements.
 *
 *    16 Jul 1998 (John P Price <linux-guru@gcfl.net>)
 *        Seperated commands into individual files.
 *
 *    19 Jul 1998 (Hans B Pufal) [HBP_001]
 *        Preserve state of echo flag across batch calls.
 *
 *    19 Jul 1998 (Hans B Pufal) [HBP_002]
 *        Implementation of FOR command
 *
 *    20-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        added error checking after cmd_alloc calls
 *
 *    27-Jul-1998 (John P Price <linux-guru@gcfl.net>)
 *        added config.h include
 *
 *    02-Aug-1998 (Hans B Pufal) [HBP_003]
 *        Fixed bug in ECHO flag restoration at exit from batch file
 *
 *    26-Jan-1999 Eric Kohl
 *        Replaced CRT io functions by Win32 io functions.
 *        Unicode safe!
 *
 *    23-Feb-2001 (Carl Nettelblad <cnettel@hem.passagen.es>)
 *        Fixes made to get "for" working.
 *
 *    02-Apr-2005 (Magnus Olsen <magnus@greatlord.com>)
 *        Remove all hardcoded strings in En.rc
 */

#include "precomp.h"

/* The stack of current batch contexts.
 * NULL when no batch is active
 */
LPBATCH_CONTEXT bc = NULL;

BOOL bEcho = TRUE;  /* The echo flag */

extern BOOL bParseError;
extern BOOL CMD_TRACE_STATE;

/* Buffer for reading Batch file lines */
TCHAR textline[BATCH_BUFFSIZE];

#ifdef CMD_CUSTOM_PLUGINS
tCMDHOOK_LoadFile	pCustomLoadFile=0;
#endif

/*
 * Returns a pointer to the n'th parameter of the current batch file.
 * If no such parameter exists returns pointer to empty string.
 * If no batch file is current, returns NULL
 *
 */

LPTSTR FindArg(TCHAR Char, BOOL *IsParam0)
{
    LPTSTR pp;
    INT n = Char - _T('0');

    TRACE ("FindArg: (%d)\n", n);

    if (n < 0 || n > 9)
        return NULL;

    n = bc->shiftlevel[n];
    *IsParam0 = (n == 0);
    pp = bc->params;

    /* Step up the strings till we reach the end */
    /* or the one we want */
    while (*pp && n--)
        pp += _tcslen (pp) + 1;

    return pp;
}


/*
 * Batch_params builds a parameter list in newlay allocated memory.
 * The parameters consist of null terminated strings with a final
 * NULL character signalling the end of the parameters.
 *
 */

LPTSTR BatchParams (LPTSTR s1, LPTSTR s2)
{
    LPTSTR dp = (LPTSTR)cmd_alloc ((_tcslen(s1) + _tcslen(s2) + 3) * sizeof (TCHAR));

    /* JPP 20-Jul-1998 added error checking */
    if (dp == NULL)
    {
        error_out_of_memory();
        return NULL;
    }

    if (s1 && *s1)
    {
        s1 = _stpcpy (dp, s1);
        *s1++ = _T('\0');
    }
    else
        s1 = dp;

    while (*s2)
    {
        BOOL inquotes = FALSE;

        /* Find next parameter */
        while (_istspace(*s2) || (*s2 && _tcschr(_T(",;="), *s2)))
            s2++;
        if (!*s2)
            break;

        /* Copy it */
        do
        {
            if (!inquotes && (_istspace(*s2) || _tcschr(_T(",;="), *s2)))
                break;
            inquotes ^= (*s2 == _T('"'));
            *s1++ = *s2++;
        } while (*s2);
        *s1++ = _T('\0');
    }

    *s1 = _T('\0');

    return dp;
}

/*
 * free the allocated memory of a batch file
 */
VOID ClearBatch()
{
    TRACE ("ClearBatch  mem = %08x    free = %d\n", bc->mem, bc->memfree);

	if (bc->mem && bc->memfree && bc->memsize)
        cmd_free(bc->mem);

	if (bc->raw_params)
        cmd_free(bc->raw_params);

    if (bc->params)
        cmd_free(bc->params);
}

/*
 * If a batch file is current, exits it, freeing the context block and
 * chaining back to the previous one.
 *
 * If no new batch context is found, sets ECHO back ON.
 *
 * If the parameter is non-null or not empty, it is printed as an exit
 * message
 */

VOID ExitBatch()
{
    ClearBatch();

    TRACE ("ExitBatch\n");

    UndoRedirection(bc->RedirList, NULL);
    FreeRedirection(bc->RedirList);

    /* Preserve echo state across batch calls */
    bEcho = bc->bEcho;

    while (bc->setlocal)
        cmd_endlocal(_T(""));

    bc = bc->prev;
    /* If there is no more batch contexts, notify the signal handler */
    if (!bc)
        CheckCtrlBreak(BREAK_OUTOFBATCH);
	}

/*
 * Exit all the nested batch calls.
 */
VOID ExitAllBatches(VOID)
{
    while (bc)
        ExitBatch();
}


/*
 * Load batch file from stdin into memory
 *
 */
 char *LoadFromStdin(DWORD *siz)
{
	int	 n, p, maxpos;
	char *buf, *nbuf;
	
	TRACE ("LoadFromStdin ()\n");
/*    mode=getenv("SSH_termmode");
	if (mode == NULL) {
	   mode=(char *)malloc(2);
	   if (mode) strcpy(mode,"9");
	}  */
	*siz=0;
	maxpos=4096;
	buf=(char *)cmd_alloc(maxpos);
	p=0;
	while ( (n=fread(&buf[p],1,1,stdin)) ) {
		p++;
		buf[p]='\0';
		if (p>=(maxpos-2)) {
			nbuf=cmd_realloc(buf,maxpos+4096);
			if (nbuf) {
				buf=nbuf;
				maxpos+=4096;
			} else
				return buf;
		}
	};
	if (IsExistingFile("c:\\temp\\laststdin.cmd")) {
	   FILE *fp;
  	   fp=fopen("c:\\temp\\laststdin.cmd", "w");
	   if (fp) {
  	   	  fputs(buf,fp);
		  fclose(fp);
	   }
	}
	*siz=p;
	return buf;
}


/*
 * Load batch file into memory
 *
 */
void BatchFile2Mem(HANDLE hBatchFile)
{
    TRACE ("BatchFile2Mem ()\n");

	if (_tcsicmp(bc->BatchFilePath, _T("stdin.cmd"))==0) {
		bc->mem = LoadFromStdin(&bc->memsize);
	} else {
	    bc->mem = NULL;
#ifdef CMD_CUSTOM_PLUGINS
  		if (pCustomLoadFile) {
			bc->mem = pCustomLoadFile(bc->BatchFilePath, &bc->memsize);
#ifdef CMD_TRACE_DYNAMIC		
			/* disable tracing if file was encrypted */
			if (bc->mem) {
				CMD_TRACE_STATE = FALSE ;
				// mem will be freed when next customloadfile is done since it is in a different
				// dll or environment, otherwise it crashes when freed
		        bc->memfree=FALSE;
  		        TRACE("BatchFile2Mem memory %08x - %d\n",bc->mem,bc->memsize);
				return;
			}
#endif
		}
#endif
		if (bc->mem == NULL)
		{
            bc->memsize = GetFileSize(hBatchFile, NULL);
            bc->mem     = (char *)cmd_alloc(bc->memsize+1);		/* 1 extra for '\0' */

            /* if memory is available, read it in and close the file */
            if (bc->mem != NULL)
            {
                TRACE ("BatchFile2Mem memory %08x - %08x\n",bc->mem,bc->memsize);
                SetFilePointer (hBatchFile, 0, NULL, FILE_BEGIN);
                ReadFile(hBatchFile, (LPVOID)bc->mem, bc->memsize,  &bc->memsize, NULL);
			}
		}
	}
	if (bc->mem != NULL) 
	{
		TRACE ("BatchFile2Mem memory %08x - %08x\n",bc->mem,bc->memsize);
        bc->mem[bc->memsize]='\0';  /* end this, so you can dump it as a string */
        bc->memfree=TRUE;           /* this one needs to be freed */
    }
    else
    {
        bc->memsize=0;              /* this will prevent mem being accessed */
        bc->memfree=FALSE;
    }
    bc->mempos = 0;                 /* set position to the start */
}

/*
 * Start batch file execution
 *
 * The firstword parameter is the full filename of the batch file.
 *
 */
INT Batch (LPTSTR fullname, LPTSTR firstword, LPTSTR param, PARSED_COMMAND *Cmd)
{
    BATCH_CONTEXT new;
    LPFOR_CONTEXT saved_fc;
    INT i;
    INT ret = 0;
    BOOL same_fn = FALSE;

    HANDLE hFile = 0;
    SetLastError(0);
    if (bc && bc->mem)
    {
        TCHAR fpname[MAX_PATH];
		if (_tcsicmp(fullname, _T("stdin.cmd"))==0)
			same_fn=TRUE;
		else {
            GetFullPathName(fullname, sizeof(fpname) / sizeof(TCHAR), fpname, NULL);
            if (_tcsicmp(bc->BatchFilePath,fpname)==0) same_fn=TRUE;
        }
	}
    TRACE ("Batch: (\'%s\', \'%s\', \'%s\')  same_fn = %d\n",
        debugstr_aw(fullname), debugstr_aw(firstword), debugstr_aw(param), same_fn);

	if (!same_fn && _tcsicmp(fullname, _T("stdin.cmd"))!=0)
    {
        hFile = CreateFile(fullname, GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL |
                           FILE_FLAG_SEQUENTIAL_SCAN, NULL);

        if (hFile == INVALID_HANDLE_VALUE)
        {
            ConErrResPuts(STRING_BATCH_ERROR);
            return 1;
        }
    }

    if (bc != NULL && Cmd == bc->current)
    {
        /* Then we are transferring to another batch */
        ClearBatch();
        AddBatchRedirection(&Cmd->Redirections);
    }
    else
    {
        struct _SETLOCAL *setlocal = NULL;

        if (Cmd == NULL)
        {
            /* This is a CALL. CALL will set errorlevel to our return value, so
             * in order to keep the value of errorlevel unchanged in the case
             * of calling an empty batch file, we must return that same value. */
            ret = nErrorLevel;
        }
        else if (bc)
        {
            /* If a batch file runs another batch file as part of a compound command
             * (e.g. "x.bat & somethingelse") then the first file gets terminated. */

            /* Get its SETLOCAL stack so it can be migrated to the new context */
            setlocal = bc->setlocal;
            bc->setlocal = NULL;
            ExitBatch();
        }

        /* Create a new context. This function will not
         * return until this context has been exited */
        new.prev = bc;
        /* copy some fields in the new structure if it is the same file */
        if (same_fn) {
            new.mem     = bc->mem;
            new.memsize = bc->memsize;
            new.mempos  = 0;
            new.memfree = FALSE;    /* don't free this, being used before this */
        }
        bc = &new;
        bc->RedirList = NULL;
        bc->setlocal = setlocal;
    }

	if (_tcsicmp(fullname, _T("stdin.cmd"))==0) 
		_tcscpy(bc->BatchFilePath,fullname);
	else
        GetFullPathName(fullname, sizeof(bc->BatchFilePath) / sizeof(TCHAR), bc->BatchFilePath, NULL);
    /*  if a new batch file, load it into memory and close the file */
    if (!same_fn)
    {
        BatchFile2Mem(hFile);
		if (hFile) CloseHandle(hFile);
    }

    bc->mempos = 0;    /* goto begin of batch file */
    bc->bEcho = bEcho; /* Preserve echo across batch calls */
    for (i = 0; i < 10; i++)
        bc->shiftlevel[i] = i;

    bc->params = BatchParams (firstword, param);
    //
    // Allocate enough memory to hold the params and copy them over without modifications
    //
    bc->raw_params = cmd_dup(param);
    if (bc->raw_params == NULL)
    {
	    error_out_of_memory();
        return 1;
    }

    /* Check if this is a "CALL :label" */
//    if (*firstword == _T(':'))
//        cmd_goto(firstword);
   /* If this is a "CALL :label args ...", call a subroutine of
     * the current batch file, only if extensions are enabled. */
    if (*firstword == _T(':'))
    {
        LPTSTR expLabel;

        /* Position at the place of the parent file (which is the same as the caller) */
        bc->mempos = (bc->prev ? bc->prev->mempos : 0);

        /*
         * Jump to the label. Strip the label's colon; as a side-effect
         * this will forbid "CALL :EOF"; however "CALL ::EOF" will work!
         */
        bc->current = Cmd;
        ++firstword;

        /* Expand the label only! (simulate a GOTO command as in Windows' CMD) */
        expLabel = DoDelayedExpansion(firstword);
        ret = cmd_goto(expLabel ? expLabel : firstword);
        if (expLabel)
            cmd_free(expLabel);
    }


    /* If we are calling from inside a FOR, hide the FOR variables */
    saved_fc = fc;
    fc = NULL;

    /* If we have created a new context, don't return
     * until this batch file has completed. */
    while (bc == &new && !bExit)
    {
        Cmd = ParseCommand(NULL);
        if (!Cmd)
		{
			if (!bParseError)
                continue;
			
            /* JPP 19980807 */
            /* Echo batch file line */
            if (bEcho && !bDisableBatchEcho && Cmd->Type != C_QUIET)
            {
                if (!bIgnoreEcho)
                    ConOutChar(_T('\n'));
                PrintPrompt();
                EchoCommand(Cmd);
                ConOutChar(_T('\n'));
            }
            /* Stop all execution */
            ExitAllBatches();
            ret = 1;
            break;
        }
        bc->current = Cmd;
        ret = ExecuteCommand(Cmd);
        FreeCommand(Cmd);
    }
    if (bExit)
    {
        /* Stop all execution */
        ExitAllBatches();
    }
    fc = saved_fc;
	    /* Always return the last command's return code */
    TRACE("Batch: returns %d\n", ret);
    return ret;
}

VOID AddBatchRedirection(REDIRECTION **RedirList)
{
    REDIRECTION **ListEnd;

    /* Prepend the list to the batch context's list */
    ListEnd = RedirList;
    while (*ListEnd)
        ListEnd = &(*ListEnd)->Next;
    *ListEnd = bc->RedirList;
    bc->RedirList = *RedirList;

    /* Null out the pointer so that the list will not be cleared prematurely.
     * These redirections should persist until the batch file exits. */
    *RedirList = NULL;
}

/*
 *   Read a single line from the batch file from the current batch/memory position.
 *   Almost a copy of FileGetString with same UNICODE handling
 */
BOOL BatchGetString (LPTSTR lpBuffer, INT nBufferLength)
{
    LPSTR lpString;
    INT len = 0;
#ifdef _UNICODE
    lpString = cmd_alloc(nBufferLength);
#else
    lpString = lpBuffer;
#endif
    /* read all chars from memory until a '\n' is encountered */
    if (bc->mem)
    {
        for (; (bc->mempos < bc->memsize  &&  len < (nBufferLength-1)); len++)
        { 
            lpString[len] = bc->mem[bc->mempos++];
            if (lpString[len] == '\n' )
            {
                len++;
                break;
            }
        }
    }

    if (!len)
    {
#ifdef _UNICODE
        cmd_free(lpString);
#endif
		lpBuffer[0]='\0';
        return FALSE;
    }

    lpString[len++] = '\0';
#ifdef _UNICODE
    MultiByteToWideChar(OutputCodePage, 0, lpString, -1, lpBuffer, len);
    cmd_free(lpString);
#endif
    return TRUE;
}

/*
 * Read and return the next executable line form the current batch file
 *
 * If no batch file is current or no further executable lines are found
 * return NULL.
 *
 * Set eflag to 0 if line is not to be echoed else 1
 */
LPTSTR ReadBatchLine ()
{
    TRACE ("ReadBatchLine ()\n");

    /* User halt */
    if (CheckCtrlBreak (BREAK_BATCHFILE))
    {
        while (bc)
            ExitBatch();
        return NULL;
    }

    if (!BatchGetString (textline, sizeof (textline) / sizeof (textline[0]) - 1))
    {
        TRACE ("ReadBatchLine(): Reached EOF!\n");
        /* End of file.... */
        ExitBatch();
        return NULL;
    }

    TRACE ("ReadBatchLine(): textline: \'%s\'\n", debugstr_aw(textline));

    if (textline[_tcslen(textline) - 1] != _T('\n'))
        _tcscat(textline, _T("\n"));

    return textline;
}

/* EOF */
