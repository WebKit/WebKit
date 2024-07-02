/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
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

#if USE(CURL)

#include <WebCore/CurlMultipartHandle.h>
#include <WebCore/CurlMultipartHandleClient.h>
#include <WebCore/CurlResponse.h>
#include <WebCore/SharedBuffer.h>
#include <wtf/text/MakeString.h>

namespace TestWebKitAPI {

namespace Curl {

using namespace WebCore;

static std::span<const uint8_t> span(const ASCIILiteral& data)
{
    return data.span8();
}

static CurlResponse createCurlResponse(std::optional<String> contentType = "multipart/x-mixed-replace"_s, std::optional<String> boundary = "boundary"_s)
{
    CurlResponse response;

    response.headers.append("x-dummy-pre-header: dummy\r\n"_s);

    if (contentType && boundary)
        response.headers.append(makeString("Content-type: "_s, *contentType, "; boundary=\""_s, *boundary, '"', "\r\n"_s));
    else if (contentType)
        response.headers.append(makeString("Content-type: "_s, *contentType, ";\r\n"_s));

    response.headers.append("x-dummy-post-header: dummy\r\n"_s);

    return response;
}

class MultipartHandleClient final : public CurlMultipartHandleClient, public CanMakeCheckedPtr<MultipartHandleClient> {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MultipartHandleClient);
public:
    void setMultipartHandle();

    void didReceiveHeaderFromMultipart(Vector<String>&& headers) final
    {
        for (auto header : headers)
            m_headers.append(WTFMove(header));
    }

    void didReceiveDataFromMultipart(std::span<const uint8_t> receivedData) final
    {
        m_data.append(receivedData);
    }

    void didCompleteFromMultipart() final
    {
        m_didComplete = true;
    }

    void clear()
    {
        m_headers.clear();
        m_data.clear();
        m_didComplete = false;
    }

    const Vector<String>& headers() { return m_headers; }
    const Vector<uint8_t>& data() { return m_data; }
    bool complete() { return m_didComplete; }

private:
    // CheckedPtr interface
    uint32_t ptrCount() const final { return CanMakeCheckedPtr::ptrCount(); }
    uint32_t ptrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::ptrCountWithoutThreadCheck(); }
    void incrementPtrCount() const final { CanMakeCheckedPtr::incrementPtrCount(); }
    void decrementPtrCount() const final { CanMakeCheckedPtr::decrementPtrCount(); }

    Vector<String> m_headers;
    Vector<uint8_t> m_data;
    bool m_didComplete { false };
};

class CurlMultipartHandleTests : public testing::Test {
public:
    CurlMultipartHandleTests() { }
};

TEST(CurlMultipartHandleTests, CreateCurlMultipartHandle)
{
    MultipartHandleClient client;

    // Content-Type header is missing
    auto curlResponse = createCurlResponse(std::nullopt, std::nullopt);
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);
    EXPECT_FALSE(handle);

    // Not multipart/x-mixed-replace
    curlResponse = createCurlResponse("text/html"_s, std::nullopt);
    handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);
    EXPECT_FALSE(handle);

    curlResponse = createCurlResponse("multipart/mixed"_s, "boundary"_s);
    handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);
    EXPECT_FALSE(handle);

    // boundary is not set for multipart/x-mixed-replace
    curlResponse = createCurlResponse("multipart/x-mixed-replace"_s, std::nullopt);
    handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);
    EXPECT_FALSE(handle);

    // Normal case
    curlResponse = createCurlResponse();
    handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);
    EXPECT_TRUE(handle);
}

