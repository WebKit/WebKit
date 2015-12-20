/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef XPCSPI_h
#define XPCSPI_h

#include <dispatch/dispatch.h>
#include <os/object.h>

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
#include <xpc/xpc.h>
#else

#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(xpc_object);
typedef xpc_object_t xpc_connection_t;

static ALWAYS_INLINE void _xpc_object_validate(xpc_object_t object)
{
    void *isa = *(void * volatile *)(OS_OBJECT_BRIDGE void *)object;
    (void)isa;
}

#define XPC_GLOBAL_OBJECT(object) ((OS_OBJECT_BRIDGE xpc_object_t)&(object))

#else // OS_OBJECT_USE_OBJC

typedef void* xpc_object_t;
typedef void* xpc_connection_t;

#define XPC_GLOBAL_OBJECT(object) (&(object))

#endif // OS_OBJECT_USE_OBJC

typedef const struct _xpc_type_s* xpc_type_t;

#if COMPILER_SUPPORTS(BLOCKS)
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t);
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);
typedef void (^xpc_handler_t)(xpc_object_t);
#endif

typedef void (*xpc_connection_handler_t)(xpc_connection_t connection);

#define XPC_ARRAY_APPEND ((size_t)(-1))
#define XPC_ERROR_CONNECTION_INVALID XPC_GLOBAL_OBJECT(_xpc_error_connection_invalid)
#define XPC_ERROR_TERMINATION_IMMINENT XPC_GLOBAL_OBJECT(_xpc_error_termination_imminent)
#define XPC_TYPE_ARRAY (&_xpc_type_array)
#define XPC_TYPE_BOOL (&_xpc_type_bool)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_TYPE_ERROR (&_xpc_type_error)
#define XPC_TYPE_STRING (&_xpc_type_string)

#endif // PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)

#if USE(APPLE_INTERNAL_SDK)
#include <xpc/private.h>
#endif

EXTERN_C const struct _xpc_dictionary_s _xpc_error_connection_invalid;
EXTERN_C const struct _xpc_dictionary_s _xpc_error_termination_imminent;

EXTERN_C const struct _xpc_type_s _xpc_type_array;
EXTERN_C const struct _xpc_type_s _xpc_type_bool;
EXTERN_C const struct _xpc_type_s _xpc_type_dictionary;
EXTERN_C const struct _xpc_type_s _xpc_type_error;
EXTERN_C const struct _xpc_type_s _xpc_type_string;

EXTERN_C xpc_object_t xpc_array_create(const xpc_object_t*, size_t count);
#if COMPILER_SUPPORTS(BLOCKS)
EXTERN_C bool xpc_array_apply(xpc_object_t, xpc_array_applier_t);
EXTERN_C bool xpc_dictionary_apply(xpc_object_t xdict, xpc_dictionary_applier_t applier);
#endif
EXTERN_C size_t xpc_array_get_count(xpc_object_t);
EXTERN_C const char* xpc_array_get_string(xpc_object_t, size_t index);
EXTERN_C void xpc_array_set_string(xpc_object_t, size_t index, const char* string);
EXTERN_C bool xpc_bool_get_value(xpc_object_t);
EXTERN_C void xpc_connection_cancel(xpc_connection_t);
EXTERN_C xpc_connection_t xpc_connection_create(const char* name, dispatch_queue_t);
EXTERN_C xpc_connection_t xpc_connection_create_mach_service(const char* name, dispatch_queue_t, uint64_t flags);
EXTERN_C pid_t xpc_connection_get_pid(xpc_connection_t);
EXTERN_C void xpc_connection_resume(xpc_connection_t);
EXTERN_C void xpc_connection_send_message(xpc_connection_t, xpc_object_t);
EXTERN_C void xpc_connection_send_message_with_reply(xpc_connection_t, xpc_object_t, dispatch_queue_t, xpc_handler_t);
EXTERN_C void xpc_connection_set_event_handler(xpc_connection_t, xpc_handler_t);
EXTERN_C void xpc_connection_set_target_queue(xpc_connection_t, dispatch_queue_t);
EXTERN_C xpc_object_t xpc_dictionary_create(const char*  const* keys, const xpc_object_t*, size_t count);
EXTERN_C xpc_object_t xpc_dictionary_create_reply(xpc_object_t);
EXTERN_C int xpc_dictionary_dup_fd(xpc_object_t, const char* key);
EXTERN_C xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t);
EXTERN_C bool xpc_dictionary_get_bool(xpc_object_t, const char* key);
EXTERN_C const char* xpc_dictionary_get_string(xpc_object_t, const char* key);
EXTERN_C uint64_t xpc_dictionary_get_uint64(xpc_object_t, const char* key);
EXTERN_C xpc_object_t xpc_dictionary_get_value(xpc_object_t, const char* key);
EXTERN_C void xpc_dictionary_set_bool(xpc_object_t, const char* key, bool value);
EXTERN_C void xpc_dictionary_set_fd(xpc_object_t, const char* key, int fd);
EXTERN_C void xpc_dictionary_set_string(xpc_object_t, const char* key, const char* string);
EXTERN_C void xpc_dictionary_set_uint64(xpc_object_t, const char* key, uint64_t value);
EXTERN_C void xpc_dictionary_set_value(xpc_object_t, const char*key, xpc_object_t value);
EXTERN_C xpc_type_t xpc_get_type(xpc_object_t);
EXTERN_C void xpc_main(xpc_connection_handler_t);
EXTERN_C const char* xpc_string_get_string_ptr(xpc_object_t);
EXTERN_C void xpc_transaction_begin(void);
EXTERN_C void xpc_track_activity(void);

EXTERN_C xpc_object_t xpc_connection_copy_entitlement_value(xpc_connection_t, const char* entitlement);
EXTERN_C void xpc_connection_get_audit_token(xpc_connection_t, audit_token_t*);
EXTERN_C void xpc_connection_kill(xpc_connection_t, int);
EXTERN_C void xpc_connection_set_instance(xpc_connection_t, uuid_t);
EXTERN_C mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t, const char*);
EXTERN_C void xpc_dictionary_set_mach_send(xpc_object_t, const char*, mach_port_t);

EXTERN_C void xpc_connection_set_bootstrap(xpc_connection_t, xpc_object_t bootstrap);
EXTERN_C xpc_object_t xpc_copy_bootstrap(void);
EXTERN_C void xpc_connection_set_oneshot_instance(xpc_connection_t, uuid_t instance);

#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#if !defined(xpc_retain)
#define xpc_retain(object) ({ xpc_object_t _o = (object); _xpc_object_validate(_o); [_o retain]; })
#endif
#else
EXTERN_C xpc_object_t xpc_retain(xpc_object_t);
#endif

#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#if !defined(xpc_retain)
#define xpc_release(object) ({ xpc_object_t _o = (object); _xpc_object_validate(_o); [_o release]; })
#endif
#else
EXTERN_C void xpc_release(xpc_object_t);
#endif

#endif // XPCSPI_h
