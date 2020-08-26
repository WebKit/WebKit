/*
 * Copyright (C) 2016 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FetchBodyConsumer.h"

#include "HTTPHeaderField.h"
#include "JSBlob.h"
#include "ReadableStreamChunk.h"
#include "TextResourceDecoder.h"
#include <wtf/StringExtras.h>
#include <wtf/URLParser.h>

namespace WebCore {

static inline Ref<Blob> blobFromData(ScriptExecutionContext* context, const unsigned char* data, unsigned length, const String& contentType)
{
    Vector<uint8_t> value(length);
    memcpy(value.data(), data, length);
    return Blob::create(context, WTFMove(value), Blob::normalizedContentType(contentType));
}

// https://mimesniff.spec.whatwg.org/#http-quoted-string-token-code-point
static bool isHTTPQuotedStringTokenCodePoint(UChar c)
{
    return c == 0x09
        || (c >= 0x20 && c <= 0x7E)
        || (c >= 0x80 && c <= 0xFF);
}

struct MimeType {
    String type;
    String subtype;
    HashMap<String, String> parameters;
};

static HashMap<String, String> parseParameters(StringView input, size_t position)
{
    HashMap<String, String> parameters;
    while (position < input.length()) {
        while (position < input.length() && RFC7230::isWhitespace(input[position]))
            position++;
        size_t nameBegin = position;
        while (position < input.length() && input[position] != '=' && input[position] != ';')
            position++;
        if (position >= input.length())
            break;
        if (input[position] == ';') {
            position++;
            continue;
        }
        StringView parameterName = input.substring(nameBegin, position - nameBegin);
        position++;
        if (position >= input.length())
            break;

        StringView parameterValue;
        if (position < input.length() && input[position] == '"') {
            position++;
            size_t valueBegin = position;
            while (position < input.length() && input[position] != '"')
                position++;
            parameterValue = input.substring(valueBegin, position - valueBegin);
            position++;
        } else {
            size_t valueBegin = position;
            while (position < input.length() && input[position] != ';')
                position++;
            parameterValue = stripLeadingAndTrailingHTTPSpaces(input.substring(valueBegin, position - valueBegin));
        }

        if (parameterName.length()
            && isValidHTTPToken(parameterName)
            && parameterValue.isAllSpecialCharacters<isHTTPQuotedStringTokenCodePoint>()) {
            String nameString = parameterName.toString();
            if (!parameters.contains(nameString))
                parameters.set(nameString, parameterValue.toString());
        }
    }
    return parameters;
}

// https://mimesniff.spec.whatwg.org/#parsing-a-mime-type
static Optional<MimeType> parseMIMEType(const String& contentType)
{
    String input = stripLeadingAndTrailingHTTPSpaces(contentType);
    size_t slashIndex = input.find('/');
    if (slashIndex == notFound)
        return WTF::nullopt;

    String type = input.substring(0, slashIndex);
    if (!type.length() || !isValidHTTPToken(type))
        return WTF::nullopt;
    
    size_t semicolonIndex = input.find(';', slashIndex);
    String subtype = stripLeadingAndTrailingHTTPSpaces(input.substring(slashIndex + 1, semicolonIndex - slashIndex - 1));
    if (!subtype.length() || !isValidHTTPToken(subtype))
        return WTF::nullopt;

    return {{ WTFMove(type), WTFMove(subtype), parseParameters(StringView(input), semicolonIndex + 1) }};
}

// https://fetch.spec.whatwg.org/#concept-body-package-data
static RefPtr<DOMFormData> packageFormData(ScriptExecutionContext* context, const String& contentType, const char* data, size_t length)
{
    auto parseMultipartPart = [context] (const char* part, size_t partLength, DOMFormData& form) -> bool {
        const char* headerEnd = static_cast<const char*>(memmem(part, partLength, "\r\n\r\n", 4));
        if (!headerEnd)
            return false;
        const char* headerBegin = part;
        size_t headerLength = headerEnd - headerBegin;

        const char* bodyBegin = headerEnd + strlen("\r\n\r\n");
        size_t bodyLength = partLength - (bodyBegin - headerBegin);

        String header = String::fromUTF8(headerBegin, headerLength);

        const char* contentDispositionCharacters = "Content-Disposition:";
        size_t contentDispositionBegin = header.find(contentDispositionCharacters);
        if (contentDispositionBegin == notFound)
            return false;
        size_t contentDispositionEnd = header.find("\r\n", contentDispositionBegin);
        size_t contentDispositionParametersBegin = header.find(';', contentDispositionBegin + strlen(contentDispositionCharacters));
        if (contentDispositionParametersBegin != notFound)
            contentDispositionParametersBegin++;

        auto parameters = parseParameters(StringView(header).substring(contentDispositionParametersBegin, contentDispositionEnd - contentDispositionParametersBegin), 0);
        String name = parameters.get("name");
        if (!name)
            return false;
        String filename = parameters.get("filename");
        if (!filename)
            form.append(name, String::fromUTF8(bodyBegin, bodyLength));
        else {
            String contentType = "text/plain"_s;

            const char* contentTypeCharacters = "Content-Type:";
            size_t contentTypePrefixLength = strlen(contentTypeCharacters);
            size_t contentTypeBegin = header.find(contentTypeCharacters);
            if (contentTypeBegin != notFound) {
                size_t contentTypeEnd = header.find("\r\n", contentTypeBegin);
                contentType = stripLeadingAndTrailingHTTPSpaces(header.substring(contentTypeBegin + contentTypePrefixLength, contentTypeEnd - contentTypeBegin - contentTypePrefixLength));
            }

            form.append(name, File::create(context, Blob::create(context, SharedBuffer::create(bodyBegin, bodyLength).get(), Blob::normalizedContentType(contentType)).get(), filename).get(), filename);
        }
        return true;
    };
    
    auto parseMultipartBoundary = [] (const Optional<MimeType>& mimeType) -> Optional<String> {
        if (!mimeType)
            return WTF::nullopt;
        if (equalIgnoringASCIICase(mimeType->type, "multipart") && equalIgnoringASCIICase(mimeType->subtype, "form-data")) {
            auto iterator = mimeType->parameters.find("boundary"_s);
            if (iterator != mimeType->parameters.end())
                return iterator->value;
        }
        return WTF::nullopt;
    };

    auto form = DOMFormData::create(UTF8Encoding());
    auto mimeType = parseMIMEType(contentType);
    if (auto multipartBoundary = parseMultipartBoundary(mimeType)) {
        String boundaryWithDashes = makeString("--", *multipartBoundary);
        CString boundary = boundaryWithDashes.utf8();
        size_t boundaryLength = boundary.length();

        const char* currentBoundary = static_cast<const char*>(memmem(data, length, boundary.data(), boundaryLength));
        if (!currentBoundary)
            return nullptr;
        const char* nextBoundary = static_cast<const char*>(memmem(currentBoundary + boundaryLength, length - (currentBoundary + boundaryLength - data), boundary.data(), boundaryLength));
        if (!nextBoundary)
            return nullptr;
        while (nextBoundary) {
            parseMultipartPart(currentBoundary + boundaryLength, nextBoundary - currentBoundary - boundaryLength - strlen("\r\n"), form.get());
            currentBoundary = nextBoundary;
            nextBoundary = static_cast<const char*>(memmem(nextBoundary + boundaryLength, length - (nextBoundary + boundaryLength - data), boundary.data(), boundaryLength));
        }
    } else if (mimeType && equalIgnoringASCIICase(mimeType->type, "application") && equalIgnoringASCIICase(mimeType->subtype, "x-www-form-urlencoded")) {
        auto dataString = String::fromUTF8(data, length);
        for (auto& pair : WTF::URLParser::parseURLEncodedForm(dataString))
            form->append(pair.key, pair.value);
    } else
        return nullptr;

    return form;
}

static void resolveWithTypeAndData(Ref<DeferredPromise>&& promise, FetchBodyConsumer::Type type, const String& contentType, const unsigned char* data, unsigned length)
{
    auto* context = promise->scriptExecutionContext();

    switch (type) {
    case FetchBodyConsumer::Type::ArrayBuffer:
        fulfillPromiseWithArrayBuffer(WTFMove(promise), data, length);
        return;
    case FetchBodyConsumer::Type::Blob:
        promise->resolveCallbackValueWithNewlyCreated<IDLInterface<Blob>>([&data, &length, &contentType, context](auto&) {
            return blobFromData(context, data, length, contentType);
        });
        return;
    case FetchBodyConsumer::Type::JSON:
        fulfillPromiseWithJSON(WTFMove(promise), TextResourceDecoder::textFromUTF8(data, length));
        return;
    case FetchBodyConsumer::Type::Text:
        promise->resolve<IDLDOMString>(TextResourceDecoder::textFromUTF8(data, length));
        return;
    case FetchBodyConsumer::Type::FormData:
        if (auto formData = packageFormData(context, contentType, reinterpret_cast<const char*>(data), length))
            promise->resolve<IDLInterface<DOMFormData>>(*formData);
        else
            promise->reject(TypeError);
        return;
    case FetchBodyConsumer::Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FetchBodyConsumer::clean()
{
    m_buffer = nullptr;
    resetConsumePromise();
    if (m_sink) {
        m_sink->clearCallback();
        return;
    }
}

void FetchBodyConsumer::resolveWithData(Ref<DeferredPromise>&& promise, const String& contentType, const unsigned char* data, unsigned length)
{
    resolveWithTypeAndData(WTFMove(promise), m_type, contentType, data, length);
}

void FetchBodyConsumer::extract(ReadableStream& stream, ReadableStreamToSharedBufferSink::Callback&& callback)
{
    ASSERT(!m_sink);
    m_sink = ReadableStreamToSharedBufferSink::create(WTFMove(callback));
    m_sink->pipeFrom(stream);
}

void FetchBodyConsumer::resolve(Ref<DeferredPromise>&& promise, const String& contentType, ReadableStream* stream)
{
    if (stream) {
        ASSERT(!m_sink);
        m_sink = ReadableStreamToSharedBufferSink::create([promise = WTFMove(promise), data = SharedBuffer::create(), type = m_type, contentType](auto&& result) mutable {
            if (result.hasException()) {
                promise->reject(result.releaseException());
                return;
            }

            if (auto chunk = result.returnValue())
                data->append(reinterpret_cast<const char*>(chunk->data), chunk->size);
            else
                resolveWithTypeAndData(WTFMove(promise), type, contentType, reinterpret_cast<const unsigned char*>(data->data()), data->size());
        });
        m_sink->pipeFrom(*stream);
        return;
    }

    if (m_isLoading) {
        setConsumePromise(WTFMove(promise));
        return;
    }

    auto* context = promise->scriptExecutionContext();

    ASSERT(m_type != Type::None);
    switch (m_type) {
    case Type::ArrayBuffer:
        fulfillPromiseWithArrayBuffer(WTFMove(promise), takeAsArrayBuffer().get());
        return;
    case Type::Blob:
        promise->resolveCallbackValueWithNewlyCreated<IDLInterface<Blob>>([this, context](auto&) {
            return takeAsBlob(context);
        });
        return;
    case Type::JSON:
        fulfillPromiseWithJSON(WTFMove(promise), takeAsText());
        return;
    case Type::Text:
        promise->resolve<IDLDOMString>(takeAsText());
        return;
    case FetchBodyConsumer::Type::FormData: {
        auto buffer = takeData();
        if (auto formData = packageFormData(context, contentType, buffer ? buffer->data() : nullptr, buffer ? buffer->size() : 0))
            promise->resolve<IDLInterface<DOMFormData>>(*formData);
        else
            promise->reject(TypeError);
        return;
    }
    case Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FetchBodyConsumer::append(const char* data, unsigned size)
{
    if (m_source) {
        m_source->enqueue(ArrayBuffer::tryCreate(data, size));
        return;
    }
    if (!m_buffer) {
        m_buffer = SharedBuffer::create(data, size);
        return;
    }
    m_buffer->append(data, size);
}

void FetchBodyConsumer::append(const unsigned char* data, unsigned size)
{
    append(reinterpret_cast<const char*>(data), size);
}

RefPtr<SharedBuffer> FetchBodyConsumer::takeData()
{
    return WTFMove(m_buffer);
}

RefPtr<JSC::ArrayBuffer> FetchBodyConsumer::takeAsArrayBuffer()
{
    if (!m_buffer)
        return ArrayBuffer::tryCreate(nullptr, 0);

    auto arrayBuffer = m_buffer->tryCreateArrayBuffer();
    m_buffer = nullptr;
    return arrayBuffer;
}

Ref<Blob> FetchBodyConsumer::takeAsBlob(ScriptExecutionContext* context)
{
    if (!m_buffer)
        return Blob::create(context, Vector<uint8_t>(), Blob::normalizedContentType(m_contentType));

    // FIXME: We should try to move m_buffer to Blob without doing extra copy.
    return blobFromData(context, reinterpret_cast<const unsigned char*>(m_buffer->data()), m_buffer->size(), m_contentType);
}

String FetchBodyConsumer::takeAsText()
{
    // FIXME: We could probably text decode on the fly as soon as m_type is set to JSON or Text.
    if (!m_buffer)
        return String();

    auto text = TextResourceDecoder::textFromUTF8(reinterpret_cast<const unsigned char*>(m_buffer->data()), m_buffer->size());
    m_buffer = nullptr;
    return text;
}

void FetchBodyConsumer::setConsumePromise(Ref<DeferredPromise>&& promise)
{
    ASSERT(!m_consumePromise);
    m_userGestureToken = UserGestureIndicator::currentUserGesture();
    m_consumePromise = WTFMove(promise);
}

void FetchBodyConsumer::resetConsumePromise()
{
    m_consumePromise = nullptr;
    m_userGestureToken = nullptr;
}

void FetchBodyConsumer::setSource(Ref<FetchBodySource>&& source)
{
    m_source = WTFMove(source);
    if (m_buffer) {
        m_source->enqueue(m_buffer->tryCreateArrayBuffer());
        m_buffer = nullptr;
    }
}

void FetchBodyConsumer::loadingFailed(const Exception& exception)
{
    m_isLoading = false;
    if (m_consumePromise) {
        m_consumePromise->reject(exception);
        resetConsumePromise();
    }
    if (m_source) {
        m_source->error(exception);
        m_source = nullptr;
    }
}

void FetchBodyConsumer::loadingSucceeded(const String& contentType)
{
    m_isLoading = false;

    if (m_consumePromise) {
        if (!m_userGestureToken || m_userGestureToken->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwardingForFetch()) || !m_userGestureToken->processingUserGesture())
            resolve(m_consumePromise.releaseNonNull(), contentType, nullptr);
        else {
            UserGestureIndicator gestureIndicator(m_userGestureToken, UserGestureToken::GestureScope::MediaOnly, UserGestureToken::IsPropagatedFromFetch::Yes);
            resolve(m_consumePromise.releaseNonNull(), contentType, nullptr);
        }
    }
    if (m_source) {
        m_source->close();
        m_source = nullptr;
    }
}

} // namespace WebCore
