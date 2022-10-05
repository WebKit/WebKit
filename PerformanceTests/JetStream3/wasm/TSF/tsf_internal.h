/*
 * Copyright (C) 2003, 2004, 2005, 2011, 2014, 2015 Filip Pizlo. All rights reserved.
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

#ifndef FP_TSF_INTERNAL_H
#define FP_TSF_INTERNAL_H

#include "tsf_internal_config.h"
#include "tsf.h"

#include <stdlib.h>
#include <stdarg.h>
#include <sys/time.h>

#include "tsf_inttypes.h"

/* define all of the structs that were left opaque in tsf.h. */
struct tsf_type_table {
    tsf_named_type_t **elements;
    uint32_t num_elements;
    
    tsf_st_table *ele_table;
};

struct tsf_type_in_map {
    void *region;           /* NULL if not allocated in a region */
    tsf_type_t **elements;
    uint32_t num_elements;
    uint32_t capacity;
};

struct tsf_ra_type_info {
    uint32_t type_code;
    uint64_t ref_count;
};

struct tsf_ra_tc_node {
    uint32_t type_code;
    tsf_ra_tc_node_t *next;
};

struct tsf_ra_type_man {
    tsf_ra_tc_node_t *type_codes;
    tsf_st_table *out_map;
    tsf_st_table *in_map;
};

struct tsf_type {
    tsf_type_kind_t kind_code;
    
    /* true if this is an ANY or if is a container that contains an ANY. */
    tsf_bool_t is_dynamic;
    
    union {
        /* additional data for array */
        struct {
            tsf_type_t *element_type;
        } a;
        
        /* additional data for struct */
        struct {
            /* struct mapping support - need to know the size of
             * the struct in memory.  this is NOT the sum of the
             * sizes of the elements, because that doesn't take
             * into account padding or the possibility of additional
             * fields in that struct that the user isn't telling us
             * about.  HOWEVER! we can be sure that this size must
             * be greater than or equal to the sum of the sizes of
             * the elements. */
            tsf_bool_t has_size;
            size_t size;

	    void (*constructor)(void *);
	    void (*pre_destructor)(void *);
	    void (*destructor)(void *);

            tsf_type_table_t *tt;
        } s;
        
        /* additional data for choice */
        struct {
            tsf_type_table_t *tt;
            
            /* struct mapping support */
            tsf_bool_t has_mapping;
	    
	    tsf_bool_t in_place;    /* use union? */
            tsf_bool_t choice_as_full_word; /* use full 32-bit word for emitting the choice. */

            size_t value_offset;    /* offset to the value field */
            size_t data_offset;     /* offset to the union if in_place
				       is true, or to the pointer if
				       it is false. */
            size_t size;            /* size of the choice structure */
        } h;
    } u;
    
    /* precomputed hash code.  since each tsf_type object is immutable
     * (that's why all functions that 'modify' a tsf_type actually take
     * a pointer to a pointer to a tsf_type - so that instead of modifying
     * the actual tsf_type, it allocates a new one and changes your
     * pointer), the hash code can safely be precomputed when the
     * tsf_type is created. */
    int hash_code;
    
    /* lazily computed type size in bytes.  if it hasn't been computed yet,
     * its value will be 0. */
    volatile uint32_t byte_size;
    
    /* holder for an optional comment supplied by the user.  only
     * commentable types can hold comments (use tsf_type_kind_is_commentable()
     * to find out if it is commentable).  the comment is a UTF-8 string
     * supplied by the user via a function call (see tsf_type_set_comment())
     * or by using javadoc comments in definitions passed to tsf_define. */
    char *comment;
    
    /* holder for an optional type name supplied by the user.  only
     * commentable types can have names (use tsf_type_kind_is_commentable()
     * to find out if it is commentable).  the name is a UTF-8 string
     * supplied by the user via a function call (see tsf_type_set_name()).
     * types defined using tsf_define automatically have names. */
    char *name;
    
    /* optional version number for a type.  the default value is 0.
     * this is actually an encoding of major.minor.patch, where major
     * is a uint16_t, minor is a uint8_t, and patch is a uint8_t.
     * this can only be changed for commentable types (use
     * tsf_type_kind_is_commentable() to find out if it is commentable). */
    uint32_t version;
    
    /* only used for non-singletons */
    tsf_atomic_count_t ref_count;

    /* for memoization: points at the memo master. may be NULL, indicating
     * that we haven't ever memoized this type. may point to self,
     * indicating that this is the memo master. */
    tsf_type_t *memo_master;
};

struct tsf_named_type {
    char *name;
    tsf_type_t *type;
    uint32_t index;
    
    /* for struct entries - struct entries may be marked optional.  note
     * that if we have structure A, B, and C, so that A does not have
     * field F, B has an optional field F, and C has field F, then, assuming
     * the structs are otherwise equal, the instanceof 'matrix' goes as
     * follows:
     *
     *   A B C
     * A Y Y N
     * B Y Y N
     * C Y Y Y
     * 
     * In other words, a structure with an optional F is not instanceof a
     * structure with a mandatory F. */
    tsf_bool_t optional;
    
    /* struct mapping support - need to know the offset of this
     * element within its parent struct.
     *
     * Note that this does not apply to choice types, where
     * all elements have the same offset. */
    tsf_bool_t has_offset;
    size_t offset;
    
    /* holder for an optional comment supplied by the user either through
     * a function call or through tsf_define. */
    char *comment;
};

struct tsf_buffer {
    tsf_type_t *type;
    tsf_type_in_map_t *types;
    tsf_bool_t keep;
    void *data;
    uint32_t size;
    
    tsf_atomic_count_t ref_count;
};

struct tsf_reflect {
    tsf_type_t *type;
    union {
        /* container, used for both struct and array.  note that it
         * is used differently for struct and for array. */
        struct {
            tsf_reflect_t **array;
            uint32_t size;
        } c;
        
        /* choice data. */
        struct {
            uint32_t choice;    /* 'unknown' if equal to UINT32_MAX */
            tsf_reflect_t *data;
        } h;
        
        /* string data. */
        char *str;
        
        /* primitives */
        int8_t   p_int8_t;
        uint8_t  p_uint8_t;
        int16_t  p_int16_t;
        uint16_t p_uint16_t;
        int32_t  p_int32_t;
        uint32_t p_uint32_t;
        int64_t  p_int64_t;
        uint64_t p_uint64_t;
        tsf_integer_t p_integer;
        tsf_long_t p_long;
        float    p_float;
        double   p_double;
        
        /* bit */
        tsf_bool_t bit;
        
