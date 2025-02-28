/* Copyright (c) 2010-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

/* Copyright (C) 1991, 1992 Free Software Foundation, Inc.
   This file was then part of the GNU C Library. */

#ifndef SIMGRID_MMALLOC_H
#define SIMGRID_MMALLOC_H

#include "src/internal_config.h"

/** Environment variable name used to pass the communication socket.
 *
 * It is set by `simgrid-mc` to enable MC support in the children processes.
 *
 * It is placed in this file so that it's visible from mmalloc and MC without sharing anythin of xbt in mmalloc
 */
#define MC_ENV_SOCKET_FD "SIMGRID_MC_SOCKET_FD"

#include <stdio.h>     /* for NULL */
#include <sys/types.h> /* for size_t */

SG_BEGIN_DECL

/* Datatype representing a separate heap. The whole point of the mmalloc module is to allow several such heaps in the
 * process. It thus works by redefining all the classical memory management functions (malloc and friends) with an
 * extra first argument: the heap in which the memory is to be taken.
 *
 * The heap structure itself is an opaque object that shouldn't be messed with.
 */
typedef struct mdesc s_xbt_mheap_t;
typedef s_xbt_mheap_t* xbt_mheap_t;

#if HAVE_MMALLOC
/* Allocate SIZE bytes of memory (and memset it to 0).  */
XBT_PUBLIC void* mmalloc(xbt_mheap_t md, size_t size);

/* Allocate SIZE bytes of memory (and don't mess with it) */
void* mmalloc_no_memset(xbt_mheap_t mdp, size_t size);

/* Re-allocate the previously allocated block in void*, making the new block SIZE bytes long.  */
XBT_PUBLIC void* mrealloc(xbt_mheap_t md, void* ptr, size_t size);

/* Free a block allocated by `mmalloc', `mrealloc' or `mcalloc'.  */
XBT_PUBLIC void mfree(xbt_mheap_t md, void* ptr);

#define XBT_MHEAP_OPTION_MEMSET 1

XBT_PUBLIC xbt_mheap_t xbt_mheap_new(void* baseaddr, int options);

XBT_PUBLIC void* xbt_mheap_destroy(xbt_mheap_t md);

/* To get the heap used when using the legacy version malloc/free/realloc and such */
xbt_mheap_t mmalloc_get_current_heap(void);

/* Returns true if we are using the internal mmalloc, and false if we are using the libc's malloc */
XBT_PUBLIC int malloc_use_mmalloc(void);

#endif
SG_END_DECL

#endif /* SIMGRID_MMALLOC_H */
