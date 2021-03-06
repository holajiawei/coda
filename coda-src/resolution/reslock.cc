/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2008 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/* reslock.c 
 * Implements volume locking for resolution
 */

#ifdef __cplusplus
extern "C" {
#endif

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include "coda_string.h"

#include <stdio.h>
#include <struct.h>
#include <rpc2/rpc2.h>
#include <rpc2/errors.h>

#include <sys/ioctl.h>
#include <util.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

#include <olist.h>
#include <res.h>
#include <vrdb.h>
#include <srv.h>
#include <volume.h>

#include "lockqueue.h"
#include "rescomm.h"
#include "resutil.h"
#include "timing.h"

long RS_LockAndFetch(RPC2_Handle RPCid, ViceFid *Fid, ResFetchType Request,
                     ViceVersionVector *VV, ResStatus *rstatus,
                     RPC2_Integer *logsize)
{
    int errorcode  = 0;
    Volume *volptr = 0;
    Vnode *vptr    = 0;
    *logsize       = 0;
    int nentries   = 0;

    SLog(1, "Entering RS_LockAndFetch %s\n", FID_(Fid));
    CODA_ASSERT(Request == FetchStatus);

    // first set out parameters
    InitVV(VV);
    *logsize = 0;
    memset((void *)rstatus, 0, sizeof(ResStatus));

    // get info from connection
    conninfo *cip = NULL;
    cip           = GetConnectionInfo(RPCid);
    if (cip == NULL) {
        SLog(0, "RS_LockAndFetch: Couldnt get conn info for %s", FID_(Fid));
        return (EINVAL);
    }

    // translate replicated VolumeId to rw id
    if (!XlateVid(&Fid->Volume)) {
        SLog(0, "RS_LockAndFetch: Couldnt translate Vid for %s", FID_(Fid));
        return EINVAL;
    }

    errorcode =
        GetVolObj(Fid->Volume, &volptr, VOL_EXCL_LOCK, 1, cip->GetRemoteHost());
    if (errorcode) {
        SLog(0, "RS_LockAndFetch: Error %d during GetVolObj for %s", errorcode,
             FID_(Fid));
        return errorcode;
    }

    /* this Vnode is obtained to get at its logs, and then put away */
    /* ignore inconsistencies */
    errorcode = GetFsObj(Fid, &volptr, &vptr, READ_LOCK, NO_LOCK, 1, 0, 0);
    if (errorcode) {
        SLog(0, "RS_LockAndFetch: GetFsObj for %s returned error %d", FID_(Fid),
             errorcode);
        goto FreeLocks;
    }

    // set out parameter
    memcpy(VV, &(Vnode_vv(vptr)), sizeof(ViceVersionVector));
    ObtainResStatus(rstatus, &(vptr->disk));

    /* set log size as the volume log size -
       that is the max log size a client
       can send in the collect logs phase */
    if (AllowResolution && V_RVMResOn(volptr)) {
        // set size to max possible
        // by using rename_rle we hope that all strings will fit
        // in the buffer length being returned.
        nentries = V_VolLog(volptr)->size;
        CODA_ASSERT(nentries > 0);

        // *logsize = nentries * (sizeof(recle) + sizeof(rename_rle));
        *logsize = nentries * 200;
        SLog(39, "RS_LockAndFetch: Returning recov. logsize = %d\n", *logsize);
    }

    /* if probing and timing is on then init the array */
    if (pathtiming && probingon && vptr->disk.type == vDirectory) {
        tpinfo = new timing_path(MAXPROBES);
        PROBE(tpinfo, RESBEGIN);
    }

FreeLocks:
    int filecode = 0;
    if (vptr) {
        VPutVnode((Error *)&filecode, vptr);
        CODA_ASSERT(filecode == 0);
        vptr = 0;
    }
    if (errorcode)
        /* volume locked but error occured later */
        /* release the lock */
        PutVolObj(&volptr, VOL_EXCL_LOCK, 1);
    else
        /* no errors, release the volume but keep the lock */
        PutVolObj(&volptr, NO_LOCK);

    return (errorcode);
}

long RS_UnlockVol(RPC2_Handle RPCid, VolumeId Vid)
{
    Volume *volptr = 0;
    int errorcode  = 0;
    conninfo *cip;
    cip = GetConnectionInfo(RPCid);
    if (cip == NULL) {
        SLog(0, "RS_UnlockVol: Couldnt get conn info");
        return (EINVAL);
    }

    if (!XlateVid(&Vid)) {
        SLog(0, "RS_UnlockVol: Couldnt XlateVid %x", Vid);
        return (EINVAL);
    }

    /* get volume and check if locked */
    errorcode = GetVolObj(Vid, &volptr, VOL_NO_LOCK, 0, 0);
    if (errorcode) {
        SLog(0, "RS_UnlockVol: GetVolObj error %d for %x", errorcode, Vid);
        return (errorcode);
    }

    /* make sure unlocker is locker */
    if (V_VolLock(volptr).IPAddress &&
        V_VolLock(volptr).IPAddress != cip->GetRemoteHost()) {
        SLog(0, "RS_UnlockVol: unlocker != locker for %x", Vid);
        VPutVolume(volptr);
        return (EINVAL); /* define new error codes */
    }
    PutVolObj(&volptr, VOL_EXCL_LOCK, 1);
    SLog(1, "RS_UnlockVol finished successfully for %x", Vid);

    PROBE(tpinfo, RESEND);
    return (0);
}
