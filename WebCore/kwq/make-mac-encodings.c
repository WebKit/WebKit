/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <CoreFoundation/CoreFoundation.h>

static void
usage(const char *program)
{
  printf("Usage: %s OUTFILE\n", program);
  exit(1);
}

int
main (int argc, char **argv)
{
  const CFStringEncoding *all_encodings;
  const CFStringEncoding *p;
  CFStringRef name;
  char cname[2048];
  FILE *output;

  if (argc != 2) {
    usage(argv[0]);
  }
  
  output = fopen (argv[1], "w");

  if (output == NULL) {
    printf("Cannot open file \"%s\"\n", argv[1]);
    exit(1);
  }

  all_encodings = CFStringGetListOfAvailableEncodings();

  for (p = all_encodings; *p != kCFStringEncodingInvalidId; p++) {
    name = CFStringConvertEncodingToIANACharSetName(*p);
    /* All IANA encoding names must be US-ASCII */
    if (name != NULL) {
      CFStringGetCString(name, cname, 2048, kCFStringEncodingASCII);
      fprintf(output, "%ld:%s\n", *p, cname);
    } else {
      switch (*p) {
        case 41:
        case kCFStringEncodingShiftJIS_X0213_00:
        case kCFStringEncodingGB_18030_2000:
        case 0xBFF:
          break;
        default:
          printf("Warning: encoding %ld does not have an IANA chararacter set name\n", *p);
      }
    }
  }
  return 0;
}
