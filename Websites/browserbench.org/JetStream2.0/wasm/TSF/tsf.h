/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2011, 2014, 2015 Filip Pizlo. All rights reserved.
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

/*
 * Stuff to know:
 *
 * Memory management conventions: unless otherwise stated, all objects
 * allocated by functions that you call become your responsibility after
 * a successful return.  also, unless otherwise stated, passing an
 * object as a first argument to a function does not clear your ownership
 * of that object.  and unless otherwise stated, objects passed as anything
 * but the first argument become the responsibility of that function,
 * regardless of whether or not that function returns successfully.
 *
 * Error management conventions: most functions return pointers or booleans.
 * in the case of pointers, if the pointer returned is NULL, then you must
 * check for errors.  in the case of booleans, you must check if the
 * return value is tsf_false.
 *
 * Error propagation: those functions that claim to be capable of error
 * propagation will return immediately as if with an error without actually
 * setting the error if one of their object arguments is NULL.  This allows
 * you to do:
 * if (!tsf_do_operation(object1,obj2, ..., tsf_do_op_that_fails(),...)) {
 *      // process error.
 * }
 * ... and safely expect that this will correctly catch errors from
 * tsf_do_op_that_fails() in addition to catching errors from
 * tsf_do_operation().
 *
 * Code generation/native code support/struct mapping: this functionality is
 * captured under the 'tsf_native' namespace, but it does not actually have
 * any struct or class or object of its own.  Instead, this functionality's
 * state is spread across the rest of the system.  Fields that have the sole
 * purpose of supporting code generation or struct mapping are marked as
 * such with a comment.
 */

#ifndef FP_TSF_H
#define FP_TSF_H

#include "tsf_config.h"
#include "tsf_atomics.h"
#include "tsf_st.h"
#include "tsf_region.h"
#include "tsf_types.h"
#include "tsf_zip_abstract.h"
#include "tsf_sha1.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dirent.h>

#ifdef __cplusplus
extern "C" {
#endif

#if 0
/* hack to prevent emacs from being confused */
}
#endif

/* macro you'll find useful */
#define tsf_offsetof(type,element) ((size_t)(&(((type*)NULL)->element)))

/* any errors in this enumeration must also be placed in tsf_error.c's
 * init_error_array() function. */
enum tsf_error {
    TSF_E_ERRNO,            /* system error; examine errno value returned
                             * from tsf_get_system_errno() */
    TSF_E_ZLIB_ERROR,       /* ZLib error; examine the error code returned
                             * from tsf_get_zlib_error_code() and also the
                             * error message returned from
                             * tsf_get_zlib_error_msg() */
    TSF_E_LIBBZIP2_ERROR,   /* libbzip2 error; examine the error code returned
                             * from tsf_get_libbzip2_error_code() and also the
                             * error message returned from
                             * tsf_get_libbzip2_error_msg().  note that the
                             * latter function returns textual messages
                             * selected by TSF and not by libbzip2.  these
                             * messages were written by me based on the
                             * libbzip2 manual. */
    TSF_E_INVALID_ARG,      /* invalid argument */
    TSF_E_BAD_STATE,        /* bad state - the given operation doesn't make
                             * sense for the current state of the object. */
    TSF_E_STR_OVERFLOW,     /* string overflow.  strings used as element names
                             * in structs and choices cannot be longer than
                             * TSF_STR_SIZE (including null byte).  comment
                             * strings also have some limit. */
    TSF_E_PARSE_ERROR,      /* parse error */
    TSF_E_UNEXPECTED_TYPE,  /* did not parse the type that I expected to
                             * parse.  this may happen if you're asking the
                             * serial in man to re-read some data that it's
                             * already read.  you can do this by using its
                             * mark and seek mechanism. */
    TSF_E_ELEMENT_NULL,     /* element in container is null */
    TSF_E_NAME_COLLISION,   /* name collision - the name is already used! */
    TSF_E_EOF,              /* end of file or end of stream */
    TSF_E_UNEXPECTED_EOF,   /* unexpected end of file or end of stream.  when
                             * returned from a reader, this error means that
                             * the EOF happened before any data was read. */
    TSF_E_ERRONEOUS_EOF,    /* erroneous end of file or end of stream.  when
                             * returned from a reader, this error means that
                             * the EOF happened after some data was already
                             * read.  to everyone else, this error should
                             * have the same exact meaning as
                             * TSF_E_UNEXPECTED_EOF */
    TSF_E_NO_EOF,           /* expected an EOF but didn't get one. */
    TSF_E_BAD_TYPE,         /* parameter has the wrong tsf_type - thrown
                             * when the type for a typed object (such as
                             * a reflect object) is not what is expected. */
    TSF_E_BAD_TYPE_KIND,    /* parameter has the wrong tsf_type_kind -
                             * thrown when you pass the wrong type object
                             * into a function. */
    TSF_E_ELE_NOT_FOUND,    /* element not found */
    TSF_E_NO_STRUCT_MAP,    /* the given type lacks a struct mapping */
    TSF_E_BAD_STRUCT_MAP,   /* the structure mapping offset or size is
                             * invalid because it does not agree with the
                             * offsets and/or sizes of the other elements.
                             * note that this is currently unused, but we
                             * keep it here to reserve space. */
    TSF_E_TYPE_MISMATCH,    /* type mismatch */
    TSF_E_BAD_CHOICE_VALUE, /* bad choice value.  note that if a bad choice
                             * value appears while parsing, you will get a
                             * TSF_E_PARSE_ERROR instead.  also, there will
                             * be times when you don't get this error even
                             * though you should have.  so be happy when
                             * you see it and don't cry to me about it if you
                             * don't. */
    TSF_E_COMPARE_FAIL,     /* a comparison failed. */
    TSF_E_COMPILE_ERROR,    /* compilation error in dyncc */
    TSF_E_DEATH_BY_SIGNAL,  /* child process died because it received a
                             * signal (again, this'll happen because of
                             * dyncc). */
    TSF_E_DLOADING_ERROR,   /* dynamic loading error in dyncc */
    TSF_E_BAD_JUMP,         /* jump to bad label in GPC prototype */
    TSF_E_SEMANTIC,         /* violation of GPC semantics */
    TSF_E_CAS_FAIL,         /* a compare-and-swap-like operation failed. */
    TSF_E_HOSTNAME,         /* hostname lookup failure. */
    TSF_E_NOT_ALLOWED,      /* you get this error if you had specified that
                             * a certain action was not allowed, but for
                             * whatever reason TSF would have had to perform
                             * that action. */
    TSF_E_NOT_IMPLEMENTED,  /* not implemented.  this differs from not
                             * supported in that something that is 'not
                             * implemented' is actually advertised as being
                             * supported (and implemented), but I just
                             * haven't moved my ass to implement it.  you
                             * should only see 'not implemented' errors in
                             * development versions. */
    TSF_E_NOT_SUPPORTED,    /* not supported.  this differs from not
                             * implemented in that 'not supported' refers
                             * to functionality that is present but cannot
                             * be used due to some limitations of your
                             * system. */
    TSF_E_EXTERNAL,         /* some external error (like a condition in
			     * in a callback provided by someone else).
			     * the C++ binding uses this for
			     * std::exception objects caught in callbacks. */
    TSF_E_INTERNAL,         /* internal error - should never happen. */
    TSF_E_LAST              /* placeholder */
};

typedef enum tsf_error tsf_error_t;

/* standard string size within tsf */
#define TSF_STR_SIZE            128

/* max size for comment strings */
#define TSF_MAX_COMMENT_SIZE    4096

/* max size for type name */
#define TSF_MAX_NAME_SIZE       256

/* default buffer size */
#define TSF_DEF_BUF_SIZE        4096

/* default buffer size for network connections */
#define TSF_DEF_NET_BUF_SIZE    (1460*32)

/* Instanceof rules:
 * -> VOID type.  everything is instanceof VOID, but VOID is instaceof
 *    VOID only.
 * -> INT types.  An INT type is instanceof any INT type that is
 *    composed of at least as many bits as it.  For example, INT8
 *    is instanceof INT8, INT16, INT32, and INT64.  INT types are
 *    not instanceof any UINT types.  INT types are instanceof
 *    those FP types that have a mantissa that is made up of at
 *    least as many bits as it.
 * -> UINT types.  A UINT type is instanceof any UINT type that is
 *    composed of at least as many bits as it.  Also, a UINT type
 *    is instanceof any INT type that has at least one more bits
 *    than it (so UINT16 will be instanceof INT32 and INT64, but
 *    not INT16).  UINT types are instanceof those FP types that
 *    have a mantissa that is made up of at least one more bits as
 *    it.
 * -> INTEGER.  This behaves like a 32-bit signed integer, but it
 *    is stored with some basic compaction under the assumption
 *    that it is usually smallish.
 * -> LONG.  Same as INTEGER, but gives you 64 bits.
 * -> FP types.  An FP type is instanceof any FP type that has at
 *    least as large of a mantissa, and at least as large of an
 *    exponent.  So FLOAT is instanceof FLOAT and DOUBLE, while
 *    DOUBLE is only instanceof DOUBLE.
 * -> ARRAY.  if A and B are array types, then A is instanceof B
 *    if element_type(A) is instanceof element_type(B).
 * -> STRUCT.  one struct is instanceof another if it contains all
 *    of the mandatory elements of the other struct, and if each
 *    such element is instanceof its respective element in the other
 *    struct.  note that if a struct contains an optional field for
 *    another struct's mandatory field, this will violate the
 *    instanceof relation.
 * -> BIT.  a BIT is only instanceof BIT.  the justification is that
 *    despite the name, which would seem to imply that a BIT is a
 *    type of integer, a BIT is intended to be used in such a
 *    fundamentally different way that it would not make sense for
 *    a BIT to be instanceof any integer.
 * -> CHOICE.  if A and B are choice types, then A is instanceof B
 *    if for those element names that A and B have in common, A's
 *    element is instanceof B's element.
 * -> STRING.  a STRING is only instanceof STRING.
 * -> ANY.  a ANY is only instanceof ANY.  this is of
 *    course quite ironic, since ANY types are our main source of
 *    polymorhpism. :-)
 *
 * Note that if you change this enumeration, you must change some of the
 * #define's further down in this file.
 */
enum tsf_type_kind {
    TSF_TK_VOID     = 0,
    
    /* ints and floats are primitives.  bits are not. */
    
    TSF_TK_INT8     = 1,
    TSF_TK_UINT8    = 2,
    TSF_TK_INT16    = 3,
    TSF_TK_UINT16   = 4,
    TSF_TK_INT32    = 5,
    TSF_TK_UINT32   = 6,
    TSF_TK_INT64    = 7,
    TSF_TK_UINT64   = 8,
    /* reserve 9-13 for bizarre integer types. */
    
    /* This is a 32-bit integer (TSF_TK_INTEGER) or 64-bit integer
     * (TSF_TK_LONG) that we compact down to 1 byte if possible whenever we
     * transmit it. The rules for integer compaction given an integer X are:
     * 
     * - If -64 <= X < 64, encode as one byte containing X with the high bit
     *   of that byte set to 0. Else:
     * - Let X' = (X < 0) ? X + 64 : X - 64. If -16192 <= X < 16192, encode
     *   as one byte containing (X' >> 8) - 1 with the high bit of that byte
     *   set to 1 followed by a second byte containing X' & 255. Else:
     * - Let X' = (X < 0) ? X + 16192 : X - 16192. If -8404800 <= X < 8404800,
     *   encode as one byte containing 254 followed by three bytes containing
     *   X'. Else:
     * - Encode as 255 followed by either four bytes (TSF_TK_INTEGER) or
     *   eight bytes (TSF_TK_LONG) containin X.
     *
     * Note that as with everything else in TSF, the protocol is defined as
     * big endian. */
    TSF_TK_INTEGER  = 14,
    TSF_TK_LONG     = 15,
    /* reserve 16-19 for more bizarre integer types. */

    TSF_TK_FLOAT    = 20,
    TSF_TK_DOUBLE   = 21,
    /* reserve 22-39 for bizarre float types.  recommend that if
     * we ever have a customizable float, it should be 30. */

    TSF_TK_BIT      = 40,
    
    TSF_TK_STRING   = 45,

    TSF_TK_ARRAY    = 50,
    TSF_TK_STRUCT   = 51,
    TSF_TK_CHOICE   = 52,
    
    TSF_TK_ANY      = 60,
};

typedef uint8_t tsf_type_kind_t;

struct tsf_type;
typedef struct tsf_type tsf_type_t;

struct tsf_named_type;
typedef struct tsf_named_type tsf_named_type_t;

/* ordered table of types keyed by int and string */
struct tsf_type_table;
typedef struct tsf_type_table tsf_type_table_t;

/* shared table of types keyed only by int */
struct tsf_type_in_map;
typedef struct tsf_type_in_map tsf_type_in_map_t;

typedef tsf_st_table tsf_type_out_map_t;

struct tsf_ra_type_info;
struct tsf_ra_tc_node;
struct tsf_ra_type_man;

typedef struct tsf_ra_type_info tsf_ra_type_info_t;
typedef struct tsf_ra_tc_node tsf_ra_tc_node_t;
typedef struct tsf_ra_type_man tsf_ra_type_man_t;

struct tsf_buffer;
typedef struct tsf_buffer tsf_buffer_t;

/* type used for arrays referenced from structs within our native
 * structure mapping.  you do not have to ever touch this type.  but
 * if you do struct mapping (which surely you will!), then your
 * arrays should resemble this here tsf_native_array. */
struct tsf_native_array {
    void *data;
    uint32_t len;
};

typedef struct tsf_native_array tsf_native_array_t;

/* types used internally for primitive arrays */
#define TSF_DEFINE_NATIVE_ARRAY(type,suffix) \
    struct tsf_native_##type##_array {\
        type##suffix *data;\
        uint32_t len;\
    };\
    \
    typedef struct tsf_native_##type##_array tsf_native_##type##_array_t

