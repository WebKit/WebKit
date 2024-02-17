/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <stdio.h>
#include <wtf/FilePrintStream.h>
#include <wtf/HashMap.h>
#include <wtf/Lock.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/CString.h>

namespace JSC {

#define JSC_PROFILER_SUPPORT_CATEGORY(macro) \
    macro(JSGlobalObjectSignpost) \

class ProfilerSupport {
    WTF_MAKE_TZONE_ALLOCATED(ProfilerSupport);
    WTF_MAKE_NONCOPYABLE(ProfilerSupport);
    friend class LazyNeverDestroyed<ProfilerSupport>;
public:
    enum class Category : uint8_t {
#define JSC_DEFINE_CATEGORY(name) name,
        JSC_PROFILER_SUPPORT_CATEGORY(JSC_DEFINE_CATEGORY)
#undef JSC_DEFINE_CATEGORY
    };

#define JSC_COUNT_CATEGORY(name) + 1
    static constexpr unsigned numberOfCategories = 0 JSC_PROFILER_SUPPORT_CATEGORY(JSC_COUNT_CATEGORY);
#undef JSC_COUNT_CATEGORY

    static void markStart(const void*, Category, const CString&);
    static void markEnd(const void*, Category, const CString&);
    static void mark(const void*, Category, const CString&);

    static uint64_t generateTimestamp();

private:
    ProfilerSupport();
    static ProfilerSupport& singleton();

    std::unique_ptr<FilePrintStream> m_file;
    std::array<HashMap<CString, uint64_t>, numberOfCategories> m_markers;
    Lock m_lock;
};

} // namespace JSC
