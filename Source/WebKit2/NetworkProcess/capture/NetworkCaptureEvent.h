/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if ENABLE(NETWORK_CAPTURE)

#include <WebCore/SharedBuffer.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Optional.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {
namespace NetworkCapture {

struct RequestSentEvent;
struct ResponseReceivedEvent;
struct RedirectReceivedEvent;
struct RedirectSentEvent;
struct DataReceivedEvent;
struct FinishedEvent;

using CaptureEvent = WTF::Variant<RequestSentEvent, ResponseReceivedEvent, RedirectReceivedEvent, RedirectSentEvent, DataReceivedEvent, FinishedEvent>;
using OptionalCaptureEvent = std::optional<CaptureEvent>;
using CaptureEvents = Vector<CaptureEvent>;
using CaptureClockType = WTF::MonotonicTime;
using CaptureTimeType = WTF::MonotonicTime;
using KeyValuePair = std::pair<String, String>;
using Headers = Vector<KeyValuePair>;

std::string eventToString(const CaptureEvent&);
OptionalCaptureEvent stringToEvent(const char*);

struct Request {
    // See comment for RequestSentEvent for why we need this default constructor.
    Request()
        : url()
        , referrer()
        , policy(0)
        , method()
        , headers() { }
    Request(String&& url, String&& referrer, int policy, String&& method, Headers&&);
    Request(const WebCore::ResourceRequest&);
    operator WebCore::ResourceRequest() const;

    String url;
    String referrer;
    int policy;
    String method;
    Headers headers;
};
static_assert(std::is_default_constructible<Request>::value, "Request is not default constructible");
static_assert(std::is_move_constructible<Request>::value, "Request is not move constructible");
static_assert(std::is_move_assignable<Request>::value, "Request is not move assignable");

struct Response {
    Response(String&& url, String&& mimeType, long long expectedLength, String&& textEncodingName, String&& version, int status, String&& reason, Headers&&);
    Response(const WebCore::ResourceResponse&);
    operator WebCore::ResourceResponse() const;

    String url;
    String mimeType;
    long long expectedLength;
    String textEncodingName;
    String version;
    int status;
    String reason;
    Headers headers;
};
static_assert(std::is_move_constructible<Response>::value, "Response is not move constructible");
static_assert(std::is_move_assignable<Response>::value, "Response is not move assignable");

struct Error {
    Error(String&& domain, String&& failingURL, String&& localizedDescription, int errorCode, int type);
    Error(const WebCore::ResourceError&);
    operator WebCore::ResourceError() const;

    String domain;
    String failingURL;
    String localizedDescription;
    int errorCode;
    int type;
};
static_assert(std::is_move_constructible<Error>::value, "Error is not move constructible");
static_assert(std::is_move_assignable<Error>::value, "Error is not move assignable");

struct TimedEvent {
    TimedEvent()
        : time(CaptureClockType::now()) { }
    TimedEvent(CaptureTimeType&& time)
        : time(WTFMove(time)) { }

    CaptureTimeType time;
};
static_assert(std::is_move_constructible<TimedEvent>::value, "TimedEvent is not move constructible");
static_assert(std::is_move_assignable<TimedEvent>::value, "TimedEvent is not move assignable");

struct RequestSentEvent final : public TimedEvent {
    // This default constructor is needed only because this struct is the first
    // type passed to CaptureEvent, which is a WTF::Variant. This means that if
    // CaptureEvent is default-constructed, it needs to default-construct an
    // instance of RequestSentEvent, the first type on its list. If we don't
    // have a default constructor for this type, the CaptureEvent will be
    // created in an invalid state and will crash when destructed.
    RequestSentEvent()
        : TimedEvent()
        , request() { }
    RequestSentEvent(const WebCore::ResourceRequest& request)
        : TimedEvent()
        , request(request) { }
    RequestSentEvent(CaptureTimeType&& time, Request&& request)
        : TimedEvent(WTFMove(time))
        , request(WTFMove(request)) { }

