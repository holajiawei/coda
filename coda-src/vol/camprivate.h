/* BLURB gpl

                           Coda File System
                              Release 6

          Copyright (c) 1987-2003 Carnegie Mellon University
                  Additional copyrights listed below

This  code  is  distributed "AS IS" without warranty of any kind under
the terms of the GNU General Public Licence Version 2, as shown in the
file  LICENSE.  The  technical and financial  contributors to Coda are
listed in the file CREDITS.

                        Additional copyrights
                           none currently

#*/

#ifndef _CAMPRIVATE_H_
#define _CAMPRIVATE_H_ 1
#include "rec_smolist.h"
/* Declarations private to the RVM part of the volume package */

/*
   VolumeData: Volume meta data in RVM
*/

/* Used to be part of struct VolumeHeader, now private to camelot storage */
struct VolumeData {
    VolumeDiskData *volumeInfo; /* pointer to VolumeDiskData */
    rec_smolist *smallVnodeLists; /* pointer to array of Vnode list ptrs */
    bit32 nsmallvnodes; /* number of alloced vnodes */
    bit32 nsmallLists; /* number of vnode lists */
    rec_smolist *largeVnodeLists; /* pointer to array of Vnode list ptrs */
    bit32 nlargevnodes; /* number of alloced vnodes */
    bit32 nlargeLists; /* number of vnode lists */
    bit32 reserved[10]; /* If you add fields, add them before
    				   here and reduce the size of this array */
};

/* Top level of camelot recoverable storage structures */
struct VolHead {
    struct VolumeHeader header;
    struct VolumeData data;
};

#endif /* _CAMPRIVATE_H_ */
