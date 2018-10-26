/*
 * Copyright (C) 2006, 2011 Filip Pizlo. All rights reserved.
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
#include "tsf_serial_protocol.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

#define Cz(reader) ((z_stream*)((reader)->stream))
#define Cbz(reader) ((bz_stream*)((reader)->stream))

static tsf_bool_t tear_down_mode(tsf_adpt_rdr_t *reader,
				 uint8_t **spillover,
				 uint32_t *spillover_len) {
    tsf_bool_t stillok=tsf_true;
    if (reader->mode==TSF_ZIP_ZLIB) {
#ifdef HAVE_ZLIB
	inflateEnd(Cz(reader));
#endif
    } else if (reader->mode==TSF_ZIP_BZIP2) {
#ifdef HAVE_BZIP2
	BZ2_bzDecompressEnd(Cbz(reader));
#endif
    }
    if (reader->mode==TSF_ZIP_ZLIB ||
	reader->mode==TSF_ZIP_BZIP2) {
	free(reader->stream);
	if (spillover!=NULL && spillover_len!=NULL) {
	    *spillover=malloc(reader->abstract.avail_in);
	    if (*spillover==NULL) {
		tsf_set_errno("Could not allocate spillover buffer");
		stillok=tsf_false;
	    } else {
		*spillover_len=reader->abstract.avail_in;
		memcpy(*spillover,reader->abstract.next_in,*spillover_len);
	    }
	}
    } else if (reader->mode==TSF_ZIP_NONE) {
	if (spillover!=NULL && spillover_len!=NULL) {
	    *spillover=malloc(reader->puti-reader->geti);
	    if (*spillover==NULL) {
		tsf_set_errno("Could not allocate spillover buffer");
		stillok=tsf_false;
	    } else {
		*spillover_len=reader->puti-reader->geti;
		memcpy(*spillover,reader->buf+reader->geti,*spillover_len);
	    }
	}
    }
    if (reader->mode!=TSF_ZIP_UNKNOWN) {
	free(reader->buf);
    }
    reader->stream=NULL;
    reader->buf=NULL;
    reader->buf_size=0;
    reader->mode=TSF_ZIP_UNKNOWN;
    return stillok;
}

static tsf_bool_t tear_to_none(tsf_adpt_rdr_t *reader) {
    tsf_assert(reader->mode==TSF_ZIP_ZLIB ||
	       reader->mode==TSF_ZIP_BZIP2);
    if (reader->mode==TSF_ZIP_ZLIB) {
#ifdef HAVE_ZLIB
	inflateEnd(Cz(reader));
#endif
    } else {
#ifdef HAVE_BZIP2
	BZ2_bzDecompressEnd(Cbz(reader));
#endif
    }
    free(reader->stream);
    reader->mode=TSF_ZIP_UNKNOWN;
    if (reader->nozip_buf_size==reader->buf_size) {
	reader->geti=reader->abstract.next_in-reader->buf;
	reader->puti=reader->geti+reader->abstract.avail_in;
    } else {
	uint8_t *buf;
	reader->buf_size=tsf_max(reader->nozip_buf_size,
				 reader->abstract.avail_in);
	buf=malloc(reader->buf_size);
	if (buf==NULL) {
	    tsf_set_errno("Could not allocate buffer in tear_to_none() in "
			  "tsf_adaptive_reader.c");
	    free(reader->buf);
	    /* oh man, this leaves the reader in a flaky state */
	    return tsf_false;
	}
	memcpy(buf,
	       reader->abstract.next_in,
	       reader->abstract.avail_in);
	free(reader->buf);
	reader->geti=0;
	reader->puti=reader->abstract.avail_in;
	reader->buf=buf;
    }
    reader->mode=TSF_ZIP_NONE;
    return tsf_true;
}

