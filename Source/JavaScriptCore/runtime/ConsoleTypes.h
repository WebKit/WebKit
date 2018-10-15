/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/Forward.h>

namespace JSC {

enum class MessageSource : uint8_t {
    XML,
    JS,
    Network,
    ConsoleAPI,
    Storage,
    AppCache,
    Rendering,
    CSS,
    Security,
    ContentBlocker,
    Other,
    Media,
    WebRTC,
};

enum class MessageType {
    Log,
    Dir,
    DirXML,
    Table,
    Trace,
    StartGroup,
    StartGroupCollapsed,
    EndGroup,
    Clear,
    Assert,
    Timing,
    Profile,
    ProfileEnd,
};

enum class MessageLevel : uint8_t {
    Log = 1,
    Warning = 2,
    Error = 3,
    Debug = 4,
    Info = 5,
};

} // namespace JSC

namespace WTF {

template<> struct EnumTraits<JSC::MessageSource> {
    using values = EnumValues<
        JSC::MessageSource,
        JSC::MessageSource::XML,
        JSC::MessageSource::JS,
        JSC::MessageSource::Network,
        JSC::MessageSource::ConsoleAPI,
        JSC::MessageSource::Storage,
        JSC::MessageSource::AppCache,
        JSC::MessageSource::Rendering,
        JSC::MessageSource::CSS,
        JSC::MessageSource::Security,
        JSC::MessageSource::ContentBlocker,
        JSC::MessageSource::Other,
        JSC::MessageSource::Media,
        JSC::MessageSource::WebRTC
    >;
};

template<> struct EnumTraits<JSC::MessageLevel> {
    using values = EnumValues<
        JSC::MessageLevel,
        JSC::MessageLevel::Log,
        JSC::MessageLevel::Warning,
        JSC::MessageLevel::Error,
        JSC::MessageLevel::Debug,
        JSC::MessageLevel::Info
    >;
};

} // namespace WTF

using JSC::MessageSource;
using JSC::MessageType;
using JSC::MessageLevel;
