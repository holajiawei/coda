#ifndef _BLURB_
#define _BLURB_
/*

            Coda: an Experimental Distributed File System
                             Release 4.0

          Copyright (c) 1987-1996 Carnegie Mellon University
                         All Rights Reserved

Permission  to  use, copy, modify and distribute this software and its
documentation is hereby granted,  provided  that  both  the  copyright
notice  and  this  permission  notice  appear  in  all  copies  of the
software, derivative works or  modified  versions,  and  any  portions
thereof, and that both notices appear in supporting documentation, and
that credit is given to Carnegie Mellon University  in  all  documents
and publicity pertaining to direct or indirect use of this code or its
derivatives.

CODA IS AN EXPERIMENTAL SOFTWARE SYSTEM AND IS  KNOWN  TO  HAVE  BUGS,
SOME  OF  WHICH MAY HAVE SERIOUS CONSEQUENCES.  CARNEGIE MELLON ALLOWS
FREE USE OF THIS SOFTWARE IN ITS "AS IS" CONDITION.   CARNEGIE  MELLON
DISCLAIMS  ANY  LIABILITY  OF  ANY  KIND  FOR  ANY  DAMAGES WHATSOEVER
RESULTING DIRECTLY OR INDIRECTLY FROM THE USE OF THIS SOFTWARE  OR  OF
ANY DERIVATIVE WORK.

Carnegie  Mellon  encourages  users  of  this  software  to return any
improvements or extensions that  they  make,  and  to  grant  Carnegie
Mellon the rights to redistribute these changes without encumbrance.
*/

static char *rcsid = "$Header: /afs/cs/project/coda-src/cvs/coda/coda-src/volutil/vol-timing.cc,v 4.3 1998/01/10 18:40:05 braam Exp $";
#endif /*_BLURB_*/





#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#include <sys/types.h>
#include <sys/time.h>

#include <unistd.h>
#include <stdlib.h>

#include <lwp.h>
#include <lock.h>
#include <timer.h>
#include <rpc2.h>
#include <se.h>
#include <volutil.h>

#ifdef __cplusplus
}
#endif __cplusplus

#include <vice.h>
#include <cvnode.h>
#include <volume.h>
#include <timing.h>
#include <util.h>

extern timing_path *tpinfo;
extern int probingon;

#define TIMINGFILE "/tmp/timing.tmp"
static FILE * timingfile;
/*
  BEGIN_HTML
  <a name="S_VolTiming"><strong>Toggle timing flag and process timing trace</strong></a>
  END_HTML
*/
long S_VolTiming(RPC2_Handle rpcid, RPC2_Integer OnFlag, SE_Descriptor *formal_sed) {
    SE_Descriptor sed;
    ProgramType *pt;
    int rc = 0;
    
    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolTiming: OnFlag = %d", OnFlag);
    assert(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);
    VInitVolUtil(volumeUtility);
    if (OnFlag) {
	probingon = 1;
	LogMsg(9, VolDebugLevel, stdout, "Probing is now %s", probingon?"ON":"OFF");
    }
    else if (probingon) {
	probingon = 0;
	timingfile = fopen(TIMINGFILE, "w");
	fprintf(timingfile, "Processing tpinfo and FileresTPinfo \n");
	/* process the buffer */
	if (tpinfo) {
	    tpinfo->postprocess(timingfile);
	    delete tpinfo;
	    tpinfo = 0;
	}
	if (FileresTPinfo) {
	    FileresTPinfo->postprocess(timingfile);
	    delete FileresTPinfo;
	    FileresTPinfo = 0;
	}
	fprintf(timingfile, "Finished processing tpinfo and FileresTPinfo \n");
	fclose(timingfile);

	/* set up SE_Descriptor for transfer */
	bzero((void *)&sed, sizeof(sed));
	sed.Tag = SMARTFTP;
	sed.Value.SmartFTPD.Tag = FILEBYNAME;
	sed.Value.SmartFTPD.TransmissionDirection = SERVERTOCLIENT;
	strcpy(sed.Value.SmartFTPD.FileInfo.ByName.LocalFileName, TIMINGFILE);
	sed.Value.SmartFTPD.FileInfo.ByName.ProtectionBits = 0755;

	if ((rc = RPC2_InitSideEffect(rpcid, &sed)) <= RPC2_ELIMIT) {
	    LogMsg(0, VolDebugLevel, stdout, "VolTiming: InitSideEffect failed with %s", 
		RPC2_ErrorMsg(rc));
	    VDisconnectFS();
	    return(-1);
	}

	if ((rc = RPC2_CheckSideEffect(rpcid, &sed, SE_AWAITLOCALSTATUS)) <=
	    RPC2_ELIMIT) {
	    LogMsg(0, VolDebugLevel, stdout, "VolTiming: CheckSideEffect failed with %s", 
		RPC2_ErrorMsg(rc));
	    VDisconnectFS();
	    return(-1);
	}
    }
    VDisconnectFS();
    return(0);
}
	
	