        /* for the any type */
        tsf_buffer_t *any;
    } u;
};

struct tsf_genrtr {
    tsf_type_t *type;
    gpc_program_t *generator;
};

struct tsf_parser {
    tsf_type_t *expected_type;
    size_t expected_type_size;
    tsf_st_table *parsers;
    
    gpc_program_t *cached_program;
    tsf_type_t *cached_type;
    
    tsf_mutex_t lock;
};

struct tsf_copier {
    gpc_program_t *copier;
};

struct tsf_destructor {
    gpc_program_t *destructor;
};

typedef struct {
    tsf_type_cback_t cback;
    void *arg;
} tsf_serial_in_man_cback_t;

struct tsf_serial_in_man {
    tsf_type_in_map_t *types;
    tsf_serial_in_man_state_t state;
    
    uint32_t types_byte_size;
    
    tsf_serial_in_man_cback_t *cbacks;
    uint32_t num_cbacks;
    
    tsf_reader_t reader;
    void *arg;
    
    tsf_limits_t *limits;
};

struct tsf_serial_out_man {
    tsf_type_out_map_t *types;
    
    tsf_writer_t writer;
    void *arg;
};

struct tsf_sha1_wtr {
    tsf_SHA1Context ctx;
};

struct tsf_buf_rdr {
    int fd;
    tsf_bool_t keep_fd;
    uint8_t *buf;
    uint32_t size;
    uint32_t puti;
    uint32_t geti;
};

struct tsf_buf_wtr {
    int fd;
    tsf_bool_t keep_fd;
    uint8_t *buf;
    uint32_t size;
    uint32_t pos;
};

struct tsf_zip_rdr {
    tsf_zip_mode_t mode;
    
    tsf_partial_reader_t reader;
    void *reader_arg;
    
    void *stream;   /* can be either of type z_stream or bz_stream */
    
    tsf_zip_abstract_t abstract;
    
    tsf_bool_t decomp_eof;  /* true if the decompression algorithm has
                             * reached EOF. */
    
    uint8_t *buf;
    uint32_t buf_size;
};

struct tsf_zip_wtr {
    tsf_zip_mode_t mode;
    
    tsf_writer_t writer;
    void *writer_arg;
    
    void *stream;   /* can be either of type z_stream of bz_stream */
    
    tsf_zip_abstract_t abstract;
    
    uint8_t *buf;
    uint32_t buf_size;
};

struct tsf_adpt_rdr {
    tsf_zip_mode_t mode;
    
    tsf_partial_reader_t reader;
    void *reader_arg;
    
    void *stream;   /* can be either of type z_stream or bz_stream */
    
    tsf_zip_abstract_t abstract;
    
    tsf_bool_t expect_switch;
    
    uint8_t *buf;
    uint32_t puti,geti; /* same as in buffered reader */
    uint32_t buf_size;

    tsf_zip_rdr_attr_t attr;
    uint32_t nozip_buf_size;
};

struct tsf_stream_file_input {
    int fd;
    tsf_bool_t keep_fd;
    
    tsf_serial_in_man_t *in_man;
    
    tsf_bool_t use_adpt;
    union {
        tsf_buf_rdr_t *buf;
        tsf_adpt_rdr_t *adpt;
        void *ptr;
    } reader;
};

struct tsf_stream_file_output {
    int fd;
    tsf_bool_t keep_fd;
    
    tsf_serial_out_man_t *out_man;
    
    tsf_bool_t use_zip;
    union {
        tsf_buf_wtr_t *buf;
        tsf_zip_wtr_t *zip;
        void *ptr;
    } writer;
    
    tsf_bool_t all_good;
};

struct tsf_fsdb_connection {
    int fd;
    tsf_bool_t clear;
    tsf_stream_file_input_t *in;
    tsf_stream_file_output_t *out;
    uint8_t *buf;
    uint32_t buf_size;
    uint32_t buf_cursor;
};

struct tsf_fsdb {
    tsf_fsdb_kind_t kind;
    union {
        struct {
            DIR *dirhandle;
            const char *dirname;
            tsf_bool_t update;
            tsf_bool_t truncate;
            uint64_t create_cnt;
        } local;
        struct {
#ifdef TSF_HAVE_BSDSOCKS
            /* FIXME: open_remote() should attempt to connect just to see which
             * address works.  it should leave that connection open until the
             * first put/get request.  and it should leave enough info in here
             * to make subsequent connection requests possible. */
#ifdef TSF_HAVE_GETADDRINFO
            struct addrinfo *result;
            struct addrinfo *cur;
#else
            uint32_t addr;
            int port;
#endif
            tsf_fsdb_connection_t *connection;
#endif
        } remote;
    } u;
    tsf_limits_t *limits;
    tsf_zip_rdr_attr_t rdr_attr;
    tsf_zip_wtr_attr_t wtr_attr;

    tsf_mutex_t lock;
};

struct tsf_fsdb_response_s;

struct tsf_fsdb_in {
    tsf_fsdb_t *db;
    union {
        struct {
            tsf_buffer_t *key;
            tsf_stream_file_input_t *in;
        } local;
        struct {
            tsf_fsdb_connection_t *connection;
            struct tsf_fsdb_response_s *rsp;
            uint32_t rsp_cursor;
            tsf_serial_in_man_t *in_man;
        } remote;
    } u;
};

struct tsf_fsdb_out {
    tsf_fsdb_t *db;
    union {
        struct {
            char *partname;
            tsf_buffer_t *key;
            tsf_stream_file_output_t *out;
        } local;
        struct {
            tsf_fsdb_connection_t *connection;
            tsf_serial_out_man_t *out_man;
        } remote;
    } u;
};

/* min and max */
#define tsf_min(a,b) ((a)<(b)?(a):(b))
#define tsf_max(a,b) ((a)>(b)?(a):(b))

/* timestamping */
#ifdef HAVE_GETTIMEOFDAY
static TSF_inline int64_t tsf_cur_time(void) {
    int64_t result;
    struct timeval tv;

    gettimeofday(&tv,NULL);
    
    result=tv.tv_sec;
    result*=1000;
    result*=1000;
    result+=tv.tv_usec;
    result*=1000;
    
    return result;
}
#else
static TSF_inline int64_t tsf_cur_time(void) {
    return ((int64_t)time(NULL))*(1000*1000*1000);
}
#endif

/* the full error setting command */
void tsf_set_error_fullv(tsf_error_t code,
                         int sys_errno,
                         int deadly_signal,
                         int zlib_error,
                         const char *zlib_msg,
                         const char *format,
                         va_list lst);

