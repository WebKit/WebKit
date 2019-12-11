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

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

#define Cz(writer) ((z_stream*)((writer)->stream))
#define Cbz(writer) ((bz_stream*)((writer)->stream))

tsf_zip_wtr_t *tsf_zip_writer_create(tsf_writer_t writer,
                                     void *writer_arg,
                                     const tsf_zip_wtr_attr_t *attr) {
    int res;
    tsf_zip_wtr_t *result=malloc(sizeof(tsf_zip_wtr_t));
    if (result==NULL) {
        tsf_set_errno("Could not malloc tsf_zip_wtr_t");
        return NULL;
    }
    
    result->writer=writer;
    result->writer_arg=writer_arg;
    result->mode=attr->mode;
    
    result->buf_size=attr->buf_size;
    result->buf=malloc(result->buf_size);
    if (result->buf==NULL) {
        tsf_set_errno("Could not malloc buffer");
        goto failure_1;
    }
    
    result->abstract.next_out=result->buf;
    result->abstract.avail_out=result->buf_size;
    
    switch (attr->mode) {
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
            
            if (attr->u.z.advanced_init) {
                res=deflateInit2(Cz(result),
                                 attr->u.z.level,
                                 attr->u.z.method,
                                 attr->u.z.windowBits,
                                 attr->u.z.memLevel,
                                 attr->u.z.strategy);
            } else {
                res=deflateInit(Cz(result),
                                attr->u.z.level);
            }
            if (res!=Z_OK) {
                tsf_set_zlib_error(res,Cz(result)->msg,
                    "Trying to initialize deflation in ZLib");
                free(Cz(result));
                goto failure_2;
            }
            break;
#else
            tsf_set_error(TSF_E_NOT_SUPPORTED,
                          "TSF_ZIP_ZLIB is not supported by "
                          "tsf_zip_writer_create() because ZLib "
                          "support was not configured into the "
                          "TSF library");
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
            
            res=BZ2_bzCompressInit(Cbz(result),
                                   attr->u.b.blockSize100k,
                                   attr->u.b.verbosity,
                                   attr->u.b.workFactor);
            if (res!=BZ_OK) {
                tsf_set_libbzip2_error(res,
                    "Trying to initialize compression in libbzip2");
                free(Cbz(result));
                goto failure_2;
            }
            break;
#else
            tsf_set_error(TSF_E_NOT_SUPPORTED,
                          "TSF_ZIP_BZIP2 is not supported by "
                          "tsf_zip_writer_create() because libbzip2 "
                          "support was not configured into the TSF "
                          "library");
            goto failure_2;
#endif
        case TSF_ZIP_NONE:
            tsf_set_error(TSF_E_INVALID_ARG,
                          "TSF_ZIP_NONE is not a valid mode for "
                          "tsf_zip_writer_create()");
            goto failure_2;
        default:
            tsf_set_error(TSF_E_INVALID_ARG,
                          "%d is not a valid mode number for "
                          "tsf_zip_writer_create()",attr->mode);
            goto failure_2;
    }
    
    return result;
    
failure_2:
    free(result->buf);
failure_1:
    free(result);
    return NULL;
}

void tsf_zip_writer_destroy(tsf_zip_wtr_t *writer) {
    switch (writer->mode) {
#ifdef HAVE_ZLIB
        case TSF_ZIP_ZLIB:
            deflateEnd(Cz(writer));
            break;
#endif
#ifdef HAVE_BZIP2
        case TSF_ZIP_BZIP2:
            BZ2_bzCompressEnd(Cbz(writer));
            break;
#endif
        default:
            tsf_abort("unknown zip writer mode");
            break;
    }
    free(writer->stream);
    free(writer->buf);
    free(writer);
}

tsf_bool_t tsf_zip_writer_write(void *_writer,
                                const void *data,
                                uint32_t len) {
    tsf_zip_wtr_t *writer = _writer;
    tsf_zip_result_t res;
    
    writer->abstract.next_in = (void*)data;
    writer->abstract.avail_in = len;
    
    while (writer->abstract.avail_in > 0) {
        res=tsf_zip_abstract_deflate(writer->mode,
                                     writer->stream,
                                     &(writer->abstract),
                                     TSF_ZIP_ACT_RUN);
        if (res == TSF_ZIP_RES_ERROR) {
            return tsf_false;
        } else if (res == TSF_ZIP_RES_OK) {
            /* parrrtay!! */
        } else {
            tsf_abort("got some weird return code from "
                      "tsf_zip_abstract_deflate()");
        }
        
        if (writer->abstract.avail_out == 0) {
            if (!writer->writer(writer->writer_arg,
                                writer->buf,
                                writer->buf_size)) {
                return tsf_false;
            }
            writer->abstract.next_out = writer->buf;
            writer->abstract.avail_out = writer->buf_size;
        }
    }
    
    return tsf_true;
}

tsf_bool_t tsf_zip_writer_flush(tsf_zip_wtr_t *writer,
                                tsf_bool_t full_flush) {
    tsf_zip_result_t res;
    
    writer->abstract.next_in=NULL;
    writer->abstract.avail_in=0;
    
    for (;;) {
        res=tsf_zip_abstract_deflate(writer->mode,
                                     writer->stream,
                                     &(writer->abstract),
                                     full_flush?
                                     TSF_ZIP_ACT_FULL_FLUSH:
                                     TSF_ZIP_ACT_SYNC_FLUSH);
        if (res==TSF_ZIP_RES_ERROR) {
            return tsf_false;
        } else if (res==TSF_ZIP_RES_OK) {
            /* parrrtay!! */
        } else {
            tsf_abort("got some weird return code from "
                      "tsf_zip_abstract_deflate()");
        }
        
        if (!writer->writer(writer->writer_arg,
                            writer->buf,
                            writer->abstract.next_out-writer->buf)) {
            return tsf_false;
        }
        
        if (writer->abstract.avail_out!=0) {
            break;
        }

        writer->abstract.next_out=writer->buf;
        writer->abstract.avail_out=writer->buf_size;
    }
    
    return tsf_true;
}

tsf_bool_t tsf_zip_writer_finish(tsf_zip_wtr_t *writer) {
    tsf_zip_result_t res;
        
    writer->abstract.next_in=NULL;
    writer->abstract.avail_in=0;
    
    for (;;) {
        res=tsf_zip_abstract_deflate(writer->mode,
                                     writer->stream,
                                     &(writer->abstract),
                                     TSF_ZIP_ACT_FINISH);
        if (res==TSF_ZIP_RES_ERROR) {
            return tsf_false;
        }
        
        if (!writer->writer(writer->writer_arg,
                            writer->buf,
                            writer->abstract.next_out-writer->buf)) {
            return tsf_false;
        }
        writer->abstract.next_out=writer->buf;
        writer->abstract.avail_out=writer->buf_size;
        
        if (res==TSF_ZIP_RES_END) {
            break;
        }
    }
    
    return tsf_true;
}






