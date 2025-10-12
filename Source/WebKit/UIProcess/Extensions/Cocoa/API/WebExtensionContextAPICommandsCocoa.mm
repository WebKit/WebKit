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
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "WebExtensionCommand.h"
#import "WebExtensionCommandParameters.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionTab.h"
#import "WebExtensionUtilities.h"

namespace WebKit {

bool WebExtensionContext::isCommandsMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && protectedExtension()->hasCommands();
}

void WebExtensionContext::commandsGetAll(CompletionHandler<void(Vector<WebExtensionCommandParameters>)>&& completionHandler)
{
    auto results = WTF::map(commands(), [](auto& command) {
        return command->parameters();
    });

    completionHandler(WTFMove(results));
}

void WebExtensionContext::fireCommandEventIfNeeded(const WebExtensionCommand& command, WebExtensionTab* tab)
{
    constexpr auto type = WebExtensionEventListenerType::CommandsOnCommand;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }, command = Ref { command }, tab = RefPtr { tab }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchCommandsCommandEvent(command->identifier(), tab ? std::optional(tab->parameters()) : std::nullopt));
    });
}

void WebExtensionContext::fireCommandChangedEventIfNeeded(const WebExtensionCommand& command, const String& oldShortcut)
{
    constexpr auto type = WebExtensionEventListenerType::CommandsOnChanged;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, [=, this, protectedThis = Ref { *this }, command = Ref { command }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchCommandsChangedEvent(command->identifier(), oldShortcut, command->shortcutString()));
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