TEST(CurlMultipartHandleTests, SimpleMessage)
{
    auto data =
        " This is the preamble.--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n This is the epilogue."_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, NoHeader)
{
    auto data =
        "--boundary\r\n\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 0);

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, NoBody)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "\r\n--boundary  \r\nContent-type: text/html\r\n\r\n"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_EQ(client.data().size(), 0);
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_EQ(client.data().size(), 0);
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, TransportPadding)
{
    auto data =
        " This is the preamble.--boundary     \r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary  \r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n This is the epilogue."_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, NoEndOfBoundary)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    // Retain "Initial CRLF + (boundary - 1)" bytes.
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<h", 2));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, NoEndOfBoundaryAfterCompleted)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);

    handle->didCompleteMessage();
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, NoCloseDelimiter)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    // Retain "Initial CRLF + (boundary - 1)" bytes
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<h", 2));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, NoCloseDelimiterAfterCompleted)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);

    handle->didCompleteMessage();
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, CloseDelimiter)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, CloseDelimiterAfterCompleted)
{
    auto data =
        "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);

    handle->didCompleteMessage();
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideFirstDelimiter)
{
    auto data = "--bound"_s;

    auto nextData = "ary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 0);

    handle->didReceiveMessage(span(nextData));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideSecondDelimiter)
{
    auto data = "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--b"_s;

    auto nextData = "oundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_EQ(client.headers().size(), 0);

    handle->didReceiveMessage(span(nextData));
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideLastDelimiter)
{
    auto data = "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundar"_s;

    auto nextData = "y--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    // Retain "Initial CRLF + (boundary - 1)" bytes
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<h", 2));
    EXPECT_TRUE(!client.complete());

    handle->didReceiveMessage(span(nextData));
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideCloseDelimiter)
{
    auto data = "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary"_s;

    auto nextData = "--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    // Retain "Initial CRLF + (boundary - 1)" bytes
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<h", 2));
    EXPECT_TRUE(!client.complete());

    handle->didReceiveMessage(span(nextData));
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideTransportPadding)
{
    auto data = "--boundary  \r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary      "_s;

    auto nextData =
        "  \r\nContent-type: text/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--        \r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_EQ(client.headers().size(), 0);
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));

    handle->didReceiveMessage(span(nextData));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideHeader)
{
    auto data = "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABCDEF"
        "\r\n--boundary\r\nContent-type: t"_s;

    auto nextData = "ext/html\r\n\r\n"
        "<html></html>"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 0);

    handle->didReceiveMessage(span(nextData));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, DivideBody)
{
    auto data = "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABC"_s;

    auto secondData = "DEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<h"_s;

    auto lastData = "tml></html>"
        "\r\n--boundary--\r\n"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_EQ(client.data().size(), 0);

    handle->didReceiveMessage(span(secondData));
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_EQ(client.data().size(), 0);

    handle->didReceiveMessage(span(lastData));
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(!client.complete());

    handle->didCompleteMessage();
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, CompleteWhileHeaderProcessing)
{
    auto data = "--boundary\r\nContent-type: text/plain\r\n\r\n"
        "ABC"_s;

    auto secondData = "DEF"
        "\r\n--boundary\r\nContent-type: text/html\r\n\r\n"
        "<h"_s;

    auto lastData = "tml></html>"_s;

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(span(data));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/plain\r\n"_s);
    EXPECT_EQ(client.data().size(), 0);
    client.clear();

    handle->didReceiveMessage(span(secondData));
    EXPECT_EQ(client.headers().size(), 0);
    EXPECT_EQ(client.data().size(), 0);

    handle->didReceiveMessage(span(lastData));
    EXPECT_EQ(client.headers().size(), 0);
    EXPECT_EQ(client.data().size(), 0);

    handle->didCompleteMessage();
    EXPECT_EQ(client.headers().size(), 0);
    EXPECT_EQ(client.data().size(), 0);
    EXPECT_TRUE(!client.complete());

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "ABCDEF", 6));
    EXPECT_EQ(client.headers().size(), 1);
    EXPECT_TRUE(client.headers().at(0) == "Content-type: text/html\r\n"_s);
    EXPECT_TRUE(!client.complete());
    client.clear();

    handle->completeHeaderProcessing();
    EXPECT_TRUE(!memcmp(static_cast<const void*>(client.data().data()), "<html></html>", 13));
    EXPECT_TRUE(client.complete());
}

TEST(CurlMultipartHandleTests, MaxHeaderSize)
{
    Vector<uint8_t> data;
    data.append("--boundary\r\n"_span);

    for (auto i = 0; i < 300 * 1024 - 4; i++)
        data.append('a');

    data.append("\r\n\r\n"_span);
    data.append("\r\n--boundary\r\n"_span);

    for (auto i = 0; i < 300 * 1024 - 3; i++)
        data.append('a');

    data.append("\r\n\r\n"_span);

    MultipartHandleClient client;

    auto curlResponse = createCurlResponse();
    auto handle = CurlMultipartHandle::createIfNeeded(client, curlResponse);

    handle->didReceiveMessage(data.span());
    handle->didCompleteMessage();
    EXPECT_FALSE(handle->hasError());

    handle->completeHeaderProcessing();
    EXPECT_TRUE(handle->hasError());
}

}

}

#endif // USE(CURL)
