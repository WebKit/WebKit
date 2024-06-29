/*
 * Copyright 2024 Simon McVittie
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <sysprof-capture.h>

#undef _NDEBUG
#include <assert.h>

#ifdef __GNUC__
#define UNUSED __attribute__((__unused__))
#else
#define UNUSED
#endif

int
main (UNUSED int   argc,
      UNUSED char *argv[])
{
  assert (sysprof_getpagesize () > 0);
  return 0;
}

