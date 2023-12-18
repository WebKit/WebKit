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

#import "config.h"
#import "WebExtensionCommand.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionCommandParameters.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextProxyMessages.h"

namespace WebKit {

WebExtensionCommand::WebExtensionCommand(WebExtensionContext& extensionContext, const WebExtension::CommandData& data)
    : m_extensionContext(extensionContext)
    , m_identifier(data.identifier)
    , m_description(data.description)
    , m_activationKey(data.activationKey)
    , m_modifierFlags(data.modifierFlags)
{
}

bool WebExtensionCommand::operator==(const WebExtensionCommand& other) const
{
    return this == &other || (m_extensionContext == other.m_extensionContext && m_identifier == other.m_identifier);
}

bool WebExtensionCommand::isActionCommand() const
{
    RefPtr context = extensionContext();
    if (!context)
        return false;

    if (context->extension().supportsManifestVersion(3))
        return identifier() == "_execute_action"_s;

    return identifier() == "_execute_browser_action"_s || identifier() == "_execute_page_action"_s;
}

WebExtensionCommandParameters WebExtensionCommand::parameters() const
{
    return {
        identifier(),
        description(),
        shortcutString()
    };
}

WebExtensionContext* WebExtensionCommand::extensionContext() const
{
    return m_extensionContext.get();
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
