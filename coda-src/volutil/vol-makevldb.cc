/* BLURB gpl

                           Coda File System
                              Release 5

          Copyright (c) 1987-1999 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights

#*/

/*
                         IBM COPYRIGHT NOTICE

                          Copyright (C) 1986
             International Business Machines Corporation
                         All Rights Reserved

This  file  contains  some  code identical to or derived from the 1986
version of the Andrew File System ("AFS"), which is owned by  the  IBM
Corporation.   This  code is provided "AS IS" and IBM does not warrant
that it is free of infringement of  any  intellectual  rights  of  any
third  party.    IBM  disclaims  liability of any kind for any damages
whatsoever resulting directly or indirectly from use of this  software
or  of  any  derivative work.  Carnegie Mellon University has obtained
permission to  modify,  distribute and sublicense this code,  which is
based on Version 2  of  AFS  and  does  not  contain  the features and
enhancements that are part of  Version 3 of  AFS.  Version 3 of AFS is
commercially   available   and  supported  by   Transarc  Corporation,
Pittsburgh, PA.

*/

#ifdef __cplusplus
extern "C" {
#endif __cplusplus

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#include <sys/param.h>
#include <sys/file.h>
#include <netdb.h>
#include <netinet/in.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include "coda_string.h"

#include <lwp/lwp.h>
#include <lwp/lock.h>
#include <volutil.h>
#include <util.h>
#ifdef __cplusplus
}
#endif __cplusplus

#include <voltypes.h>
#include <voldefs.h>
#include <vldb.h>
#include <vice.h>
#include <srv.h>
#include <errors.h>
#include <cvnode.h>
#include <volume.h>
#include <vutil.h>

/* ReadWrite volumes:   are stored exactly twice in the data base.
			Once under name of volume, once under number.
			Only one server site will be recorded.
			Latest read-only and backup-volumes corresponding to
			the read-write volume will be recorded in the read
			write entry.

   Backup volumes:	Only the latest backup volume is stored by name
   			in the data base; it is also stored by number as are
			any old backup volumes which may still exist or appear
			to exist.  The corresponding read-write and read-only
			volumes will not be recorded.

   Readonly volumes:	Same as backup volumes.
			
 */

/* static int debug = 0; */

static char *ServerName[256];
static char *args[50]={NULL};
static struct vldb *vldb_array;/* In core copy of vldb */
static long *Dates;	  /* copy date for the volume at this slot in vldb */
static struct vldb **RWindex; /* index of named rw entry */
static int vldbSize;	  /* array size, in elements */
static int vldbHashSize; /* Hash index space (1 to vldbHashSize) */
static int haveEntry = 0;
static int MaxStride;

#define vldbindex(p)	((p) - &vldb_array[0])

#define RWLIST_PATH	Vol_vicefile("vol/RWlist")
#define PART_TEMP     Vol_vicefile("vol/partitions.new")
#define PART_PATH     Vol_vicefile("vol/partitions")

#define ALL_TEMP	Vol_vicefile("vol/AllVolumes.new")
#define ALL_PATH	Vol_vicefile("vol/AllVolumes")

static FILE *BackupList, *RWList, *PartList, *AllList, *volumelist;

static int Pass(char type);
static void InitAddEntry();
static void VolumeEntry(char type, int byname, char *name,
		unsigned long volume, int server, unsigned long readwrite,
		int creationdate, int copydate, int backupdate);
static void AddReadWriteEntry(struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate);
static void AddReadOnlyEntry(struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate);
static void AddBackupEntry(struct vldb *vnew, int byname,
		char *name, unsigned long volume, int server,
		unsigned long readwrite, int creationdate,
		int copydate, int backupdate);
static struct vldb *Lookup(char *key, long *date);
static void Add(struct vldb *vnew, long date);
static void Replace(struct vldb *old, struct vldb *vnew, long date);
static void AddServer(struct vldb *old, struct vldb *vnew);
static void AddAssociate(struct vldb *old, struct vldb *vnew);
static void CheckRWindex(unsigned long volume, char *name);
static void GetPartitionInfo(char **argp, int nargs);
static void GetArgs(char *line, char **args, int *nargs);
static char *AllocString(char *s);
static void GetServerNames();

