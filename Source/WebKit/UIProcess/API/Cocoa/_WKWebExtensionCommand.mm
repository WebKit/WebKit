/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "_WKWebExtensionCommandInternal.h"

#import "WebExtensionCommand.h"
#import "WebExtensionContext.h"

#if USE(APPKIT)
using CocoaModifierFlags = NSEventModifierFlags;
using CocoaMenuItem = NSMenuItem;
#else
using CocoaModifierFlags = UIKeyModifierFlags;
using CocoaMenuItem = UIMenuElement;
#endif

@implementation _WKWebExtensionCommand

#if ENABLE(WK_WEB_EXTENSIONS)

- (void)dealloc
{
    ASSERT(isMainRunLoop());

    _webExtensionCommand->~WebExtensionCommand();
}

- (NSUInteger)hash
{
    return self.identifier.hash;
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<_WKWebExtensionCommand>(object);
    if (!other)
        return NO;

    return *_webExtensionCommand == *other->_webExtensionCommand;
}

- (NSString *)description
{
    return self.discoverabilityTitle;
}

- (NSString *)debugDescription
{
    return [NSString stringWithFormat:@"<%@: %p; identifier = %@; shortcut = %@>", NSStringFromClass(self.class), self, self.identifier, self.activationKey.length ? (NSString *)self._webExtensionCommand.shortcutString() : @"(none)"];
}

- (_WKWebExtensionContext *)webExtensionContext
{
    if (auto *context = _webExtensionCommand->extensionContext())
        return context->wrapper();
    return nil;
}

- (NSString *)identifier
{
    return _webExtensionCommand->identifier();
}

- (NSString *)discoverabilityTitle
{
    return _webExtensionCommand->description();
}

- (NSString *)activationKey
{
    if (auto& activationKey = _webExtensionCommand->activationKey(); !activationKey.isEmpty())
        return activationKey;
    return nil;
}

- (void)setActivationKey:(NSString *)activationKey
{
    bool result = _webExtensionCommand->setActivationKey(activationKey);
    NSAssert(result, @"Invalid parameter: an unsupported character was provided");
}

- (CocoaModifierFlags)modifierFlags
{
    return _webExtensionCommand->modifierFlags().toRaw();
}

- (void)setModifierFlags:(CocoaModifierFlags)modifierFlags
{
    auto optionSet = OptionSet<WebKit::WebExtension::ModifierFlags>::fromRaw(modifierFlags) & WebKit::WebExtension::allModifierFlags();
    NSAssert(optionSet.toRaw() == modifierFlags, @"Invalid parameter: an unsupported modifier flag was provided");

    _webExtensionCommand->setModifierFlags(optionSet);
}

- (CocoaMenuItem *)menuItem
{
    return _webExtensionCommand->platformMenuItem();
}

#if PLATFORM(IOS_FAMILY)
- (UIKeyCommand *)keyCommand
{
    return _webExtensionCommand->keyCommand();
}
#endif

- (NSString *)_shortcut
{
    return _webExtensionCommand->shortcutString();
}

#if USE(APPKIT)
- (BOOL)_matchesEvent:(NSEvent *)event
{
    NSParameterAssert([event isKindOfClass:NSEvent.class]);

    return _webExtensionCommand->matchesEvent(event);
}
#endif

#pragma mark WKObject protocol implementation

- (API::Object&)_apiObject
{
    return *_webExtensionCommand;
}

- (WebKit::WebExtensionCommand&)_webExtensionCommand
{
    return *_webExtensionCommand;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (_WKWebExtensionContext *)webExtensionContext
{
    return nil;
}

- (NSString *)identifier
{
    return nil;
}

- (NSString *)discoverabilityTitle
{
    return nil;
}

- (NSString *)activationKey
{
    return nil;
}

- (void)setActivationKey:(NSString *)activationKey
{
}

- (CocoaModifierFlags)modifierFlags
{
    return 0;
}

- (void)setModifierFlags:(CocoaModifierFlags)modifierFlags
{
}

- (CocoaMenuItem *)menuItem
{
    return nil;
}

#if PLATFORM(IOS_FAMILY)
- (UIKeyCommand *)keyCommand
{
    return nil;
}
#endif

- (NSString *)_shortcut
{
    return nil;
}

#if USE(APPKIT)
- (BOOL)_matchesEvent:(NSEvent *)event
{
    return NO;
}
#endif

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
