/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2018 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

/*
 *
 *    Specification of the Venus Recoverable Storage manager.
 *
 */

#ifndef _VENUS_RECOV_H_
#define _VENUS_RECOV_H_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <rpc2/rpc2.h>
#include <rvmlib.h>

#ifdef __cplusplus
}
#endif

/* from venus */
#include "venus.private.h"

/* Forward declarations. */
struct RecovVenusGlobals;
class fsdb;
class vdb;
class RealmDB;
class hdb;

/*  *****  Constants  *****  */

const int UNSET_IMD = 0; /* do not initialize meta data */
#define DFLT_RVMT UFS /* meta data store type */
const unsigned long DFLT_VDDS  = 0x400000; /* Venus meta-data device size */
const unsigned long UNSET_VDDS = (unsigned long)-1;
const unsigned long MIN_VDDS   = 0x080000;
const int DataToLogSizeRatio   = 4;
const unsigned long DFLT_VLDS =
    DFLT_VDDS / DataToLogSizeRatio; /* Venus log device size */
const unsigned long UNSET_VLDS  = (unsigned long)-1;
const unsigned long MIN_VLDS    = MIN_VDDS / DataToLogSizeRatio;
const int DFLT_RDSCS            = 64; /* RDS chunk size */
const int UNSET_RDSCS           = -1;
const int DFLT_RDSNL            = 16; /* RDS nlists */
const int UNSET_RDSNL           = -1;
const int DFLT_CMFP             = 600; /* Connected-Mode Flush Period */
const int UNSET_CMFP            = -1;
const int DFLT_DMFP             = 30; /* Disconnected-Mode Flush Period */
const int UNSET_DMFP            = -1;
const int DFLT_MAXFP            = 3600; /* Maximum Flush Period */
const int UNSET_MAXFP           = -1;
const int DFLT_WITT             = 60; /* Worker-Idle time threshold */
const int UNSET_WITT            = -1;
const unsigned long DFLT_MAXFS  = 64 * 1024; /* Maximum Flush-Buffer Size */
const unsigned long UNSET_MAXFS = (unsigned long)-1;
const unsigned long DFLT_MAXTS  = 256 * 1024; /* Maximum Truncate Size */
const unsigned long UNSET_MAXTS = (unsigned long)-1;

const int RecovMagicNumber   = 0x8675309;
const int RecovVersionNumber = 41; /* Update this when format changes. */

/*  *****  Types  *****  */
/* local-repair modification */
struct RecovVenusGlobals {
    int recov_MagicNumber; /* Sanity check */
    int recov_VersionNumber;
    time_t recov_LastInit; /* last initialization time */

    int recov_CleanShutDown;

    fsdb *recov_FSDB; /* FSO database */
    vdb *recov_VDB; /* Volume database */
    RealmDB *recov_REALMDB; /* Realm database */
    hdb *recov_HDB; /* Hoard database */

    char *recov_HeapAddr; /* Base of recoverable heap */
    unsigned int recov_HeapLength; /* Length of recoverable heap (in bytes) */

    /* We need to have a identifier that is guaranteed to be identical across
     * crashes and reboots, but unique with respect to all other venii (that
     * do stores to the same server/volume), _and_ venus reinitializations. So
     * we cannot use a timestamp, or the local ip/ether-address. This calls
     * for a UUID, but using that will require modifications to the RPC2
     * protocol. So for now a random integer is used. */
#define VenusGenID (rvg->recov_UUID.fields.time_low)

    /* This UUID should be stored in network byte order.
     * "draft-leach-uuids-guids-01.txt". */
    union {
        unsigned char bytes[16];
        struct {
            unsigned int time_low;
            unsigned short time_mid;
            unsigned short time_hi_version;
            unsigned char clock_seq_hi_variant;
            unsigned char clock_seq_low;
            unsigned char node[6];
        } fields;
    } recov_UUID;
    unsigned int recov_StoreId;

    int validate();
    void print();
    void print(FILE *);
    void print(int);
};

/*  *****  Variables  *****  */

extern int RecovInited;
extern RecovVenusGlobals *rvg;
extern int TransCount;
extern float TransElapsed;
extern int RecovTimeToFlush;
extern int MapPrivate;

extern int InitMetaData, InitNewInstance;
extern rvm_type_t RvmType;
extern const char *VenusLogDevice;
extern unsigned long VenusLogDeviceSize;
extern const char *VenusDataDevice;
extern unsigned long VenusDataDeviceSize;
extern int RdsChunkSize;
extern int RdsNlists;
extern int CMFP;
extern int DMFP;
extern int WITT;
extern int MAXFP;
extern unsigned long MAXFS;
extern unsigned long MAXTS;

/*  *****  Functions  *****  */

#define Recov_BeginTrans() _Recov_BeginTrans(__FILE__, __LINE__)
void _Recov_BeginTrans(const char file[], int line);
void Recov_EndTrans(int);
void Recov_SetBound(int);
void RecovInit();
void RecovFlush(int = 0); /* XXX - parameter is now redundant! */
void RecovTruncate(int = 0); /* XXX - parameter is now redundant! */
void RecovTerminate();
void RecovPrint(int);
RPC2_String Copy_RPC2_String(RPC2_String &);
void Free_RPC2_String(RPC2_String &);
void RECOVD_Init(void);
void RecovDaemon(void);

void Recov_GenerateStoreId(ViceStoreId *sid);

#define VALID_REC_PTR(rec_ptr)                   \
    ((char *)(rec_ptr) >= rvg->recov_HeapAddr && \
     (char *)(rec_ptr) < rvg->recov_HeapAddr + rvg->recov_HeapLength)

#endif /* _VENUS_RECOV_H_ */
