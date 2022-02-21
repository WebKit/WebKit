/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#pragma once

#include <dispatch/dispatch.h>
#include <os/object.h>

#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
#include <xpc/xpc.h>
#else

#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(xpc_object);
typedef xpc_object_t xpc_connection_t;
typedef xpc_object_t xpc_endpoint_t;
typedef xpc_object_t xpc_activity_t;

static ALWAYS_INLINE void _xpc_object_validate(xpc_object_t object)
{
    void *isa = *(void * volatile *)(OS_OBJECT_BRIDGE void *)object;
    (void)isa;
}

#define XPC_GLOBAL_OBJECT(object) ((OS_OBJECT_BRIDGE xpc_object_t)&(object))

#else // OS_OBJECT_USE_OBJC

typedef void* xpc_object_t;
typedef void* xpc_connection_t;
typedef void* xpc_endpoint_t;
typedef void* xpc_activity_t;

#define XPC_GLOBAL_OBJECT(object) (&(object))

#endif // OS_OBJECT_USE_OBJC

enum {
    XPC_ACTIVITY_STATE_CHECK_IN,
    XPC_ACTIVITY_STATE_WAIT,
    XPC_ACTIVITY_STATE_RUN,
    XPC_ACTIVITY_STATE_DEFER,
    XPC_ACTIVITY_STATE_CONTINUE,
    XPC_ACTIVITY_STATE_DONE,
};
typedef long xpc_activity_state_t;
typedef const struct _xpc_type_s* xpc_type_t;
extern "C" const xpc_object_t XPC_ACTIVITY_CHECK_IN;
extern "C" const char * const XPC_ACTIVITY_INTERVAL;
extern "C" const char * const XPC_ACTIVITY_GRACE_PERIOD;
extern "C" const char * const XPC_ACTIVITY_PRIORITY;
extern "C" const char * const XPC_ACTIVITY_PRIORITY_MAINTENANCE;
extern "C" const char * const XPC_ACTIVITY_ALLOW_BATTERY;
extern "C" const char * const XPC_ACTIVITY_REPEATING;

#if PLATFORM(IOS_FAMILY) && __has_attribute(noescape)
#define XPC_NOESCAPE __attribute__((__noescape__))
#endif

#if COMPILER_SUPPORTS(BLOCKS)
typedef bool (^xpc_array_applier_t)(size_t index, xpc_object_t);
typedef bool (^xpc_dictionary_applier_t)(const char *key, xpc_object_t value);
typedef void (^xpc_handler_t)(xpc_object_t);
#endif // COMPILER_SUPPORTS(BLOCKS)

typedef void (*xpc_connection_handler_t)(xpc_connection_t connection);

#define XPC_ARRAY_APPEND ((size_t)(-1))
#define XPC_CONNECTION_MACH_SERVICE_LISTENER (1 << 0)
#define XPC_ERROR_CONNECTION_INTERRUPTED XPC_GLOBAL_OBJECT(_xpc_error_connection_interrupted)
#define XPC_ERROR_CONNECTION_INVALID XPC_GLOBAL_OBJECT(_xpc_error_connection_invalid)
#define XPC_ERROR_KEY_DESCRIPTION _xpc_error_key_description
#define XPC_ERROR_TERMINATION_IMMINENT XPC_GLOBAL_OBJECT(_xpc_error_termination_imminent)
#define XPC_TYPE_ARRAY (&_xpc_type_array)
#define XPC_TYPE_BOOL (&_xpc_type_bool)
#define XPC_TYPE_CONNECTION (&_xpc_type_connection)
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_TYPE_ENDPOINT (&_xpc_type_endpoint)
#define XPC_TYPE_ERROR (&_xpc_type_error)
#define XPC_TYPE_STRING (&_xpc_type_string)

extern const char * const _xpc_error_key_description;