static tsf_bool_t select_mode(tsf_adpt_rdr_t *reader,
			      uint8_t *spillover,
			      uint32_t spillover_len,
			      tsf_bool_t free_spillover) {
    uint8_t magic[4];
    uint32_t tocopy=tsf_min(4,spillover_len);
    void *tofree;
    if (free_spillover && spillover_len>0) {
	tofree=spillover;
    } else {
	tofree=NULL;
    }
    memcpy(magic,spillover,tocopy);
    spillover+=tocopy;
    spillover_len-=tocopy;
    if (tocopy<4) {
	tsf_assert(spillover_len==0);
	if (!tsf_full_read_of_partial(reader->reader,reader->reader_arg,
				      magic+tocopy,4-tocopy)) {
	    if (tofree!=NULL) free(tofree);
	    return tsf_false;
	}
    }
    if (!memcmp(magic,TSF_SP_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
	reader->buf_size=tsf_max(reader->nozip_buf_size,
				 spillover_len+TSF_SP_MAGIC_LEN);
	reader->buf=malloc(reader->buf_size);
	if (reader->buf==NULL) {
	    tsf_set_errno("Could not allocate buffer");
	    if (tofree!=NULL) free(tofree);
	    return tsf_false;
	}
	memcpy(reader->buf,spillover,spillover_len);
	memcpy(reader->buf+spillover_len,TSF_SP_MAGIC_CODE,TSF_SP_MAGIC_LEN);
	reader->puti=spillover_len+TSF_SP_MAGIC_LEN;
	reader->geti=0;
	if (tofree!=NULL) free(tofree);
	reader->mode=TSF_ZIP_NONE;
    } else if (!memcmp(magic,TSF_SP_BZIP2_MAGIC_CODE,TSF_SP_MAGIC_LEN) ||
	       !memcmp(magic,TSF_SP_ZLIB_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
	int res;
	
	if (!memcmp(magic,TSF_SP_ZLIB_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
	    reader->buf_size=tsf_max(reader->attr.z.buf_size,spillover_len);
	} else {
	    reader->buf_size=tsf_max(reader->attr.b.buf_size,spillover_len);
	}

	reader->buf=malloc(reader->buf_size);
	if (reader->buf==NULL) {
	    tsf_set_errno("Could not allocate buffer");
	    if (tofree!=NULL) free(tofree);
	    return tsf_false;
	}
	
	memcpy(reader->buf,spillover,spillover_len);
	if (tofree!=NULL) free(tofree);
	
	reader->abstract.next_in=reader->buf;
	reader->abstract.avail_in=spillover_len;
	
	if (!memcmp(magic,TSF_SP_ZLIB_MAGIC_CODE,TSF_SP_MAGIC_LEN)) {
	    if (!reader->attr.z.allow) {
		tsf_set_error(TSF_E_PARSE_ERROR,
			      "The stream is compressed using ZLib, but "
			      "the given attributes specify that "
			      "ZLib is not allowed");
		if (tofree!=NULL) free(tofree);
		free(reader->buf);
		return tsf_false;
	    }
#ifdef HAVE_ZLIB
	    reader->stream=malloc(sizeof(z_stream));
	    if (reader->stream==NULL) {
		tsf_set_errno("Could not malloc z_stream");
		free(reader->buf);
		return tsf_false;
	    }
	    
	    Cz(reader)->zalloc=NULL;
	    Cz(reader)->zfree=NULL;
	    Cz(reader)->opaque=NULL;
	    
	    if (reader->attr.z.advanced_init) {
		res=inflateInit2(Cz(reader),
				 reader->attr.z.windowBits);
	    } else {
		res=inflateInit(Cz(reader));
	    }
	    if (res!=Z_OK) {
		tsf_set_zlib_error(res,Cz(reader)->msg,
				   "Trying to initialize inflation in ZLib");
		free(Cz(reader));
		free(reader->buf);
		return tsf_false;
	    }
	    reader->mode=TSF_ZIP_ZLIB;
#else
	    tsf_set_error(TSF_E_NOT_SUPPORTED,
			  "TSF_ZIP_ZLIB is not supported by "
			  "tsf_zip_reader_create() because ZLib support "
			  "was not configured into the TSF library");
	    if (tofree!=NULL) free(tofree);
	    free(reader->buf);
	    return tsf_false;
#endif
	} else {
	    if (!reader->attr.b.allow) {
		tsf_set_error(TSF_E_PARSE_ERROR,
			      "The stream is compressed using libbzip2, but "
			      "the given attributes specify that "
			      "libbzip2 is not allowed");
		if (tofree!=NULL) free(tofree);
		free(reader->buf);
		return tsf_false;
	    }
#ifdef HAVE_BZIP2
            reader->stream=malloc(sizeof(bz_stream));
            if (reader->stream==NULL) {
                tsf_set_errno("Could not malloc bz_stream");
		free(reader->buf);
                return tsf_false;
            }
            
            Cbz(reader)->bzalloc=NULL;
            Cbz(reader)->bzfree=NULL;
            Cbz(reader)->opaque=NULL;
            
            res=BZ2_bzDecompressInit(Cbz(reader),
                                     reader->attr.b.verbosity,
                                     reader->attr.b.small);
            if (res!=BZ_OK) {
                tsf_set_libbzip2_error(res,
                    "Trying to initialize decompression in libbzip2");
                free(Cbz(reader));
		free(reader->buf);
                return tsf_false;
            }
            
	    reader->mode=TSF_ZIP_BZIP2;
#else
            tsf_set_error(TSF_E_NOT_SUPPORTED,
                          "TSF_ZIP_BZIP2 is not supported by "
                          "tsf_zip_reader_create() because libbzip2 "
                          "support was not configured into the TSF "
                          "library");
	    free(reader->buf);
	    if (tofree!=NULL) free(tofree);
            return tsf_false;
#endif
	}
    } else {
	if (tofree!=NULL) free(tofree);
	tsf_set_error(TSF_E_PARSE_ERROR,
		      "Not a valid magic code for any TSF stream file format");
	return tsf_false;
    }
    return tsf_true;
}

static tsf_bool_t tear_down_and_select_mode(tsf_adpt_rdr_t *reader) {
    uint8_t *spillover;
    uint32_t spillover_len;
    return tear_down_mode(reader,&spillover,&spillover_len)
	&& select_mode(reader,spillover,spillover_len,tsf_true);
}

tsf_adpt_rdr_t *tsf_adaptive_reader_create(tsf_partial_reader_t reader,
					   void *reader_arg,
					   uint32_t nozip_buf_size,
					   const tsf_zip_rdr_attr_t *attr) {
    tsf_adpt_rdr_t *result=malloc(sizeof(tsf_adpt_rdr_t));
    if (result==NULL) {
	tsf_set_errno("Could not malloc tsf_adpt_rdr_t");
	return NULL;
    }
    
    result->mode=TSF_ZIP_UNKNOWN;
    
    result->reader=reader;
    result->reader_arg=reader_arg;
    
    result->stream=NULL;
    result->expect_switch=tsf_true;
    
    result->buf=NULL;
    result->buf_size=0;
    
    if (attr==NULL) {
	tsf_zip_rdr_attr_get_default(&(result->attr));
    } else {
	result->attr=*attr;
    }
    result->nozip_buf_size=nozip_buf_size;
    
    return result;
}

void tsf_adaptive_reader_destroy(tsf_adpt_rdr_t *reader) {
    tear_down_mode(reader,NULL,NULL);
    free(reader);
}

void tsf_adaptive_reader_hint_switch(tsf_adpt_rdr_t *reader) {
    reader->expect_switch=tsf_true;
}

tsf_bool_t tsf_adaptive_reader_read(void *_reader,
				    void *_data,
				    uint32_t len) {
    tsf_adpt_rdr_t *reader=_reader;
    uint8_t *data=_data;
    
    if (len==0) {
	return tsf_true;
    }
    
    /* this just constitutes lazy initialization upon read */
    if (reader->mode==TSF_ZIP_UNKNOWN) {
	if (!select_mode(reader,NULL,0,tsf_false)) {
	    return tsf_false;
	}
    }

    switch (reader->mode) {
    case TSF_ZIP_NONE: {
	/* ok - this code is complicated.  it is complicated because it
	   needs to deal with the possibility of a format change - if the
	   magic code for one of the zip formats comes in, this thing's job
	   is to recognize it and switch from no-zip buffering to unzipping.
	   we use the hint_switch() function calls as a hint of when to
	   look for the format changes. */
	if (reader->expect_switch && reader->puti>reader->geti) {
	    reader->expect_switch=tsf_false;
	    if (reader->buf[reader->geti]==TSF_SP_C_SWITCH_FORMAT) {
		if (!tear_down_and_select_mode(reader)) {
		    return tsf_false;
		}
		return tsf_adaptive_reader_read(reader,data,len);
	    }
	}
	uint32_t toread=tsf_min(reader->puti-reader->geti,len);
	memcpy(data,reader->buf+reader->geti,toread);
	reader->geti+=toread;
	data+=toread;
	len-=toread;
	if (len>reader->buf_size) {
	    tsf_assert(reader->geti==reader->puti);
	    if (reader->expect_switch) {
		int32_t res=reader->reader(reader->reader_arg,
					   data,len);
		if (res<0) {
		    return tsf_false;
		}
		tsf_assert(res>0);
		if (data[0]==TSF_SP_C_SWITCH_FORMAT) {
		    tear_down_mode(reader,NULL,NULL);
		    if (!select_mode(reader,data,res,tsf_false)) {
			return tsf_false;
		    }
		    return tsf_adaptive_reader_read(reader,data,len);
		} else {
		    data+=res;
		    len-=res;
		}
	    }
	    if (!tsf_full_read_of_partial(reader->reader,
					  reader->reader_arg,
					  data,
					  len)) {
		if (tsf_get_error_code()==TSF_E_UNEXPECTED_EOF
		    && data!=_data) {
		    tsf_set_error(TSF_E_ERRONEOUS_EOF,
				  "End-of-file after part of a full buffered "
				  "read; original error: %s",tsf_get_error());
		}
		return tsf_false;
	    }
	} else if (len>0) {
	    reader->puti=0;
	    reader->geti=0;
	    for (;;) {
		int32_t res;
		uint32_t tocopy;
		res=reader->reader(reader->reader_arg,
				   reader->buf,
				   reader->buf_size);
		if (res<0) {
		    if (tsf_get_error_code()==TSF_E_UNEXPECTED_EOF
			&& data!=_data) {
			tsf_set_error(TSF_E_ERRONEOUS_EOF,
				      "End-of-file after part of a full buffered "
				      "read; original error: %s",tsf_get_error());
		    }
		    return tsf_false;
		}
		tsf_assert(res>0);
		if (reader->expect_switch) {
		    reader->expect_switch=tsf_false;
		    if (reader->buf[0]==TSF_SP_C_SWITCH_FORMAT) {
			reader->puti=res;
			if (!tear_down_and_select_mode(reader)) {
			    return tsf_false;
			}
			return tsf_adaptive_reader_read(reader,data,len);
		    }
		}
		tocopy=tsf_min(res,len);
		memcpy(data,reader->buf,tocopy);
		data+=tocopy;
		len-=tocopy;
		if (len==0) {
		    reader->puti=res;
		    reader->geti=tocopy;
		    break;
		}
	    }
	}
	break;
    }
    case TSF_ZIP_ZLIB:
    case TSF_ZIP_BZIP2: {
	reader->abstract.next_out=data;
	reader->abstract.avail_out=len;
	
	for (;;) {
	    int res;

	    /* is there ever a case where we would want to call inflate even though
	       the input buffer is empty? */
	    if (reader->abstract.avail_in!=0) {
		res=tsf_zip_abstract_inflate(reader->mode,
					     reader->stream,
					     &(reader->abstract),
					     TSF_ZIP_ACT_RUN);
		
		if (res==TSF_ZIP_RES_ERROR) {
		    return tsf_false;
		} else if (res==TSF_ZIP_RES_END) {
		    if (reader->abstract.avail_out!=0) {
			tsf_set_error(TSF_E_PARSE_ERROR,
				      "A read request straddles a format change");
		    }
		    tear_to_none(reader);
		}
		
		if (reader->abstract.avail_out==0) {
		    break;
		}
		
		tsf_assert(reader->abstract.avail_in==0);
	    }
	    
	    res=reader->reader(reader->reader_arg,
			       reader->buf,
			       reader->buf_size);
	    
	    if (res==-1) {
		if (tsf_get_error_code()==TSF_E_UNEXPECTED_EOF) {
		    tsf_set_error(TSF_E_ERRONEOUS_EOF,
				  "Got an EOF from the reader before the "
				  "decompressor had reached end of stream");
		}
		return tsf_false;
	    }
	    
	    reader->abstract.next_in=reader->buf;
	    reader->abstract.avail_in=res;
	}
	break;
    }
    default:
	tsf_set_error(TSF_E_INTERNAL,
		      "Invalid reader mode in tsf_adaptive_reader_read()");
	return tsf_false;
    }
    
    return tsf_true;
}

uint32_t tsf_adaptive_reader_get_remaining(tsf_adpt_rdr_t *reader) {
    switch (reader->mode) {
    case TSF_ZIP_UNKNOWN:
        return 0;
    case TSF_ZIP_NONE:
        return reader->puti-reader->geti;
    case TSF_ZIP_ZLIB:
    case TSF_ZIP_BZIP2:
        return reader->abstract.avail_in;
    default:
        /* should never happen */
        tsf_abort("bad reader mode");
    }
}