void tsf_set_error_full(tsf_error_t code,
                        int sys_errno,
                        int deadly_signal,
			int zlib_error,
			const char *zlib_msg,
                        const char *format,
                        ...);

/* this is an strerror thingy for zlib.  it will abort if called when
 * no zlib support is compiled. */
const char *tsf_zlib_strerror(int code);

/* this is an strerror thingy for libbzip2.  it will abort if called
 * when no libbzip2 support is compiled. */
const char *tsf_libbzip2_strerror(int code);

/* this is just plain dumb.  thanks a lot, GNU ppl. */
#if defined(HAVE_STPCPY) && !defined(stpcpy)
char *stpcpy(char *dest,const char *src);
#endif

/* tsf_sort just calls qsort if available.  otherwise it is some shitty
 * sorting algo that I implemented. */
void tsf_sort(void *base,size_t nmemb,size_t size,
              int (*compar)(const void *a,const void *b));

/* sorts an array of uint32_t in ascending order. */
void tsf_ui32_sort(uint32_t *base,size_t nmemb);

/* string creation stuff.  on error, both functions just set errno. */
char *tsf_vasprintf(const char *format,va_list lst);

char *tsf_asprintf(const char *format,...);

/* hashtable stuff */
tsf_st_table *tsf_st_init_typetable();

/* limits stuff */

/* max number of types in one stream */
#define tsf_limits_max_types(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_types)

/* maximum size in bytes of a serialized type */
#define tsf_limits_max_type_size(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_type_size)

/* maximum total size of all types read in a session */
#define tsf_limits_max_total_type_size(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_total_type_size)

/* maximum size in bytes of a buffer */
#define tsf_limits_max_buf_size(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_buf_size)

/* container depth limit */
#define tsf_limits_depth_limit(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->depth_limit)

/* comments */
#define tsf_limits_allow_comments(limits) \
            ((limits)==NULL?tsf_true:(limits)->allow_comments)

/* maximum array length */
#define tsf_limits_max_array_length(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_array_length)

/* maximum struct size, in number of elements */
#define tsf_limits_max_struct_size(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_struct_size)

/* maximum choice size, in number of elements */
#define tsf_limits_max_choice_size(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_choice_size)

/* maximum string size */
#define tsf_limits_max_string_size(limits) \
            ((limits)==NULL?UINT32_MAX:(limits)->max_string_size)

/* endianness stuff */

/* get ntohl, htonl, ntohs, htons */
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#else
#ifdef HAVE_LITTLE_ENDIAN
static inline uint32_t ntohl(uint32_t x) {
    union {
        uint32_t x;
        uint8_t c[4];
    } u;
    u.x=x;
    u.c[0]^=u.c[3]^=u.c[0]^=u.c[3];
    u.c[1]^=u.c[2]^=u.c[1]^=u.c[2];
    return u.x;
}
static inline uint16_t ntohs(uint16_t x) {
    union {
        uint16_t x;
        uint8_t c[2];
    } u;
    u.x=x;
    u.c[0]^=u.c[1]^=u.c[0]^=u.c[1];
    return u.x;
}
#define htonl(x) (ntohl(x))
#define htons(x) (ntohs(x))
#else
#define ntohl(x) (x)
#define htonl(x) (x)
#define ntohs(x) (x)
#define htons(x) (x)
#endif
#endif

/* define stupid trivial stuff */
#define htonc(x) (x)
#define ntohc(x) (x)

#ifdef HAVE_BIG_ENDIAN

/* #undef NEED_INT_CONVERSION */

/* define long long equivalent of the arpa/inet functions */
#ifndef htonll
#define htonll(x) (x)
#endif

/* define misaligned copy equivalents of all of the arpa/inet functions */
static inline void copy_htonc(uint8_t *dst,
                              uint8_t *src) {
    *dst=*src;
}

static inline void copy_htons(uint8_t *dst,
                              uint8_t *src) {
#ifdef CAN_DO_MISALIGNED_LDST
    *((uint16_t*)dst)=*((uint16_t*)src);
#else
    dst[0]=src[0];
    dst[1]=src[1];
#endif
}

static inline void copy_htonl(uint8_t *dst,
                              uint8_t *src) {
#ifdef CAN_DO_MISALIGNED_LDST
    uint32_t tmp=*((uint32_t*)src);
    *((uint32_t*)dst)=tmp;
#else
    dst[0]=src[0];
    dst[1]=src[1];
    dst[2]=src[2];
    dst[3]=src[3];
#endif
}

static inline void copy_htonll(uint8_t *dst,
                               uint8_t *src) {
#ifdef CAN_DO_MISALIGNED_LDST
    *((uint64_t*)dst)=*((uint64_t*)src);
#else
    dst[0]=src[0];
    dst[1]=src[1];
    dst[2]=src[2];
    dst[3]=src[3];
    dst[4]=src[4];
    dst[5]=src[5];
    dst[6]=src[6];
    dst[7]=src[7];
#endif
}

#define copy_ntohc(dst,src) copy_htonc(dst,src)
#define copy_ntohs(dst,src) copy_htons(dst,src)
#define copy_ntohl(dst,src) copy_htonl(dst,src)
#define copy_ntohll(dst,src) copy_htonll(dst,src)

#else

#define NEED_INT_CONVERSION

#ifndef htonll
/* define long long equivalent of the arpa/inet functions */
static inline uint64_t htonll(uint64_t x) {
    /* probably not very efficient, but oh well. */
    return ((x >> 56) & UINT64_C(0x00000000000000ff))
        |  ((x >> 40) & UINT64_C(0x000000000000ff00))
        |  ((x >> 24) & UINT64_C(0x0000000000ff0000))
        |  ((x >>  8) & UINT64_C(0x00000000ff000000))
        |  ((x <<  8) & UINT64_C(0x000000ff00000000))
        |  ((x << 24) & UINT64_C(0x0000ff0000000000))
        |  ((x << 40) & UINT64_C(0x00ff000000000000))
        |  ((x << 56) & UINT64_C(0xff00000000000000));
}
#endif /* #ifndef htonll */

/* define misaligned copy equivalents of all of the arpa/inet functsions */
static inline void copy_htonc(uint8_t *dst,
                              uint8_t *src) {
    *dst=*src;
}

static inline void copy_htons(uint8_t *dst,
                              uint8_t *src) {
    dst[0]=src[1];
    dst[1]=src[0];
}

static inline void copy_htonl(uint8_t *dst,
                              uint8_t *src) {
    dst[0]=src[3];
    dst[1]=src[2];
    dst[2]=src[1];
    dst[3]=src[0];
}

