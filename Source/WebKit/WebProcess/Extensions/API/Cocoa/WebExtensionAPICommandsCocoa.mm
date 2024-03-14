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
#import "WebExtensionAPICommands.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionAPITabs.h"
#import "WebExtensionCommandParameters.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"

namespace WebKit {

static NSString * const nameKey = @"name";
static NSString * const descriptionKey = @"description";
static NSString * const shortcutKey = @"shortcut";
static NSString * const newShortcutKey = @"newShortcut";
static NSString * const oldShortcutKey = @"oldShortcut";

static inline NSDictionary *toAPI(const WebExtensionCommandParameters& command)
{
    return @{
        nameKey: (NSString *)command.identifier,
        descriptionKey: (NSString *)command.description,
        shortcutKey: (NSString *)command.shortcut
    };
}

static inline NSArray *toAPI(const Vector<WebExtensionCommandParameters>& commands)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:commands.size()];

    for (auto& command : commands)
        [result addObject:toAPI(command)];

    return [result copy];
}

void WebExtensionAPICommands::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands/getAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::CommandsGetAll(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Vector<WebExtensionCommandParameters> commands) {
        callback->call(toAPI(commands));
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPICommands::onCommand()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands/onCommand

    if (!m_onCommand)
        m_onCommand = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::CommandsOnCommand);

    return *m_onCommand;
}

WebExtensionAPIEvent& WebExtensionAPICommands::onChanged()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands/onChanged

    if (!m_onChanged)
        m_onChanged = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::CommandsOnChanged);

    return *m_onChanged;
}

void WebExtensionContextProxy::dispatchCommandsCommandEvent(const String& identifier, const std::optional<WebExtensionTabParameters>& tabParameters)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands/onCommand

    auto *tab = tabParameters ? toWebAPI(tabParameters.value()) : nil;

    enumerateFramesAndNamespaceObjects([&](auto& frame, auto& namespaceObject) {
        RefPtr coreFrame = frame.protectedCoreLocalFrame();
        WebCore::UserGestureIndicator gestureIndicator(WebCore::IsProcessingUserGesture::Yes, coreFrame ? coreFrame->document() : nullptr);
        namespaceObject.commands().onCommand().invokeListenersWithArgument((NSString *)identifier, tab);
    });
}

void WebExtensionContextProxy::dispatchCommandsChangedEvent(const String& identifier, const String& oldShortcut, const String& newShortcut)
{
    auto *changeInfo = @{
        nameKey: (NSString *)identifier,
        oldShortcutKey: (NSString *)oldShortcut,
        newShortcutKey: (NSString *)newShortcut
    };

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/commands/onChanged
        namespaceObject.commands().onChanged().invokeListenersWithArgument(changeInfo);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
