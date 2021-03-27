/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "InspectorExtensionDelegate.h"

#if ENABLE(INSPECTOR_EXTENSIONS)

#import "WebInspectorUIProxy.h"
#import "_WKInspectorExtensionDelegate.h"
#import "_WKInspectorExtensionInternal.h"
#import <wtf/UniqueRef.h>

namespace WebKit {

InspectorExtensionDelegate::InspectorExtensionDelegate(_WKInspectorExtension *inspectorExtension, id <_WKInspectorExtensionDelegate> delegate)
    : m_inspectorExtension(inspectorExtension)
    , m_delegate(delegate)
{
    m_delegateMethods.inspectorExtensionDidShowTabWithIdentifier = [delegate respondsToSelector:@selector(inspectorExtension:didShowTabWithIdentifier:)];
    m_delegateMethods.inspectorExtensionDidHideTabWithIdentifier = [delegate respondsToSelector:@selector(inspectorExtension:didHideTabWithIdentifier:)];

    inspectorExtension->_extension->setClient(makeUniqueRef<InspectorExtensionClient>(*this));
}

InspectorExtensionDelegate::~InspectorExtensionDelegate() = default;

RetainPtr<id <_WKInspectorExtensionDelegate>> InspectorExtensionDelegate::delegate()
{
    return m_delegate.get();
}

InspectorExtensionDelegate::InspectorExtensionClient::InspectorExtensionClient(InspectorExtensionDelegate& delegate)
    : m_inspectorExtensionDelegate(delegate)
{
}

InspectorExtensionDelegate::InspectorExtensionClient::~InspectorExtensionClient()
{
}

void InspectorExtensionDelegate::InspectorExtensionClient::didShowExtensionTab(const Inspector::ExtensionTabID& extensionTabID)
{
    if (!m_inspectorExtensionDelegate.m_delegateMethods.inspectorExtensionDidShowTabWithIdentifier)
        return;

    auto& delegate = m_inspectorExtensionDelegate.m_delegate;
    if (!delegate)
        return;

    [delegate inspectorExtension:m_inspectorExtensionDelegate.m_inspectorExtension.get().get() didShowTabWithIdentifier:extensionTabID];
}

void InspectorExtensionDelegate::InspectorExtensionClient::didHideExtensionTab(const Inspector::ExtensionTabID& extensionTabID)
{
    if (!m_inspectorExtensionDelegate.m_delegateMethods.inspectorExtensionDidHideTabWithIdentifier)
        return;

    auto& delegate = m_inspectorExtensionDelegate.m_delegate;
    if (!delegate)
        return;

    [delegate inspectorExtension:m_inspectorExtensionDelegate.m_inspectorExtension.get().get() didHideTabWithIdentifier:extensionTabID];
}

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