TSF_DEFINE_NATIVE_ARRAY(int8,_t);
TSF_DEFINE_NATIVE_ARRAY(int16,_t);
TSF_DEFINE_NATIVE_ARRAY(int32,_t);
TSF_DEFINE_NATIVE_ARRAY(int64,_t);
TSF_DEFINE_NATIVE_ARRAY(uint8,_t);
TSF_DEFINE_NATIVE_ARRAY(uint16,_t);
TSF_DEFINE_NATIVE_ARRAY(uint32,_t);
TSF_DEFINE_NATIVE_ARRAY(uint64,_t);
TSF_DEFINE_NATIVE_ARRAY(float,);
TSF_DEFINE_NATIVE_ARRAY(double,);

/* special type used for arrays of unknown type */
struct tsf_native_void_array {
    uint32_t len;
};

typedef struct tsf_native_void_array tsf_native_void_array_t;

/* special type used for arrays of bits referenced from structs
 * within our native structure mapping. */
struct tsf_native_bitvector {
    uint8_t *bits;  /* array of length (num_bits+7)>>3 */
    uint32_t num_bits;
};

typedef struct tsf_native_bitvector tsf_native_bitvector_t;

/* type of the choice itself within choice types. */
typedef uint32_t tsf_choice_t;

struct tsf_reflect;
typedef struct tsf_reflect tsf_reflect_t;

struct tsf_genrtr;
struct tsf_parser;
struct tsf_copier;
struct tsf_destructor;
typedef struct tsf_genrtr tsf_genrtr_t;
typedef struct tsf_parser tsf_parser_t;
typedef struct tsf_copier tsf_copier_t;
typedef struct tsf_destructor tsf_destructor_t;

/* function callback used for reading.  return tsf_true on success and
 * tsf_false on error.  on error, use tsf_set_error() and tsf_set_errno()
 * to indicate what error occurred (this part is not optional - you
 * cannot return tsf_false without first calling tsf_set_error() or
 * tsf_set_errno()).  an all-or-nothing read is expected, so if you're
 * implementing this as a read on a socket or pipe, you may have to
 * loop a couple of times. */
typedef tsf_bool_t (*tsf_reader_t)(void *arg,
                                   void *buf,
                                   uint32_t len);

/* function callback used for write.  return tsf_true on success and
 * tsf_false on error.  on error, use tsf_set_error() and tsf_set_errno()
 * to indicate what error occurred (this part is not optional - you
 * cannot return tsf_false without first calling tsf_set_error() or
 * tsf_set_errno()).  an all-or-nothing write is expected, so if you're
 * implementing this as a write on a socket or pipe, you may have to
 * loop a couple of times. */
typedef tsf_bool_t (*tsf_writer_t)(void *arg,
                                   const void *buf,
                                   uint32_t len);

/* function callback used for reading.  returns non-negative count of
 * the number of bytes read on success and -1 on error.  on error, use
 * tsf_set_error() and friends.  the caller expects read-as-much-as-
 * you-can semantics, so this function should never block if some data
 * is already available. */
typedef int32_t (*tsf_partial_reader_t)(void *arg,
                                        void *buf,
                                        uint32_t len);

struct tsf_limits {
    /* maximum number of types accepted in a stream file in between reset
     * types calls. */
    uint32_t max_types;
    
    /* byte size limits */
    
    /* maximum size in bytes of a serialized type */
    uint32_t max_type_size;
    
    /* maximum size that all types read during a session can have put
     * together. */
    uint32_t max_total_type_size;
    
    /* maximum size in bytes of a buffer */
    uint32_t max_buf_size;
    
    /* container depth limit */
    uint32_t depth_limit;
    
    /* whether or not to allow comments */
    tsf_bool_t allow_comments;
    
    /* allowing individual types.  access this as a regular bitvector. */
    uint8_t allow_type_kind[32];
    
    /* type-specific limits */
    
    /* maximum array length */
    uint32_t max_array_length;  /* unimplemented */
    
    /* maximum struct size, in number of elements */
    uint32_t max_struct_size;
    
    /* maximum choice size, in number of elements */
    uint32_t max_choice_size;
    
    /* maximum string size */
    uint32_t max_string_size;   /* unimplemented */
};

typedef struct tsf_limits tsf_limits_t;

struct tsf_serial_in_man;
struct tsf_serial_out_man;

typedef struct tsf_serial_in_man tsf_serial_in_man_t;
typedef struct tsf_serial_out_man tsf_serial_out_man_t;

/* callback that gets called by the input manager when a new type comes
 * in. */
typedef void (*tsf_type_cback_t)(tsf_serial_in_man_t *in_man,
                                 tsf_type_t *type,
                                 void *arg);

typedef uint32_t tsf_serial_in_man_state_t;

/* size aware - delegates to another reader, counts the bytes read,
 * and fires a TSF_E_NOT_ALLOWED error when the limit reaches 0.  name
 * should be a short (preferrably one-word) description of what you're
 * reading; it'll be used for the error message. */
typedef struct {
    tsf_reader_t reader;
    void *arg;
    uint32_t limit;
    const char *name;
} tsf_size_aware_rdr_t;

/* light-weight representation of a buffer for writing. */
typedef struct {
    uint8_t *base;
    uint8_t *cur;
    uint8_t *end;
} tsf_resizable_buffer_t;

/* light-weight representation of a buffer */
typedef struct {
    uint8_t *cur,
            *end;
} tsf_light_buffer_t;

typedef struct {
    tsf_reader_t reader;
    void *arg;
} tsf_comparing_writer_t;

struct tsf_sha1_wtr;
typedef struct tsf_sha1_wtr tsf_sha1_wtr_t;

struct tsf_buf_rdr;
struct tsf_buf_wtr;
typedef struct tsf_buf_rdr tsf_buf_rdr_t;
typedef struct tsf_buf_wtr tsf_buf_wtr_t;

/* the zlib rdr/wtr can also do bzip2. */

struct tsf_zip_wtr_attr {
    tsf_zip_mode_t mode;
    
    uint32_t buf_size;
    
    union {
        struct {
            tsf_bool_t advanced_init;
            
            int level;
            
            int method;
            int windowBits;
            int memLevel;
            int strategy;
            
            int reserved1;
            int reserved2;
            int reserved3;
        } z;
        struct {
            int blockSize100k;
            int verbosity;
            int workFactor;
            
            int reserved1;
            int reserved2;
            int reserved3;
        } b;
    } u;
};

struct tsf_zip_rdr_attr {
    struct {
        tsf_bool_t allow;
        
        uint32_t buf_size;
        tsf_bool_t advanced_init;
        int windowBits;
        
        int reserved1;
        int reserved2;
        int reserved3;
    } z;
    struct {
        tsf_bool_t allow;
        
        uint32_t buf_size;
        int verbosity;
        int small;
        
        int reserved1;
        int reserved2;
        int reserved3;
    } b;
};

typedef struct tsf_zip_wtr_attr tsf_zip_wtr_attr_t;
typedef struct tsf_zip_rdr_attr tsf_zip_rdr_attr_t;

struct tsf_zip_rdr;
struct tsf_zip_wtr;
typedef struct tsf_zip_rdr tsf_zip_rdr_t;
typedef struct tsf_zip_wtr tsf_zip_wtr_t;

struct tsf_adpt_rdr;
typedef struct tsf_adpt_rdr tsf_adpt_rdr_t;

/* you're responsible for allocating these guys yourself.  but that does
 * not mean that you can edit them!  treat this as an opaque!  also note
 * that the state is effectively a pointer, so this mark is undefined if
 * serialized... so you cannot use these things to build an external
 * index.  sorry. */
struct tsf_stream_file_mark {
    tsf_off_t offset;
    tsf_serial_in_man_state_t state;
};

typedef struct tsf_stream_file_mark tsf_stream_file_mark_t;

struct tsf_stream_file_input;
struct tsf_stream_file_output;
typedef struct tsf_stream_file_input tsf_stream_file_input_t;
typedef struct tsf_stream_file_output tsf_stream_file_output_t;

enum tsf_fsdb_kind {
    TSF_FSDB_LOCAL,
    TSF_FSDB_REMOTE
};

typedef enum tsf_fsdb_kind tsf_fsdb_kind_t;

struct tsf_fsdk_connection;
typedef struct tsf_fsdb_connection tsf_fsdb_connection_t;

struct tsf_fsdb;
typedef struct tsf_fsdb tsf_fsdb_t;

struct tsf_fsdb_in;
struct tsf_fsdb_out;
typedef struct tsf_fsdb_in tsf_fsdb_in_t;
typedef struct tsf_fsdb_out tsf_fsdb_out_t;

/* get an error code.  this is the error code from the last function that
 * failed in the current thread.  functions in the TSF API always tell you
 * whether or not they failed, usually by returning NULL (if the function
 * returns pointers), tsf_false (if the function returns tsf_bool_t), or -1
 * (if the function returns some sort of integer).  calls to this function
 * made when no error has occurred result in undefined behavior (read:
 * segfault). */
tsf_error_t     tsf_get_error_code(void);

/* given an error code, tells you what the default message is.  the string
 * returned here is truly a constant, so you can safely refer to it
 * indefinitely.  note that if you want a more descriptive message for an
 * error that just occurred, use tsf_get_error().  tsf_get_error() will
 * almost always return a string that contains more information than using
 * tsf_get_message_for_error_code(tsf_get_error_code()).  note that this
 * function does not check if code is a valid TSF error code; if it isn't,
 * bad things will happen. */
const char*     tsf_get_message_for_error_code(tsf_error_t code);

/* get the system errno associated with the current error.  the return
 * value is undefined if the last function that failed did not fail due to
 * a system error. */
int             tsf_get_system_errno(void);

/* get the ZLib error code associated with the current error.  the return
 * value is undefined if the last function that failed did not fail due to
 * a ZLib error. */
int             tsf_get_zlib_error_code(void);

/* get the ZLib error message associated with the current error.  the return
 * value is undefined if the last function that failed did not fail due to
 * a ZLib error.  (NOTE: if a ZLib error actually did happen, this will
 * not be NULL.)*/
const char*     tsf_get_zlib_error_msg(void);

/* get the libbzip2 error code associated with the current error.  the return
 * value is undefined if the last function that failed did not fail due to
 * a libbzip2 error. */
int             tsf_get_libbzip2_error_code(void);

/* get the libbzip2 error message associated with the current error.  the
 * return value is undefined if the last function that failed did not fail
 * due to a libbzip2 error.*/
const char*     tsf_get_libbzip2_error_msg(void);

/* get the signal that killed the child process (only for
 * TSF_E_DEATH_BY_SIGNAL).  the return value is undefined if the last
 * function that failed did not fail due to TSF_E_DEATH_BY_SIGNAL. */
int             tsf_get_deadly_signal(void);

/* get a description of the error.  the string that is returned
 * is not owned by you; it is only valid until the next call to
 * any tsf function other than tsf_get_error_code(), tsf_get_error(),
 * and the other error-getting functions above.  (If the library is
 * compiled with thread-safety then the previous statement refers to
 * the next call by the same thread, rather than the next call
 * globally.)  calls to this function made when no error has
 * occurred (within this thread) result in undefined behavior
 * (quite possibly and very likely a segfault inside tsf_get_error()). */
const char*     tsf_get_error(void);

/* get just the message for the current error code.  this will always
 * be the prefix of what tsf_get_error() returns. */
#define         tsf_get_error_prefix() \
    (tsf_get_message_for_error_code(tsf_get_error_code()))

/* get just the message that is acquired from the source of the error.
 * for errno, this is strerror(errno); , and so on.  this will be NULL for
 * errors in which this does not apply.  if the error is set using the
 * ordinary functions in this header file, this string will fall somewhere
 * in the string returned by tsf_get_error() - but to be safe you should
 * not rely on this fact. */
const char*     tsf_get_error_infix(void);

/* get just the message that was supplied by the user.  this will be
 * NULL if the user did not supply anything.  if non-null, this will
 * always be the suffix of what tsf_get_error() returns. */
const char*     tsf_get_error_suffix(void);

/* specify that an error occurred.  format may be NULL, in which case
 * the only error string is the one that comes with the given error
 * code. */
void            tsf_set_error(tsf_error_t code,
                              const char *format,
                              ...);

/* specify that a system error ocurred.  if format is NULL, then the
 * error message contains the string "System error" and the strerror
 * for the given errno.  if format is non-NULL, then your text
 * gets appended at the end of that. */
void            tsf_set_specific_errno(int cur_errno,
                                       const char *format,...);

/* specify that a system error ocurred.  exactly like
 * tsf_set_specific_errno() except that the global errno is used
 * instead of the given errno. */
void            tsf_set_errno(const char *format,...);

/* specify that some child process died with the given signal. */
void            tsf_set_deadly_signal(int deadly_signal,
                                      const char *format,...);

/* specify that a ZLib error occurred.  if format is NULL, then the
 * error message contains the string "ZLib error" and the error message
 * you provide.  If the format is not NULL, the text specified by the
 * format is appended to that.
 *
 * Note that if ZLib support is not compiled in, this function will
 * mean an immediate abort. */
void            tsf_set_zlib_error(int error_code,
                                   const char *error_msg,
                                   const char *format,
                                   ...);

/* specify that a libbzip2 error occurred.  if format is NULL, then the
 * error message contains the string "libbzip2 error" and an error message
 * that is appropriate to the error code.  If the format is not NULL, the
 * text specified by the format is appended to that.
 *
 * Note that if libbzip2 support is not compiled in, this function will
 * mean an immediate abort. */
void            tsf_set_libbzip2_error(int error_code,
                                       const char *format,
                                       ...);

/* returns the library's version string. */
const char*     tsf_version(void);

/* returns the library's major version number. */
int             tsf_version_major(void);

/* returns the library's minor version number. */
int             tsf_version_minor(void);

/* returns the library's patch version number. */
int             tsf_version_patch(void);

/* creates default limits */
tsf_limits_t*   tsf_limits_create(void);

