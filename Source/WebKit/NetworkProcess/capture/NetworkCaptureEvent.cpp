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

#include "config.h"
#include "NetworkCaptureEvent.h"

#if ENABLE(NETWORK_CAPTURE)

#define JSON_NOEXCEPTION 1
#undef __EXCEPTIONS

#include "NetworkCaptureLogging.h"
#include "json.hpp"
#include <WebCore/ResourceError.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <wtf/Assertions.h>
#include <wtf/Brigand.h>
#include <wtf/URLParser.h>
#include <wtf/text/Base64.h>

namespace WebKit {
namespace NetworkCapture {

const char RequestSentEvent::typeName[] = "RequestSentEvent";
const char ResponseReceivedEvent::typeName[] = "ResponseReceivedEvent";
const char RedirectReceivedEvent::typeName[] = "RedirectReceivedEvent";
const char RedirectSentEvent::typeName[] = "RedirectSentEvent";
const char DataReceivedEvent::typeName[] = "DataReceivedEvent";
const char FinishedEvent::typeName[] = "FinishedEvent";

static Headers copyHeaders(const WebCore::HTTPHeaderMap& httpHeaders)
{
    Headers headers;
    for (const auto& header : httpHeaders)
        headers.append(std::make_pair(header.key, header.value));
    return headers;
}

// ----------

Request::Request(String&& url, String&& referrer, int policy, String&& method, Headers&& headers)
    : url(WTFMove(url))
    , referrer(WTFMove(referrer))
    , policy(WTFMove(policy))
    , method(WTFMove(method))
    , headers(WTFMove(headers))
{
}

Request::Request(const WebCore::ResourceRequest& request)
    : url(request.url().string())
    , referrer(request.httpReferrer())
    , policy(static_cast<int>(request.cachePolicy()))
    , method(request.httpMethod())
    , headers(copyHeaders(request.httpHeaderFields()))
{
}

Request::operator WebCore::ResourceRequest() const
{
    WebCore::ResourceRequest request(URL({ }, url), referrer, static_cast<WebCore::ResourceRequestCachePolicy>(policy));
    request.setHTTPMethod(method);

    for (const auto& header : headers)
        request.setHTTPHeaderField(header.first, header.second);

    return request;
}

// ----------

Response::Response(String&& url, String&& mimeType, long long expectedLength, String&& textEncodingName, String&& version, int status, String&& reason, Headers&& headers)
    : url(WTFMove(url))
    , mimeType(WTFMove(mimeType))
    , expectedLength(WTFMove(expectedLength))
    , textEncodingName(WTFMove(textEncodingName))
    , status(WTFMove(status))
    , reason(WTFMove(reason))
    , headers(WTFMove(headers))
{
}

Response::Response(const WebCore::ResourceResponse& response)
    : url(response.url().string())
    , mimeType(response.mimeType())
    , expectedLength(response.expectedContentLength())
    , textEncodingName(response.textEncodingName())
    , version(response.httpVersion())
    , status(response.httpStatusCode())
    , reason(response.httpStatusText())
    , headers(copyHeaders(response.httpHeaderFields()))
{
}

Response::operator WebCore::ResourceResponse() const
{
    WebCore::ResourceResponse response(URL({ }, url), mimeType, expectedLength, textEncodingName);
    response.setHTTPVersion(version);
    response.setHTTPStatusCode(status);
    response.setHTTPStatusText(reason);

    for (const auto& header : headers)
        response.setHTTPHeaderField(header.first, header.second);

    return response;
}

// ----------

Error::Error(String&& domain, String&& failingURL, String&& localizedDescription, int errorCode, int type)
    : domain(WTFMove(domain))
    , failingURL(WTFMove(failingURL))
    , localizedDescription(WTFMove(localizedDescription))
    , errorCode(WTFMove(errorCode))
    , type(WTFMove(type))
{
}

Error::Error(const WebCore::ResourceError& error)
    : domain(error.domain())
    , failingURL(error.failingURL().string())
    , localizedDescription(error.localizedDescription())
    , errorCode(error.errorCode())
    , type(static_cast<int>(error.type()))
{
}

Error::operator WebCore::ResourceError() const
{
    WebCore::ResourceError error(domain, errorCode, URL({ }, failingURL), localizedDescription, static_cast<WebCore::ResourceError::Type>(type));

    return error;
}

// ----------

// SEE THE NOTE IN json.hpp REGARDING ITS USE IN THIS PROJECT. IN SHORT, DO NOT
// USE json.hpp ANYWHERE ELSE. IT WILL BE GOING AWAY.
using json = nlohmann::basic_json<>;

template<typename Type>
struct JSONCoder {
    static json encode(Type val)
    {
        return json(val);
    }

    static Type decode(const json& jVal)
    {
        return jVal.get<Type>();
    }
};

template<>
struct JSONCoder<const char*> {
    static json encode(const char* val)
    {
        return json(val);
    }
};

template<>
struct JSONCoder<String> {
    static json encode(const String& val)
    {
        return json(std::string(static_cast<const char*>(val.utf8().data()), val.length()));
    }