static inline void copy_htonll(uint8_t *dst,
                               uint8_t *src) {
    dst[0]=src[7];
    dst[1]=src[6];
    dst[2]=src[5];
    dst[3]=src[4];
    dst[4]=src[3];
    dst[5]=src[2];
    dst[6]=src[1];
    dst[7]=src[0];
}

#define copy_ntohc(dst,src) copy_htonc(dst,src)
#define copy_ntohs(dst,src) copy_htons(dst,src)
#define copy_ntohl(dst,src) copy_htonl(dst,src)
#define copy_ntohll(dst,src) copy_htonll(dst,src)

#endif

#ifndef ntohll
#define ntohll(x) (htonll(x))
#endif

/* floating point stuff */

#ifdef TSF_HAVE_IEEE_FLOAT

#ifdef NEED_INT_CONVERSION
#define NEED_FLOAT_CONVERSION
#endif

static inline float htonf(float x) {
    uint32_t ret=htonl(*((long*)&x));
    return *((float*)&ret);
}

static inline double htond(double x) {
    uint64_t ret=htonll(*((uint64_t*)&x));
    return *((double*)&ret);
}

#define ntohf(x) (htonf(x))
#define ntohd(x) (htond(x))

#if !defined(NEED_INT_CONVERSION) &&\
    defined(CAN_DO_MISALIGNED_LDST) &&\
    !defined(CAN_DO_FLOAT_MISALIGNED_LDST)
static inline void copy_htonf(uint8_t *dst,
                              uint8_t *src) {
    dst[0]=src[0];
    dst[1]=src[1];
    dst[2]=src[2];
    dst[3]=src[3];
}

static inline void copy_htond(uint8_t *dst,
                              uint8_t *src) {
    dst[0]=src[0];
    dst[1]=src[1];
    dst[2]=src[2];
    dst[3]=src[3];
    dst[4]=src[4];
    dst[5]=src[5];
    dst[6]=src[6];
    dst[7]=src[7];
}
#else
#define copy_htonf(dst,src) copy_htonl(dst,src)
#define copy_htond(dst,src) copy_htonll(dst,src)
#endif

#define copy_ntohf(dst,src) copy_htonf(dst,src)
#define copy_ntohd(dst,src) copy_htond(dst,src)

#else

#define NEED_FLOAT_CONVERSION

#error "Currently, we only support IEEE floats!"

#endif

#if defined(NEED_FLOAT_CONVERSION) || defined(NEED_INT_COVERSION)
#define NEED_CONVERSION
#endif

/* destination-incrementing misaligned copy macros.  these do
 * no bounds checking. */

#define copy_htonc_incdst(dst,src) do {     \
    copy_htonc(dst,src);                    \
    (dst)=(void*)(((uint8_t*)(dst))+1);     \
} while (0)

#define copy_ntohc_incdst(dst,src) do {     \
    copy_ntohc(dst,src);                    \
    (dst)=(void*)(((uint8_t*)(dst))+1);     \
} while (0)

#define copy_htons_incdst(dst,src) do {     \
    copy_htons(dst,src);                    \
    (dst)=(void*)(((uint16_t*)(dst))+1);    \
} while (0)

#define copy_ntohs_incdst(dst,src) do {     \
    copy_ntohs(dst,src);                    \
    (dst)=(void*)(((uint16_t*)(dst))+1);    \
} while (0)

#define copy_htonl_incdst(dst,src) do {     \
    copy_htonl(dst,src);                    \
    (dst)=(void*)(((uint32_t*)(dst))+1);    \
} while (0)

#define copy_ntohl_incdst(dst,src) do {     \
    copy_ntohl(dst,src);                    \
    (dst)=(void*)(((uint32_t*)(dst))+1);    \
} while (0)

#define copy_htonll_incdst(dst,src) do {    \
    copy_htonll(dst,src);                   \
    (dst)=(void*)(((uint64_t*)(dst))+1);    \
} while (0)

#define copy_ntohll_incdst(dst,src) do {    \
    copy_ntohll(dst,src);                   \
    (dst)=(void*)(((uint64_t*)(dst))+1);    \
} while (0)

#define copy_htonf_incdst(dst,src) do {     \
    copy_htonf(dst,src);                    \
    (dst)=(void*)(((float*)(dst))+1);       \
} while (0)

#define copy_ntohf_incdst(dst,src) do {     \
    copy_ntohf(dst,src);                    \
    (dst)=(void*)(((float*)(dst))+1);       \
} while (0)

#define copy_htond_incdst(dst,src) do {     \
    copy_htond(dst,src);                    \
    (dst)=(void*)(((double*)(dst))+1);      \
} while (0)

#define copy_ntohd_incdst(dst,src) do {     \
    copy_ntohd(dst,src);                    \
    (dst)=(void*)(((double*)(dst))+1);      \
} while (0)

/* source-incrementing misaligned copy macros.  these do
 * no bounds checking. */

#define copy_htonc_incsrc(dst,src) do {     \
    copy_htonc(dst,src);                    \
    (src)=(void*)(((uint8_t*)(src))+1);     \
} while (0)

#define copy_ntohc_incsrc(dst,src) do {     \
    copy_ntohc(dst,src);                    \
    (src)=(void*)(((uint8_t*)(src))+1);     \
} while (0)

#define copy_htons_incsrc(dst,src) do {     \
    copy_htons(dst,src);                    \
    (src)=(void*)(((uint16_t*)(src))+1);    \
} while (0)

#define copy_ntohs_incsrc(dst,src) do {     \
    copy_ntohs(dst,src);                    \
    (src)=(void*)(((uint16_t*)(src))+1);    \
} while (0)

#define copy_htonl_incsrc(dst,src) do {     \
    copy_htonl(dst,src);                    \
    (src)=(void*)(((uint32_t*)(src))+1);    \
} while (0)

#define copy_ntohl_incsrc(dst,src) do {     \
    copy_ntohl(dst,src);                    \
    (src)=(void*)(((uint32_t*)(src))+1);    \
} while (0)

#define copy_htonll_incsrc(dst,src) do {    \
    copy_htonll(dst,src);                   \
    (src)=(void*)(((uint64_t*)(src))+1);    \
} while (0)

#define copy_ntohll_incsrc(dst,src) do {    \
    copy_ntohll(dst,src);                   \
    (src)=(void*)(((uint64_t*)(src))+1);    \
} while (0)

#define copy_htonf_incsrc(dst,src) do {     \
    copy_htonf(dst,src);                    \
    (src)=(void*)(((float*)(src))+1);       \
} while (0)

