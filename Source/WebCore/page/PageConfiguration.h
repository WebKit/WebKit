/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef PageConfiguration_h
#define PageConfiguration_h

#include <wtf/Noncopyable.h>
#include <wtf/RefPtr.h>

#if defined(__has_include) && __has_include(<WebKitAdditions/PageConfigurationIncludes.h>)
#include <WebKitAdditions/PageConfigurationIncludes.h>
#endif

namespace WebCore {

class AlternativeTextClient;
class ApplicationCacheStorage;
class BackForwardClient;
class ChromeClient;
class DatabaseProvider;
class DiagnosticLoggingClient;
class DragClient;
class EditorClient;
class FrameLoaderClient;
class InspectorClient;
class PlugInClient;
class ProgressTrackerClient;
class StorageNamespaceProvider;
class UserContentController;
class ValidationMessageClient;
class VisitedLinkStore;

#if ENABLE(CONTEXT_MENUS)
class ContextMenuClient;
#endif

class PageConfiguration {
    WTF_MAKE_NONCOPYABLE(PageConfiguration); WTF_MAKE_FAST_ALLOCATED;
public:
    WEBCORE_EXPORT PageConfiguration();
    WEBCORE_EXPORT ~PageConfiguration();

    AlternativeTextClient* alternativeTextClient { nullptr };
    ChromeClient* chromeClient { nullptr };
#if ENABLE(CONTEXT_MENUS)
    ContextMenuClient* contextMenuClient { nullptr };
#endif
    EditorClient* editorClient { nullptr };
    DragClient* dragClient { nullptr };
    InspectorClient* inspectorClient { nullptr };
    PlugInClient* plugInClient { nullptr };
    ProgressTrackerClient* progressTrackerClient { nullptr };
    RefPtr<BackForwardClient> backForwardClient;
    ValidationMessageClient* validationMessageClient { nullptr };
    FrameLoaderClient* loaderClientForMainFrame { nullptr };
    DiagnosticLoggingClient* diagnosticLoggingClient { nullptr };

#if defined(__has_include) && __has_include(<WebKitAdditions/PageConfigurationMembers.h>)
#include <WebKitAdditions/PageConfigurationMembers.h>
#endif

    RefPtr<ApplicationCacheStorage> applicationCacheStorage;
    RefPtr<DatabaseProvider> databaseProvider;
    RefPtr<StorageNamespaceProvider> storageNamespaceProvider;
    RefPtr<UserContentController> userContentController;
    RefPtr<VisitedLinkStore> visitedLinkStore;
};

}

#endif