/* destroys limits, but only if they're not NULL */
void            tsf_limits_destroy(tsf_limits_t *limits);

/* clones limits.  returns NULL if the type it's given is NULL.  this
 * counts as error propagation. */
tsf_limits_t*   tsf_limits_clone(tsf_limits_t *limits);

/* set the allow bit of the particular kind type */
static TSF_inline
void            tsf_limits_set_allow_type_kind(tsf_limits_t *limits,
                                               tsf_type_kind_t kind_code) {
    limits->allow_type_kind[kind_code>>3]|=(1<<(kind_code&7));
}

/* reset the allow bit of the particular kind type */
static TSF_inline
void            tsf_limits_reset_allow_type_kind(tsf_limits_t *limits,
                                                 tsf_type_kind_t kind_code) {
    limits->allow_type_kind[kind_code>>3]&=~(1<<(kind_code&7));
}

/* determine if the allow bit of the particular kind type is set.  always
 * returns tsf_true if limits is NULL. */
static TSF_inline
tsf_bool_t      tsf_limits_is_type_kind_allowed(tsf_limits_t *limits,
                                                tsf_type_kind_t kind_code) {
    if (limits==NULL) {
        return tsf_true;
    }
    return (limits->allow_type_kind[kind_code>>3]&(1<<(kind_code&7)))
           ?tsf_true:tsf_false;
}

/* given a kind code, gives you the textual tsf name for it. */
static TSF_inline
const char*     tsf_type_kind_tsf_name(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return "TSF_TK_INT8";
    case TSF_TK_UINT8:   return "TSF_TK_UINT8";
    case TSF_TK_INT16:   return "TSF_TK_INT16";
    case TSF_TK_UINT16:  return "TSF_TK_UINT16";
    case TSF_TK_INT32:   return "TSF_TK_INT32";
    case TSF_TK_UINT32:  return "TSF_TK_UINT32";
    case TSF_TK_INT64:   return "TSF_TK_INT64";
    case TSF_TK_UINT64:  return "TSF_TK_UINT64";
    case TSF_TK_INTEGER: return "TSF_TK_INTEGER";
    case TSF_TK_LONG:    return "TSF_TK_LONG";
    case TSF_TK_FLOAT:   return "TSF_TK_FLOAT";
    case TSF_TK_DOUBLE:  return "TSF_TK_DOUBLE";
    case TSF_TK_ARRAY:   return "TSF_TK_ARRAY";
    case TSF_TK_STRUCT:  return "TSF_TK_STRUCT";
    case TSF_TK_BIT:     return "TSF_TK_BIT";
    case TSF_TK_CHOICE:  return "TSF_TK_CHOICE";
    case TSF_TK_VOID:    return "TSF_TK_VOID";
    case TSF_TK_STRING:  return "TSF_TK_STRING";
    case TSF_TK_ANY:     return "TSF_TK_ANY";
    default:             return "unknown";
    }
}

/* tells you if a kind code represents a primitive type */
static TSF_inline
tsf_bool_t      tsf_type_kind_is_primitive(tsf_type_kind_t kind_code) {
    return kind_code>=TSF_TK_INT8 && kind_code<=TSF_TK_DOUBLE;
}

/* given a primitive kind code, it returns to you the size in bytes.  this
 * is the native size of the type, and for everything but TSF_TK_INTEGER
 * and TSF_TK_LONG, it's also the encoded size. */
static TSF_inline
uint32_t        tsf_primitive_type_kind_size_of(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return 1;
    case TSF_TK_UINT8:   return 1;
    case TSF_TK_INT16:   return 2;
    case TSF_TK_UINT16:  return 2;
    case TSF_TK_INT32:   return 4;
    case TSF_TK_UINT32:  return 4;
    case TSF_TK_INTEGER: return 4;
    case TSF_TK_INT64:   return 8;
    case TSF_TK_UINT64:  return 8;
    case TSF_TK_LONG:    return 8;
    case TSF_TK_FLOAT:   return 4;
    case TSF_TK_DOUBLE:  return 8;
    default:             return 0;
    }
}

/* returns a range lower bound ID */
static TSF_inline
int32_t         tsf_primitive_type_kind_lower_bound(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return -1;
    case TSF_TK_UINT8:   return 0;
    case TSF_TK_INT16:   return -3;
    case TSF_TK_UINT16:  return 0;
    case TSF_TK_INT32:   return -5;
    case TSF_TK_INTEGER: return -5;
    case TSF_TK_UINT32:  return 0;
    case TSF_TK_INT64:   return -7;
    case TSF_TK_UINT64:  return 0;
    case TSF_TK_LONG:    return -7;
    case TSF_TK_FLOAT:   return -9;
    case TSF_TK_DOUBLE:  return -10;
    default:             return 0;
    }
}

/* returns a range upper bound ID */
static TSF_inline
int32_t         tsf_primitive_type_kind_upper_bound(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return 1;
    case TSF_TK_UINT8:   return 2;
    case TSF_TK_INT16:   return 3;
    case TSF_TK_UINT16:  return 4;
    case TSF_TK_INT32:   return 5;
    case TSF_TK_INTEGER: return 5;
    case TSF_TK_UINT32:  return 6;
    case TSF_TK_INT64:   return 7;
    case TSF_TK_UINT64:  return 8;
    case TSF_TK_LONG:    return 8;
    case TSF_TK_FLOAT:   return 9;
    case TSF_TK_DOUBLE:  return 10;
    default:             return 0;
    }
}

/* returns the precision of a primitive type */
static TSF_inline
int32_t         tsf_primitive_type_kind_precision(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return 8;
    case TSF_TK_UINT8:   return 8;
    case TSF_TK_INT16:   return 16;
    case TSF_TK_UINT16:  return 16;
    case TSF_TK_INT32:   return 32;
    case TSF_TK_UINT32:  return 32;
    case TSF_TK_INTEGER: return 32;
    case TSF_TK_INT64:   return 64;
    case TSF_TK_UINT64:  return 64;
    case TSF_TK_LONG:    return 64;
    case TSF_TK_FLOAT:   return 23;
    case TSF_TK_DOUBLE:  return 52;
    default:             return 0;
    }
}

/* given a primitive kind code, tells you if it is floating point. */
static TSF_inline
tsf_bool_t      tsf_primitive_type_kind_is_float(tsf_type_kind_t kind_code) {
    return kind_code==TSF_TK_FLOAT || kind_code==TSF_TK_DOUBLE;
}

/* same as above. */
#define         tsf_type_kind_is_float(kind_code) \
                    (tsf_primitive_type_kind_is_float(kind_code))

/* given a primitive kind code, tells you if it is floating point. */
static TSF_inline
tsf_bool_t      tsf_primitive_type_kind_is_int(tsf_type_kind_t kind_code) {
    return tsf_type_kind_is_primitive(kind_code) &&
           !tsf_type_kind_is_float(kind_code);
}

/* same as above. */
#define         tsf_type_kind_is_int(kind_code) \
                    (tsf_primitive_type_kind_is_int(kind_code))

/* given a primitive kind code, tells you if it is signed. */
static TSF_inline
tsf_bool_t      tsf_primitive_type_kind_is_signed(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:
    case TSF_TK_INT16:
    case TSF_TK_INT32:
    case TSF_TK_INT64:
    case TSF_TK_INTEGER:
    case TSF_TK_LONG:    return tsf_true;
    default:             return tsf_primitive_type_kind_is_float(kind_code);
    }
}

/* same as above. */
#define         tsf_type_kind_is_signed(kind_code) \
                    (tsf_primitive_type_kind_is_signed(kind_code))

/* given a primitive kind code, gives you the textual name of the
 * C type. */
static TSF_inline
const char*     tsf_primitive_type_kind_c_name(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return "int8_t";
    case TSF_TK_UINT8:   return "uint8_t";
    case TSF_TK_INT16:   return "int16_t";
    case TSF_TK_UINT16:  return "uint16_t";
    case TSF_TK_INT32:   return "int32_t";
    case TSF_TK_UINT32:  return "uint32_t";
    case TSF_TK_INT64:   return "int64_t";
    case TSF_TK_UINT64:  return "uint64_t";
    case TSF_TK_INTEGER: return "tsf_integer_t";
    case TSF_TK_LONG:    return "tsf_long_t";
    case TSF_TK_FLOAT:   return "float";
    case TSF_TK_DOUBLE:  return "double";
    default:             return "unknown";
    }
}

/* given a primitive kind code, gives you the textual name of the type
 * using lower case. */
static TSF_inline
const char*     tsf_primitive_type_kind_lc_name(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return "int8";
    case TSF_TK_UINT8:   return "uint8";
    case TSF_TK_INT16:   return "int16";
    case TSF_TK_UINT16:  return "uint16";
    case TSF_TK_INT32:   return "int32";
    case TSF_TK_UINT32:  return "uint32";
    case TSF_TK_INT64:   return "int64";
    case TSF_TK_UINT64:  return "uint64";
    case TSF_TK_INTEGER: return "integer";
    case TSF_TK_LONG:    return "long";
    case TSF_TK_FLOAT:   return "float";
    case TSF_TK_DOUBLE:  return "double";
    default:             return "unknown";
    }
}

/* given a kind code, gives you the textual lower-case name */
static TSF_inline
const char*     tsf_type_kind_lc_name(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_ARRAY:  return "array";
    case TSF_TK_STRUCT: return "struct";
    case TSF_TK_BIT:    return "bit";
    case TSF_TK_CHOICE: return "choice";
    case TSF_TK_VOID:   return "void";
    case TSF_TK_STRING: return "string";
    case TSF_TK_ANY:    return "any";
    default:            return tsf_primitive_type_kind_lc_name(kind_code);
    }
}

/* given a primitive kind code, gives you the ntoh/hton name that
 * we use for it. */
static TSF_inline
const char*     tsf_primitive_type_kind_nh_name(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_INT8:    return "c";
    case TSF_TK_UINT8:   return "c";
    case TSF_TK_INT16:   return "s";
    case TSF_TK_UINT16:  return "s";
    case TSF_TK_INT32:   return "l";
    case TSF_TK_UINT32:  return "l";
    case TSF_TK_INT64:   return "ll";
    case TSF_TK_UINT64:  return "ll";
    case TSF_TK_INTEGER: return "l";
    case TSF_TK_LONG:    return "ll";
    case TSF_TK_FLOAT:   return "f";
    case TSF_TK_DOUBLE:  return "d";
    default:             return "unknown";
    }
}

/* does a primitive instance of.  one primitive is instance of another
 * if the domain of the first is contained in the domain of the second.
 * if you wish to assign one primitive to another, then to avoid
 * a loss of significant or an overflow, you must ensure that the
 * source is instance of the destination. */
tsf_bool_t      tsf_primitive_type_kind_instanceof(tsf_type_kind_t one,
                                                   tsf_type_kind_t two);

/* returns a primitive type kind that matches the requires specification. */
tsf_type_kind_t tsf_primitive_type_kind_for_spec(int32_t lower_bound,
						 int32_t upper_bound,
						 int32_t precision);

/* returns the smallest common subtype of the two types, or void if
 * a common subtype does not exist. */
tsf_type_kind_t tsf_primitive_type_kind_mix(tsf_type_kind_t one,
					    tsf_type_kind_t two);

/* tells you if types with the given kind type are containers. */
static TSF_inline
tsf_bool_t      tsf_type_kind_is_container(tsf_type_kind_t kind_code) {
    switch (kind_code) {
    case TSF_TK_ARRAY:
    case TSF_TK_STRUCT:
    case TSF_TK_CHOICE: return tsf_true;
    default:            return tsf_false;
    }
}

/* tells you if types with the given kind type can store comments. */
static TSF_inline
tsf_bool_t      tsf_type_kind_is_commentable(tsf_type_kind_t kind_code) {
    return tsf_type_kind_is_container(kind_code);
}

/* create a type table (you should almost never have to do this
 * directly.) */
tsf_type_table_t* tsf_type_table_create(void);

/* copy the contents of one type table into another */
tsf_bool_t      tsf_type_table_copy(tsf_type_table_t *dest,
                                    tsf_type_table_t *src);

/* destroy a type table */
void            tsf_type_table_destroy(tsf_type_table_t *tt);

/* write the type table */
tsf_bool_t      tsf_type_table_write(tsf_type_table_t *tt,
                                     tsf_writer_t writer,
                                     void *arg);

/* append a type to the type table */
tsf_bool_t      tsf_type_table_append(tsf_type_table_t *tt,
                                      const char *name,
                                      tsf_type_t *type);

/* remove a type from the type table. note that this changes the
 * indexing of the type table if this isn't the last element.
 * the only error that this could report is TSF_E_ELE_NOT_FOUND
 * if the element wasn't in the table. */
tsf_bool_t      tsf_type_table_remove(tsf_type_table_t *tt,
                                      const char *name);

/* find a named type node in the table using the name. */
tsf_named_type_t* tsf_type_table_find_by_name(tsf_type_table_t *tt,
                                              const char *name);

/* find a named type node in the table using the index. */
tsf_named_type_t* tsf_type_table_find_by_index(tsf_type_table_t *tt,
                                               uint32_t index);

/* get the number of elements in the type table. */
uint32_t        tsf_type_table_get_num_elements(tsf_type_table_t *tt);

/* get the hash for a type table.  you are guaranteed that this is just
 * the sum of the hash codes of the constituent tsf_named_type_t objects.
 * hence, you can always determine what the hash code is after an
 * apppend operation. */
int             tsf_type_table_get_hash(tsf_type_table_t *tt);

/* determine is the type table contains any dynamic types. */
tsf_bool_t      tsf_type_table_contains_dynamic(tsf_type_table_t *tt);

/* compare two type tables. */
tsf_bool_t      tsf_type_table_compare(tsf_type_table_t *a,
                                       tsf_type_table_t *b);

/* create a table of types. */
tsf_type_in_map_t* tsf_type_in_map_create(void);

/* create a table of types inside of the provided region. */
tsf_type_in_map_t* tsf_type_in_map_create_in(void *region);