#define copy_ntohf_incsrc(dst,src) do {     \
    copy_ntohf(dst,src);                    \
    (src)=(void*)(((float*)(src))+1);       \
} while (0)

#define copy_htond_incsrc(dst,src) do {     \
    copy_htond(dst,src);                    \
    (src)=(void*)(((double*)(src))+1);      \
} while (0)

#define copy_ntohd_incsrc(dst,src) do {     \
    copy_ntohd(dst,src);                    \
    (src)=(void*)(((double*)(src))+1);      \
} while (0)

/* source-incrementing misaligned copy macros.  these do
 * bounds checks.  when a bounds check fails, they jump to
 * the label you give them. */

#define copy_htonc_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<1) {        \
        goto label;                                     \
    }                                                   \
    copy_htonc(dst,src);                                \
    (src)=(void*)(((uint8_t*)(src))+1);                 \
} while (0)

#define copy_ntohc_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<1) {        \
        goto label;                                     \
    }                                                   \
    copy_ntohc(dst,src);                                \
    (src)=(void*)(((uint8_t*)(src))+1);                 \
} while (0)

#define copy_htons_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<2) {        \
        goto label;                                     \
    }                                                   \
    copy_htons(dst,src);                                \
    (src)=(void*)(((uint16_t*)(src))+1);                \
} while (0)

#define copy_ntohs_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<2) {        \
        goto label;                                     \
    }                                                   \
    copy_ntohs(dst,src);                                \
    (src)=(void*)(((uint16_t*)(src))+1);                \
} while (0)

#define copy_htonl_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<4) {        \
        goto label;                                     \
    }                                                   \
    copy_htonl(dst,src);                                \
    (src)=(void*)(((uint32_t*)(src))+1);                \
} while (0)

#define copy_ntohl_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<4) {        \
        goto label;                                     \
    }                                                   \
    copy_ntohl(dst,src);                                \
    (src)=(void*)(((uint32_t*)(src))+1);                \
} while (0)

#define copy_htonll_incsrc_bc(dst,src,end,label) do {   \
    if (((uint8_t*)(end))-((uint8_t*)(src))<8) {        \
        goto label;                                     \
    }                                                   \
    copy_htonll(dst,src);                               \
    (src)=(void*)(((uint64_t*)(src))+1);                \
} while (0)

#define copy_ntohll_incsrc_bc(dst,src,end,label) do {   \
    if (((uint8_t*)(end))-((uint8_t*)(src))<8) {        \
        goto label;                                     \
    }                                                   \
    copy_ntohll(dst,src);                               \
    (src)=(void*)(((uint64_t*)(src))+1);                \
} while (0)

#define copy_htonf_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<4) {        \
        goto label;                                     \
    }                                                   \
    copy_htonf(dst,src);                                \
    (src)=(void*)(((float*)(src))+1);                   \
} while (0)

#define copy_ntohf_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<4) {        \
        goto label;                                     \
    }                                                   \
    copy_ntohf(dst,src);                                \
    (src)=(void*)(((float*)(src))+1);                   \
} while (0)

#define copy_htond_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<8) {        \
        goto label;                                     \
    }                                                   \
    copy_htond(dst,src);                                \
    (src)=(void*)(((double*)(src))+1);                  \
} while (0)

#define copy_ntohd_incsrc_bc(dst,src,end,label) do {    \
    if (((uint8_t*)(end))-((uint8_t*)(src))<8) {        \
        goto label;                                     \
    }                                                   \
    copy_ntohd(dst,src);                                \
    (src)=(void*)(((double*)(src))+1);                  \
} while (0)

static inline size_t size_of_tsf_integer(tsf_integer_t src) {
    if (src >= -TSF_INTEGER_SMALL_LIM && src < TSF_INTEGER_SMALL_LIM) {
        return 1;
    }
    
    if (src >= -TSF_INTEGER_MEDIUM_LIM && src < TSF_INTEGER_MEDIUM_LIM) {
        return 2;
    }
    
    if (src >= -TSF_INTEGER_LARGE_LIM && src < TSF_INTEGER_LARGE_LIM) {
        return 4;
    }
    
    return 5;
}

static inline size_t size_of_tsf_unsigned(tsf_unsigned_t src) {
    if (src < TSF_UNSIGNED_SMALL_LIM) {
        return 1;
    }
    
    if (src < TSF_UNSIGNED_MEDIUM_LIM) {
        return 2;
    }
    
    if (src < TSF_UNSIGNED_LARGE_LIM) {
        return 4;
    }
    
    return 5;
}

static inline size_t size_of_tsf_long(tsf_long_t src) {
    if (src >= -TSF_INTEGER_SMALL_LIM && src < TSF_INTEGER_SMALL_LIM) {
        return 1;
    }
    
    if (src >= -TSF_INTEGER_MEDIUM_LIM && src < TSF_INTEGER_MEDIUM_LIM) {
        return 2;
    }
    
    if (src >= -TSF_INTEGER_LARGE_LIM && src < TSF_INTEGER_LARGE_LIM) {
        return 4;
    }
    
    return 9;
}

static inline size_t write_tsf_integer(uint8_t *dst, tsf_integer_t src) {
    if (src >= -TSF_INTEGER_SMALL_LIM && src < TSF_INTEGER_SMALL_LIM) {
        dst[0] = ((uint8_t)(int8_t)src) & ~TSF_INTEGER_MEDIUM_BIT;
        return 1;
    }
    
    /* FIXME: The remainder of this function could be an out-of-line slow path. */
    
    if (src >= -TSF_INTEGER_MEDIUM_LIM && src < TSF_INTEGER_MEDIUM_LIM) {
        if (src < 0) {
            src += TSF_INTEGER_SMALL_LIM;
        } else {
            src -= TSF_INTEGER_SMALL_LIM;
        }
        
        dst[0] =
            (((int8_t)((src >> 8) & 255)) - TSF_INTEGER_MEDIUM_DIFF) |
            TSF_INTEGER_MEDIUM_BIT;
        dst[1] = src & 255;
        return 2;
    }
    
    if (src >= -TSF_INTEGER_LARGE_LIM && src < TSF_INTEGER_LARGE_LIM) {
        if (src < 0) {
            src += TSF_INTEGER_MEDIUM_LIM;
        } else {
            src -= TSF_INTEGER_MEDIUM_LIM;
        }
        
        dst[0] = TSF_INTEGER_LARGE_TAG;
        dst[1] = (src >> 16) & 255;
        dst[2] = (src >> 8) & 255;
        dst[3] = src & 255;
        return 4;
    }
    
    dst[0] = TSF_INTEGER_HUGE_TAG;
    dst[1] = (src >> 24) & 255;
    dst[2] = (src >> 16) & 255;
    dst[3] = (src >> 8) & 255;
    dst[4] = src & 255;
    return 5;
}

