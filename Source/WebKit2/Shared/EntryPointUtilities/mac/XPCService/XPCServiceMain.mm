/*
 * Copyright (C) 2013, 2015, 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import <CoreFoundation/CoreFoundation.h>
#import <wtf/OSObjectPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/spi/darwin/XPCSPI.h>

namespace WebKit {

static void XPCServiceEventHandler(xpc_connection_t peer)
{
    xpc_connection_set_target_queue(peer, dispatch_get_main_queue());
    xpc_connection_set_event_handler(peer, ^(xpc_object_t event) {
        xpc_type_t type = xpc_get_type(event);
        if (type == XPC_TYPE_ERROR) {
            if (event == XPC_ERROR_CONNECTION_INVALID || event == XPC_ERROR_TERMINATION_IMMINENT) {
                // FIXME: Handle this case more gracefully.
                exit(EXIT_FAILURE);
            }
        } else {
            assert(type == XPC_TYPE_DICTIONARY);

            if (!strcmp(xpc_dictionary_get_string(event, "message-name"), "bootstrap")) {
                CFBundleRef webKitBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebKit"));
                CFStringRef entryPointFunctionName = (CFStringRef)CFBundleGetValueForInfoDictionaryKey(CFBundleGetMainBundle(), CFSTR("WebKitEntryPoint"));

                typedef void (*InitializerFunction)(xpc_connection_t, xpc_object_t);
                InitializerFunction initializerFunctionPtr = reinterpret_cast<InitializerFunction>(CFBundleGetFunctionPointerForName(webKitBundle, entryPointFunctionName));
                if (!initializerFunctionPtr) {
                    NSLog(@"Unable to find entry point in WebKit.framework with name: %@", (NSString *)entryPointFunctionName);
                    exit(EXIT_FAILURE);
                }

                auto reply = adoptOSObject(xpc_dictionary_create_reply(event));
                xpc_dictionary_set_string(reply.get(), "message-name", "process-finished-launching");
                xpc_connection_send_message(xpc_dictionary_get_remote_connection(event), reply.get());

                int fd = xpc_dictionary_dup_fd(event, "stdout");
                if (fd != -1)
                    dup2(fd, STDOUT_FILENO);

                fd = xpc_dictionary_dup_fd(event, "stderr");
                if (fd != -1)
                    dup2(fd, STDERR_FILENO);

                initializerFunctionPtr(peer, event);
            }

            // Leak a boost onto the NetworkProcess.
            if (!strcmp(xpc_dictionary_get_string(event, "message-name"), "pre-bootstrap"))
                xpc_retain(event);
        }
    });

    xpc_connection_resume(peer);
}

} // namespace WebKit

using namespace WebKit;

int main(int argc, char** argv)
{
    auto bootstrap = adoptOSObject(xpc_copy_bootstrap());
#if PLATFORM(IOS)
    auto containerEnvironmentVariables = xpc_dictionary_get_value(bootstrap.get(), "ContainerEnvironmentVariables");
    xpc_dictionary_apply(containerEnvironmentVariables, ^(const char *key, xpc_object_t value) {
        setenv(key, xpc_string_get_string_ptr(value), 1);
        return true;
    });
#endif

    if (bootstrap) {
        if (xpc_object_t languages = xpc_dictionary_get_value(bootstrap.get(), "OverrideLanguages")) {
            @autoreleasepool {
                NSDictionary *existingArguments = [[NSUserDefaults standardUserDefaults] volatileDomainForName:NSArgumentDomain];
                NSMutableDictionary *newArguments = [existingArguments mutableCopy];
                RetainPtr<NSMutableArray *> newLanguages = adoptNS([[NSMutableArray alloc] init]);
                xpc_array_apply(languages, ^(size_t index, xpc_object_t value) {
                    [newLanguages addObject:[NSString stringWithCString:xpc_string_get_string_ptr(value) encoding:NSUTF8StringEncoding]];
                    return true;
                });
                [newArguments setValue:newLanguages.get() forKey:@"AppleLanguages"];
                [[NSUserDefaults standardUserDefaults] setVolatileDomain:newArguments forName:NSArgumentDomain];
            }
        }
    }

    xpc_main(XPCServiceEventHandler);
    return 0;
}
