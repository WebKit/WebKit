/*
 * Copyright (C) 2003, 2004, 2005 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "tsf_internal.h"

#include <stdio.h>
#include <string.h>

char *tsf_vasprintf(const char *format,va_list lst1) {
    char *ret;
    int res;
#ifdef TSF_HAVE_VASPRINTF
    res=vasprintf(&ret,format,lst1);
    if (res<0 && ret!=NULL) {
        free(ret);
        ret=NULL;
    }
#else
    {
        va_list lst2 = lst1;
        char fake_buf[32];
        res=vsnprintf(fake_buf,sizeof(fake_buf),format,lst1);
        if (res<0) {
            return NULL;
        }
        if (res<32) {
            return strdup(fake_buf);
        }
        ret=malloc(res+1);
        if (ret==NULL) {
            return NULL;
        }
        res=vsnprintf(ret,res+1,format,lst2);
        if (res<0) {
            free(ret);
            ret=NULL;
        }
    }
#endif
    return ret;
}

char *tsf_asprintf(const char *format,...) {
    va_list lst;
    char *ret;
    va_start(lst, format);
    ret=tsf_vasprintf(format, lst);
    va_end(lst);
    return ret;
}

