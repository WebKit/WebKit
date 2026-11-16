/*
 * Copyright (C) 2003, 2004, 2005, 2014 Filip Pizlo. All rights reserved.
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#ifdef HAVE_PTHREAD
#include <pthread.h>
#endif

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

#ifdef HAVE_BZIP2
#include <bzlib.h>
#endif

static const char *enomem_msg = "System error: Cannot allocate memory";

static const char *error_array[] = {
    "System error",
    "ZLib error",
    "libbzip2 error",
    "Invalid argument",
    "Bad state",
    "String too long",
    "Parse error",
    "Parsed an unexpected type",
    "Element in container is NULL",
    "Name already used",
    "End of file",
    "Unexpected end of file",
    "Erroneous end of file",
    "End of file not reached",
    "Parameter has bad type",
    "Parameter has bad type kind",
    "Element not found",
    "Type lacks struct mapping",
    "The provided structure mapping is invalid",
    "Type mismatch",
    "Bad choice value",
    "Comparison failed",
    "C compile error",
    "Child process died because it received a signal",
    "Dynamic loading error",
    "Jump to bad label in GPC prototype",
    "Violation of GPC semantics",
    "Compare-and-swap failed",
    "Hostname lookup failure",
    "Operation not allowed",
    "Operation not implemented",
    "Operation not supported",
    "External error",
    "Internal error"
};

const char *tsf_get_message_for_error_code(tsf_error_t code) {
    return error_array[code];
}

static void tsf_set_error_impl(tsf_error_t *error_code,
                               int *error_errno,
                               int *error_deadly_signal,
                               int *error_zlib_code,
                               const char **error_zlib_msg,
                               char **error_str,
                               tsf_error_t code,
                               int sys_errno,
                               int deadly_signal,
                               int zlib_code,
                               const char *zlib_msg,
                               const char *format,
                               va_list lst) {
    char *str;

    if (*error_str!=NULL && *error_str!=enomem_msg) {
        free(*error_str);
    }
    
    *error_code=code;
    *error_errno=sys_errno;
    *error_deadly_signal=deadly_signal;
    *error_zlib_code=zlib_code;
    *error_zlib_msg=zlib_msg;
    
    if (format==NULL) {
        *error_str=strdup(error_array[code]);
    } else {
        str=tsf_vasprintf(format, lst);
        if (str==NULL) {
            *error_str=NULL;
        } else {
            *error_str=tsf_asprintf("%s: %s",
                                    error_array[code],
                                    str);
            free(str);
        }
    }
    
    if (*error_str==NULL) {
        *error_str=(char*)enomem_msg;
        *error_code=TSF_E_ERRNO;
        *error_errno=errno;
    }
}

#ifdef HAVE_PTHREAD

struct tsf_error_struct {
    tsf_error_t error_code;
    int sys_errno;
    int deadly_signal;
    int zlib_code;
    const char *zlib_msg;
    char *error_str;
};

static pthread_once_t once_control = PTHREAD_ONCE_INIT;
static pthread_key_t error;

static void error_destroy(void *arg) {
    if (arg!=NULL) {
        struct tsf_error_struct *es=arg;
        if (es->error_str!=NULL) {
            free(es->error_str);
        }
        free(es);
    }
}

static void init() {
    pthread_key_create(&error,error_destroy);
}

void tsf_set_error_fullv(tsf_error_t code,
                         int sys_errno,
                         int deadly_signal,
                         int zlib_error,
                         const char *zlib_msg,
                         const char *format,
                         va_list lst) {
    struct tsf_error_struct *es;
    pthread_once(&once_control,init);
    
    es=pthread_getspecific(error);
    if (es==NULL) {
        es=malloc(sizeof(struct tsf_error_struct));
        if (es==NULL) {
            /* we're dead! */
            tsf_abort("Could not allocate error struct in "
                      "tsf_set_error\n");
        }
        
        es->error_code=0;
        es->sys_errno=0;
        es->deadly_signal=0;
        es->zlib_code=0;
        es->zlib_msg=NULL;
        es->error_str=NULL;
        
        pthread_setspecific(error,es);
    }
    
    tsf_set_error_impl(&es->error_code,
                       &es->sys_errno,
                       &es->deadly_signal,
                       &es->zlib_code,
                       &es->zlib_msg,
                       &es->error_str,
                       code,
                       sys_errno,
                       deadly_signal,
                       zlib_error,
                       zlib_msg,
                       format,
                       lst);
}