/* debugging hack: */
char rootname[32];


/*
S_VolMakeVLDB: Rebuild the VLDB from the file listed as parameter
*/
long S_VolMakeVLDB(RPC2_Handle rpcid, RPC2_String formal_infile) 
{
    int nentries;
    struct vldbHeader *head;
    int fd;
    int MaxRO, MaxBK, MaxRW;
    int err = 0;
    ProgramType *pt;
    char cmd[1024];

    /* To keep C++ 2.0 happy */
    char *infile = (char *)formal_infile;

    LogMsg(9, VolDebugLevel, stdout, "Entering S_VolMakeVLDB; infile %s", infile);
    GetServerNames();
    InitAddEntry();

    CODA_ASSERT(LWP_GetRock(FSTAG, (char **)&pt) == LWP_SUCCESS);

    volumelist = fopen(infile, "r");
    if (volumelist == NULL) {
	LogMsg(0, VolDebugLevel, stdout, "S_VolMakeVLDB: unable to open file %s", infile);
	return(VNOVNODE);
    }
    PartList = fopen(PART_TEMP, "w");
    nentries = 2*Pass('P');	/* Count lines & get partition info;
    				   have roughly twice as many data base
				   entries as lines
				   since each is entered under two keys */
    vldbHashSize = nentries+(nentries/2);
    /* allow for overflow of high hash values */
    vldbSize = vldbHashSize + 100;
    vldb_array = (struct vldb *) calloc(vldbSize,  sizeof(struct vldb));
    Dates = (long *) calloc(vldbSize, sizeof (*Dates));
    RWindex = (struct vldb **) calloc(vldbSize, sizeof(*RWindex));

    BackupList = fopen(BACKUPLIST_PATH, "w");
    if (BackupList == NULL) {
	printf("makevldb:  Unable to create %s; aborted\n", BACKUPLIST_PATH);
	return(VNOVNODE);
    }

    RWList = fopen(RWLIST_PATH, "w");
    if (RWList == NULL) {
	printf("makevldb:  Unable to create %s; aborted\n", RWLIST_PATH);
	return(VNOVNODE);
    }

#ifdef __CYGWIN32__
    AllList = fopen(ALL_TEMP, "w");
    if (AllList == NULL) {
	printf("makevldb:  Unable to create %s; aborted\n", ALL_TEMP);
  	return(VFAIL);
    }
#else
    snprintf(cmd, sizeof(cmd), "sort > %s", ALL_TEMP);
    AllList = (FILE *) popen(cmd, "w");
    if (AllList == NULL) {
	printf("makevldb:  Unable to \"run %s\"; aborted\n", cmd);
  	return(VFAIL);
    }
#endif

    Pass('W');	/* Read-write volumes */
    MaxRW = MaxStride;
    Pass('R');	/* Read only (cloned) volumes */
    MaxRO = MaxStride;
    Pass('B');	/* Backup volumes */
    MaxBK = MaxStride;
    head = (struct vldbHeader *) vldb_array;
    head->magic = htonl(VLDB_MAGIC);
    head->hashSize = htonl(vldbHashSize);
    
    fclose(volumelist);
    fclose(BackupList);
    fclose(RWList);

    if (haveEntry == 0) {
	printf("makevldb:  no data base entries found in input file; aborted\n");
	return(VNOVNODE);
    }
    fd = open(VLDB_TEMP, O_TRUNC|O_WRONLY|O_CREAT, 0644);
    if (fd == -1) {
	printf("makevldb:  Unable to create %s; aborted\n", VLDB_TEMP);
	return(VNOVNODE);
    }
    if (write(fd, (char *)vldb_array, vldbSize*sizeof(struct vldb)) !=  vldbSize*(int)sizeof(struct vldb)) {
	perror("makevldb");
	unlink(VLDB_TEMP);
	return(VNOVNODE);
    }
    close(fd);
    if (rename(VLDB_TEMP, VLDB_PATH) == -1) {
	printf("Unable to rename %s to %s; new vldb not created\n", VLDB_TEMP, VLDB_PATH);
	err = 1;
    }
    else
        printf("VLDB created.  Search lengths: RO %d, RW %d, BK %d.\n", MaxRO, MaxRW, MaxBK);

    fclose(PartList);
    if (rename(PART_TEMP, PART_PATH) == -1) {
	printf("Unable to rename %s to %s; new partition list not created\n", PART_TEMP, PART_PATH);
	err = 1;
    }

    fprintf(AllList, "# [Last line of unsorted volume list]\n");
#ifdef __CYGWIN32__
    fclose(AllList);
#else
    pclose(AllList);
#endif
    if (rename(ALL_TEMP, ALL_PATH) == -1) {
	LogMsg(0, VolDebugLevel, stdout, "Unable to rename %s to %s; new volume list not created", ALL_TEMP, ALL_PATH);
	err = 1;
    }
    else
	LogMsg(0, VolDebugLevel, stdout, "%s written", ALL_PATH);

    /* tell fileserver to read in new database */
    VCheckVLDB();

    return(err?VFAIL:RPC2_SUCCESS);
}

