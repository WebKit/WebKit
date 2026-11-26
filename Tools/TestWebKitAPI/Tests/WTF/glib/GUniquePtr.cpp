/*
 * Copyright (C) 2014 Igalia S.L.
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

#include "config.h"
#include <wtf/glib/GUniquePtr.h>

#include <dlfcn.h>
#include <gio/gio.h>

static thread_local bool loggingEnabled;

inline std::ostringstream& log()
{
    static std::ostringstream log;
    return log;
}

inline std::string takeLogStr()
{
    std::string string = log().str();
    log().str("");
    return string;
}

class FreeLogging {
public:
    FreeLogging()
    {
        takeLogStr();
        loggingEnabled = true;
    }

    ~FreeLogging()
    {
        loggingEnabled = false;
    }
};

#define DEFINE_LOGGING_FREE_FUNCTION(type, function) \
    extern "C" void (function)(type ptr) \
    { \
        static auto* realFunction = reinterpret_cast<void (*)(type)>(dlsym(RTLD_NEXT, #function)); \
        if (loggingEnabled) \
            log() << #function "(" << static_cast<const void*>(ptr) << ");"; \
        realFunction(ptr); \
    }

DEFINE_LOGGING_FREE_FUNCTION(gpointer, g_free)
DEFINE_LOGGING_FREE_FUNCTION(GError*, g_error_free)
DEFINE_LOGGING_FREE_FUNCTION(GList*, g_list_free)
DEFINE_LOGGING_FREE_FUNCTION(GSList*, g_slist_free)
DEFINE_LOGGING_FREE_FUNCTION(GPatternSpec*, g_pattern_spec_free)
DEFINE_LOGGING_FREE_FUNCTION(GDir*, g_dir_close)
DEFINE_LOGGING_FREE_FUNCTION(GTimer*, g_timer_destroy)
DEFINE_LOGGING_FREE_FUNCTION(GKeyFile*, g_key_file_free)

namespace TestWebKitAPI {

TEST(WTF_GUniquePtr, Basic)
{
    FreeLogging logging;
    std::ostringstream actual;

    {
        GUniquePtr<char> a(g_strdup("a"));
        actual << "g_free(" << static_cast<const void*>(a.get()) << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GError> a(g_error_new_literal(G_IO_ERROR, G_IO_ERROR_NOT_FOUND, "a"));
        actual << "g_error_free(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GList> a(g_list_prepend(nullptr, g_strdup("a")));
        actual << "g_list_free(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GSList> a(g_slist_prepend(nullptr, g_strdup("a")));
        actual << "g_slist_free(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GPatternSpec> a(g_pattern_spec_new("a"));
        actual << "g_pattern_spec_free(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GDir> a(g_dir_open("/tmp", 0, nullptr));
        actual << "g_dir_close(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GTimer> a(g_timer_new());
        actual << "g_timer_destroy(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniquePtr<GKeyFile> a(g_key_file_new());
        actual << "g_key_file_free(" << a.get() << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");
}

static void returnOutChar(char** outChar)
{
    *outChar = g_strdup("a");
}

TEST(WTF_GUniquePtr, OutPtr)
{
    FreeLogging logging;
    std::ostringstream actual;

    {
        GUniqueOutPtr<char> a;
        ASSERT_EQ(nullptr, a.get());
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniqueOutPtr<char> a;
        returnOutChar(&a.outPtr());
        actual << "g_free(" << static_cast<const void*>(a.get()) << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniqueOutPtr<char> a;
        returnOutChar(&a.outPtr());
        actual << "g_free(" << static_cast<const void*>(a.get()) << ");";
        returnOutChar(&a.outPtr());
        ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
        actual.str("");
        actual << "g_free(" << static_cast<const void*>(a.get()) << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");

    {
        GUniqueOutPtr<char> a;
        returnOutChar(&a.outPtr());
        GUniquePtr<char> b(a.release());
        ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
        actual << "g_free(" << static_cast<const void*>(b.get()) << ");";
    }
    ASSERT_STREQ(actual.str().c_str(), takeLogStr().c_str());
    actual.str("");
}

} // namespace TestWebKitAPI
