/* Initialization for access to a mmap'd malloc managed region. */

/* Copyright (c) 2012-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

/* Copyright 1992, 2000 Free Software Foundation, Inc.

   Contributed by Fred Fish at Cygnus Support.   fnf@cygnus.com

   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If
   not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <sys/types.h>
#include <fcntl.h>              /* After sys/types.h, at least for dpx/2.  */
#include <sys/stat.h>
#include <string.h>
#include "mmprivate.h"

/* Initialize access to a mmalloc managed region.

   The mapping is established starting at the specified address BASEADDR
   in the process address space.

   The provided BASEADDR should be chosen carefully in order to avoid
   bumping into existing mapped regions or future mapped regions.

   On success, returns a "malloc descriptor" which is used in subsequent
   calls to other mmalloc package functions.  It is explicitly "void *"
   so that users of the package don't have to worry about the actual
   implementation details.

   On failure, returns NULL. */

xbt_mheap_t xbt_mheap_new(void* baseaddr, int options)
{
  /* NULL is not a valid baseaddr as we cannot map anything there. C'mon, user. Think! */
  if (baseaddr == NULL)
    return NULL;

  /* We start off with the malloc descriptor allocated on the stack, until we build it up enough to
   * call _mmalloc_mmap_morecore() to allocate the first page of the region and copy it there.  Ensure that it is
   * zero'd and then initialize the fields that we know values for. */

  struct mdesc mtemp;
  xbt_mheap_t mdp = &mtemp;
  memset((char *) mdp, 0, sizeof(mtemp));
  strncpy(mdp->magic, MMALLOC_MAGIC, MMALLOC_MAGIC_SIZE);
  mdp->headersize = sizeof(mtemp);
  mdp->version = MMALLOC_VERSION;
  mdp->base = mdp->breakval = mdp->top = baseaddr;
  mdp->next_mdesc = NULL;
  mdp->options = options;

  /* If we have not been passed a valid open file descriptor for the file
     to map to, then open /dev/zero and use that to map to. */

  /* Now try to map in the first page, copy the malloc descriptor structure there, and arrange to return a pointer to
   * this new copy.  If the mapping fails, then close the file descriptor if it was opened by us, and arrange to return
   * a NULL. */

  void* mbase = mmorecore(mdp, sizeof(mtemp));
  if (mbase == NULL) {
    fprintf(stderr, "morecore failed to get some more memory!\n");
    abort();
  }
  memcpy(mbase, mdp, sizeof(mtemp));

  /* Add the new heap to the linked list of heaps attached by mmalloc */
  if(__mmalloc_default_mdp){
    mdp = __mmalloc_default_mdp;
    while(mdp->next_mdesc)
      mdp = mdp->next_mdesc;

    mdp->next_mdesc = (struct mdesc *)mbase;
  }

  return mbase;
}

/** Terminate access to a mmalloc managed region by unmapping all memory pages associated with the region, and closing
 *  the file descriptor if it is one that we opened.

    Returns NULL on success.

    Returns the malloc descriptor on failure, which can subsequently be used for further action, such as obtaining more
    information about the nature of the failure.

    Note that the malloc descriptor that we are using is currently located in region we are about to unmap, so we first
    make a local copy of it on the stack and use the copy. */

void *xbt_mheap_destroy(xbt_mheap_t mdp)
{
  if (mdp != NULL) {
    /* Remove the heap from the linked list of heaps attached by mmalloc */
    struct mdesc* mdptemp = __mmalloc_default_mdp;
    while(mdptemp->next_mdesc != mdp )
      mdptemp = mdptemp->next_mdesc;

    mdptemp->next_mdesc = mdp->next_mdesc;

    struct mdesc mtemp = *mdp;

    /* Now unmap all the pages associated with this region by asking for a
       negative increment equal to the current size of the region. */

    if (mmorecore(&mtemp, (char *)mtemp.base - (char *)mtemp.breakval) == NULL) {
      /* Deallocating failed.  Update the original malloc descriptor with any changes */
      *mdp = mtemp;
    } else {
      mdp = NULL;
    }
  }

  return mdp;
}

/* Safety gap from the heap's break address.
 * Try to increase this first if you experience strange errors under valgrind. */
#define HEAP_OFFSET   (128UL<<20)

/* Initialize the default malloc descriptor.
 *
 * There is no malloc_postexit() destroying the default mdp, because it would break ldl trying to free its memory
 */
xbt_mheap_t mmalloc_preinit(void)
{
  if (__mmalloc_default_mdp == NULL) {
    if (!mmalloc_pagesize)
      mmalloc_pagesize = getpagesize();
    unsigned long mask    = ~((unsigned long)mmalloc_pagesize - 1);
    void* addr            = (void*)(((unsigned long)sbrk(0) + HEAP_OFFSET) & mask);
    __mmalloc_default_mdp = xbt_mheap_new(addr, XBT_MHEAP_OPTION_MEMSET);
  }
  mmalloc_assert(__mmalloc_default_mdp != NULL, "__mmalloc_default_mdp cannot be NULL");

  return __mmalloc_default_mdp;
}