static int Pass(char type)
{
    char *name;
    unsigned long volume;
    int server;
    char *partition = "";
    int minquota;
    int maxquota;
    int diskusage;
    unsigned long readwrite;
    int copydate, backupdate, creationdate;
    int volumeusage = 0; /* Some sort of metric for recent volume usage */
    char line[500];
    char idname[20]; 
    int nargs;
    int linenumber = 0;

    LogMsg(29, VolDebugLevel, stdout, "Entering Pass %c", type);
    MaxStride = 0;
    rewind(volumelist);
    while (fgets(line, sizeof(line), volumelist) != NULL) {
	linenumber++;
	if (line[0] == type) {
	    register char **argp = args;
	    GetArgs(line,argp,&nargs);
	    name = &argp[0][1];
	    if (type == 'P')
		GetPartitionInfo(argp, nargs);
	    else {
		while (++argp,--nargs) {
		    switch (**argp) {
			case 'I':
			    sscanf(&(*argp)[1], "%lx", &volume);
			    break;
			case 'H':
			    sscanf(&(*argp)[1], "%x", &server);
			    break;
			case 'P':
			    partition = &(*argp)[1];
			    break;
			case 'm':
			    sscanf(&(*argp)[1], "%x", &minquota);
			    break;
			case 'M':
			    sscanf(&(*argp)[1], "%x", &maxquota);
			    break;
			case 'U':
			    sscanf(&(*argp)[1], "%x", &diskusage);
			    break;
			case 'W':
			    sscanf(&(*argp)[1], "%lx", &readwrite);
			    break;
			case 'D':
			    sscanf(&(*argp)[1], "%x", &copydate);
			    break;
			case 'B':
			    sscanf(&(*argp)[1], "%x", &backupdate);
			    break;
			case 'C':
			    sscanf(&(*argp)[1], "%x", &creationdate);
			    break;
			case 'A':
			    sscanf(&(*argp)[1], "%x", &volumeusage);
			    break;
			default:
			    LogMsg(0, VolDebugLevel, stdout, "Bad input field, line %d, field \"%s\": VolMakeVLDB aborted",
				linenumber, *argp);
			    return(VFAIL);
		    }
		}
		sprintf(idname, "%lu", volume);
		/* debugging hack: */
		sprintf(rootname, "%lu", volume);
		VolumeEntry(type, 0, idname, volume, server, readwrite, creationdate, copydate, backupdate);
		VolumeEntry(type, 1, name, volume, server, readwrite, creationdate, copydate, backupdate);
		fprintf(AllList, "%s %lx %s %s %u %u %u %c %u %u %u\n", name, volume, ServerName[server],
			    partition, diskusage, minquota, maxquota, type, creationdate, copydate, volumeusage);
		if (type == 'B') {
		    register char *dot;
		    if ( (dot = (char *) rindex(name, '.')) )
			*dot = 0;
		    fprintf(BackupList, "%s %lx %s %s\n", name, readwrite, ServerName[server], partition);
		}
		else if (type == 'W') {
		    fprintf(RWList, "%s %lx %s %s %u %u %u\n", name, volume, ServerName[server],
			    partition, diskusage, minquota, maxquota);
		}
	    }
	}
    }
    return linenumber;
}