extern "C" void xpc_connection_activate(xpc_connection_t connection);
extern "C" const void* xpc_dictionary_get_data(xpc_object_t xdict, const char* key, size_t* length);
extern "C" xpc_object_t xpc_data_create_with_dispatch_data(dispatch_data_t ddata);
extern "C" xpc_activity_state_t xpc_activity_get_state(xpc_activity_t activity);
extern "C" xpc_object_t xpc_activity_copy_criteria(xpc_activity_t activity);
extern "C" void xpc_activity_set_criteria(xpc_activity_t activity, xpc_object_t criteria);
#if COMPILER_SUPPORTS(BLOCKS)
typedef void (^xpc_activity_handler_t)(xpc_activity_t activity);
extern "C" void xpc_activity_register(const char *identifier, xpc_object_t criteria,
    xpc_activity_handler_t handler);
#endif // COMPILER_SUPPORTS(BLOCKS)

#endif // PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)

#if USE(APPLE_INTERNAL_SDK)
#include <os/transaction_private.h>
#include <xpc/private.h>
#if HAVE(OS_LAUNCHD_JOB)
#include <AppServerSupport/OSLaunchdJob.h>
#endif // HAVE(OS_LAUNCHD_JOB)
#else // USE(APPLE_INTERNAL_SDK)

#ifdef __OBJC__
#import <Foundation/NSError.h>
#if HAVE(OS_LAUNCHD_JOB)
@interface OSLaunchdJob : NSObject
- (instancetype)initWithPlist:(xpc_object_t)plist;
- (BOOL)submit:(NSError **)errorOut;
@end
#endif // HAVE(OS_LAUNCHD_JOB)
#endif // __OBJC__

extern "C" const char * const XPC_ACTIVITY_RANDOM_INITIAL_DELAY;
extern "C" const char * const XPC_ACTIVITY_REQUIRE_NETWORK_CONNECTIVITY;

#if HAVE(XPC_CONNECTION_COPY_INVALIDATION_REASON)
extern "C" char * xpc_connection_copy_invalidation_reason(xpc_connection_t connection);
#endif

#if OS_OBJECT_USE_OBJC
OS_OBJECT_DECL(os_transaction);
#else
typedef struct os_transaction_s *os_transaction_t;
#endif

enum {
    DISPATCH_MACH_SEND_POSSIBLE = 0x8,
};
#endif // USE(APPLE_INTERNAL_SDK)

#if !defined(XPC_NOESCAPE)
#define XPC_NOESCAPE
#endif

WTF_EXTERN_C_BEGIN

extern const struct _xpc_dictionary_s _xpc_error_connection_interrupted;
extern const struct _xpc_dictionary_s _xpc_error_connection_invalid;
extern const struct _xpc_dictionary_s _xpc_error_termination_imminent;

extern const struct _xpc_type_s _xpc_type_array;
extern const struct _xpc_type_s _xpc_type_bool;
extern const struct _xpc_type_s _xpc_type_connection;
extern const struct _xpc_type_s _xpc_type_dictionary;
extern const struct _xpc_type_s _xpc_type_endpoint;
extern const struct _xpc_type_s _xpc_type_error;
extern const struct _xpc_type_s _xpc_type_string;