    static String decode(const json& jVal)
    {
        return String(jVal.get_ref<const std::string&>().c_str());
    }
};

template<>
struct JSONCoder<CaptureTimeType> {
    static json encode(const CaptureTimeType& time)
    {
        return JSONCoder<double>::encode(time.secondsSinceEpoch().seconds());
    }

    static CaptureTimeType decode(const json& jTime)
    {
        return CaptureTimeType::fromRawSeconds(JSONCoder<double>::decode(jTime));
    }
};

template<>
struct JSONCoder<KeyValuePair> {
    static json encode(const KeyValuePair& pair)
    {
        return json {
            JSONCoder<String>::encode(pair.first),
            JSONCoder<String>::encode(pair.second)
        };
    }

    static KeyValuePair decode(const json& jPair)
    {
        return KeyValuePair {
            JSONCoder<String>::decode(jPair[0]),
            JSONCoder<String>::decode(jPair[1])
        };
    }
};

template<typename T>
struct JSONCoder<Vector<T>> {
    static json encode(const Vector<T>& vector)
    {
        json jVector;

        for (const auto& element : vector)
            jVector.push_back(JSONCoder<T>::encode(element));

        return jVector;
    }

    static Vector<T> decode(const json& jVector)
    {
        Vector<T> vector;

        for (const auto& element : jVector)
            vector.append(JSONCoder<T>::decode(element));

        return vector;
    }
};

template<>
struct JSONCoder<Request> {
    static json encode(const Request& request)
    {
        return json {
            { "url", JSONCoder<String>::encode(request.url) },
            { "referrer", JSONCoder<String>::encode(request.referrer) },
            { "policy", JSONCoder<int>::encode(request.policy) },
            { "method", JSONCoder<String>::encode(request.method) },
            { "headers", JSONCoder<Headers>::encode(request.headers) }
        };
    }

    static Request decode(const json& jRequest)
    {
        return Request {
            JSONCoder<String>::decode(jRequest["url"]),
            JSONCoder<String>::decode(jRequest["referrer"]),
            JSONCoder<int>::decode(jRequest["policy"]),
            JSONCoder<String>::decode(jRequest["method"]),
            JSONCoder<Headers>::decode(jRequest["headers"])
        };
    }
};

template<>
struct JSONCoder<Response> {
    static json encode(const Response& response)
    {
        return json {
            { "url", JSONCoder<String>::encode(response.url) },
            { "mimeType", JSONCoder<String>::encode(response.mimeType) },
            { "expectedLength", JSONCoder<long long>::encode(response.expectedLength) },
            { "textEncodingName", JSONCoder<String>::encode(response.textEncodingName) },
            { "version", JSONCoder<String>::encode(response.version) },
            { "status", JSONCoder<int>::encode(response.status) },
            { "reason", JSONCoder<String>::encode(response.reason) },
            { "headers", JSONCoder<Headers>::encode(response.headers) }
        };
    }

    static Response decode(const json& jResponse)
    {
        return Response {
            JSONCoder<String>::decode(jResponse["url"]),
            JSONCoder<String>::decode(jResponse["mimeType"]),
            JSONCoder<long long>::decode(jResponse["expectedLength"]),
            JSONCoder<String>::decode(jResponse["textEncodingName"]),
            JSONCoder<String>::decode(jResponse["version"]),
            JSONCoder<int>::decode(jResponse["status"]),
            JSONCoder<String>::decode(jResponse["reason"]),
            JSONCoder<Headers>::decode(jResponse["headers"])
        };
    }
};

template<>
struct JSONCoder<Error> {
    static json encode(const Error& error)
    {
        return json {
            { "domain", JSONCoder<String>::encode(error.domain) },
            { "failingURL", JSONCoder<String>::encode(error.failingURL) },
            { "localizedDescription", JSONCoder<String>::encode(error.localizedDescription) },
            { "errorCode", JSONCoder<int>::encode(error.errorCode) },
            { "type", JSONCoder<int>::encode(error.type) }
        };
    }

    static Error decode(const json& jError)
    {
        return Error {
            JSONCoder<String>::decode(jError["domain"]),
            JSONCoder<String>::decode(jError["failingURL"]),
            JSONCoder<String>::decode(jError["localizedDescription"]),
            JSONCoder<int>::decode(jError["errorCode"]),
            JSONCoder<int>::decode(jError["type"])
        };
    }
};

template<>
struct JSONCoder<WebCore::SharedBuffer> {
    static json encode(const WebCore::SharedBuffer& data)
    {
        Vector<char> buffer;
        base64Encode(data.data(), data.size(), buffer);
        return json(std::string(&buffer[0], buffer.size()));
    }

