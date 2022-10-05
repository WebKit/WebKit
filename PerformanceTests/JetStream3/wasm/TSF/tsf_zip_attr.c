/*
 * Copyright (C) 2004, 2005 Filip Pizlo. All rights reserved.
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

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

#include <string.h>

#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif

void tsf_zip_wtr_attr_get_default(tsf_zip_wtr_attr_t *attr) {
    memset(attr,0,sizeof(tsf_zip_wtr_attr_t));
    attr->mode=TSF_ZIP_NONE;
}

tsf_bool_t tsf_zip_wtr_attr_get_zlib_default(tsf_zip_wtr_attr_t *attr) {
    if (attr!=NULL) {
        memset(attr,0,sizeof(tsf_zip_wtr_attr_t));
    }
#ifdef HAVE_ZLIB
    if (attr!=NULL) {
        attr->mode=TSF_ZIP_ZLIB;
        attr->buf_size=65536;
        attr->u.z.advanced_init=tsf_false;
        attr->u.z.level=Z_DEFAULT_COMPRESSION;
        attr->u.z.method=Z_DEFLATED;
        attr->u.z.windowBits=15;
        attr->u.z.memLevel=8;
        attr->u.z.strategy=Z_DEFAULT_STRATEGY;
    }
    return tsf_true;
#else
    tsf_set_error(TSF_E_NOT_SUPPORTED,
                  "zlib support is not configured.");
    return tsf_false;
#endif
}

tsf_bool_t tsf_zip_wtr_attr_get_bzip2_default(tsf_zip_wtr_attr_t *attr) {
    if (attr!=NULL) {
        memset(attr,0,sizeof(tsf_zip_wtr_attr_t));
    }
#ifdef HAVE_BZIP2
    if (attr!=NULL) {
        attr->mode=TSF_ZIP_BZIP2;
        attr->buf_size=65536;
        attr->u.b.blockSize100k=9;
        attr->u.b.verbosity=0;
        attr->u.b.workFactor=0;
    }
    return tsf_true;
#else
    tsf_set_error(TSF_E_NOT_SUPPORTED,
                  "libbzip2 support is not configured.");
    return tsf_false;
#endif
}

tsf_bool_t tsf_zip_wtr_attr_get_for_mode(tsf_zip_wtr_attr_t *attr,
					 tsf_zip_mode_t mode) {
    switch (mode) {
    case TSF_ZIP_NONE:
	tsf_zip_wtr_attr_get_default(attr);
	break;
    case TSF_ZIP_ZLIB:
	tsf_zip_wtr_attr_get_zlib_default(attr);
	break;
    case TSF_ZIP_BZIP2:
	tsf_zip_wtr_attr_get_bzip2_default(attr);
	break;
    default:
	tsf_set_error(TSF_E_INVALID_ARG,
		      "Invalid mode in tsf_zpi_wtr_attr_get_for_mode(): %d",
		      mode);
	return tsf_false;
    }
    return tsf_true;
}

void tsf_zip_rdr_attr_get_default(tsf_zip_rdr_attr_t *attr) {
#ifdef HAVE_ZLIB
    attr->z.allow=tsf_true;
#else
    attr->z.allow=tsf_false;
#endif
    attr->z.buf_size=65536;
    attr->z.advanced_init=tsf_false;
    attr->z.windowBits=15;

#ifdef HAVE_BZIP2
    attr->b.allow=tsf_true;
#else
    attr->b.allow=tsf_false;
#endif
    attr->b.buf_size=65536;
    attr->b.verbosity=0;
    attr->b.small=0;
}

void tsf_zip_rdr_attr_get_nozip(tsf_zip_rdr_attr_t *attr) {
    tsf_zip_rdr_attr_get_default(attr);
    attr->z.allow=tsf_false;
    attr->b.allow=tsf_false;
}

tsf_bool_t tsf_zip_rdr_attr_is_nozip(tsf_zip_rdr_attr_t *attr) {
    return !attr->z.allow && !attr->b.allow;
}




