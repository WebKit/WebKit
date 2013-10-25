/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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

#if ENABLE(REMOTE_INSPECTOR)

#import "WebInspectorXPCWrapper.h"

#import <wtf/Assertions.h>

#if __has_include(<CoreFoundation/CFXPCBridge.h>)
#include <CoreFoundation/CFXPCBridge.h>
#else
extern xpc_object_t _CFXPCCreateXPCMessageWithCFObject(CFTypeRef);
extern CFTypeRef _CFXPCCreateCFObjectFromXPCMessage(xpc_object_t);
#endif

// Constants private to this file for message serialization on both ends.
#define WebInspectorXPCWrapperMessageNameKey @"messageName"
#define WebInspectorXPCWrapperUserInfoKey @"userInfo"
#define WebInspectorXPCWrapperSerializedMessageKey "msgData"

@interface WebInspectorXPCWrapper ()
- (void)_handleEvent:(xpc_object_t)object;
@end


@implementation WebInspectorXPCWrapper

@dynamic available;

@synthesize delegate = _delegate;
@synthesize connection = _connection;
@synthesize tag = _tag;

- (WebInspectorXPCWrapper *)initWithConnection:(xpc_connection_t)connection
{
    self = [super init];
    if (!self)
        return nil;

    _connection = xpc_retain(connection);
    xpc_connection_set_target_queue(_connection, dispatch_get_main_queue());
    xpc_connection_set_event_handler(_connection, ^(xpc_object_t object) { [self _handleEvent:object]; });
    xpc_connection_resume(_connection);

    return self;
}

- (void)close
{
    if (_connection) {
        xpc_connection_cancel(_connection);
        xpc_release(_connection);
        _connection = NULL;
    }
}

- (void)dealloc
{
    [self close];
    [_tag release];
    [super dealloc];
}

- (NSDictionary *)_deserializeMessage:(xpc_object_t)object
{
    ASSERT_WITH_MESSAGE(xpc_get_type(object) == XPC_TYPE_DICTIONARY, "Got unexpected object from an xpc connection");
    if (xpc_get_type(object) != XPC_TYPE_DICTIONARY)
        return nil;

    xpc_object_t xpcDictionary = xpc_dictionary_get_value(object, WebInspectorXPCWrapperSerializedMessageKey);
    if (!xpcDictionary || xpc_get_type(xpcDictionary) != XPC_TYPE_DICTIONARY) {
        // This isn't a message type that we know how to deserialize, so we let our delegate worry about it.
        if ([_delegate respondsToSelector:@selector(xpcConnection:unhandledMessage:)])
            [_delegate xpcConnection:self unhandledMessage:object];
        return nil;
    }

    NSDictionary *dictionary = _CFXPCCreateCFObjectFromXPCMessage(xpcDictionary);
    ASSERT_WITH_MESSAGE(dictionary, "Unable to deserialize xpc message");
    if (!dictionary)
        return nil;

    return [dictionary autorelease];
}

- (void)_handleEvent:(xpc_object_t)object
{
    if (!_connection)
        return;

    if (xpc_get_type(object) == XPC_TYPE_ERROR) {
        [_delegate xpcConnectionFailed:self];
        [self close];
        return;
    }

    if (!_delegate)
        return;

    NSDictionary *dataDictionary = [self _deserializeMessage:object];
    if (!dataDictionary)
        return;

    NSString *message = [dataDictionary objectForKey:WebInspectorXPCWrapperMessageNameKey];
    NSDictionary *userInfo = [dataDictionary objectForKey:WebInspectorXPCWrapperUserInfoKey];
    [_delegate xpcConnection:self receivedMessage:message userInfo:userInfo];
}

- (void)sendMessage:(NSString *)messageName userInfo:(NSDictionary *)userInfo
{
    if (!_connection)
        return;

    NSMutableDictionary *dictionary = [NSMutableDictionary dictionaryWithObject:messageName forKey:WebInspectorXPCWrapperMessageNameKey];
    if (userInfo)
        [dictionary setObject:userInfo forKey:WebInspectorXPCWrapperUserInfoKey];

    xpc_object_t xpcDictionary = _CFXPCCreateXPCMessageWithCFObject((CFDictionaryRef)dictionary);
    ASSERT_WITH_MESSAGE(xpcDictionary && xpc_get_type(xpcDictionary) == XPC_TYPE_DICTIONARY, "Unable to serialize xpc message");
    if (!xpcDictionary)
        return;

    xpc_object_t msg = xpc_dictionary_create(NULL, NULL, 0);
    xpc_dictionary_set_value(msg, WebInspectorXPCWrapperSerializedMessageKey, xpcDictionary);
    xpc_release(xpcDictionary);

    xpc_connection_send_message(_connection, msg);

    xpc_release(msg);
}

- (BOOL)available
{
    return _connection != NULL;
}

@end

#endif