    static Ref<WebCore::SharedBuffer> decode(const json& jData)
    {
        Vector<char> data;
        const auto& str = jData.get_ref<const std::string&>();
        auto result = base64Decode(str.c_str(), str.size(), data);
        ASSERT_UNUSED(result, result);
        return WebCore::SharedBuffer::create(WTFMove(data));
    }
};

template<>
struct JSONCoder<RequestSentEvent> {
    static json encode(const RequestSentEvent& event)
    {
        return json {
            { "type", JSONCoder<const char*>::encode(event.typeName) },
            { "time", JSONCoder<CaptureTimeType>::encode(event.time) },
            { "request", JSONCoder<Request>::encode(event.request) }
        };
    }

    static RequestSentEvent decode(const json& jEvent)
    {
        return RequestSentEvent {
            JSONCoder<CaptureTimeType>::decode(jEvent["time"]),
            JSONCoder<Request>::decode(jEvent["request"])
        };
    }
};

template<>
struct JSONCoder<ResponseReceivedEvent> {
    static json encode(const ResponseReceivedEvent& event)
    {
        return json {
            { "type", JSONCoder<const char*>::encode(event.typeName) },
            { "time", JSONCoder<CaptureTimeType>::encode(event.time) },
            { "response", JSONCoder<Response>::encode(event.response) }
        };
    }

    static ResponseReceivedEvent decode(const json& jEvent)
    {
        return ResponseReceivedEvent {
            JSONCoder<CaptureTimeType>::decode(jEvent["time"]),
            JSONCoder<Response>::decode(jEvent["response"])
        };
    }
};

template<>
struct JSONCoder<RedirectReceivedEvent> {
    static json encode(const RedirectReceivedEvent& event)
    {
        return json {
            { "type", JSONCoder<const char*>::encode(event.typeName) },
            { "time", JSONCoder<CaptureTimeType>::encode(event.time) },
            { "request", JSONCoder<Request>::encode(event.request) },
            { "response", JSONCoder<Response>::encode(event.response) }
        };
    }

    static RedirectReceivedEvent decode(const json& jEvent)
    {
        return RedirectReceivedEvent {
            JSONCoder<CaptureTimeType>::decode(jEvent["time"]),
            JSONCoder<Request>::decode(jEvent["request"]),
            JSONCoder<Response>::decode(jEvent["response"])
        };
    }
};

template<>
struct JSONCoder<RedirectSentEvent> {
    static json encode(const RedirectSentEvent& event)
    {
        return json {
            { "type", JSONCoder<const char*>::encode(event.typeName) },
            { "time", JSONCoder<CaptureTimeType>::encode(event.time) },
            { "request", JSONCoder<Request>::encode(event.request) },
        };
    }

    static RedirectSentEvent decode(const json& jEvent)
    {
        return RedirectSentEvent {
            JSONCoder<CaptureTimeType>::decode(jEvent["time"]),
            JSONCoder<Request>::decode(jEvent["request"])
        };
    }
};

template<>
struct JSONCoder<DataReceivedEvent> {
    static json encode(const DataReceivedEvent& event)
    {
        return json {
            { "type", JSONCoder<const char*>::encode(event.typeName) },
            { "time", JSONCoder<CaptureTimeType>::encode(event.time) },
            { "data", JSONCoder<WebCore::SharedBuffer>::encode(event.data.get()) }
        };
    }

    static DataReceivedEvent decode(const json& jEvent)
    {
        return DataReceivedEvent {
            JSONCoder<CaptureTimeType>::decode(jEvent["time"]),
            JSONCoder<WebCore::SharedBuffer>::decode(jEvent["data"])
        };
    }
};

template<>
struct JSONCoder<FinishedEvent> {
    static json encode(const FinishedEvent& event)
    {
        return json {
            { "type", JSONCoder<const char*>::encode(event.typeName) },
            { "time", JSONCoder<CaptureTimeType>::encode(event.time) },
            { "error", JSONCoder<Error>::encode(event.error) }
        };
    }

    static FinishedEvent decode(const json& jEvent)
    {
        return FinishedEvent {
            JSONCoder<CaptureTimeType>::decode(jEvent["time"]),
            JSONCoder<Error>::decode(jEvent["error"])
        };
    }
};

std::string eventToString(const CaptureEvent& event)
{
    json result;

    WTF::visit([&result](const auto& event) {
        using EventType = std::decay_t<decltype(event)>;
        result = JSONCoder<EventType>::encode(event);
    }, event);

    return result.dump(4);
}

OptionalCaptureEvent stringToEvent(const char* jsonStr)
{
    auto jValue = json::parse(jsonStr);
    const auto& type = jValue["type"].get_ref<const std::string&>();

    OptionalCaptureEvent result { std::nullopt };
    brigand::for_each<CaptureEvent>([&](auto T) {
        using Type = typename decltype(T)::type;
        if (!result && type == Type::typeName)
            result = OptionalCaptureEvent(JSONCoder<Type>::decode(jValue));
    });
    return result;
}

} // namespace NetworkCapture
} // namespace WebKit

#undef JSON_NOEXCEPTIONS

#endif // ENABLE(NETWORK_CAPTURE)