/* return the global empty table of types.  this one should never
 * be modified. */
tsf_type_in_map_t* tsf_type_in_map_get_empty_singleton(void);

/* clone a table of types. */
tsf_type_in_map_t* tsf_type_in_map_clone(tsf_type_in_map_t *types);

/* clone a table of types, creating the new one in a region. */
tsf_type_in_map_t* tsf_type_in_map_clone_in(tsf_type_in_map_t *types,
                                            void *region);

/* destroy a table of types.  has no effect if passed the
 * empty singleton.  cannot be used on region-allocated tables. */
void            tsf_type_in_map_destroy(tsf_type_in_map_t *types);

/* prepare some number of slots for types in the table.  a call to this
 * function serves as a hint and so does not have any guaranteed effect.
 * as such, success in this function should not be taken to mean that
 * the subsequent append operations are guaranteed to succeed and
 * failure in this operation should not be taken to mean that any
 * number of subsequent appends should fail.
 *
 * (In other words, the return value can be safely ignored.  It is there
 * to aid in debugging only.) */
tsf_bool_t      tsf_type_in_map_prepare(tsf_type_in_map_t *types,
                                        uint32_t num);

/* append a type to the table.  the index will be the number of
 * types prior to the append. */
tsf_bool_t      tsf_type_in_map_append(tsf_type_in_map_t *types,
                                       tsf_type_t *type);

/* returns the number of types in the table. */
uint32_t        tsf_type_in_map_get_num_types(tsf_type_in_map_t *types);

/* returns the type at the given index in the table. */
tsf_type_t*     tsf_type_in_map_get_type(tsf_type_in_map_t *types,
                                         uint32_t index);

/* reduces the size of the type table to the specified new size.
 * unpredictably horrible things will happen if the new size is
 * greater than the current size. */
void            tsf_type_in_map_truncate(tsf_type_in_map_t *types,
                                         uint32_t new_size);

/* compare two type tables.  two type tables are equal if they
 * have the same number of types, and for each index, of the types
 * at that index are the same according to tsf_type_compare(). */
tsf_bool_t      tsf_type_in_map_compare(tsf_type_in_map_t *a,
                                        tsf_type_in_map_t *b);

/* creates a new empty type map. */
tsf_type_out_map_t* tsf_type_out_map_create(void);

/* destroy a type map. */
void            tsf_type_out_map_destroy(tsf_type_out_map_t *type_map);

/* look up a type code asserting that it must be found.  an error is
 * returned if it is not found. */
tsf_bool_t      tsf_type_out_map_find_type_code(tsf_type_out_map_t *type_map,
                                                tsf_type_t *type,
                                                uint32_t *type_code);

/* look up a type code, potentially creating a new one.  fails only if
 * a type code had to be created AND that creation failed. */
tsf_bool_t      tsf_type_out_map_get_type_code(tsf_type_out_map_t *type_map,
                                               tsf_type_t *type,
                                               uint32_t *type_code,
                                               tsf_bool_t *created);

/* destroy a type out map and produce a type in map thingy from it. */
tsf_type_in_map_t* tsf_type_in_map_from_type_out_map(
                                                tsf_type_out_map_t *type_map);

/* destroy a type out map and produce a type in map thingy from it.
 * creates the type in thingy in the provided region. */
tsf_type_in_map_t* tsf_type_in_map_from_type_out_map_in(
                                                tsf_type_out_map_t *type_map,
                                                void *region);

/* create an empty random-access type manager. */
tsf_ra_type_man_t* tsf_ra_type_man_create(void);

/* parse a random-access type manager. */
tsf_ra_type_man_t* tsf_ra_type_man_read(tsf_reader_t reader,
					void *arg);

/* destroy a random-access type manager. */
void            tsf_ra_type_man_destroy(tsf_ra_type_man_t *man);

/* write the stuff in the type manager out to somewhere. */
tsf_bool_t      tsf_ra_type_man_write(tsf_ra_type_man_t *man,
				      tsf_writer_t writer,
				      void *arg);

/* inset a type into the type manager.  do this only if you know
   that the type is not already in the type manager. */
tsf_bool_t      tsf_ra_type_man_insert(tsf_ra_type_man_t *man,
				       tsf_type_t *type,
				       uint32_t type_code,
				       uint64_t ref_count);

/* remove a type from the type manager.  do this only if you know
   that the type is in the type manager. */
void            tsf_ra_type_man_remove(tsf_ra_type_man_t *man,
				       tsf_type_t *type);

/* find a type in the type manager using the type code.  if it's
   not found, NULL will be retorned and an error code will be set. */
tsf_type_t*     tsf_ra_type_man_find(tsf_ra_type_man_t *man,
				     uint32_t type_code);

/* determine if the type manager has the given type. */
tsf_bool_t      tsf_ra_type_man_has_type(tsf_ra_type_man_t *man,
					 tsf_type_t *type);

/* get the type code for a type.  only call this if you know that
   the type manager has the type. */
uint32_t        tsf_ra_type_man_get_type_code(tsf_ra_type_man_t *man,
					      tsf_type_t *type);

/* increase the reference count for the given type.  if the type
   is not in the type manager, it is added to the type manager,
   assigned a type code, and given a reference count of 1. */
tsf_bool_t      tsf_ra_type_man_get_reference(tsf_ra_type_man_t *man,
					      tsf_type_t *type);

/* decrease the reference count for a given type.  if the reference
   count drops to 0, the type is removed. */
void            tsf_ra_type_man_remove_reference(tsf_ra_type_man_t *man,
						 tsf_type_t *type);

uint32_t        tsf_ra_type_man_num_types(tsf_ra_type_man_t *man);

/* determine if the type manager contains all of the types used by
   the given buffer. */
tsf_bool_t      tsf_ra_type_man_has_buf_types(tsf_ra_type_man_t *man,
					      tsf_buffer_t *buf);

/* increase the reference count for the types in the given buffer.
   creates any types that don't exist. */
tsf_bool_t      tsf_ra_type_man_get_buf_references(tsf_ra_type_man_t *man,
						   tsf_buffer_t *buf);

/* decrease the reference count for the types in the given buffer. */
void            tsf_ra_type_man_remove_buf_references(tsf_ra_type_man_t *man,
						      tsf_buffer_t *buf);

/* calculate the number of type codes on the free-list. */
uint32_t        tsf_ra_type_man_get_type_code_free_list_size(
						     tsf_ra_type_man_t *man);

/* create a type object.  this function will not accept TSF_TK_ARRAY.
 * if you want to create a type with kind ccode TSF_TK_ARRAY, use
 * tsf_type_create_array().
 *
 * this can only fail due to memory allocation failure or if the kind_code
 * is invalid. */
tsf_type_t*     tsf_type_create(tsf_type_kind_t kind_code);

/* create an array type object.
 *
 * this can only fail due to memory allocation failure or if element_type is
 * NULL, in which case it returns without setting error. */
tsf_type_t*     tsf_type_create_array(tsf_type_t *element_type);

/* create a struct type all at once.  note that you can use
 * tsf_type_create() in combination with tsf_struct_type_append() to do
 * what this method does.  also, you can safely use
 * tsf_struct_type_append() on the type returned from this method.
 *
 * the way you would actually use this is to pass struct element names
 * paired with types.  for example:
 *
 * tsf_type_t *my_type =
 *     tsf_type_create_struct("x",
 *                            tsf_type_create(TSF_TK_FLOAT),
 *                            "y",
 *                            tsf_type_create(TSF_TK_FLOAT),
 *                            NULL);
 *
 * note that you must pass NULL after the last pair.  otherwise you'll
 * get random crashes and stuff.
 *
 * the types passed to this function become owned by the function
 * regardless of success or error. */
tsf_type_t*     tsf_type_create_struct(const char *first_name,
                                       ...);

/* the same as the above but tailored to allow you to use it more easily
 * from your own subrutines that use variable args. */
tsf_type_t*     tsf_type_create_structv(const char *first_name,
                                        va_list lst);

/* create a choice type all at once.  note that you can use
 * tsf_type_create() in combination with tsf_choice_type_append() to do
 * what this method does.  also, you can safely use
 * tsf_choice_type_append() on the type returned from this method.
 *
 * the way you would actually use this is to pass struct element names
 * paired with types.  for example:
 *
 * tsf_type_t *my_type =
 *     tsf_type_create_choice("x",
 *                            tsf_type_create(TSF_TK_FLOAT),
 *                            "y",
 *                            tsf_type_create(TSF_TK_FLOAT),
 *                            NULL);
 *
 * note that you must pass NULL after the last pair.  otherwise you'll
 * get random crashes and stuff.
 *
 * the types passed to this function become owned by the function
 * regardless of success or error. */
tsf_type_t*     tsf_type_create_choice(const char *first_name,
                                       ...);

/* the same as the above but tailored to allow you to use it more easily
 * from your own subrutines that use variable args. */
tsf_type_t*     tsf_type_create_choicev(const char *first_name,
                                        va_list lst);

/* read a type. */
tsf_type_t*     tsf_type_read(tsf_reader_t reader,
                              void *arg,
                              tsf_limits_t *limits);

/* write a type. */
tsf_bool_t      tsf_type_write(tsf_type_t *type,
                               tsf_writer_t writer,
                               void *arg);

/* duplicate a type.  this gives you a copy of the type that must be
 * separately destroyed.  so, if you wish to retain the type after a
 * call to a function that might destroy it, call this first.  this
 * will only return NULL if the type you pass it is NULL, for the
 * purpose of error propagation.  this will never create an error. */
tsf_type_t*     tsf_type_dup(tsf_type_t *type);

/* destroy a type object. */
void            tsf_type_destroy(tsf_type_t *type);

/* get the size in bytes of the type.  note that this operation cannot
 * fail.  note further that the size is lazily computed, so the first time
 * you call this, it will be expensive, but it will become cheap for all
 * subsequent calls. */
uint32_t        tsf_type_get_byte_size(tsf_type_t *type);

/* verify a type against some limits; this is a fairly costly process,
 * probably at least as expensive as writing or reading a type.  tsf_true
 * is returned if the type is OK.  if tsf_false is returned,
 * TSF_E_NOT_ALLOWED is set.  if limits is NULL, all types should
 * verify */
tsf_bool_t      tsf_type_verify(tsf_type_t *type,
                                tsf_limits_t *limits);

/* get a hash code for a type. note that if you want to hash types, you
 * may benefit from memoizing the type first; then you can use a pointer
 * hash. */
int             tsf_type_get_hash(tsf_type_t *type);

/* memoize the given type and return the memo master.  the memo master
 * for two types for ehich tsf_type_compare returns true should be
 * identical, i.e. point to the same tsf_type object.  this does not
 * consume the type you pass it.  the returned type is not owned by you
 * and is only guaranteed to stay alive as long as you don't destroy
 * the type you passed.  therefore you might want to do:
 *
 * memo_type = tsf_type_dup(tsf_type_memo(type));
 *
 * if you want to be safe.  if you want to relinquish ownership of the
 * original type and have ownership of the memo, you probably want:
 *
 * memo_type = tsf_type_dup(tsf_type_memo(type));
 * tsf_type_destroy(type);
 *
 * note that it's possible for tsf_type_memo() to simply return the
 * type it was passed, if that is the only extant type that looks like
 * that. also note that memoization ignores the native mapping, so
 * using the above memo-and-relinquish dance is not useful for types
 * that have native mapping. however, it's still useful to memoize such
 * types if you want to speed up hashing and comparing. */
tsf_type_t*     tsf_type_memo(tsf_type_t *type);

/* determine if the type is dynamic (that is, if it has any anies in
   it) */
tsf_bool_t      tsf_type_is_dynamic(tsf_type_t *type);

/* compare two types.  neither type becomes owned by the function; both
 * are still your responsibility after the call.  returns tsf_true
 * if the types are equal, tsf_false otherwise. */
tsf_bool_t      tsf_type_compare(tsf_type_t *a, tsf_type_t *b);

/* determines if the first type is an instance of the second type.
 * the instanceof relationship is defined in a big comment up above
 * the definition of the tsf_type_kind enumeration. */
tsf_bool_t      tsf_type_instanceof(tsf_type_t *one,
                                    tsf_type_t *two);

/* get the kind code for a type object. */
tsf_type_kind_t tsf_type_get_kind_code(tsf_type_t *type);

/* if the size of the type is known statically, it returns it.  otherwise,
 * it returns UINT32_MAX. */
uint32_t        tsf_type_get_static_size(tsf_type_t *type);

/* tells you the comment on the type.  may be NULL if there is no comment
 * or if the type is not commentable. */
const char*     tsf_type_get_comment(tsf_type_t *type);

/* tells you if the type has a comment */
tsf_bool_t      tsf_type_has_comment(tsf_type_t *type);

/* sets the comment on the type.  will return TSF_E_BAD_TYPE_KIND if the
 * type is not commentable. */
tsf_bool_t      tsf_type_set_comment(tsf_type_t **type,
                                     const char *comment);

/* tells you the name of the type.  may be NULL if there is no name or if
 * the type is not commentable. */
const char*     tsf_type_get_name(tsf_type_t *type);

/* tells you if the type has a name. */
tsf_bool_t      tsf_type_has_name(tsf_type_t *type);

/* sets the name of the type.  will return TSF_E_BAD_TYPE_KIND if the
 * type is not commentable. */
tsf_bool_t      tsf_type_set_name(tsf_type_t **type,
                                  const char *name);

/* tells you if the version is non-zero (i.e. not equal to 0.0.0) */
tsf_bool_t      tsf_type_has_version(tsf_type_t *type);

/* tells you the raw version of the type. */
uint32_t        tsf_type_get_raw_version(tsf_type_t *type);

/* tells you the major version of the type. */
uint16_t        tsf_type_get_major_version(tsf_type_t *type);

/* tells you the minor version of the type. */
uint8_t         tsf_type_get_minor_version(tsf_type_t *type);

/* tells you the patch version of the type. */
uint8_t         tsf_type_get_patch_version(tsf_type_t *type);