    Request request;
    static const char typeName[];
};
static_assert(std::is_default_constructible<RequestSentEvent>::value, "RequestSentEvent is not default constructible");
static_assert(std::is_move_constructible<RequestSentEvent>::value, "RequestSentEvent is not move constructible");
static_assert(std::is_move_assignable<RequestSentEvent>::value, "RequestSentEvent is not move assignable");

struct ResponseReceivedEvent final : public TimedEvent {
    ResponseReceivedEvent(const WebCore::ResourceResponse& response)
        : TimedEvent()
        , response(response) { }
    ResponseReceivedEvent(CaptureTimeType&& time, Response&& response)
        : TimedEvent(WTFMove(time))
        , response(WTFMove(response)) { }

    Response response;
    static const char typeName[];
};
static_assert(std::is_move_constructible<ResponseReceivedEvent>::value, "ResponseReceivedEvent is not move constructible");
static_assert(std::is_move_assignable<ResponseReceivedEvent>::value, "ResponseReceivedEvent is not move assignable");

struct RedirectReceivedEvent final : public TimedEvent {
    RedirectReceivedEvent(const WebCore::ResourceRequest& request, const WebCore::ResourceResponse& response)
        : TimedEvent()
        , request(request)
        , response(response) { }
    RedirectReceivedEvent(CaptureTimeType&& time, Request&& request, Response&& response)
        : TimedEvent(WTFMove(time))
        , request(WTFMove(request))
        , response(WTFMove(response)) { }

    Request request;
    Response response;
    static const char typeName[];
};
static_assert(std::is_move_constructible<RedirectReceivedEvent>::value, "RedirectReceivedEvent is not move constructible");
static_assert(std::is_move_assignable<RedirectReceivedEvent>::value, "RedirectReceivedEvent is not move assignable");

struct RedirectSentEvent final : public TimedEvent {
    RedirectSentEvent(const WebCore::ResourceRequest& request)
        : TimedEvent()
        , request(request) { }
    RedirectSentEvent(CaptureTimeType&& time, Request&& request)
        : TimedEvent(WTFMove(time))
        , request(WTFMove(request)) { }

    Request request;
    static const char typeName[];
};
static_assert(std::is_move_constructible<RedirectSentEvent>::value, "RedirectSentEvent is not move constructible");
static_assert(std::is_move_assignable<RedirectSentEvent>::value, "RedirectSentEvent is not move assignable");

struct DataReceivedEvent final : public TimedEvent {
    DataReceivedEvent()
        : TimedEvent()
        , data(WebCore::SharedBuffer::create()) { }
    DataReceivedEvent(WebCore::SharedBuffer& data)
        : TimedEvent()
        , data(Ref<WebCore::SharedBuffer>(data)) { }
    DataReceivedEvent(CaptureTimeType&& time, WebCore::SharedBuffer& data)
        : TimedEvent(WTFMove(time))
        , data(Ref<WebCore::SharedBuffer>(data)) { }

    Ref<WebCore::SharedBuffer> data;
    static const char typeName[];
};
static_assert(std::is_move_constructible<DataReceivedEvent>::value, "DataReceivedEvent is not move constructible");
static_assert(std::is_move_assignable<DataReceivedEvent>::value, "DataReceivedEvent is not move assignable");

struct FinishedEvent final : public TimedEvent {
    FinishedEvent(const WebCore::ResourceError& error)
        : TimedEvent()
        , error(error) { }
    FinishedEvent(CaptureTimeType&& time, Error&& error)
        : TimedEvent(WTFMove(time))
        , error(WTFMove(error)) { }

    Error error;
    static const char typeName[];
};
static_assert(std::is_move_constructible<FinishedEvent>::value, "FinishedEvent is not move constructible");
static_assert(std::is_move_assignable<FinishedEvent>::value, "FinishedEvent is not move assignable");

} // namespace NetworkCapture
} // namespace WebKit

#endif // ENABLE(NETWORK_CAPTURE)