static inline size_t write_tsf_unsigned(uint8_t *dst, tsf_unsigned_t src) {
    if (src < TSF_UNSIGNED_SMALL_LIM) {
        dst[0] = src;
        return 1;
    }
    
    /* FIXME: The remainder of this function could be an out-of-line slow path. */
    
    if (src < TSF_UNSIGNED_MEDIUM_LIM) {
        src -= TSF_UNSIGNED_SMALL_LIM;
        
        dst[0] = ((src >> 8) & 255) | TSF_UNSIGNED_MEDIUM_BIT;
        dst[1] = src & 255;
        return 2;
    }
    
    if (src < TSF_UNSIGNED_LARGE_LIM) {
        src -= TSF_UNSIGNED_MEDIUM_LIM;
        
        dst[0] = TSF_UNSIGNED_LARGE_TAG;
        dst[1] = (src >> 16) & 255;
        dst[2] = (src >> 8) & 255;
        dst[3] = src & 255;
        return 4;
    }
    
    dst[0] = TSF_UNSIGNED_HUGE_TAG;
    dst[1] = (src >> 24) & 255;
    dst[2] = (src >> 16) & 255;
    dst[3] = (src >> 8) & 255;
    dst[4] = src & 255;
    return 5;
}

static inline size_t write_tsf_long(uint8_t *dst, tsf_long_t src) {
    if (src >= -TSF_INTEGER_SMALL_LIM && src < TSF_INTEGER_SMALL_LIM) {
        dst[0] = ((uint8_t)(int8_t)src) & ~TSF_INTEGER_MEDIUM_BIT;
        return 1;
    }
    
    /* FIXME: The remainder of this function could be an out-of-line slow path. */
    
    if (src >= -TSF_INTEGER_MEDIUM_LIM && src < TSF_INTEGER_MEDIUM_LIM) {
        if (src < 0) {
            src += TSF_INTEGER_SMALL_LIM;
        } else {
            src -= TSF_INTEGER_SMALL_LIM;
        }
        
        dst[0] =
            (((int8_t)((src >> 8) & 255)) - TSF_INTEGER_MEDIUM_DIFF) |
            TSF_INTEGER_MEDIUM_BIT;
        dst[1] = src & 255;
        return 2;
    }
    
    if (src >= -TSF_INTEGER_LARGE_LIM && src < TSF_INTEGER_LARGE_LIM) {
        if (src < 0) {
            src += TSF_INTEGER_MEDIUM_LIM;
        } else {
            src -= TSF_INTEGER_MEDIUM_LIM;
        }
        
        dst[0] = TSF_INTEGER_LARGE_TAG;
        dst[1] = (src >> 16) & 255;
        dst[2] = (src >> 8) & 255;
        dst[3] = src & 255;
        return 4;
    }
    
    dst[0] = TSF_INTEGER_HUGE_TAG;
    dst[1] = (src >> 56) & 255;
    dst[2] = (src >> 48) & 255;
    dst[3] = (src >> 40) & 255;
    dst[4] = (src >> 32) & 255;
    dst[5] = (src >> 24) & 255;
    dst[6] = (src >> 16) & 255;
    dst[7] = (src >> 8) & 255;
    dst[8] = src & 255;
    return 9;
}

static inline size_t size_of_encoded_tsf_integer(int8_t value) {
    if (!(value & TSF_INTEGER_MEDIUM_BIT)) {
        return 1;
    }
    
    switch (value) {
    case TSF_INTEGER_LARGE_TAG:
        return 4;
    case TSF_INTEGER_HUGE_TAG:
        return 5;
    default:
        return 2;
    }
}

static inline size_t size_of_encoded_tsf_unsigned(uint8_t value) {
    if (!(value & TSF_UNSIGNED_MEDIUM_BIT)) {
        return 1;
    }
    
    switch (value) {
    case TSF_UNSIGNED_LARGE_TAG:
        return 4;
    case TSF_UNSIGNED_HUGE_TAG:
        return 5;
    default:
        return 2;
    }
}

static inline size_t size_of_encoded_tsf_long(int8_t value) {
    if (!(value & TSF_INTEGER_MEDIUM_BIT)) {
        return 1;
    }
    
    switch (value) {
    case TSF_INTEGER_LARGE_TAG:
        return 4;
    case TSF_INTEGER_HUGE_TAG:
        return 9;
    default:
        return 2;
    }
}

static inline size_t read_tsf_integer(tsf_integer_t *dst, uint8_t *src, size_t src_remaining) {
    if (!src_remaining)
        return 0;
    
    int8_t head_value = src[0];
    if (!(head_value & TSF_INTEGER_MEDIUM_BIT)) {
        *dst = ((int8_t)(head_value << 1)) >> 1;
        return 1;
    }
    
    /* FIXME: The remainder of this function could be an out-of-line slow path. */
    
    switch (head_value) {
    case TSF_INTEGER_LARGE_TAG: {
        if (src_remaining < 4)
            return 0;
        int32_t value =
            (((int32_t)(int8_t)src[1]) << 16) |
            (((int32_t)(uint32_t)src[2]) << 8) |
            ((int32_t)(uint32_t)src[3]);
        if (value < 0)
            value -= TSF_INTEGER_MEDIUM_LIM;
        else
            value += TSF_INTEGER_MEDIUM_LIM;
        *dst = value;
        return 4;
    }
    case TSF_INTEGER_HUGE_TAG: {
        if (src_remaining < 5)
            return 0;
        int32_t value =
            (((int32_t)(int8_t)src[1]) << 24) |
            (((int32_t)(uint32_t)src[2]) << 16) |
            (((int32_t)(uint32_t)src[3]) << 8) |
            ((int32_t)(uint32_t)src[4]);
        *dst = value;
        return 5;
    }
    default: {
        if (src_remaining < 2)
            return 0;
        int32_t value =
            ((int32_t)(uint32_t)src[1]) |
            (((int32_t)((((int8_t)(head_value << 1)) >> 1) + TSF_INTEGER_MEDIUM_DIFF)) << 8);
        if (value < 0)
            value -= TSF_INTEGER_SMALL_LIM;
        else
            value += TSF_INTEGER_SMALL_LIM;
        *dst = value;
        return 2;
    } }
}

