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

#pragma once

#if ENABLE(INSPECTOR_EXTENSIONS)

#import "APIInspectorExtensionClient.h"
#import "WKFoundation.h"
#import <wtf/WeakObjCPtr.h>

@class _WKInspectorExtension;
@protocol _WKInspectorExtensionDelegate;

namespace WebKit {

class InspectorExtensionDelegate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorExtensionDelegate(_WKInspectorExtension *, id <_WKInspectorExtensionDelegate>);
    ~InspectorExtensionDelegate();

    RetainPtr<id <_WKInspectorExtensionDelegate>> delegate();
    void setDelegate(id <_WKInspectorExtensionDelegate>);

private:
    class InspectorExtensionClient final : public API::InspectorExtensionClient {
        WTF_MAKE_FAST_ALLOCATED;
    public:
        explicit InspectorExtensionClient(InspectorExtensionDelegate&);
        ~InspectorExtensionClient();

    private:
        // API::InspectorExtensionClient
        void didShowExtensionTab(const Inspector::ExtensionTabID&) override;
        void didHideExtensionTab(const Inspector::ExtensionTabID&) override;
        void inspectedPageDidNavigate(const URL&) override;

        InspectorExtensionDelegate& m_inspectorExtensionDelegate;
    };

    WeakObjCPtr<_WKInspectorExtension> m_inspectorExtension;
    WeakObjCPtr<id <_WKInspectorExtensionDelegate>> m_delegate;

    struct {
        bool inspectorExtensionDidShowTabWithIdentifier : 1;
        bool inspectorExtensionDidHideTabWithIdentifier : 1;
        bool inspectorExtensionInspectedPageDidNavigate : 1;
    } m_delegateMethods;
};

} // namespace WebKit

#endif // ENABLE(INSPECTOR_EXTENSIONS)