/* sets the raw version of the type.  will return TSF_E_BAD_TYPE_KIND if the
 * type is not commentable. */
tsf_bool_t      tsf_type_set_raw_version(tsf_type_t **type,
                                         uint32_t version);

/* sets the version of the type.  will return TSF_E_BAD_TYPE_KIND if the
 * type is not commentable. */
tsf_bool_t      tsf_type_set_version(tsf_type_t **type,
                                     uint16_t major,
                                     uint8_t minor,
                                     uint8_t patch);
                              
/* get the element type of an array type object.  the returned
 * object is not owned by you; it is owned by the array type. */
tsf_type_t*     tsf_array_type_get_element_type(tsf_type_t *type);

/* append a new field to a struct type.  the string argument
 * does not become owned by the function; it is still your
 * responsibility to free it.  note that the string cannot
 * exceed TSF_STR_SIZE; if it does, an error will be returned.
 * this will propagate errors on the *ele_type.
 *
 * this only fails if memory allocation fails, if the name string
 * is too long, or ele_type is NULL. */
tsf_bool_t      tsf_struct_type_append(tsf_type_t **type,
                                       const char *name,
                                       tsf_type_t *ele_type);

/* set the 'optional' attribute of a structure field. */
tsf_bool_t      tsf_struct_type_set_optional(tsf_type_t **type,
					     const char *name,
					     tsf_bool_t optional);

/* set the comment for a structure field. */
tsf_bool_t      tsf_struct_type_set_comment(tsf_type_t **type,
                                            const char *name,
                                            const char *comment);

/* remove a struct field. note that this changes the ordering of
 * fields. */
tsf_bool_t      tsf_struct_type_remove(tsf_type_t **type,
                                       const char *name);

/* find an element in a struct type.  this search should be
 * fast, as it uses a hashtable. */
tsf_type_t*     tsf_struct_type_find(tsf_type_t *type,
                                     const char *name);

/* find the pair element in a struct type; the pair
 * element contains more info.  this search should be fast,
 * as it uses a hashtable. */
tsf_named_type_t* tsf_struct_type_find_node(tsf_type_t *type,
                                            const char *name);

/* returns the number of elements in the struct. */
uint32_t        tsf_struct_type_get_num_elements(tsf_type_t *type);

/* get the ith element in the struct */
tsf_named_type_t *tsf_struct_type_get_element(tsf_type_t *type,
                                              uint32_t index);

/* get the name of the ith element in the struct */
const char*     tsf_struct_type_get_element_name(tsf_type_t *type,
                                                 uint32_t index);

/* get the type of the ith element in the struct */
tsf_type_t*     tsf_struct_type_get_element_type(tsf_type_t *type,
                                                 uint32_t index);

/* append a new field to a choice type.  the string argument
 * does not become owned by the function; it is still your
 * responsibility to free it.  note that the string cannot
 * exceed TSF_STR_SIZE; if it does, an error will be returned.
 * this will propagate errors on the *ele_type. */
tsf_bool_t      tsf_choice_type_append(tsf_type_t **type,
                                       const char *name,
                                       tsf_type_t *ele_type);

/* set the comment for a choice field. */
tsf_bool_t      tsf_choice_type_set_comment(tsf_type_t **type,
                                            const char *name,
                                            const char *comment);

/* find an element in a choice type.  this search should be
 * fast, as it uses a hashtable. */
tsf_type_t*     tsf_choice_type_find(tsf_type_t *type,
                                     const char *name);

/* find the pair element in a choice type; the pair
 * element contains more info.  this search should be fast,
 * as it uses a hashtable. */
tsf_named_type_t* tsf_choice_type_find_node(tsf_type_t *type,
                                            const char *name);

/* returns the number of elements in the choice. */
uint32_t        tsf_choice_type_get_num_elements(tsf_type_t *type);

/* get the ith element in the choice */
tsf_named_type_t* tsf_choice_type_get_element(tsf_type_t *type,
                                              uint32_t index);

/* get the name of the ith element in the choice */
const char*     tsf_choice_type_get_element_name(tsf_type_t *type,
                                                 uint32_t index);
		     
/* get the type of the ith element in the choice */
tsf_type_t*     tsf_choice_type_get_element_type(tsf_type_t *type,
                                                 uint32_t index);

/* returns true if the choice has any non-void types. */
tsf_bool_t      tsf_choice_type_has_non_void(tsf_type_t *type);

/* returns true if:
 * -> the latter choice type has at least as many elements as the
 *    first, and
 * -> for each of the elements in the first choice type, the element
 *    in the second choice type that has the same index also has the
 *    same name.
 *
 * note that the element types do not have to match for this
 * predicate to hold.  however, if you are interested in adding an
 * instanceof constraint for the corresponding elements of the two
 * choice types, you can do so by performing an instanceof test on
 * a and b. */
tsf_bool_t      tsf_choice_type_uses_same_indices(tsf_type_t *a,
                                                  tsf_type_t *b);

/* creates a named type object with the given name, type, and index.
 * failure to create this thing results in the type being destroyed.
 * the has_offset field is set to false, the offset field is set to 0,
 * and the comment field is set to NULL. */
tsf_named_type_t* tsf_named_type_create(const char *name,
                                        tsf_type_t *type,
                                        uint32_t index);

/* destroy a named type object.  destroys the type.  frees the
 * comment if there is one. */
void            tsf_named_type_destroy(tsf_named_type_t *node);

/* returns the name of the type in the node.  what gets returned
 * should be treated as a constant string that you do not own. */
const char*     tsf_named_type_get_name(tsf_named_type_t *node);

/* returns the type that the node holds.  what gets returned is
 * of type tsf_type_t* and should be treated as constant that you
 * do not own. */
tsf_type_t*     tsf_named_type_get_type(tsf_named_type_t *node);

/* returns the index of a named type node within its structure
 * type. */
uint32_t        tsf_named_type_get_index(tsf_named_type_t *node);
		     
/* set the optional attribute */
tsf_bool_t      tsf_named_type_get_optional(tsf_named_type_t *node);

/* set the optional attribute */
void            tsf_named_type_set_optional(tsf_named_type_t *node,
                                            tsf_bool_t optional);

/* returns the comment of a names type.  may be NULL, indicating that
 * no comment was ever set */
const char*     tsf_named_type_get_comment(tsf_named_type_t *node);

/* tells you if the named type has a comment */
tsf_bool_t      tsf_named_type_has_comment(tsf_named_type_t *node);

/* set the comment on a named type.  deletes the old one if
 * necessary. */
tsf_bool_t      tsf_named_type_set_comment(tsf_named_type_t *node,
                                           const char *comment);

/* get the hash code for the named type.  you are guaranteed that types
 * of type TSF_TK_STRUCT and TSF_TK_CHOICE will use the sum of these hash
 * codes as part of computing their own hash code.  you're guaranteed
 * that tsf_type_table_table_t will use the sum of these hash codes as
 * part of computing their own hash code. */
int             tsf_named_type_get_hash(tsf_named_type_t *node);

/* create a buffer.  the type is duped by the buffer.  the types table
 * becomes owned.  and, to make things more interesting, the boolean
 * argument determines if the void* is owned by the buffer.  if it is
 * owned by the buffer, then if this function fails, the buffer will be
 * freed. */
tsf_buffer_t*   tsf_buffer_create(tsf_type_t *type,
                                  tsf_type_in_map_t *types,
                                  void *data,
                                  uint32_t size,
                                  tsf_bool_t keep);

/* create a buffer in a region.  exactly like tsf_buffer_create()
 * except that the buffer is created in a region.  it is assumed
 * that the void* data is allocated in the same region and that
 * tsf_type_in_map_t* is, too.  note that the tsf_bool_t argument
 * is omitted since tsf_buffer_destroy() is not applicable to
 * region-allocated buffers. */
tsf_buffer_t*   tsf_buffer_create_in(tsf_type_t *type,
                                     tsf_type_in_map_t *types,
                                     void *data,
                                     uint32_t size,
                                     void *region);

/* get the empty buffer singleton. */
tsf_buffer_t*   tsf_buffer_get_empty_singleton(void);

/* clone the buffer.  the resulting buffer has the keep bit set to
 * true.  note that this is a deep clone, so that the actual data
 * itself is also copied.  this means that this operation, and any
 * operations on the new buffer have no effect on the original buffer. */
tsf_buffer_t*   tsf_buffer_clone(tsf_buffer_t *buf);

/* clone the buffer into the given region.  exactly like
 * tsf_buffer_clone() except for the region part. */
tsf_buffer_t*   tsf_buffer_clone_in(tsf_buffer_t *buf,
                                    void *region);

/* duplicate a buffer.  this will give you access to the given buffer
 * until you call tsf_buffer_destroy on it.  if the buffer was heap-
 * allocated and has the keep bit set to true, this operation will be
 * very fast - it will increment a count and maybe do some simple
 * locking/unlocking.  if the buffer was region allocated or if the
 * keep bit was set to false, then this has the same cost as
 * tsf_buffer_clone().  as such, you must do error checking after a
 * a call to tsf_buffer_dup(). */
tsf_buffer_t*   tsf_buffer_dup(tsf_buffer_t *buf);

/* destroy a buffer.  cannot be used on region allocated buffers. */
void            tsf_buffer_destroy(tsf_buffer_t *buf);

/* tells you the size of the buffer */
uint32_t        tsf_buffer_get_size(tsf_buffer_t *buf);

/* gives you the data payload of the buffer */
const void*     tsf_buffer_get_data(tsf_buffer_t *buf);

/* read a buffer from another buffer and associate it with the given
 * type.  this is also a bit of a trusted routine...
 * if the region pointer is non-NULL, then the resulting buffer will
 * be region-allocated.  otherwise it will be malloc-allocated and
 * you will be expected to call tsf_buffer_destroy() at some point. */
tsf_buffer_t*   tsf_buffer_read_from_buf(tsf_type_in_map_t *enclosing_types,
                                         uint8_t **cur,
                                         uint8_t *end,
                                         void *region);

/* skip a buffer (in a buffer, not on disk).  takes care of skipping the
   translation table if dealing with a dynamic type. */
tsf_bool_t      tsf_buffer_skip_in_buf(tsf_type_in_map_t *enclosing_types,
                                       uint8_t **cur,
                                       uint8_t *end);

/* calculate the size that would be required to write this buffer into
 * another buffer. */
uint32_t        tsf_buffer_calc_size(tsf_buffer_t *buf);

/* write a buffer into another buffer */
tsf_bool_t      tsf_buffer_write_to_buf(tsf_buffer_t *buf,
                                        tsf_type_out_map_t *type_map,
                                        uint8_t **target);

/* write a buffer and its types to the given writer.  this is not an
 * optimized operation, but it should be fine if the size of the buffer's
 * data is large compared to the size of the type. */
tsf_bool_t      tsf_buffer_write_whole(tsf_buffer_t *buf,
                                       tsf_writer_t writer,
                                       void *arg);

/* read a buffer and its types from the given reader. */
tsf_buffer_t*   tsf_buffer_read_whole(tsf_reader_t reader,
                                      void *arg,
                                      tsf_limits_t *limits);

/* get the SHA1 hash of the whole buffer (including types).  this is not
 * an optimized operation.  note that the result is placed in the
 * uint32_t* buffer, which must be big enough to hold 5 uint32_t's. */
tsf_bool_t      tsf_buffer_calc_sha1(tsf_buffer_t *buf,
                                     uint32_t *result);

/* compare two buffers.  two buffers are equal if they contain exactly
 * the same payload bytes, if their types are the same according to
 * tsf_type_compare(), and if their type tables are the same according
 * to tsf_type_in_map_compare(). */
tsf_bool_t      tsf_buffer_compare(tsf_buffer_t *a,
                                   tsf_buffer_t *b);

/* given a buffer, gives you the type. */
tsf_type_t*     tsf_buffer_get_type(tsf_buffer_t *buf);

/* given a buffer, tells you if its type is an instance of another type */
tsf_bool_t      tsf_buffer_instanceof(tsf_buffer_t *buf,
                                      tsf_type_t *type);

/* given a buffer, gives you the shared types. */
tsf_type_in_map_t* tsf_buffer_get_shared_types(tsf_buffer_t *buf);

/* get the size of a bitvector that has some number of bits. */
static TSF_inline
size_t          tsf_sizeof_bitvector(uint32_t num_bits) {
    return (num_bits + 7) >> 3;
}

/* set a bit in a bitvector. */
static TSF_inline
void            tsf_bitvector_set(tsf_native_bitvector_t *bitvector,
                                  size_t bit_index,
                                  tsf_bool_t value) {
    uint8_t *byte = bitvector->bits + (bit_index >> 3);
    uint8_t bit_mask = 1 << (bit_index & 7);
    if (value)
        *byte |= bit_mask;
    else
        *byte &= ~bit_mask;
}

/* get a bit from a bitvector. */
static TSF_inline
tsf_bool_t      tsf_bitvector_get(tsf_native_bitvector_t *bitvector,
                                  size_t bit_index) {
    uint8_t *byte = bitvector->bits + (bit_index >> 3);
    uint8_t bit_mask = 1 << (bit_index & 7);
    return (*byte & bit_mask) != 0;
}

/* create an serial input manager.  a serial input manager expects zero or
 * more type headers to precede data.  instances of this type are not
 * thread-safe; if you want to use one from multiple threads then you need
 * to use locking around all calls. */
tsf_serial_in_man_t* tsf_serial_in_man_create(tsf_reader_t reader,
                                              void *arg,
                                              tsf_limits_t *limits);

/* destroy an input manager. */
void            tsf_serial_in_man_destroy(tsf_serial_in_man_t *in_man);