static inline size_t read_tsf_unsigned(tsf_unsigned_t *dst, uint8_t *src, size_t src_remaining) {
    if (!src_remaining)
        return 0;
    
    uint8_t head_value = src[0];
    if (!(head_value & TSF_UNSIGNED_MEDIUM_BIT)) {
        *dst = head_value;
        return 1;
    }
    
    /* FIXME: The remainder of this function could be an out-of-line slow path. */
    
    switch (head_value) {
    case TSF_UNSIGNED_LARGE_TAG: {
        if (src_remaining < 4)
            return 0;
        int32_t value =
            (((int32_t)(int8_t)src[1]) << 16) |
            (((int32_t)(uint32_t)src[2]) << 8) |
            ((int32_t)(uint32_t)src[3]);
        value += TSF_UNSIGNED_MEDIUM_LIM;
        *dst = value;
        return 4;
    }
    case TSF_UNSIGNED_HUGE_TAG: {
        if (src_remaining < 5)
            return 0;
        int32_t value =
            (((int32_t)(int8_t)src[1]) << 24) |
            (((int32_t)(uint32_t)src[2]) << 16) |
            (((int32_t)(uint32_t)src[3]) << 8) |
            ((int32_t)(uint32_t)src[4]);
        *dst = value;
        return 5;
    }
    default: {
        if (src_remaining < 2)
            return 0;
        int32_t value =
            ((int32_t)(uint32_t)src[1]) |
            ((head_value & ~TSF_UNSIGNED_MEDIUM_BIT) << 8);
        value += TSF_UNSIGNED_SMALL_LIM;
        *dst = value;
        return 2;
    } }
}

static inline size_t read_tsf_long(tsf_long_t *dst, uint8_t *src, size_t src_remaining) {
    if (!src_remaining)
        return 0;
    
    int8_t head_value = src[0];
    if (!(head_value & TSF_INTEGER_MEDIUM_BIT)) {
        *dst = ((int8_t)(head_value << 1)) >> 1;
        return 1;
    }
    
    /* FIXME: The remainder of this function could be an out-of-line slow path. */
    
    switch (head_value) {
    case TSF_INTEGER_LARGE_TAG: {
        if (src_remaining < 4)
            return 0;
        int64_t value =
            (((int64_t)(int8_t)src[1]) << 16) |
            (((int64_t)(uint64_t)src[2]) << 8) |
            ((int64_t)(uint64_t)src[3]);
        if (value < 0)
            value -= TSF_INTEGER_MEDIUM_LIM;
        else
            value += TSF_INTEGER_MEDIUM_LIM;
        *dst = value;
        return 4;
    }
    case TSF_INTEGER_HUGE_TAG: {
        if (src_remaining < 9)
            return 0;
        int64_t value =
            (((int64_t)(int8_t)src[1]) << 56) |
            (((int64_t)(uint64_t)src[2]) << 48) |
            (((int64_t)(uint64_t)src[3]) << 40) |
            (((int64_t)(uint64_t)src[4]) << 32) |
            (((int64_t)(uint64_t)src[5]) << 24) |
            (((int64_t)(uint64_t)src[6]) << 16) |
            (((int64_t)(uint64_t)src[7]) << 8) |
            ((int64_t)(uint64_t)src[8]);
        *dst = value;
        return 9;
    }
    default: {
        if (src_remaining < 2)
            return 0;
        int64_t value =
            ((int64_t)(uint64_t)src[1]) |
            (((int64_t)((((int8_t)(head_value << 1)) >> 1) + TSF_INTEGER_MEDIUM_DIFF)) << 8);
        if (value < 0)
            value -= TSF_INTEGER_SMALL_LIM;
        else
            value += TSF_INTEGER_SMALL_LIM;
        *dst = value;
        return 2;
    } }
}

#define read_tsf_integer_incsrc(dst, src, end, bounds_error) do {       \
        size_t total_bytes_read =                                       \
            read_tsf_integer((dst), (src), (end) - (src));              \
        if (!total_bytes_read) {                                        \
            goto bounds_error;                                          \
        }                                                               \
                                                                        \
        (src) += total_bytes_read;                                      \
    } while (0)

#define read_tsf_unsigned_incsrc(dst, src, end, bounds_error) do {      \
        size_t total_bytes_read =                                       \
            read_tsf_unsigned((dst), (src), (end) - (src));             \
        if (!total_bytes_read) {                                        \
            goto bounds_error;                                          \
        }                                                               \
                                                                        \
        (src) += total_bytes_read;                                      \
    } while (0)

#define read_tsf_long_incsrc(dst, src, end, bounds_error) do {          \
        size_t total_bytes_read =                                       \
            read_tsf_long((dst), (src), (end) - (src));                 \
        if (!total_bytes_read) {                                        \
            goto bounds_error;                                          \
        }                                                               \
                                                                        \
        (src) += total_bytes_read;                                      \
    } while (0)

static inline size_t skip_string(const char *start, const char *end) {
    const char *cur = start;
    for (;;) {
        if (cur >= end)
            return 0;
        if (!*cur)
            return cur + 1 - start;
        cur++;
    }
}

/* internal recursive function for reading types */
tsf_type_t *tsf_type_read_rec(tsf_reader_t reader,
                              void *arg,
                              tsf_limits_t *limits,
                              uint32_t depth);

/* read a type table */
tsf_type_table_t* tsf_type_table_read(const char *name,
                                      tsf_reader_t reader,
                                      void *arg,
                                      tsf_limits_t *limits,
                                      uint32_t size_limit,
                                      uint32_t depth);

/* clones a type, returning a new one just like it. note this is
 * actually deep copy. */
tsf_type_t* tsf_type_clone(tsf_type_t *type);

/* gives you a version of the given type that you own; i.e. if you make
 * changes to it in-place then nobody else will notice. this is a O(1)
 * operation that returns the passed-in type object if it already has a
 * reference count of 1. otherwise the type is copied using
 * tsf_type_clone(). */
tsf_type_t* tsf_type_own_begin(tsf_type_t *type);

/* commits to an ownership operation by relinquishing our reference to
 * the original type. */
void tsf_type_own_commit(tsf_type_t **dst, tsf_type_t *ret);

/* undoes an ownership operation by deleting the clone if necessary. */
void tsf_type_own_abort(tsf_type_t *dst, tsf_type_t *ret);

/* recompute the hash code */
void tsf_type_recompute_hash(tsf_type_t *type);

/* create a tsf_buffer in which the caller owns the tsf_buffer object itself
   and the original backing store. this can be used to read stack-allocated
   buffers. this must be destroyed with tsf_buffer_destroy_custom(). few
   functions work on the buffer that this returns. the tsf_buffer_read_simple
   and tsf_buffer_read_small_with_type_code functions are two functions that
   do work with custom buffers.

   note that this really needs to be internal API, since it requires that the
   caller knows exactly the sizeof(tsf_buffer_t). if we wanted an external
   vesion of this API, we'd have the user just pass in a backing store and we'd
   partition it into a tsf_buffer_t and the actual backing store. we'd also
   kindly report error if the buffer wasn't big enough for a tsf_buffer_t. */
