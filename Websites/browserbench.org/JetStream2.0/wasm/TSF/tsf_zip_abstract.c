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
#include "tsf_zip_abstract.h"
#include "tsf_format.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

tsf_zip_result_t tsf_zip_abstract_deflate(tsf_zip_mode_t mode,
                                          void *stream,
                                          tsf_zip_abstract_t *abstract,
                                          tsf_zip_action_t action) {
    int res,zact;
#ifdef HAVE_ZLIB
    z_stream *zstream;
#endif
#ifdef HAVE_BZIP2
    bz_stream *bzstream;
#endif

    switch (mode) {
#ifdef HAVE_ZLIB
        case TSF_ZIP_ZLIB:
            zstream=stream;
            
            zstream->next_in=(Bytef*)abstract->next_in;
            zstream->avail_in=(uInt)abstract->avail_in;
            zstream->next_out=(Bytef*)abstract->next_out;
            zstream->avail_out=(uInt)abstract->avail_out;
            
            switch (action) {
                case TSF_ZIP_ACT_RUN:        zact=Z_NO_FLUSH; break;
                case TSF_ZIP_ACT_SYNC_FLUSH: zact=Z_SYNC_FLUSH; break;
                case TSF_ZIP_ACT_FULL_FLUSH: zact=Z_FULL_FLUSH; break;
                case TSF_ZIP_ACT_FINISH:     zact=Z_FINISH; break;
                default: tsf_abort("bad action value"); break;
            }
            
            res=deflate(zstream,zact);
            
            abstract->next_in=(uint8_t*)zstream->next_in;
            abstract->avail_in=(uint32_t)zstream->avail_in;
            abstract->next_out=(uint8_t*)zstream->next_out;
            abstract->avail_out=(uint32_t)zstream->avail_out;
            
            switch (res) {
                case Z_OK: return TSF_ZIP_RES_OK;
                case Z_STREAM_END: return TSF_ZIP_RES_END;
                default:
                    tsf_set_zlib_error(res,zstream->msg,NULL);
                    return TSF_ZIP_RES_ERROR;
            }
#endif
#ifdef HAVE_BZIP2
        case TSF_ZIP_BZIP2:
            bzstream=stream;
            
            bzstream->next_in=(char*)abstract->next_in;
            bzstream->avail_in=(unsigned int)abstract->avail_in;
            bzstream->next_out=(char*)abstract->next_out;
            bzstream->avail_out=(unsigned int)abstract->avail_out;
            
            switch (action) {
                case TSF_ZIP_ACT_RUN:        zact=BZ_RUN; break;
                case TSF_ZIP_ACT_SYNC_FLUSH:
                case TSF_ZIP_ACT_FULL_FLUSH: zact=BZ_FLUSH; break;
                case TSF_ZIP_ACT_FINISH:     zact=BZ_FINISH; break;
                default: tsf_abort("bad action value"); break;
            }
            
            res=BZ2_bzCompress(bzstream,zact);
            
            abstract->next_in=(uint8_t*)bzstream->next_in;
            abstract->avail_in=(uint32_t)bzstream->avail_in;
            abstract->next_out=(uint8_t*)bzstream->next_out;
            abstract->avail_out=(uint32_t)bzstream->avail_out;
            
            switch (res) {
                case BZ_OK:
                case BZ_RUN_OK:
                case BZ_FLUSH_OK:
                case BZ_FINISH_OK:
                    return TSF_ZIP_RES_OK;
                case BZ_STREAM_END:
                    return TSF_ZIP_RES_END;
                default:
                    tsf_set_libbzip2_error(res,NULL);
                    return TSF_ZIP_RES_ERROR;
            }
#endif
        default: break;
    }
    
    tsf_set_error(TSF_E_INTERNAL,
                  "Bad mode value in tsf_zip_abstract_deflate()");
    
    return TSF_ZIP_RES_ERROR;
}

tsf_zip_result_t tsf_zip_abstract_inflate(tsf_zip_mode_t mode,
                                          void *stream,
                                          tsf_zip_abstract_t *abstract,
                                          tsf_zip_action_t action) {
    int res,zact;
#ifdef HAVE_ZLIB
    z_stream *zstream;
#endif
#ifdef HAVE_BZIP2
    bz_stream *bzstream;
#endif

    switch (mode) {
#ifdef HAVE_ZLIB
        case TSF_ZIP_ZLIB:
            zstream=stream;
            
            zstream->next_in=(Bytef*)abstract->next_in;
            zstream->avail_in=(uInt)abstract->avail_in;
            zstream->next_out=(Bytef*)abstract->next_out;
            zstream->avail_out=(uInt)abstract->avail_out;
            
            switch (action) {
                case TSF_ZIP_ACT_RUN:
                case TSF_ZIP_ACT_SYNC_FLUSH:
                case TSF_ZIP_ACT_FULL_FLUSH: zact=Z_SYNC_FLUSH; break;
                case TSF_ZIP_ACT_FINISH:     zact=Z_FINISH; break;
                default: tsf_abort("bad action value"); break;
            }
            
            res=inflate(zstream,zact);
            
            abstract->next_in=(uint8_t*)zstream->next_in;
            abstract->avail_in=(uint32_t)zstream->avail_in;
            abstract->next_out=(uint8_t*)zstream->next_out;
            abstract->avail_out=(uint32_t)zstream->avail_out;
            
            switch (res) {
                case Z_OK: return TSF_ZIP_RES_OK;
                case Z_STREAM_END: return TSF_ZIP_RES_END;
                default:
                    tsf_set_zlib_error(res,zstream->msg,NULL);
                    return TSF_ZIP_RES_ERROR;
            }
#endif
#ifdef HAVE_BZIP2
        case TSF_ZIP_BZIP2:
            bzstream=stream;
            
            bzstream->next_in=(char*)abstract->next_in;
            bzstream->avail_in=(unsigned int)abstract->avail_in;
            bzstream->next_out=(char*)abstract->next_out;
            bzstream->avail_out=(unsigned int)abstract->avail_out;
            
            res=BZ2_bzDecompress(bzstream);
            
            abstract->next_in=(uint8_t*)bzstream->next_in;
            abstract->avail_in=(uint32_t)bzstream->avail_in;
            abstract->next_out=(uint8_t*)bzstream->next_out;
            abstract->avail_out=(uint32_t)bzstream->avail_out;
            
            switch (res) {
                case BZ_OK:
                case BZ_RUN_OK:
                case BZ_FLUSH_OK:
                case BZ_FINISH_OK:
                    return TSF_ZIP_RES_OK;
                case BZ_STREAM_END:
                    return TSF_ZIP_RES_END;
                default:
                    tsf_set_libbzip2_error(res,NULL);
                    return TSF_ZIP_RES_ERROR;
            }
#endif
        default: break;
    }
    
    tsf_set_error(TSF_E_INTERNAL,
                  "Bad mode value in tsf_zip_abstract_inflate()");
    
    return TSF_ZIP_RES_ERROR;
}



