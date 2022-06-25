/*
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2021 Metrological Group B.V.
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

#if WK_HAVE_C_SPI

#include <JavaScriptCore/ConsoleMessage.h>

namespace TestWebKitAPI {

TEST(Inspector, ConsoleMessageBasicMessage)
{
    Inspector::ConsoleMessage msg(JSC::MessageSource::JS, JSC::MessageType::Log, JSC::MessageLevel::Debug, "Basic error message"_s);
    EXPECT_STREQ("Basic error message", msg.message().utf8().data());
    EXPECT_STREQ("Basic error message", msg.toString().utf8().data());
}

TEST(Inspector, ConsoleMessageJSONValueBasicMessage)
{
    Inspector::ConsoleMessage msg(JSC::MessageSource::JS, JSC::MessageType::Log, JSC::MessageLevel::Debug, {
        {JSONLogValue::Type::String, "JSONValue basic error message"_s}
    }, nullptr, 0);
    EXPECT_STREQ("JSONValue basic error message", msg.message().utf8().data());
    EXPECT_STREQ("JSONValue basic error message", msg.toString().utf8().data());
}

TEST(Inspector, ConsoleMessageSeveralJSONValues)
{
    Inspector::ConsoleMessage msg(JSC::MessageSource::JS, JSC::MessageType::Log, JSC::MessageLevel::Debug, {
        {JSONLogValue::Type::String, "foo"_s},
        {JSONLogValue::Type::JSON, "{\"key\": \"value\"}"_s},
        {JSONLogValue::Type::String, "bar"_s},
    }, nullptr, 0);
    EXPECT_STREQ("foo", msg.message().utf8().data());
    EXPECT_STREQ("foo{\"key\": \"value\"}bar", msg.toString().utf8().data());
}

} // namespace TestWebKitAPI

#endif // WK_HAVE_C_SPI