tsf_error_t tsf_get_error_code() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->error_code;
}

int tsf_get_system_errno() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->sys_errno;
}

int tsf_get_deadly_signal() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->deadly_signal;
}

int tsf_get_zlib_error_code() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->zlib_code;
}

const char *tsf_get_zlib_error_msg() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->zlib_msg;
}

int tsf_get_libbzip2_error_code() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->zlib_code;
}

const char *tsf_get_libbzip2_error_msg() {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->zlib_msg;
}

const char *tsf_get_error(void) {
    return ((struct tsf_error_struct*)pthread_getspecific(error))->error_str;
}

#else

static tsf_error_t error_code=TSF_E_LAST;
static int sys_errno=0;
static int deadly_signal=0;
static int zlib_code=0;
static const char *zlib_msg=NULL;
static char *error_str=NULL;

void tsf_set_error_fullv(tsf_error_t code,
                         int my_errno,
                         int my_deadly_signal,
                         int my_zlib_error,
                         const char *my_zlib_msg,
                         const char *format,
                         va_list lst) {
    tsf_set_error_impl(&error_code,
                       &sys_errno,
                       &deadly_signal,
                       &zlib_code,
                       &zlib_msg,
                       &error_str,
                       code,
                       my_errno,
                       my_deadly_signal,
                       my_zlib_error,
                       my_zlib_msg,
                       format,
                       lst);
}

tsf_error_t tsf_get_error_code() {
    return error_code;
}

int tsf_get_system_errno() {
    return sys_errno;
}

int tsf_get_deadly_signal() {
    return deadly_signal;
}

int tsf_get_zlib_error_code() {
    return zlib_code;
}

const char *tsf_get_zlib_error_msg() {
    return zlib_msg;
}

int tsf_get_libbzip2_error_code() {
    return zlib_code;
}

const char *tsf_get_libbzip2_error_msg() {
    return zlib_msg;
}

const char *tsf_get_error() {
    return error_str;
}

#endif

void tsf_set_error_full(tsf_error_t code,
                        int my_errno,
                        int my_deadly_signal,
                        int my_zlib_error,
                        const char *my_zlib_msg,
                        const char *format,
                        ...) {
    va_list lst;
    
    va_start(lst, format);
    tsf_set_error_fullv(code,
                        my_errno,
                        my_deadly_signal,
                        my_zlib_error,
                        my_zlib_msg,
                        format,
                        lst);
    va_end(lst);
}

#define error_body(errno,sig,zl_code,zl_msg,code,prefmt,strerr) \
do { \
    va_list lst; \
    char *msg; \
     \
    if (format==NULL) { \
        tsf_set_error_full(code, \
                           errno, \
                           sig, \
                           zl_code, \
                           zl_msg, \
                           prefmt, \
                           strerr); \
        return; \
    } \
     \
    va_start(lst, format); \
    msg=tsf_vasprintf(format, lst); \
    va_end(lst); \
     \
    if (msg==NULL) { \
        tsf_set_error_full(TSF_E_ERRNO, \
                           errno, \
                           0, \
                           0, \
                           0, \
                           NULL, \
                           "%s", \
                           strerror(errno)); \
        return; \
    } \
     \
    tsf_set_error_full(code, \
                       errno, \
                       sig, \
                       zl_code, \
                       zl_msg, \
                       prefmt ": %s", \
                       strerr, \
                       msg); \
     \
    free(msg); \
} while (0)

#define simple_error_body(code) \
do { \
    va_list lst; \
    va_start(lst, format); \
    tsf_set_error_fullv(code, \
                        0, \
                        0, \
                        0, \
                        NULL, \
                        format, \
                        lst); \
    va_end(lst); \
} while (0)

void tsf_set_error(tsf_error_t code,
                   const char *format,
                   ...) {
    simple_error_body(code);
}