xpc_object_t xpc_array_create(const xpc_object_t*, size_t count);
#if COMPILER_SUPPORTS(BLOCKS)
bool xpc_array_apply(xpc_object_t, XPC_NOESCAPE xpc_array_applier_t);
bool xpc_dictionary_apply(xpc_object_t xdict, XPC_NOESCAPE xpc_dictionary_applier_t applier);
#endif
size_t xpc_array_get_count(xpc_object_t);
const char* xpc_array_get_string(xpc_object_t, size_t index);
void xpc_array_set_string(xpc_object_t, size_t index, const char* string);
bool xpc_bool_get_value(xpc_object_t);
void xpc_connection_cancel(xpc_connection_t);
xpc_connection_t xpc_connection_create(const char* name, dispatch_queue_t);
xpc_endpoint_t xpc_endpoint_create(xpc_connection_t);
xpc_connection_t xpc_connection_create_from_endpoint(xpc_endpoint_t);
xpc_connection_t xpc_connection_create_mach_service(const char* name, dispatch_queue_t, uint64_t flags);
pid_t xpc_connection_get_pid(xpc_connection_t);
void xpc_connection_resume(xpc_connection_t);
void xpc_connection_suspend(xpc_connection_t);
void xpc_connection_send_message(xpc_connection_t, xpc_object_t);
void xpc_connection_send_message_with_reply(xpc_connection_t, xpc_object_t, dispatch_queue_t, xpc_handler_t);
void xpc_connection_set_event_handler(xpc_connection_t, xpc_handler_t);
void xpc_connection_set_target_queue(xpc_connection_t, dispatch_queue_t);
xpc_object_t xpc_dictionary_create(const char*  const* keys, const xpc_object_t*, size_t count);
xpc_object_t xpc_dictionary_create_reply(xpc_object_t);
int xpc_dictionary_dup_fd(xpc_object_t, const char* key);
xpc_connection_t xpc_dictionary_get_remote_connection(xpc_object_t);
bool xpc_dictionary_get_bool(xpc_object_t, const char* key);
const char* xpc_dictionary_get_string(xpc_object_t, const char* key);
uint64_t xpc_dictionary_get_uint64(xpc_object_t, const char* key);
xpc_object_t xpc_dictionary_get_value(xpc_object_t, const char* key);
void xpc_dictionary_set_bool(xpc_object_t, const char* key, bool value);
void xpc_dictionary_set_fd(xpc_object_t, const char* key, int fd);
void xpc_dictionary_set_string(xpc_object_t, const char* key, const char* string);
void xpc_dictionary_set_uint64(xpc_object_t, const char* key, uint64_t value);
void xpc_dictionary_set_data(xpc_object_t, const char *key, const void *bytes,
    size_t length);
void xpc_dictionary_set_value(xpc_object_t, const char* key, xpc_object_t value);
xpc_type_t xpc_get_type(xpc_object_t);
const char* xpc_type_get_name(xpc_type_t);
void xpc_main(xpc_connection_handler_t);
const char* xpc_string_get_string_ptr(xpc_object_t);
os_transaction_t os_transaction_create(const char *description);
void xpc_transaction_exit_clean(void);
void xpc_track_activity(void);

xpc_object_t xpc_connection_copy_entitlement_value(xpc_connection_t, const char* entitlement);
void xpc_connection_get_audit_token(xpc_connection_t, audit_token_t*);
void xpc_connection_kill(xpc_connection_t, int);
void xpc_connection_set_instance(xpc_connection_t, uuid_t);
mach_port_t xpc_dictionary_copy_mach_send(xpc_object_t, const char*);
void xpc_dictionary_set_mach_send(xpc_object_t, const char*, mach_port_t);

void xpc_connection_set_bootstrap(xpc_connection_t, xpc_object_t);
xpc_object_t xpc_copy_bootstrap();
void xpc_connection_set_oneshot_instance(xpc_connection_t, uuid_t instance);

void xpc_array_append_value(xpc_object_t xarray, xpc_object_t value);
xpc_object_t xpc_array_get_value(xpc_object_t xarray, size_t index);
xpc_object_t xpc_data_create(const void* bytes, size_t length);
const void * xpc_data_get_bytes_ptr(xpc_object_t xdata);
size_t xpc_data_get_length(xpc_object_t xdata);
xpc_object_t xpc_dictionary_get_array(xpc_object_t xdict, const char* key);


#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#if !defined(xpc_retain)
#define xpc_retain(object) ({ xpc_object_t _o = (object); _xpc_object_validate(_o); [_o retain]; })
#endif
#else
xpc_object_t xpc_retain(xpc_object_t);
#endif

#if OS_OBJECT_USE_OBJC_RETAIN_RELEASE
#if !defined(xpc_release)
#define xpc_release(object) ({ xpc_object_t _o = (object); _xpc_object_validate(_o); [_o release]; })
#endif
#else
void xpc_release(xpc_object_t);
#endif

WTF_EXTERN_C_END