void tsf_buffer_initialize_custom(tsf_buffer_t *buf,
                                  void *backing_store,
                                  size_t backing_store_size);

/* destroy a tsf_buffer created with tsf_buffer_initialize_custom(). */
void tsf_buffer_destroy_custom(tsf_buffer_t *buf);

/* create a tsf_buffer that is ready for use with tsf_buffer_read_simple,
   tsf_buffer_read_small_with_type_code, and the GPC generator. */
tsf_buffer_t *tsf_buffer_create_bare(void);

/* create a tsf_buffer that is ready for use with GPC generator with a non-NULL
 * region. */
tsf_buffer_t *tsf_buffer_create_bare_in(void* region);

/* destroy a tsf_buffer that was created with tsf_buffer_create_bare()
   but didn't yet have anything else done to it. */
void tsf_buffer_destroy_bare(tsf_buffer_t *buf);

/* read/write a buffer using the old buffer framing format, which is still
 * appropriate for large buffers. */
tsf_bool_t tsf_buffer_read_simple(tsf_buffer_t *buf,
                                  tsf_type_in_map_t *enclosing_types,
                                  tsf_reader_t reader,
                                  void *arg,
                                  tsf_limits_t *limits);
tsf_bool_t tsf_buffer_write_simple(tsf_buffer_t *buf,
                                   uint32_t type_code,
                                   tsf_type_out_map_t *type_map,
                                   tsf_writer_t writer,
                                   void *arg);

/* read/write a small buffer using the new framing format, which only works
 * for small buffers. */
tsf_bool_t tsf_buffer_read_small_with_type_code(tsf_buffer_t *buf,
                                                uint32_t type_code,
                                                tsf_type_in_map_t *enclosing_types,
                                                tsf_reader_t reader,
                                                void *arg,
                                                tsf_limits_t *limits);
tsf_bool_t tsf_buffer_write_small_with_type_code(tsf_buffer_t *buf,
                                                 tsf_type_out_map_t *type_map,
                                                 tsf_writer_t writer,
                                                 void *arg);

/* read into an already-created buffer. the buffer should have been created
   using either tsf_buffer_initialize_custom() or tsf_buffer_create_bare() */
tsf_bool_t tsf_serial_in_man_read_existing_buffer(tsf_serial_in_man_t *in_man,
                                                  tsf_buffer_t *buf);

/* read a buffer from a file that was openned for reading using an already-created
   buffer. */
tsf_bool_t tsf_stream_file_input_read_existing_buffer(tsf_stream_file_input_t *file,
                                                      tsf_buffer_t *buf);

/* generate into an existing buffer created usign tsf_buffer_initialize_custom(). */
tsf_bool_t tsf_generator_generate_existing_buffer(tsf_genrtr_t *gen,
                                                  void *data,
                                                  tsf_buffer_t *buf);

/* write a type according to the serial protocol */
tsf_bool_t tsf_serial_out_man_write_type(tsf_writer_t writer,
                                         void *arg,
                                         tsf_type_t *type);

/* verify that the data is valid.  basically just does NULL checks. */
tsf_bool_t tsf_reflect_verify(tsf_reflect_t *data);

/* generate a gpint prototype for a generator.  a generator takes
 * the following arguments on the stack: a pointer to a tsf_type_t,
 * and a pointer to the user structure.  the generator returns a
 * pointer to a tsf_buffer_t, or 0 if an error occurred. */
gpc_proto_t *tsf_gpc_code_gen_generate_generator(tsf_type_t *type);

/* generate a gpint prototype for a parser.  a parser takes the
 * following arguments on the stack: a pointer to a tsf_buffer_t.
 * it returns a pointer to a user structure that is also a region. */
gpc_proto_t *tsf_gpc_code_gen_generate_parser(tsf_type_t *dest_type,
                                              tsf_type_t *src_type);

/* generate a gpint prototype for a converter.  a converter takes
 * the following arguments on the stack: a pointer to the destination,
 * which may be NULL indicating that the destination should be
 * allocated, and a pointer to the source.  the region register
 * may be NULL only if the destination is NULL; if the region
 * is NULL it means that a new region should be allocated.  a
 * converter returns the destination. */
gpc_proto_t *tsf_gpc_code_gen_generate_converter(tsf_type_t *dest_type,
                                                 tsf_type_t *src_type);

/* generate a gpint prototype for a destroyer.  a destroyer takes the
 * following arguments on the stack: a pointer to the user structure.
 * it returns 0. */
gpc_proto_t *tsf_gpc_code_gen_generate_destroyer(tsf_type_t *type);

/* internal FSDB put function.  just gives you a file descriptor
 * for writing to the file and the file name. */
tsf_bool_t tsf_fsdb_guts_put(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key,
                             int *fd,
                             char **partflnm);

/* internal FSDB commit function. */
tsf_bool_t tsf_fsdb_guts_commit(tsf_fsdb_t *fsdb,
                                char *partflnm,
                                tsf_buffer_t *key,
                                char **dataflnm);

/* internal FSDB abort function. */
tsf_bool_t tsf_fsdb_guts_abort(tsf_fsdb_t *fsdb,
                               char *partflnm);

/* internal FSDB get function. */
tsf_bool_t tsf_fsdb_guts_get(tsf_fsdb_t *fsdb,
                             tsf_buffer_t *key,
                             int *fd,
                             char **dataflnm);

/* internal FSDB rm function. */
tsf_bool_t tsf_fsdb_guts_rm(tsf_fsdb_t *fsdb,
                            tsf_buffer_t *key,
                            char **dataflnm);

/* abort because we screwed up big time */
#define tsf_abort(msg) do {						\
	fprintf(stderr,"tsf_abort(\"%s\"): called at %s:%d\n",		\
		msg,__FILE__,__LINE__);					\
	abort();							\
    } while (0)

/* function version of abort */
void tsf_f_abort(const char *msg);

/* make an assertion that, if it fails, causes us to abort. we don't
 * support the notion of optional assertions; they are always enabled. */
#define tsf_assert(exp) do {						\
	if (!(exp)) {							\
	    fprintf(stderr,"tsf_assert(%s): failed at %s:%d\n",		\
		    #exp,__FILE__,__LINE__);				\
	    abort();							\
	}								\
    } while (0)

#endif