void tsf_set_specific_errno(int cur_errno,
                            const char *format,
                            ...) {
    error_body(cur_errno,0,0,NULL,TSF_E_ERRNO,"%s",strerror(cur_errno));
}

void tsf_set_errno(const char *format,
                   ...) {
    int my_errno = errno;
    error_body(my_errno,0,0,NULL,TSF_E_ERRNO,"%s",strerror(my_errno));
}

void tsf_set_deadly_signal(int deadly_signal,
                           const char *format,...) {
    error_body(0,
               deadly_signal,
               0,
               NULL,
               TSF_E_DEATH_BY_SIGNAL,
               "Received signal #%d",
               deadly_signal);
}

#ifdef HAVE_ZLIB

const char *tsf_zlib_strerror(int code) {
    switch (code) {
        case Z_VERSION_ERROR:   return "ZLib compiled with wrong header/"
                                       "library combination.";
        case Z_STREAM_ERROR:    return "Stream error.";
        case Z_MEM_ERROR:       return "Attempt to request memory failed.";
        case Z_DATA_ERROR:      return "Data integrity error.";
        case Z_BUF_ERROR:       return "Buffering error.";
        case Z_ERRNO:           return "System error.  Check errno.";
        default:                return "Unknown error code.";
    }
}

void tsf_set_zlib_error(int error_code,
                        const char *error_msg,
                        const char *format,...) {
    switch (error_code) {
    case Z_ERRNO: {
        int my_errno = errno; // Save errno since it can change very easily.
        error_body(my_errno, 0, 0, NULL, TSF_E_ERRNO, "%s", strerror(my_errno));
        break;
    }
    case Z_MEM_ERROR:
        error_body(ENOMEM, 0, 0, NULL, TSF_E_ERRNO, "%s", strerror(ENOMEM));
        break;
    default:
        if (error_msg == NULL) {
            error_msg = tsf_zlib_strerror(error_code);
        }
        error_body(0,
                   0,
                   error_code,
                   error_msg,
                   TSF_E_ZLIB_ERROR,
                   "%s",
                   error_msg);
        break;
    }
}

#else

const char *tsf_zlib_strerror(int code) {
    tsf_abort("tsf_zlib_strerror() called even though zlib support is not "
              "compiled in.");
}

void tsf_set_zlib_error(int error_code,
                        const char *error_msg,
                        const char *format,...) {
    tsf_abort("tsf_set_zlib_error() called even though zlib support is not "
              "compiled in.");
}

#endif

#ifdef HAVE_BZIP2

const char *tsf_libbzip2_strerror(int code) {
    switch (code) {
        case BZ_CONFIG_ERROR:   return "libbzip2 is not configured to run on"
                                       "this platform.  This is a fatal error.";
        case BZ_SEQUENCE_ERROR: return "Incorrect sequence of calls to "
                                       "libbzip2 functions.  This is a fatal "
                                       "error.";
        case BZ_PARAM_ERROR:    return "Parameter to libbzip2 function is out "
                                       "of range.  This is a fatal error.";
        case BZ_MEM_ERROR:      return "Attempt to request memory failed.";
        case BZ_DATA_ERROR:     return "Data integrity error.";
        case BZ_DATA_ERROR_MAGIC: return "Data does not contain correct magic "
                                       "bytes.";
        case BZ_IO_ERROR:       return "I/O error.  Check system's errno.";
        case BZ_UNEXPECTED_EOF: return "Unexpected end of file.";
        case BZ_OUTBUFF_FULL:   return "Output buffer is too small.";
        default:                return "Unknown error code.";
    }
}

void tsf_set_libbzip2_error(int error_code,
                            const char *format, ...) {
    switch (error_code) {
    case BZ_MEM_ERROR:
        error_body(ENOMEM, 0, 0, NULL, TSF_E_ERRNO, "%s", strerror(ENOMEM));
        break;
    case BZ_IO_ERROR: {
        int my_errno = errno;
        error_body(my_errno, 0, 0, NULL, TSF_E_ERRNO, "%s", strerror(my_errno));
        break;
    }
    case BZ_UNEXPECTED_EOF:
        simple_error_body(TSF_E_ERRONEOUS_EOF);
        break;
    default:
        error_body(0,
                   0,
                   error_code,
                   tsf_libbzip2_strerror(error_code),
                   TSF_E_LIBBZIP2_ERROR,
                   "%s",
                   tsf_libbzip2_strerror(error_code));
        break;
    }
}