/* force the serial input manager to read a TSF_SP_C_RESET_TYPES command.
 * if this command is not what is found in the input stream, an error is
 * reported.  provided that a TSF_SP_C_RESET_TYPES command is in fact
 * found, the type mapping will be reset.
 *
 * will set error to TSF_E_UNEXPECTED_EOF if there weren't enough bytes for
 * a TSF_SP_C_RESET_TYPES command.
 *
 * will set error to TSF_E_PARSE_ERROR if the bytes read were not the
 * currect ones (effectively, this is complaining of a bad magic number).*/
tsf_bool_t      tsf_serial_in_man_read_reset_types(tsf_serial_in_man_t *in_man);

/* take the given file descriptor and read the reset types command from
 * it.  returns true if there was a reset types command and false on
 * error.  returns the same errors as tsf_serial_in_man_read_reset_types()
 * unless something really weird happens. */
tsf_bool_t      tsf_serial_in_man_read_reset_types_from_fd(int fd);

/* take the given file, open it, and invoke
 * tsf_serial_in_man_read_reset_types_from_fd on it. */
tsf_bool_t      tsf_serial_in_man_read_reset_types_from_file(
                                                        const char *filename);

/* register a callback that will get called every time the input manager
 * encounters a new type. */
tsf_bool_t      tsf_serial_in_man_register_type_cback(
                                                tsf_serial_in_man_t *in_man,
                                                tsf_type_cback_t cback,
                                                void *arg);

/* unregister all type callbacks that match the given function pointer
 * and void* argument. */
uint32_t        tsf_serial_in_man_unregister_type_cback(
                                                tsf_serial_in_man_t *in_man,
                                                tsf_type_cback_t cback,
                                                void *arg);

/* read data using the serial input manager. */
tsf_buffer_t*   tsf_serial_in_man_read(tsf_serial_in_man_t *in_man);

/* get the state of the input manager right now.  if you are using random
 * access files and you wish to seek back to the position you're in now,
 * you also need to save the state of the input manager and restore it
 * right after the seek. */
tsf_serial_in_man_state_t tsf_serial_in_man_get_state(tsf_serial_in_man_t *in_man);

/* set the state of the input manager.  do this whenever you seek the file
 * that the serial input manager is being used to read. */
void            tsf_serial_in_man_set_state(tsf_serial_in_man_t *in_man,
                                            tsf_serial_in_man_state_t state);

/* create an output manager.  instances of this type are not thread-safe; if
 * you want to use one from multiple threads then you need to lock around
 * calls. */
tsf_serial_out_man_t* tsf_serial_out_man_create(tsf_writer_t writer,
                                                void *arg);

/* destroy an output manager. */
void            tsf_serial_out_man_destroy(tsf_serial_out_man_t *out_man);

tsf_bool_t      tsf_serial_out_man_reset_types(tsf_serial_out_man_t *out_man);

/* write data using an output manager.  this will do error
 * propagation on *buf. */
tsf_bool_t      tsf_serial_out_man_write(tsf_serial_out_man_t *out_man,
                                         tsf_buffer_t *buf);

/* create a tsf data structure using the given type.  the type
 * gets dupped for the returned structure.  this will
 * do error propagation on *type.
 *
 * Note that this function should never be used to create
 * primitives, bits, voids, or strings. */
tsf_reflect_t*  tsf_reflect_create(tsf_type_t *type);

/* create a tsf data structure to hold a value of the given type. */
tsf_reflect_t*  tsf_reflect_create_int8_t(int8_t value);
tsf_reflect_t*  tsf_reflect_create_uint8_t(uint8_t value);
tsf_reflect_t*  tsf_reflect_create_int16_t(int16_t value);
tsf_reflect_t*  tsf_reflect_create_uint16_t(uint16_t value);
tsf_reflect_t*  tsf_reflect_create_int32_t(int32_t value);
tsf_reflect_t*  tsf_reflect_create_uint32_t(uint32_t value);
tsf_reflect_t*  tsf_reflect_create_int64_t(int64_t value);
tsf_reflect_t*  tsf_reflect_create_uint64_t(uint64_t value);
tsf_reflect_t*  tsf_reflect_create_integer(tsf_integer_t value);
tsf_reflect_t*  tsf_reflect_create_long(tsf_long_t value);
tsf_reflect_t*  tsf_reflect_create_float(float value);
tsf_reflect_t*  tsf_reflect_create_double(double value);