/*int (*AddEntry[MAXVOLTYPES])();*/
int (*AddEntry[MAXVOLTYPES])(...);

static void InitAddEntry() {
    AddEntry[backupVolume] = (int (*)(...))AddBackupEntry;
    AddEntry[readonlyVolume] = (int (*)(...))AddReadOnlyEntry;
    AddEntry[readwriteVolume] = (int (*)(...))AddReadWriteEntry;
}

static void VolumeEntry(char type, int byname, char *name, unsigned long volume,
		int server, unsigned long readwrite, int creationdate,
		int copydate, int backupdate)
{
    struct vldb vnew;
    haveEntry = 1;
    bzero((char *)&vnew, sizeof(vnew));
    strncpy(vnew.key, name, sizeof(vnew.key)-1);
    vnew.hashNext = 0;
    vnew.volumeType = (type=='B' ? backupVolume : type=='R' ? readonlyVolume : readwriteVolume);
    vnew.nServers = 1;
    vnew.volumeId[readwriteVolume] = htonl(readwrite);
    vnew.volumeId[vnew.volumeType] = htonl(volume);
    vnew.serverNumber[0] = server;
    (*AddEntry[vnew.volumeType])(&vnew, byname, name, volume, server, readwrite, creationdate, copydate, backupdate);
}



static void AddReadWriteEntry(register struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate)
{
    register struct vldb *old;
    long olddate;
    old = Lookup(name, &olddate);
    if (old) {
	if (old->volumeType != readwriteVolume || copydate > olddate)
	    Replace(old, vnew, copydate);
    }
    else {
	Add(vnew, copydate);
    }
    if (byname)
	CheckRWindex(volume, name);
}

static void AddReadOnlyEntry(register struct vldb *vnew, int byname,
			char *name, unsigned long volume, int server,
			unsigned long readwrite, int creationdate,
			int copydate, int backupdate)
{
    long olddate;
    register struct vldb *old;
    int added = 0;
    char rwname[100];
    old = Lookup(name, &olddate);
    if (old) {
	if (old->volumeType == readonlyVolume) {
	    if (creationdate > olddate) {
		Replace(old, vnew, creationdate);
		added = 1;
	    }
	    else if (creationdate == olddate) {
		AddServer(old, vnew);
	    }
	}
    }
    else {
	Add(vnew, creationdate);
	added = 1;
    }
    sprintf(rwname, "%lu", readwrite);
    if (byname && added) {
        register struct vldb *rwp;
	rwp = Lookup(rwname, &olddate);
        if (rwp) {
	    AddAssociate(rwp, vnew);
	    rwp = RWindex[vldbindex(rwp)];
	    if (rwp)
	        AddAssociate(rwp, vnew);
	}
    }
}

static void AddBackupEntry(register struct vldb *vnew, int byname,
		char *name, unsigned long volume, int server,
		unsigned long readwrite, int creationdate,
		int copydate, int backupdate)
{
    long olddate;
    register struct vldb *old;
    char rwname[100];
    int added = 0;
    old = Lookup(name, &olddate);
    if (old) {
	if (old->volumeType == backupVolume) {
	    if (backupdate > olddate) {
		Replace(old, vnew, backupdate);
		added = 1;
	    }
	}
    }
    else {
	Add(vnew, backupdate);
	added = 1;
    }
    sprintf(rwname, "%lu", readwrite);
    if (byname && added) {
        register struct vldb *rwp;
	rwp = Lookup(rwname, &olddate);
        if (rwp) {
	    AddAssociate(rwp, vnew);
	    rwp = RWindex[vldbindex(rwp)];
	    if (rwp)
	        AddAssociate(rwp, vnew);
	}
    }
}

static struct vldb *Lookup(char *key, long *date)
{
    register struct vldb *p;
    int index = HashString(key, vldbHashSize);
    for(p = &vldb_array[index]; p->key[0]; p += p->hashNext) {
	if (strcmp(p->key, key) == 0) {
	    *date = Dates[vldbindex(p)];
	    return p;
	}
	if (p->hashNext == 0)
	    break;
    }
    return 0;
}

