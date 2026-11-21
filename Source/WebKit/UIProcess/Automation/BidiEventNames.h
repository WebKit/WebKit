/*
 * Copyright (C) 2025 Igalia S.L.
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

#include <array>
#include <wtf/text/ASCIILiteral.h>

namespace WebKit {
namespace BidiEventNames {

// These constants describe the protocol-recognized event surface. The
// presence of an event here does not imply that WebKit can emit it yet.
namespace BrowsingContext {
static constexpr auto ContextCreated = "browsingContext.contextCreated"_s;
static constexpr auto ContextDestroyed = "browsingContext.contextDestroyed"_s;
static constexpr auto DomContentLoaded = "browsingContext.domContentLoaded"_s;
static constexpr auto DownloadEnd = "browsingContext.downloadEnd"_s;
static constexpr auto DownloadWillBegin = "browsingContext.downloadWillBegin"_s;
static constexpr auto FragmentNavigated = "browsingContext.fragmentNavigated"_s;
static constexpr auto HistoryUpdated = "browsingContext.historyUpdated"_s;
static constexpr auto Load = "browsingContext.load"_s;
static constexpr auto NavigationAborted = "browsingContext.navigationAborted"_s;
static constexpr auto NavigationCommitted = "browsingContext.navigationCommitted"_s;
static constexpr auto NavigationFailed = "browsingContext.navigationFailed"_s;
static constexpr auto NavigationStarted = "browsingContext.navigationStarted"_s;
static constexpr auto UserPromptClosed = "browsingContext.userPromptClosed"_s;
static constexpr auto UserPromptOpened = "browsingContext.userPromptOpened"_s;

struct Module {
    static constexpr auto s_name = "browsingContext"_s;
    static constexpr auto s_eventNames = std::array {
        ContextCreated,
        ContextDestroyed,
        DomContentLoaded,
        DownloadEnd,
        DownloadWillBegin,
        FragmentNavigated,
        HistoryUpdated,
        Load,
        NavigationAborted,
        NavigationCommitted,
        NavigationFailed,
        NavigationStarted,
        UserPromptClosed,
        UserPromptOpened,
    };
};
} // namespace BrowsingContext

namespace Input {
static constexpr auto FileDialogOpened = "input.fileDialogOpened"_s;

struct Module {
    static constexpr auto s_name = "input"_s;
    static constexpr auto s_eventNames = std::array {
        FileDialogOpened,
    };
};
} // namespace Input

namespace Log {
static constexpr auto EntryAdded = "log.entryAdded"_s;

struct Module {
    static constexpr auto s_name = "log"_s;
    static constexpr auto s_eventNames = std::array {
        EntryAdded,
    };
};
} // namespace Log

namespace Network {
static constexpr auto AuthRequired = "network.authRequired"_s;
static constexpr auto BeforeRequestSent = "network.beforeRequestSent"_s;
static constexpr auto FetchError = "network.fetchError"_s;
static constexpr auto ResponseCompleted = "network.responseCompleted"_s;
static constexpr auto ResponseStarted = "network.responseStarted"_s;

struct Module {
    static constexpr auto s_name = "network"_s;
    static constexpr auto s_eventNames = std::array {
        AuthRequired,
        BeforeRequestSent,
        FetchError,
        ResponseCompleted,
        ResponseStarted,
    };
};
} // namespace Network

namespace Script {
static constexpr auto Message = "script.message"_s;
static constexpr auto RealmCreated = "script.realmCreated"_s;
static constexpr auto RealmDestroyed = "script.realmDestroyed"_s;

struct Module {
    static constexpr auto s_name = "script"_s;
    static constexpr auto s_eventNames = std::array {
        Message,
        RealmCreated,
        RealmDestroyed,
    };
};
} // namespace Script

} // namespace BidiEventNames

} // namespace WebKit