#else

const char *tsf_libbzip2_strerror(int code) {
    tsf_abort("tsf_libbzip2_strerror() got called, but libbzip2 support "
              "was not compiled in.");
    return NULL;
}

void tsf_set_libbzip2_error(int error_code,
                            const char *format,...) {
    tsf_abort("tsf_set_libbzip2_error() got called, but libbzip2 support "
              "was not compiled in.");
}

#endif

#ifdef HAVE_PTHREAD
static pthread_once_t buf_once_control=PTHREAD_ONCE_INIT;
static pthread_key_t str_buf;

static void init_str_buf(void) {
    pthread_key_create(&str_buf,free);
}

static char *get_str_buf(void) {
    pthread_once(&buf_once_control,init_str_buf);
    char *result=pthread_getspecific(str_buf);
    if (result==NULL) {
	result=malloc(100);
	pthread_setspecific(str_buf,result);
    }
    return result;
}
#else
static char str_buf[100];

static char *get_str_buf(void) {
    return str_buf;
}
#endif

const char *tsf_get_error_infix(void) {
    switch (tsf_get_error_code()) {
    case TSF_E_ERRNO: return strerror(tsf_get_system_errno());
    case TSF_E_DEATH_BY_SIGNAL:
	if (get_str_buf()==NULL) {
	    return NULL;
	}
	sprintf(get_str_buf(),"Received signal #%d",
		tsf_get_deadly_signal());
	return get_str_buf();
#ifdef HAVE_ZLIB
    case TSF_E_ZLIB_ERROR:
	return tsf_get_zlib_error_msg();
#endif
#ifdef HAVE_BZIP2
    case TSF_E_LIBBZIP2_ERROR:
	return tsf_get_libbzip2_error_msg();
#endif
    default:
	return NULL;
    }
}

const char *tsf_get_error_suffix(void) {
    const char *whole_msg=tsf_get_error();
    const char *prefix=tsf_get_error_prefix();
    unsigned prefix_len=strlen(prefix);

    /* check if there is anything other than the prefix */
    if (strlen(whole_msg)==prefix_len) {
	return NULL;
    } else {
	/* turns out not.  now there are two possibilities to
	   consider:
	   
	   - the remainder is ": " followed by some text, or

	   - the remainder is something else, meaning that this
	     is a completely invalid error string! */

	const char *infix=tsf_get_error_infix();
	const char *result=whole_msg+prefix_len+2;

	tsf_assert(whole_msg[prefix_len]==':' &&
		   whole_msg[prefix_len+1]==' ');

	/* ok.  we know that the error string is properly
	   formatted.  now there are three other possibilites to
	   deal with:

	   - the remainder contains the so-called infix (which is
	     just the error string that we got from some dependency
	     that generated the error), followed by nothing else.
	     this means that there is no suffix.

	   - the remainder contains the so-called infix followed by
	     a ": " and the actual suffix.

	   - the remainder either doesn't contain the infix, or
	     else it contains the infix but without the ": "; this
	     means we assume that the whole remainder is the suffix. */

	/* printf("infix = %s\nresult = %s\n",infix,result); */

	if (infix!=NULL &&
	    strstr(result,infix)==result) {
	    unsigned infix_len=strlen(infix);
	    if (result[infix_len]==0) {
		/* there is no suffix (the whole remainder is just
		   infix. */
		return NULL;
	    }
	    if (result[infix_len]==':' &&
		result[infix_len+1]==' ') {
		/* there is a suffix after the infix. */
		return result+infix_len+2;
	    } else {
		/* the infix is there, but there is no ": ", so we
		   assume that there is no infix afterall. */
		return result;
	    }
	} else {
	    /* there is no infix. */
	    return result;
	}
    }
}

void tsf_f_abort(const char *msg) {
    fprintf(stderr,"tsf_f_abort(\"%s\")\n",msg);
    abort();
}


