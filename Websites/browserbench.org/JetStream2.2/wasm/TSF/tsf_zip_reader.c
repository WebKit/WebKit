/*
 * Copyright (C) 2004, 2005, 2015 Filip Pizlo. All rights reserved.
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
#include "tsf_format.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

#define Cz(reader) ((z_stream*)((reader)->stream))
#define Cbz(reader) ((bz_stream*)((reader)->stream))

tsf_zip_rdr_t *tsf_zip_reader_create(tsf_partial_reader_t reader,
                                     void *reader_arg,
                                     tsf_zip_mode_t mode,
                                     const tsf_zip_rdr_attr_t *attr) {
    int res;
    tsf_zip_rdr_t *result=malloc(sizeof(tsf_zip_rdr_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_zip_rdr_t");
        return NULL;
    }
    
    result->decomp_eof=tsf_false;
    
    result->reader=reader;
    result->reader_arg=reader_arg;
    
    result->mode=mode;
    
    switch (mode) {
        case TSF_ZIP_ZLIB:
            result->buf_size=attr->z.buf_size;
            break;
        case TSF_ZIP_BZIP2:
            result->buf_size=attr->b.buf_size;
            break;
        case TSF_ZIP_NONE:
            tsf_set_error(TSF_E_INVALID_ARG,
                          "TSF_ZIP_NONE is not a valid mode for "
                          "tsf_zip_reader_create()");
            goto failure_1;
        default:
            tsf_set_error(TSF_E_INVALID_ARG,
                          "%d is not a valid mode number for "
                          "tsf_zip_reader_create()",mode);
            goto failure_1;
    }
    
    result->buf=malloc(result->buf_size);
    if (result->buf==NULL) {
        tsf_set_errno("Could not malloc buffer");
        goto failure_1;
    }
    
    result->abstract.next_in=result->buf;
    result->abstract.avail_in=0;
    
    switch (mode) {
        case TSF_ZIP_ZLIB:
#ifdef HAVE_ZLIB
            result->stream=malloc(sizeof(z_stream));
            if (result->stream==NULL) {
                tsf_set_errno("Could not malloc z_stream");
                goto failure_2;
            }
            
            Cz(result)->zalloc=NULL;
            Cz(result)->zfree=NULL;
            Cz(result)->opaque=NULL;
            
            if (attr->z.advanced_init) {
                res=inflateInit2(Cz(result),
                                 attr->z.windowBits);
            } else {
                res=inflateInit(Cz(result));
            }
            if (res!=Z_OK) {
                tsf_set_zlib_error(res,Cz(result)->msg,
                    "Trying to initialize inflation in ZLib");
                free(Cz(result));
                goto failure_2;
            }
            break;
#else
            tsf_set_error(TSF_E_NOT_SUPPORTED,
                          "TSF_ZIP_ZLIB is not supported by "
                          "tsf_zip_reader_create() because ZLib support "
                          "was not configured into the TSF library");
            goto failure_2;
#endif
        case TSF_ZIP_BZIP2:
#ifdef HAVE_BZIP2
            result->stream=malloc(sizeof(bz_stream));
            if (result->stream==NULL) {
                tsf_set_errno("Could not malloc bz_stream");
                goto failure_2;
            }
            
            Cbz(result)->bzalloc=NULL;
            Cbz(result)->bzfree=NULL;
            Cbz(result)->opaque=NULL;
            
            res=BZ2_bzDecompressInit(Cbz(result),
                                     attr->b.verbosity,
                                     attr->b.small);
            if (res!=BZ_OK) {
                tsf_set_libbzip2_error(res,
                    "Trying to initialize decompression in libbzip2");
                free(Cbz(result));
                goto failure_2;
            }
            
            break;
#else
            tsf_set_error(TSF_E_NOT_SUPPORTED,
                          "TSF_ZIP_BZIP2 is not supported by "
                          "tsf_zip_reader_create() because libbzip2 "
                          "support was not configured into the TSF "
                          "library");
            goto failure_2;
#endif
        default:
            tsf_abort("should've already caught this case");
            break;
    }
    
    return result;
    
failure_2:
    free(result->buf);
failure_1:
    free(result);
    return NULL;
}

void tsf_zip_reader_destroy(tsf_zip_rdr_t *reader) {
    switch (reader->mode) {
#ifdef HAVE_ZLIB
        case TSF_ZIP_ZLIB:
            inflateEnd(Cz(reader));
            break;
#endif
#ifdef HAVE_BZIP2
        case TSF_ZIP_BZIP2:
            BZ2_bzDecompressEnd(Cbz(reader));
            break;
#endif
        default:
            tsf_abort("unknown zip reader mode");
            break;
    }
    free(reader->stream);
    free(reader->buf);
    free(reader);
}

tsf_bool_t tsf_zip_reader_read(void *_reader,
                               void *data,
                               uint32_t len) {
    tsf_zip_rdr_t *reader = _reader;
    int32_t reader_res;
    tsf_zip_result_t zip_res;
    
    if (len == 0) {
        return tsf_true;
    }
    
    if (reader->decomp_eof) {
        reader_res = reader->reader(reader->reader_arg,
                                    reader->buf, 1);
        tsf_assert(reader_res == -1 || reader_res == 1);
        if (reader_res == 1) {
            tsf_set_error(TSF_E_NO_EOF,
                          "Expected EOF (because the "
                          "decompressor reached its end "
                          "of stream), but did not get "
                          "one (I was still able to read a byte)");
        }
        return tsf_false;
    }
    
    reader->abstract.next_out = data;
    reader->abstract.avail_out = len;
    
    for (;;) {
	/* is there ever a case where we would want to call inflate even though
	   the input buffer is empty? */
        if (reader->abstract.avail_in != 0) {
            zip_res = tsf_zip_abstract_inflate(reader->mode,
                                               reader->stream,
                                               &(reader->abstract),
                                               TSF_ZIP_ACT_RUN);
            
            if (zip_res == TSF_ZIP_RES_ERROR) {
                return tsf_false;
            } else if (zip_res == TSF_ZIP_RES_END) {
                if (reader->abstract.avail_in != 0) {
                    tsf_set_error(TSF_E_NO_EOF,
                                  "Expected EOF (because the "
                                  "decompressor reached its end "
                                  "of stream), but did not get "
                                  "one (there was still data left "
                                  "in the buffer)");
                    return tsf_false;
                }
                reader->decomp_eof = tsf_true;
                if (reader->abstract.avail_out == 0) {
                    break;
                }
                tsf_set_error((void*)reader->abstract.next_out == data ?
                              TSF_E_UNEXPECTED_EOF : TSF_E_ERRONEOUS_EOF,
                              NULL);
                return tsf_false;
            }
            
            if (reader->abstract.avail_out == 0) {
                break;
            }
            
            tsf_assert(reader->abstract.avail_in == 0);
        }
        
        reader_res = reader->reader(reader->reader_arg,
                                    reader->buf,
                                    reader->buf_size);
        
        if (reader_res == -1) {
            if (tsf_get_error_code() == TSF_E_UNEXPECTED_EOF) {
                tsf_set_error(TSF_E_ERRONEOUS_EOF,
                              "Got an EOF from the reader before the "
                              "decompressor had reached end of stream");
            }
            return tsf_false;
        }
        
        reader->abstract.next_in = reader->buf;
        reader->abstract.avail_in = reader_res;
    }
    
    return tsf_true;
}




