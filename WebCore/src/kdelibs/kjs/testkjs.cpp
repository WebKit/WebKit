/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 */

#include <stdio.h>

#include "kjs.h"

#include "object.h"
#include "types.h"
#include "internal.h"

int main(int argc, char **argv)
{
  // expecting a filename
  if (argc < 2) {
    fprintf(stderr, "You have to specify at least one filename\n");
    return -1;
  }

  // create interpreter
  KJScript *kjs = new KJScript();

  // add debug() function
  kjs->enableDebug();

  const int BufferSize = 100000;
  char code[BufferSize];

  bool ret = true;
  for (int i = 1; i < argc; i++) {
    const char *file = argv[i];
    FILE *f = fopen(file, "r");
    if (!f) {
      fprintf(stderr, "Error opening %s.\n", file);
      return -1;
    }
    int num = fread(code, 1, BufferSize, f);
    code[num] = '\0';
    if(num >= BufferSize)
      fprintf(stderr, "Warning: File may have been too long.\n");

    // run
    ret = ret && kjs->evaluate(code);
    if (kjs->errorType() != 0)
      printf("%s returned: %s\n", file, kjs->errorMsg());
    else if (kjs->returnValue())
      printf("returned a value\n");

    fclose(f);
  }

  delete kjs;

#ifdef KJS_DEBUG_MEM
  printf("List::count = %d\n", KJS::List::count);
  assert(KJS::List::count == 0);
  printf("Imp::count = %d\n", KJS::Imp::count);
  assert(KJS::Imp::count == 0);
#endif

  fprintf(stderr, "OK.\n");
  return ret;
}
