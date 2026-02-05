/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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
#import "WKWebExtensionCommandInternal.h"

#import "WebExtensionCommand.h"
#import "WebExtensionContext.h"

#if USE(APPKIT)
using CocoaModifierFlags = NSEventModifierFlags;
using CocoaMenuItem = NSMenuItem;
#else
using CocoaModifierFlags = UIKeyModifierFlags;
using CocoaMenuItem = UIMenuElement;
#endif

@implementation WKWebExtensionCommand

#if ENABLE(WK_WEB_EXTENSIONS)

WK_OBJECT_DEALLOC_IMPL_ON_MAIN_THREAD(WKWebExtensionCommand, WebExtensionCommand, _webExtensionCommand);

- (NSUInteger)hash
{
    return self.identifier.hash;
}

- (BOOL)isEqual:(id)object
{
    if (self == object)
        return YES;

    auto *other = dynamic_objc_cast<WKWebExtensionCommand>(object);
    if (!other)
        return NO;

    return *_webExtensionCommand == *other->_webExtensionCommand;
}

- (NSString *)description
{
    return self.title;
}

- (NSString *)debugDescription
{
    return [NSString stringWithFormat:@"<%@: %p; identifier = %@; shortcut = %@>", NSStringFromClass(self.class), self,
        self.identifier, self.activationKey.length ? self._protectedWebExtensionCommand->shortcutString().createNSString().get() : @"(none)"];
}

- (WKWebExtensionContext *)webExtensionContext
{
    if (RefPtr context = self._protectedWebExtensionCommand->extensionContext())
        return context->wrapper();
    return nil;
}

- (NSString *)identifier
{
    return _webExtensionCommand->identifier().createNSString().autorelease();
}

- (NSString *)title
{
    return _webExtensionCommand->description().createNSString().autorelease();
}

- (NSString *)activationKey
{
    if (auto& activationKey = self._protectedWebExtensionCommand->activationKey(); !activationKey.isEmpty())
        return activationKey.createNSString().autorelease();
    return nil;
}

- (void)setActivationKey:(NSString *)activationKey
{
    bool result = self._protectedWebExtensionCommand->setActivationKey(activationKey);
    NSAssert(result, @"Invalid parameter: an unsupported character was provided");
}

- (CocoaModifierFlags)modifierFlags
{
    return self._protectedWebExtensionCommand->modifierFlags().toRaw();
}

- (void)setModifierFlags:(CocoaModifierFlags)modifierFlags
{
    auto optionSet = OptionSet<WebKit::WebExtension::ModifierFlags>::fromRaw(modifierFlags) & WebKit::WebExtension::allModifierFlags();
    NSAssert(optionSet.toRaw() == modifierFlags, @"Invalid parameter: an unsupported modifier flag was provided");

    self._protectedWebExtensionCommand->setModifierFlags(optionSet);
}

- (CocoaMenuItem *)menuItem
{
    return self._protectedWebExtensionCommand->platformMenuItem();
}

#if PLATFORM(IOS_FAMILY)
- (UIKeyCommand *)keyCommand
{
    return protect(*_webExtensionCommand)->keyCommand();
}
#endif

- (NSString *)_shortcut
{
    return self._protectedWebExtensionCommand->shortcutString().createNSString().autorelease();
}

- (NSString *)_userVisibleShortcut
{
    return self._protectedWebExtensionCommand->userVisibleShortcut().createNSString().autorelease();
}

- (BOOL)_isActionCommand
{
    return self._protectedWebExtensionCommand->isActionCommand();
}

#if USE(APPKIT)
- (BOOL)_matchesEvent:(NSEvent *)event
{
    NSParameterAssert([event isKindOfClass:NSEvent.class]);

    return self._protectedWebExtensionCommand->matchesEvent(event);
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

- (Ref<WebKit::WebExtensionCommand>)_protectedWebExtensionCommand
{
    return *_webExtensionCommand;
}

#else // ENABLE(WK_WEB_EXTENSIONS)

- (WKWebExtensionContext *)webExtensionContext
{
    return nil;
}

- (NSString *)identifier
{
    return nil;
}

- (NSString *)title
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

- (NSString *)_userVisibleShortcut
{
    return nil;
}

- (BOOL)_isActionCommand
{
    return NO;
}

#if USE(APPKIT)
- (BOOL)_matchesEvent:(NSEvent *)event
{
    return NO;
}
#endif

#endif // ENABLE(WK_WEB_EXTENSIONS)

@end
