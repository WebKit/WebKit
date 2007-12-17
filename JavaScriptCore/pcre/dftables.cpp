/* This is JavaScriptCore's variant of the PCRE library. While this library
started out as a copy of PCRE, many of the features of PCRE have been
removed. This library now supports only the regular expression features
required by the JavaScript language specification, and has only the functions
needed by JavaScriptCore and the rest of WebKit.

                 Originally written by Philip Hazel
           Copyright (c) 1997-2006 University of Cambridge
    Copyright (C) 2002, 2004, 2006, 2007 Apple Inc. All rights reserved.

-----------------------------------------------------------------------------
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

    * Neither the name of the University of Cambridge nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
-----------------------------------------------------------------------------
*/

/* This is a freestanding support program to generate a file containing
character tables. The tables are built according to the default C
locale. */

#define DFTABLES

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "pcre_internal.h"

int main(int argc, char **argv)
{
if (argc != 2)
  {
  fprintf(stderr, "dftables: one filename argument is required\n");
  return 1;
  }

FILE* f = fopen(argv[1], "wb");
if (f == NULL)
  {
  fprintf(stderr, "dftables: failed to open %s for writing\n", argv[1]);
  return 1;
  }

int i;

fprintf(f,
  "/*************************************************\n"
  "*      Perl-Compatible Regular Expressions       *\n"
  "*************************************************/\n\n"
  "/* This file is automatically written by the dftables auxiliary \n"
  "program. If you edit it by hand, you might like to edit the Makefile to \n"
  "prevent its ever being regenerated.\n\n");
fprintf(f,
  "This file contains the default tables for characters with codes less than\n"
  "128 (ASCII characters). These tables are used when no external tables are\n"
  "passed to PCRE. */\n\n"
  "const unsigned char kjs_pcre_default_tables[%d] = {\n\n"
  "/* This table is a lower casing table. */\n\n", tables_length);

if (lcc_offset != 0)
  abort();

fprintf(f, "  ");
for (i = 0; i < 128; i++)
  {
  if ((i & 7) == 0 && i != 0) fprintf(f, "\n  ");
  fprintf(f, "0x%02X", tolower(i));
  if (i != 127) fprintf(f, ", ");
  }
fprintf(f, ",\n\n");

fprintf(f, "/* This table is a case flipping table. */\n\n");

if (fcc_offset != 128)
  abort();

fprintf(f, "  ");
for (i = 0; i < 128; i++)
  {
  if ((i & 7) == 0 && i != 0) fprintf(f, "\n  ");
  fprintf(f, "0x%02X", islower(i) ? toupper(i) : tolower(i));
  if (i != 127) fprintf(f, ", ");
  }
fprintf(f, ",\n\n");

fprintf(f,
  "/* This table contains bit maps for various character classes.\n"
  "Each map is 32 bytes long and the bits run from the least\n"
  "significant end of each byte. The classes are: space, digit, word. */\n\n");

if (cbits_offset != fcc_offset + 128)
  abort();

unsigned char cbit_table[cbit_length];
memset(cbit_table, 0, cbit_length);
for (i = '0'; i <= '9'; i++)
  cbit_table[cbit_digit + i / 8] |= 1 << (i & 7);
cbit_table[cbit_word + '_' / 8] |= 1 << ('_' & 7);
for (i = 0; i < 128; i++)
  {
  if (isalnum(i)) cbit_table[cbit_word + i/8] |= 1 << (i & 7);
  if (isspace(i)) cbit_table[cbit_space + i/8] |= 1 << (i & 7);
  }

fprintf(f, "  ");
for (i = 0; i < cbit_length; i++)
  {
  if ((i & 7) == 0 && i != 0)
    {
    if ((i & 31) == 0) fprintf(f, "\n");
    fprintf(f, "\n  ");
    }
  fprintf(f, "0x%02X", cbit_table[i]);
  if (i != cbit_length - 1) fprintf(f, ", ");
  }
fprintf(f, ",\n\n");

fprintf(f,
  "/* This table identifies various classes of character by individual bits:\n"
  "  0x%02x   white space character\n"
  "  0x%02x   hexadecimal digit\n"
  "  0x%02x   alphanumeric or '_'\n*/\n\n",
  ctype_space, ctype_xdigit, ctype_word);

if (ctypes_offset != cbits_offset + cbit_length)
    abort();

fprintf(f, "  ");
for (i = 0; i < 128; i++)
  {
  int x = 0;
  if (isspace(i)) x += ctype_space;
  if (isxdigit(i)) x += ctype_xdigit;
  if (isalnum(i) || i == '_') x += ctype_word;
  fprintf(f, "0x%02X", x);
  if (i != 127)
    fprintf(f, ", ");
  else
    fprintf(f, "};");
  if ((i & 7) == 7)
    {
    fprintf(f, " /* ");
    if (isprint(i - 7)) fprintf(f, " %c -", i - 7);
      else fprintf(f, "%3d-", i - 7);
    if (isprint(i)) fprintf(f, " %c ", i);
      else fprintf(f, "%3d", i);
    fprintf(f, " */\n");
    if (i != 127)
      fprintf(f, "  ");
    }
  }

if (tables_length != ctypes_offset + 128)
    abort();

fprintf(f, "\n\n/* End of chartables.c */\n");

fclose(f);
return 0;
}