/* macro that allows one to parameterize the above. */
#define         tsf_reflect_create_primitive(type,value) \
                    (tsf_reflect_create_##type(value))

/* create a tsf data struct to hold a bit with the given value. */
tsf_reflect_t*  tsf_reflect_create_bit(tsf_bool_t value);

/* create a 'void' data structure; this is just a singleton for
 * the void type.  you'll probably only have to deal with this if
 * you use the choice type for enumerations. */
tsf_reflect_t*  tsf_reflect_create_void(void);

/* create a string.  the actual char* is duplicated. */
tsf_reflect_t*  tsf_reflect_create_string(const char *str);

/* create an 'any'.  this is a dynamic holder for damn-near
 * anything.  the buffer becomes owned by the reflect object,
 * so that even if this creation fails, the buffer is no longer
 * available to you. */
tsf_reflect_t*  tsf_reflect_create_any(tsf_buffer_t *buf);

/* clone a data structure */
tsf_reflect_t*  tsf_reflect_clone(tsf_reflect_t *data);

/* destroy a data structure */
void            tsf_reflect_destroy(tsf_reflect_t *data);

/* produce a buffer from data */
tsf_buffer_t*   tsf_reflect_make_buffer(tsf_reflect_t *data);

/* produce data from a buffer */
tsf_reflect_t*  tsf_reflect_from_buffer(tsf_buffer_t *buf);

tsf_type_t*     tsf_reflect_get_type(tsf_reflect_t *data);

tsf_type_kind_t tsf_reflect_get_kind_code(tsf_reflect_t *data);

/* set a structure element.  this will do error propagation on
 * *ele_data. */
tsf_bool_t      tsf_struct_reflect_set_element(tsf_reflect_t *data,
                                               const char *name,
                                               tsf_reflect_t *ele_data);

/* set a structure element by index.  this will do error propagation
   on *ele. */
tsf_bool_t      tsf_struct_reflect_set_element_by_index(tsf_reflect_t *data,
                                                        uint32_t index,
                                                        tsf_reflect_t *ele);

/* get a structure element */
tsf_reflect_t*  tsf_struct_reflect_get_element(tsf_reflect_t *data,
                                               const char *name);

/* get a structure element by index */
tsf_reflect_t*  tsf_struct_reflect_get_element_by_index(tsf_reflect_t *data,
                                                        uint32_t index);

/* get the number of elements in the structure */
uint32_t        tsf_struct_reflect_get_num_elements(tsf_reflect_t *data);

/* set the choice.  as a convenience shortcut, if choice is NULL, this is
   equivalent to calling tsf_choice_reflect_set_unknown().  this means
   that ele_data is ignored. */
tsf_bool_t      tsf_choice_reflect_set(tsf_reflect_t *data,
                                       const char *choice,
                                       tsf_reflect_t *ele_data);

/* set the choice by index.  as a convenience shortcut, if choice is
   UINT32_MAX, this is equivalent to calling
   tsf_choice_reflect_set_unknown().  this means that ele_data is ignored. */
tsf_bool_t      tsf_choice_reflect_set_by_index(tsf_reflect_t *data,
                                                uint32_t index,
                                                tsf_reflect_t *ele_data);

/* set the choice to unknown. */
tsf_bool_t      tsf_choice_reflect_set_unknown(tsf_reflect_t *data);

/* tells you the choice index */
uint32_t        tsf_choice_reflect_get_index(tsf_reflect_t *data);

/* tells you if the choice is know (not unknown) */
tsf_bool_t      tsf_choice_reflect_is_known(tsf_reflect_t *data);

/* get the choice.  if unknown, will return NULL. */
const char*     tsf_choice_reflect_get_choice(tsf_reflect_t *data);

/* get the data associated with the choice.  if unknown, will
 * return NULL. */
tsf_reflect_t*  tsf_choice_reflect_get_data(tsf_reflect_t *data);

/* append an element to the array.  this will do error propagation
 * on *ele_data. */
tsf_bool_t      tsf_array_reflect_append(tsf_reflect_t *data,
                                         tsf_reflect_t *ele_data);

/* get an array element */
tsf_reflect_t*  tsf_array_reflect_get_element(tsf_reflect_t *data,
                                              uint32_t index);

/* get the number of elements in the array */
uint32_t        tsf_array_reflect_get_num_elements(tsf_reflect_t *data);

/* get the primitives... */
int8_t          tsf_int8_t_reflect_get(tsf_reflect_t *data);
uint8_t         tsf_uint8_t_reflect_get(tsf_reflect_t *data);
int16_t         tsf_int16_t_reflect_get(tsf_reflect_t *data);
uint16_t        tsf_uint16_t_reflect_get(tsf_reflect_t *data);
int32_t         tsf_int32_t_reflect_get(tsf_reflect_t *data);
uint32_t        tsf_uint32_t_reflect_get(tsf_reflect_t *data);
int64_t         tsf_int64_t_reflect_get(tsf_reflect_t *data);
uint64_t        tsf_uint64_t_reflect_get(tsf_reflect_t *data);
tsf_integer_t   tsf_integer_reflect_get(tsf_reflect_t *data);
tsf_long_t      tsf_long_reflect_get(tsf_reflect_t *data);
float           tsf_float_reflect_get(tsf_reflect_t *data);
double          tsf_double_reflect_get(tsf_reflect_t *data);
#define         tsf_primitive_reflect_get(data,type) \
                    (tsf_##type##_reflect_get(data))

/* get a bit */
tsf_bool_t      tsf_bit_reflect_get(tsf_reflect_t *data);

/* get the string. */
const char*     tsf_string_reflect_get(tsf_reflect_t *data);

/* get the buffer from an any. */
tsf_buffer_t*   tsf_any_reflect_get_buffer(tsf_reflect_t *data);

/* returns tsf_true if the given type has a complete structure mapping. 
 * this does not tell you if the given type has a mapping within its
 * parent type - it only tells you if the types it contains have
 * mappings.  hence, this always returns tsf_true for types that are not
 * containers.  if this function returns tsf_false, then calling any of
 * the tsf_native functions on this type will result in unpredictable
 * behavior (unless otherwise noted). */
tsf_bool_t      tsf_native_type_has_struct_mapping(tsf_type_t *type);

/* get the native size of a particular type within its parent
 * struct.  this can only be called on types that have a struct
 * mapping. */
size_t          tsf_native_type_get_size(tsf_type_t *type);

/* get a bound on the total size of the native data structure rooted at
 * the given type. will return UINTm_MAX if the bound cannot be deduced. */
void            tsf_native_type_get_total_size_bounds(tsf_type_t *type,
                                                      size_t *min,
                                                      size_t *max);

/* returns true if a memcpy() from source to destination with the source's
 * size will give the 'desired' result. */
tsf_bool_t      tsf_native_type_can_blit(tsf_type_t *dest_type,
                                         tsf_type_t *src_type);

/* returns the offset of a particular named type node. */
size_t          tsf_native_named_type_get_offset(tsf_named_type_t *node);

/* gives a mapping (offset) for an element within a struct.  note
 * that this function is not particularly fast.  however, you shouldn't
 * care about the speed of this function since you should only ever
 * call it once for each structure element for the lifetime of your
 * program. */
tsf_bool_t      tsf_native_struct_type_map(tsf_type_t **type,
                                           const char *name,
                                           size_t offset);

/* gives a size to a struct.  note that like tsf_native_struct_type_map(),
 * this function is not particularly fast. */
tsf_bool_t      tsf_native_struct_type_set_size(tsf_type_t **type,
                                                size_t size);

/* creates a struct and sets its native size. equivalent to calling
 * tsf_type_create(TSF_TK_STRUCT) and then calling
 * tsf_native_struct_type_set_size(). */
tsf_type_t*     tsf_native_struct_type_create(size_t size);

/* appends a struct field and sets its offset. equivalent to calling
 * tsf_struct_type_append() and then tsf_native_struct_type_map(). */
tsf_bool_t      tsf_native_struct_type_append(tsf_type_t **type,
                                              const char *name,
                                              tsf_type_t *ele_type,
                                              size_t offset);

/* sets the constructor callback that is called just before the fields of
 * the struct are populated during parsing of copying. this is appropriate
 * for initializing other fields that the user-defined struct might have. */
tsf_bool_t      tsf_native_struct_type_set_constructor(tsf_type_t **type,
                                                       void (*cons)(void*));

/* sets the destructor callback that is called just before the struct is
 * deallocated. for region-allocated structs, this gets added as a region
 * destruction callback. */
tsf_bool_t      tsf_native_struct_type_set_pre_destructor(tsf_type_t **type,
                                                          void (*pre_dest)(void*));

/* sets the destructor callback that is called instead of normal struct
 * destruction in tsf_destructor. setting this means that destroying the struct
 * using a destructor will not call the pre_destructor. */
tsf_bool_t      tsf_native_struct_type_set_destructor(tsf_type_t **type,
                                                      void (*dest)(void*));

/* get the constructor to call when the struct gets allocated */
void (*tsf_native_struct_type_get_constructor(tsf_type_t *type))(void*);

/* get the destructor to call when the struct gets deallocated */
void (*tsf_native_struct_type_get_pre_destructor(tsf_type_t *type))(void*);

/* get the destructor to call when the struct gets deallocated */
void (*tsf_native_struct_type_get_destructor(tsf_type_t *type))(void*);

/* set all of the offsets and things required by choice */
tsf_bool_t      tsf_native_choice_type_map(tsf_type_t **type,
					   tsf_bool_t in_place,
                                           size_t value_offset,
                                           size_t data_offset,
                                           size_t size);

/* creates a choice type and sets all of its offsets. */
tsf_type_t*     tsf_native_choice_type_create(tsf_bool_t in_place,
                                              size_t value_offset,
                                              size_t data_offset,
                                              size_t size);

/* is the choice mapping in place? */
tsf_bool_t      tsf_native_choice_type_is_in_place(tsf_type_t *type);

/* retrieve the data offset */
size_t          tsf_native_choice_type_get_data_offset(tsf_type_t *type);

/* retrieve the value offset */
size_t          tsf_native_choice_type_get_value_offset(tsf_type_t *type);

/* retrieve the size */
size_t          tsf_native_choice_type_get_size(tsf_type_t *type);

/* automatically produces some sort of mapping.  you probably don't want
 * to use this method, since you get no guarantee that this will map to
 * any of your structures.  this method's main intent is to aid in the
 * writing of regression tests.
 *
 * if do_align is tsf_true, each element will be aligned according to
 * the detected native alignment (the alignment necessary for the largest
 * data type, which is usually determined by either pointers or doubles).
 *
 * if do_align is tsf_false, no alignment guarantee is made.  you have
 * to be quite careful when using this option, as it might cause the
 * GPC programs generated for this type to crash. */
tsf_bool_t      tsf_native_type_produce_some_mapping(tsf_type_t **type,
						     tsf_bool_t do_align);

/* given a type that has complete struct mapping, produce a generator
 * that can be used to produce tsf_buffer_t objects from pointers
 * to the struct. */
tsf_genrtr_t*   tsf_generator_create(tsf_type_t *type);

/* generate a buffer using some data and a generator. */
tsf_buffer_t*   tsf_generator_generate(tsf_genrtr_t *gen,
                                       void *data);

/* generate a buffer in a region using some data and a generator. */
tsf_buffer_t*   tsf_generator_generate_in(tsf_genrtr_t *gen,
                                          void *data,
                                          void *region);

/* destroy the generator. */
void            tsf_generator_destroy(tsf_genrtr_t *gen);

/* given a type that has a complete struct mapping, produce a parser
 * that can be used to read tsf_buffer_t objects and produce data
 * that has the structure you specified in the struct mapping. */
tsf_parser_t*   tsf_parser_create(tsf_type_t *type);

/* parse a buffer and generate data according to the struct mapping
 * in the type you gave to tsf_parser_create().
 *
 * stuff returned from this function can be freed using
 * tsf_region_free() (see tsf_region.h). */
void*           tsf_parser_parse(tsf_parser_t *parser,
                                 tsf_buffer_t *buffer);

/* parse a buffer and generate data according to the struct mapping
 * in the type you gave to tsf_parser_create().  the data is
 * allocated in the given region instead of having a new region
 * created. */
void*           tsf_parser_parse_in(tsf_parser_t *parser,
                                    tsf_buffer_t *buffer,
                                    void *region);

/* parse directly into an instance of the expected type using the given
 * region. you can pass NULL for the region if you want to use malloc for
 * all allocations. */
tsf_bool_t      tsf_parser_parse_into(tsf_parser_t *parser,
                                      tsf_buffer_t *buffer,
                                      void *result,
                                      void *region);

/* destroy a parser. */
void            tsf_parser_destroy(tsf_parser_t *parser);

/* get the number of gpc codes that the parser is using.  this is a
 * measure of the parser's memory efficiency. */
uint32_t        tsf_parser_num_codes(tsf_parser_t *parser);

/* create a copier given a destination type and a source type.  if the
 * source is not instanceof the destination, you will get an error. */
tsf_copier_t*   tsf_copier_create(tsf_type_t *dest,
                                  tsf_type_t *src);

/* destroy the copier. */
void            tsf_copier_destroy(tsf_copier_t *copier);

/* clone the source data, which has the source type, and produce an
 * object of the destination type.  the object returned can be
 * freed with tsf_region_free(). */
void*           tsf_copier_clone(tsf_copier_t *copier,
                                 void *src);

/* clone the source data, which has the source type, and produce an
 * object of the destination type.  the object returned will be
 * allocated in the given region. */
void*           tsf_copier_clone_in(tsf_copier_t *copier,
				    void *src,
				    void *region);

/* copy from the source to the destination, following the specified types.
 * also, use the provided region for allocation. note that you can pass a
 * NULL region, which causes this to use malloc for allocations. */
tsf_bool_t      tsf_copier_copy(tsf_copier_t *copier,
                                void *dest,
                                void *src,
                                void *region);

/* create a destructor. a destroyer can be used to free the malloc'd
 * memory used by objects of a given type, provided that the objects were
 * allocated with malloc and not with a region. */
tsf_destructor_t* tsf_destructor_create(tsf_type_t *type);

/* destroy the destroyer. */
void            tsf_destructor_destroy(tsf_destructor_t *destructor);

/* destruct an object. */
void            tsf_destructor_destruct(tsf_destructor_t *destructor,
                                        void *object);

/* file descriptor writer; the void *arg should be a pointer to
 * an integer holding the file descriptor. */
tsf_bool_t      tsf_fd_writer(void *arg,
                              const void *data,
                              uint32_t len);

/* file descriptor reader; the void *arg should be a pointer to
 * an integer holding the file descriptor.  if an EOF happens before
 * any data is ready, this function will set the TSF_E_UNEXPECTED_EOF
 * error.  if an EOF happens after any data is ready,
 * TSF_E_ERRONEOUS_EOF will be set instead. if the EOF was expected,
 * you should change the TSF_E_UNEXPECTED_EOF error to TSF_E_EOF if
 * you propagate that error (see tsf_in_man.c for a good example of
 * what I'm talking about). */
tsf_bool_t      tsf_fd_reader(void *arg,
                              void *data,
                              uint32_t len);

/* file descriptor partial reader; the void *arg should be a pointer
 * to an integer holding the file descriptor.  this differs from
 * tsf_fd_reader() in that complete writes are never guaranteed.
 * if an EOF happens, this function will set the TSF_E_UNEXPECTED_EOF
 * error.  if the EOF was expected, you should change the
 * TSF_E_UNEXPECTED_EOF error to TSF_E_EOF if you propagate that error
 * (see tsf_in_man.c for a good example of what I'm talking about). */
int32_t         tsf_fd_partial_reader(void *arg,
                                      void *data,
                                      uint32_t len);

tsf_bool_t      tsf_full_read_of_partial(tsf_partial_reader_t reader,
					 void *arg,
					 void *data,
					 uint32_t len);

/* read a string using a reader */
char*           tsf_read_string(tsf_reader_t reader,
                                void *arg,
                                size_t max_size);

/* write a string using a writer */
tsf_bool_t      tsf_write_string(tsf_writer_t writer,
                                 void *arg,
                                 const char *str);

/* size calculating writer; the arg should be a uint32_t*, where
 * the referenced uint32_t is the length.  you should initialize
 * the length to 0 before using this writer.  note that this
 * writer does not write the contents to anywhere; it only tells
 * you the contents' size. */
tsf_bool_t      tsf_size_calc_writer(void *arg,
                                     const void *buf,
                                     uint32_t len);

/* reader that delegates to another reader, but aborts once the
 * amount read exceeds a limit.  the error will be
 * TSF_E_PARSE_ERROR.  the arg should be a tsf_size_aware_rdr_t*,
 * which you should initialize with the reader to delegate to,
 * the arg that that reader expects, a limit, and a name.  the
 * name is used for generating the message for the parse error,
 * if it occurs, and should be a short (preferably one word)
 * description of what it is you're reading. */
tsf_bool_t      tsf_size_aware_reader(void *arg,
                                      void *buf,
                                      uint32_t len);

/* buffer-only writer.  writes to a buffer of a pre-defined size.  you can
 * then use this buffer with the buffer-only reader.  the argument is a
 * uint8_t**.  this function will dereference it and use it as an iterator
 * as it writes. */
tsf_bool_t      tsf_buffer_only_writer(void *arg,
                                       const void *buf,
                                       uint32_t len);

/* buffer-only writer that resizes the output buffer when there isn't
 * enough room. takes a tsf_resizable_buffer_t. it's valid to start by
 * memsetting the tsf_resizable_buffer_t to zero. */
tsf_bool_t      tsf_resizable_buffer_writer(void *arg,
                                            const void *buf,
                                            uint32_t len);

/* buffer-only reader.  reads from a light-weight buffer. */
tsf_bool_t      tsf_buffer_only_reader(void *arg,
                                       void *buf,
                                       uint32_t len);

/* reads from a reader and compares to what you write to it.  fails if any
   byte doesn't match. */
tsf_bool_t      tsf_comparing_writer(void *arg,
                                     const void *buf,
                                     uint32_t len);

/* computes a hash of the text you give it.  the arg must point to a
   32-bit integer. */
tsf_bool_t      tsf_hashing_writer(void *arg,
                                   const void *buf,
                                   uint32_t len);

/* create a SHA1 hashing writer. */
tsf_sha1_wtr_t* tsf_sha1_writer_create(void);

/* destroy a SHA1 hashing writer. */
void            tsf_sha1_writer_destroy(tsf_sha1_wtr_t *sha1);

/* write using the SHA1 hashing writer. */
tsf_bool_t      tsf_sha1_writer_write(void *arg,
                                      const void *buf,
                                      uint32_t len);

/* finish writing and get the hash as an array of 5 uint32_t's.  after
 * this function is called you cannot write into this writer anymore.
 * calling this function repeatedly will simply return the same
 * result. */
uint32_t*       tsf_sha1_writer_finish(tsf_sha1_wtr_t *sha1);

/* get the resulting hash as an array of 5 uint32_t's; this returns
 * the same array as tsf_sha1_writer_finish().  you should not call
 * this function until after you have called tsf_sha1_writer_finish().
 * in this sense, this function is somewhat redundant to
 * tsf_sha1_writer_finish(), but is provided for convenience. */
uint32_t*       tsf_sha1_writer_result(tsf_sha1_wtr_t *sha1);

/* returns a string containing the hex format SHA1 sum, given the
 * sum in the argument.  note, the returned string is newly allocated,
 * so you must free it when you're done. */
char*           tsf_sha1_sum_to_str(uint32_t *sum);

/* create a buffered reader using the given file descriptor and
 * buffer size.  if keep_fd is tsf_true, the file descriptor will be
 * closed if: an error occurs while trying to create the buffer,
 * and when the buffer is destroyed. */
tsf_buf_rdr_t*  tsf_buf_reader_create(int fd,
                                      uint32_t buf_size,
                                      tsf_bool_t keep_fd);

/* destroy a read buffer */
void            tsf_buf_reader_destroy(tsf_buf_rdr_t *buf);

/* read using a read buffer */
tsf_bool_t      tsf_buf_reader_read(void *buf,
                                    void *data,
                                    uint32_t len);

/* tells you how many bytes of data are remaining in the buffer. */
uint32_t        tsf_buf_reader_get_remaining(tsf_buf_rdr_t *buf);

/* resets the buffer. */
void            tsf_buf_reader_reset(tsf_buf_rdr_t *buf);

/* create a buffered writer using the given file descriptor, buffer size.
 * if keep_fd is tsf_true, the file descriptor will be closed if: an error
 * occurs while trying to create the buffer, and when the buffer is destroyed. */
tsf_buf_wtr_t*  tsf_buf_writer_create(int fd,
                                      uint32_t buf_size,
                                      tsf_bool_t keep_fd);

/* destroy the buffer.  if keep_fd is tsf_true, the file descriptor
 * is closed.  note that this operation does not result in the
 * buffer being flushed, so calling this without calling the
 * flush function (see below) first may result in data loss. */
void            tsf_buf_writer_destroy(tsf_buf_wtr_t *buf);

/* this is the function that you pass to all of the writing operations
 * if you actually want to put this writer to good use. */
tsf_bool_t      tsf_buf_writer_write(void *buf,
                                     const void *data,
                                     uint32_t len);

/* flush the buffer. */
tsf_bool_t      tsf_buf_writer_flush(tsf_buf_wtr_t *buf);

/* get the 'default' zip mode.  this is actually NONE, which will
 * result in illegal argument errors in tsf_zip_reader_create() and
 * tsf_zip_writer_create().  however, tsf_stream_file_input and
 * tsf_stream_file_output will gracefully accept it and take it
 * to mean that normal buffered IO should be performed. */
void            tsf_zip_wtr_attr_get_default(tsf_zip_wtr_attr_t *attr);

/* get the default ZLib mode.  this will cause ZLib to be used with
 * default settings (a compression level of Z_DEFAULT_COMPRESSION and
 * advanced_init set to false).  this function will return tsf_true
 * if ZLib is supported, and tsf_false otherwise.  if tsf_false is
 * returned, the error will be set to TSF_E_NOT_SUPPORTED.  to make
 * life easier in some cases, you can pass NULL to this function and
 * the function will still perform the check if Zlib is supported. */
tsf_bool_t      tsf_zip_wtr_attr_get_zlib_default(tsf_zip_wtr_attr_t *attr);

/* get the default libbzip2 mode.  this will cause libbzip2 to be
 * used with default settings (blockSize100k = 9, verbosity = 0,
 * workFactor = 0, small = 0).  this function will return tsf_true
 * if libbzip2 is supported, and tsf_false otherwise.  if tsf_false
 * is returned, the error will be set to TSF_E_NOT_SUPPORTED.  to
 * make life easier in some cases, you can pass NULL to this function
 * and the function will still perform the check if libbzip2 is
 * supported. */
tsf_bool_t      tsf_zip_wtr_attr_get_bzip2_default(tsf_zip_wtr_attr_t *attr);

/* get the attributes that correspond to the given mode. */
tsf_bool_t      tsf_zip_wtr_attr_get_for_mode(tsf_zip_wtr_attr_t *attr,
					      tsf_zip_mode_t mode);

/* get the default decompression settings.  this enables only those
 * complression schemes that are actually supported. */
void            tsf_zip_rdr_attr_get_default(tsf_zip_rdr_attr_t *attr);

/* get the settings with all compression disabled.  this sets the
 * allow bits to false, but all of the other fields are set to the
 * default settings - so this is the perfect starting place if you want
 * to enable only your approved compression schemes (just call this
 * and then set the bits of the schemes that make you happy). */
void            tsf_zip_rdr_attr_get_nozip(tsf_zip_rdr_attr_t *attr);

/* is all compression disabled? */
tsf_bool_t      tsf_zip_rdr_attr_is_nozip(tsf_zip_rdr_attr_t *attr);

/* convenience macro that checks if zlib support is available */
#define         tsf_zlib_supported() \
                    (tsf_zip_wtr_attr_get_zlib_default(NULL))

/* convenience macro that checks if libbzip2 support is available */
#define         tsf_libbzip2_supported() \
                    (tsf_zip_wtr_attr_get_bzip2_default(NULL))

/* create a zipping reader.  it also does buffering.  accepts a partial
 * reader as a delegate to actually read the data.  as such, this thing
 * can adapt to many different underlying devices.  returns an error
 * if you try to use a mode or attribute set that is not supported.
 * In particular, using TSF_ZIP_NONE is not supported!
 *
 * note that this function does not check if the selected mode is allowed
 * according to the attributes.  it is the caller's responsibility to
 * ensure that the mode is allowed. */
tsf_zip_rdr_t*  tsf_zip_reader_create(tsf_partial_reader_t reader,
                                      void *reader_arg,
                                      tsf_zip_mode_t mode,
                                      const tsf_zip_rdr_attr_t *attr);

/* destroy the reader. */
void            tsf_zip_reader_destroy(tsf_zip_rdr_t *reader);

/* read some data. */
tsf_bool_t      tsf_zip_reader_read(void *reader,
                                    void *data,
                                    uint32_t len);

/* create a zipping writer.  it also does buffering.  accepts a writer
 * as a delegate to actually write the data.  as such, this thing can
 * adapt to many different underlying devices.  note that it makes little
 * or no sense to pass in a buffering writer, since this writer already
 * does buffering (in fact, it has to do buffering to interact with the
 * underlying compression engine).  returns an error if you try to use
 * a mode or attribute set that is not supported.  in particular,
 * TSF_ZIP_NONE is not supported! */
tsf_zip_wtr_t*  tsf_zip_writer_create(tsf_writer_t writer,
                                      void *writer_arg,
                                      const tsf_zip_wtr_attr_t *attr);

/* destroy the writer. */
void            tsf_zip_writer_destroy(tsf_zip_wtr_t *writer);

/* write some data. */
tsf_bool_t      tsf_zip_writer_write(void *writer,
                                     const void *data,
                                     uint32_t len);

/* flush data.  calling this will almost certainly hurt compression, so
 * call it only if you really have to.  if the full_flush parameter is
 * true, the flush will not only be complete enough to allow the reading
 * end to get all of the data, but will also result in the compressor
 * resetting its state (so to speak) in order to make all future data
 * be independant of the already written data.  passing tsf_true for
 * full_flush will help error recovery but will hurt your compression
 * ratios even more.
 *
 * note that when using libbzip2, full_flush is ignored.  in general,
 * any flushing done with libbzip2 will hurt a lot more than any flushing
 * done with zlib, regardless of the value of full_flush.  so keep that
 * in mind.  in many cases, the extremely bad performance that results
 * from flushing in libbzip2 will be sufficient reason to use zlib
 * instead. */
tsf_bool_t      tsf_zip_writer_flush(tsf_zip_wtr_t *writer,
                                     tsf_bool_t full_flush);

/* finish the stream.  this allows the compressor to output its end-of-
 * stream marker.  this also does an implicit flush, so calling flush
 * just before calling finish will not help (it shouldn't hurt, either).
 * not that if you do a flush but do not do a finish, the receiver
 * should be able to get all of the data but will probably report an
 * error at the end of the file, complaining that the end of stream was
 * never really hit.
 *
 * after finish returns, you may not use this writer anymore.
 * attempting to call writer, flush, or finish on a writer that has
 * already had finish called on it will lead to unpredictable behavior.
 * after calling finish, you should just call destroy. */
tsf_bool_t      tsf_zip_writer_finish(tsf_zip_wtr_t *writer);

/* create an adaptive zipping reader.  this reader will be able to switch
 * between different zipping strategies (including no zipping) as the input
 * data indicates.  however, it only works under two non-trivial
 * conditions:
 * 1) the input is formatted according to the serial protocol with
 *    stream file headers indicating compression settings, and
 * 2) you call tsf_adaptive_reader_hint_switch() at the right times. */
tsf_adpt_rdr_t* tsf_adaptive_reader_create(tsf_partial_reader_t reader,
					   void *reader_arg,
					   uint32_t nozip_buf_size,
					   const tsf_zip_rdr_attr_t *attr);

/* close the adaptive reader.  cleans up everything.  never fails. */
void            tsf_adaptive_reader_destroy(tsf_adpt_rdr_t *reader);

/* inform the reader that there may be a format change in the next byte.
 * if we are currently reading with compression turned off, this hint tells
 * the reader to inspect the next four bytes to see if it contains a code
 * corresponding to the start of a ZLIB or BZIP2 sequence.  if we are using
 * a ZLIB or BZIP2 reader, this specifies that an end-of-stream from the
 * underlying reader is correct (otherwise it is flagged as erroneous). */
void            tsf_adaptive_reader_hint_switch(tsf_adpt_rdr_t *reader);

/* tells you how many bytes of data are remaining in the buffer. */
uint32_t        tsf_adaptive_reader_get_remaining(tsf_adpt_rdr_t *reader);

/* the actual read method. */
tsf_bool_t      tsf_adaptive_reader_read(void *reader,
					 void *data,
					 uint32_t len);

/* determine the zip mode of the given file by openning it and reading the
 * first four bytes.  if the mode returned is TSF_ZIP_UNKNOWN then there
 * was an error.  if the error was TSF_E_NOT_SUPPORTED then the file is
 * not in a valid TSF stream file format. */
tsf_zip_mode_t  tsf_stream_file_get_mode(const char *filename);

/* same as above except that it reads the first four bytes from the given
 * file descriptor. */
tsf_zip_mode_t  tsf_stream_file_get_mode_from_fd(int fd);

/* open a file for reading.  if the zip reader attributes are NULL then they
 * are set to the defaults.  if the limits are NULL then no limits are used.
 * instances of this type are not thread-safe; use locking around calls into
 * the object. */
tsf_stream_file_input_t*
                tsf_stream_file_input_open(const char *filename,
                                           tsf_limits_t *limits,
                                           tsf_zip_rdr_attr_t *attr);

/* make a stream file input hijack a file descriptor.  if the zip reader
 * attributes are NULL then they are set to the defaults.
 *
 * if keep is true, the file descriptor will be closed when
 * tsf_stream_file_input_close() is called.  otherwise, the file
 * descriptor will remain open. */
tsf_stream_file_input_t*
                tsf_stream_file_input_fd_open(int fd,
                                              tsf_limits_t *limits,
                                              tsf_zip_rdr_attr_t *attr,
                                              tsf_bool_t keep,
                                              uint32_t buf_size);

/* close a file that was openned for reading. */
void            tsf_stream_file_input_close(tsf_stream_file_input_t *file);

/* returns true if the file allows compression.  compressed files have some
 * limitations.  for example, mark & seek is not supported for compressed
 * files. */
tsf_bool_t      tsf_stream_file_input_allows_compression(tsf_stream_file_input_t *file);

/* register a callback that will get called every time the input manager
 * encounters a new type. */
tsf_bool_t      tsf_stream_file_input_register_type_cback(tsf_stream_file_input_t *file,
                                                          tsf_type_cback_t cback,
                                                          void *arg);

/* unregister all type callbacks that match the given function pointer
 * and void* argument. */
tsf_bool_t      tsf_stream_file_input_unregister_type_cback(tsf_stream_file_input_t *file,
                                                            tsf_type_cback_t cback,
                                                            void *arg);

/* read a buffer from a file that was openned for reading. */
tsf_buffer_t*   tsf_stream_file_input_read(tsf_stream_file_input_t *file);

/* get the mark of the thing you are about to read.  the mark is something
 * that you get to allocate.  but you must treat it as an opaque!
 *
 * note that mark & seek is not supported for files that are compressed.
 * it is also not supported if the underlying file descriptor does not
 * support lseek().  if the file is compressed, you will get a
 * TSF_E_NOT_SUPPORTED error.  it is perfectly safe (and fast) to determine
 * if a file is compressed by calling this function.  you can of course
 * also use tsf_stream_file_input_is_compressed().  if the file does not
 * support lseek(), you will get some sort of system error, which may
 * differ depending on how well your system complies to POSIX. :-) */
tsf_bool_t      tsf_stream_file_input_get_mark(tsf_stream_file_input_t *file,
                                               tsf_stream_file_mark_t *mark);

/* seek to previously acquired mark. */
tsf_bool_t      tsf_stream_file_input_seek(tsf_stream_file_input_t *file,
                                           const tsf_stream_file_mark_t *mark);

/* open a file for writing.  if the zip attributes are NULL, the default
 * ones will be used (which means no compression).  instances of this type
 * are not thread-safe; use locking when calling its methods. */
tsf_stream_file_output_t*
                tsf_stream_file_output_open(const char *filename,
                                            tsf_bool_t append,
                                            tsf_zip_wtr_attr_t *attr);

/* make a stream file output hijack a file descriptor.  if keep is true, the
 * file descriptor will be closed when tsf_stream_file_output_close() is
 * called.  otherwise, it will remain open. */
tsf_stream_file_output_t*
                tsf_stream_file_output_fd_open(int fd,
                                               tsf_zip_wtr_attr_t *attr,
                                               tsf_bool_t keep,
                                               uint32_t buf_size);

/* flush and close a stream file output.  returns tsf_true if the flushing
 * went OK and tsf_false if it didn't.  the file gets closed either way,
 * though. */
tsf_bool_t      tsf_stream_file_output_close(tsf_stream_file_output_t *file);

/* flush the stream file output.  if this fails, the stream becomes corrupt
 * at the point of failure and should not be used. */
tsf_bool_t      tsf_stream_file_output_flush(tsf_stream_file_output_t *file);

/* write something into the file.  if this fails, the stream becomes corrupt
 * at the point of failure and should not be used. */
tsf_bool_t      tsf_stream_file_output_write(tsf_stream_file_output_t *file,
                                             tsf_buffer_t *buffer);

/* open a filesystem database.  a filesystem database allows for key-value
 * storage optimized for storing huge values. additionally, a filesystem
 * database allows a value to consist of a sequence of buffers (i.e. a
 * stream), and it has support for connecting to a remote database.
 *
 * one of the main features of FSDB is its excellent concurrency support.
 * even if an FSDB is opened by multiple different processes, or multiple
 * threads in a single process, or multiple threads share the same
 * tsf_fsdb_t* handle, the following properties are guaranteed:
 *
 * - tsf_fsdb_put() is not visible unless there is a subsequent
 *   tsf_fsdb_out_commit().
 *
 * - if multiple tsf_fsdb_out_commit()'s are run using the same key,
 *   then any tsf_fsdb_get()'s behave as if the only tsf_fsdb_put()
 *   and tsf_fsdb_out_commit() that had ever been executed was the last
 *   one at the time of the tsf_fsdb_get().
 *
 * - tsf_fsdb_get() returns a stream that does not change even if
 *   new tsf_fsdb_put()/tsf_fsdb_out_write()/tsf_fsdb_out_commit() are
 *   executed on the same key.
 *
 * - tsf_fsdb_out_abort() makes it appear as if the tsf_fsdb_put() and
 *   any subsequent tsf_fsdb_out_write()'s never happened.
 *
 * - if tsf_fsdb_out_commit() fails it is no different than if you had
 *   called tsf_fsdb_out_abort().
 *
 * These properties are guaranteed even when accessing the database
 * using different tsf_fsdb_t* handles (even from separate processes),
 * so long as the underlying filesystem supports the atomicity
 * guarantees of open(O_EXCL) and rename().
 *
 * The memory management is such that the FSDB API never claims
 * ownership of any objects, but the tsf_fsdb_t* must be kept alive so
 * long as there are tsf_fsdb_in_t* or tsf_fsdb_out_t* that have been
 * created from it. */
tsf_fsdb_t*     tsf_fsdb_open_local(const char *dirname,
                                    tsf_limits_t *limits,
                                    tsf_zip_rdr_attr_t *zip_rdr_attr,
                                    tsf_zip_wtr_attr_t *zip_wtr_attr,
                                    tsf_bool_t update,
                                    tsf_bool_t truncate);

/* open a remote FSDB database using a host and port. */
tsf_fsdb_t*     tsf_fsdb_open_remote(const char *host,
                                     int port,
                                     tsf_limits_t *limits,
                                     tsf_zip_rdr_attr_t *zip_rdr_attr,
                                     tsf_zip_wtr_attr_t *zip_wtr_attr);

/* close a filesystem database. */
void            tsf_fsdb_close(tsf_fsdb_t *fsdb);

/* open a stream to write a sequence of values at a key in the database. */
tsf_fsdb_out_t* tsf_fsdb_put(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key);

/* open a stream to read a sequence of values at a key in the database. */
tsf_fsdb_in_t*  tsf_fsdb_get(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key);

/* delete an entry from the database. */
tsf_bool_t      tsf_fsdb_rm(tsf_fsdb_t *fsdb,
                            tsf_buffer_t *key);

/* close a filesystem database input stream. */
void            tsf_fsdb_in_close(tsf_fsdb_in_t *in);

/* read a buffer from a filesystem database input stream. */
tsf_buffer_t*   tsf_fsdb_in_read(tsf_fsdb_in_t *in);

/* commit a filesystem database output stream, and destroy the stream.
 * if this experiences an error, then it means that the stream is
 * automatically aborted. */
tsf_bool_t      tsf_fsdb_out_commit(tsf_fsdb_out_t *out);

/* abort a filesystem database output stream, and destroy the stream. */
void            tsf_fsdb_out_abort(tsf_fsdb_out_t *out);

/* write to a filesystem database output stream. */
tsf_bool_t      tsf_fsdb_out_write(tsf_fsdb_out_t *out,
                                   tsf_buffer_t *buf);

#ifdef __cplusplus
};
#endif

#endif