static void Add(register struct vldb *vnew, long date)
{
    register struct vldb *p, *prev, *first;
    int index = HashString(vnew->key, vldbHashSize);
    LogMsg(19, VolDebugLevel, stdout, "Adding VLDB Entry for vol with key = %s", vnew->key);
    LogMsg(19, VolDebugLevel, stdout, "Add: index = %d, hashsize = %d", index, vldbHashSize);
    for(first = p = &vldb_array[index]; p->hashNext; p += p->hashNext)
        ;
    prev = p;
    while (p->key[0])
        p++;
    *p = *vnew;
    prev->hashNext = p - prev;
    if (MaxStride < p-first)
        MaxStride = p-first;
    Dates[vldbindex(p)] = date;
    return;
}

static void Replace(register struct vldb *old, register struct vldb *vnew,
							long date)
{
    vnew->hashNext = old->hashNext;
    *old = *vnew;
    Dates[vldbindex(old)] = date;
}

static void AddServer(register struct vldb *old, register struct vldb *vnew)
{
    register int i;
    for (i = 0; i<old->nServers; i++) {
	if (vnew->serverNumber[0] == old->serverNumber[i])
	    return;
    }
    old->serverNumber[old->nServers++] = vnew->serverNumber[0];
}

static void AddAssociate(register struct vldb *old, register struct vldb *vnew)
{
    old->volumeId[vnew->volumeType] = vnew->volumeId[vnew->volumeType];
}

static void CheckRWindex(unsigned long volume, char *name)
{
    long date;
    register struct vldb *idp, *namep;
    char idname[100];
    sprintf(idname, "%lu", volume);
    idp = Lookup(idname, &date);
    namep = Lookup(name, &date);
    if (idp == 0 || namep == 0)
        return;
    if (idp->volumeType != readwriteVolume || namep->volumeType != readwriteVolume)
	return;
    RWindex[vldbindex(idp)] = namep;
}

static void GetPartitionInfo(char **argp, int nargs)
{
    char *partition;
    char *server = 0;
    unsigned int total, free;
    partition = &argp[0][1];
    while (++argp, --nargs) {
	switch (**argp) {
	    case 'H':
		server = &argp[0][1];
		break;
	    case 'T':
	        sscanf(&(*argp)[1], "%x", &total);
		break;
	    case 'F':
	        sscanf(&(*argp)[1], "%x", &free);
		break;
	}
    }
    fprintf(PartList, "%s %s %u %u\n", server, partition, total, free);
}

static void GetArgs(register char *line,register char **args,
						register int *nargs)
{
    *nargs = 0;
    while (*line) {
	register char *last = line;
	while (*line == ' ')
	    line++;
	if (*last == ' ')
	    *last = 0;
	if (!*line)
	    break;
	*args++  = line, (*nargs)++;
	while (*line && *line != ' ')
	    line++;
    }
}

static char *AllocString(char *s)
{
    char *as = (char *) malloc(strlen(s)+1);
    strcpy(as, s);
    return(as);
}

static void GetServerNames() {
    char line[200];
    char *serverList = SERVERLISTPATH;
    FILE *file;
    file = fopen(serverList, "r");
    if (file == NULL) {
	printf("Unable to read file %s; aborted\n", serverList);
	exit(1);
    }
    while (fgets(line, sizeof(line), file) != NULL) {
        char sname[50];
	struct hostent *hostent;
	long sid;
	if (sscanf(line, "%s%ld", sname, &sid) == 2) {
	    if (sid > N_SERVERIDS) {
		printf("Warning: host %s is assigned a bogus server number (%lu) in %s; host ignored\n",
		  sname, sid, serverList);
		continue;
	    }
	    hostent = gethostbyname(sname);
	    if (hostent == NULL)
		printf("Warning: host %s (listed in %s) is not in /etc/hosts\n", sname, serverList);
	    else
		ServerName[sid] = AllocString(hostent->h_name);
	}
    }
    fclose(file);
}

