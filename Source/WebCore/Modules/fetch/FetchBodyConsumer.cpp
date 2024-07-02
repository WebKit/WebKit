/*
 * Copyright (C) 2016-2024 Apple Inc.
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

#include "ContextDestructionObserverInlines.h"
#include "DOMFormData.h"
#include "FetchBodyOwner.h"
#include "FormData.h"
#include "FormDataConsumer.h"
#include "HTTPHeaderField.h"
#include "HTTPParsers.h"
#include "JSBlob.h"
#include "JSDOMFormData.h"
#include "JSDOMPromiseDeferred.h"
#include "TextResourceDecoder.h"
#include <wtf/StringExtras.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

static inline Ref<Blob> blobFromData(ScriptExecutionContext* context, Vector<uint8_t>&& data, const String& contentType)
{
    return Blob::create(context, WTFMove(data), Blob::normalizedContentType(contentType));
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
        while (position < input.length() && isTabOrSpace(input[position]))
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
            parameterValue = input.substring(valueBegin, position - valueBegin).trim(isASCIIWhitespaceWithoutFF<UChar>);
        }

        if (parameterName.length()
            && isValidHTTPToken(parameterName)
            && parameterValue.containsOnly<isHTTPQuotedStringTokenCodePoint>()) {
            parameters.ensure(parameterName.toString(), [&] { return parameterValue.toString(); });
        }
    }
    return parameters;
}

// https://mimesniff.spec.whatwg.org/#parsing-a-mime-type
static std::optional<MimeType> parseMIMEType(const String& contentType)
{
    String input = contentType.trim(isASCIIWhitespaceWithoutFF<UChar>);
    size_t slashIndex = input.find('/');
    if (slashIndex == notFound)
        return std::nullopt;

    String type = input.left(slashIndex);
    if (!type.length() || !isValidHTTPToken(type))
        return std::nullopt;
    
    size_t semicolonIndex = input.find(';', slashIndex);
    String subtype = input.substring(slashIndex + 1, semicolonIndex - slashIndex - 1).trim(isASCIIWhitespaceWithoutFF<UChar>);
    if (!subtype.length() || !isValidHTTPToken(subtype))
        return std::nullopt;

    return {{ WTFMove(type), WTFMove(subtype), parseParameters(StringView(input), semicolonIndex + 1) }};
}

FetchBodyConsumer::FetchBodyConsumer(Type type)
    : m_type(type)
{
}

FetchBodyConsumer::FetchBodyConsumer(FetchBodyConsumer&&) = default;
FetchBodyConsumer::~FetchBodyConsumer() = default;
FetchBodyConsumer& FetchBodyConsumer::operator=(FetchBodyConsumer&&) = default;

// https://fetch.spec.whatwg.org/#concept-body-package-data
RefPtr<DOMFormData> FetchBodyConsumer::packageFormData(ScriptExecutionContext* context, const String& contentType, std::span<const uint8_t> data)
{
    auto parseMultipartPart = [context] (std::span<const uint8_t> part, DOMFormData& form) -> bool {
        auto* headerEnd = static_cast<const uint8_t*>(memmem(part.data(), part.size(), "\r\n\r\n", 4));
        if (!headerEnd)
            return false;
        auto headerBytes = part.first(static_cast<size_t>(headerEnd - part.data()));

        auto body = part.subspan(headerBytes.size() + strlen("\r\n\r\n"));

        auto header = String::fromUTF8(headerBytes);

        constexpr auto contentDispositionCharacters = "content-disposition:"_s;
        size_t contentDispositionBegin = header.findIgnoringASCIICase(contentDispositionCharacters);
        if (contentDispositionBegin == notFound)
            return false;
        size_t contentDispositionEnd = header.find("\r\n"_s, contentDispositionBegin);
        size_t contentDispositionParametersBegin = header.find(';', contentDispositionBegin + contentDispositionCharacters.length());
        if (contentDispositionParametersBegin != notFound)
            contentDispositionParametersBegin++;

        auto parameters = parseParameters(StringView(header).substring(contentDispositionParametersBegin, contentDispositionEnd - contentDispositionParametersBegin), 0);
        String name = parameters.get<HashTranslatorASCIILiteral>("name"_s);
        if (!name)
            return false;
        String filename = parameters.get<HashTranslatorASCIILiteral>("filename"_s);
        if (!filename)
            form.append(name, String::fromUTF8(body));
        else {
            String contentType = "text/plain"_s;

            constexpr auto contentTypeCharacters = "content-type:"_s;
            size_t contentTypePrefixLength = contentTypeCharacters.length();
            size_t contentTypeBegin = header.findIgnoringASCIICase(contentTypeCharacters);
            if (contentTypeBegin != notFound) {
                size_t contentTypeEnd = header.find("\r\n"_s, contentTypeBegin);
                contentType = StringView(header).substring(contentTypeBegin + contentTypePrefixLength, contentTypeEnd - contentTypeBegin - contentTypePrefixLength).trim(isASCIIWhitespaceWithoutFF<UChar>).toString();
            }

            form.append(name, File::create(context, Blob::create(context, Vector(body), Blob::normalizedContentType(contentType)).get(), filename).get(), filename);
        }
        return true;
    };
    
    auto parseMultipartBoundary = [] (const std::optional<MimeType>& mimeType) -> std::optional<String> {
        if (!mimeType)
            return std::nullopt;
        if (equalLettersIgnoringASCIICase(mimeType->type, "multipart"_s) && equalLettersIgnoringASCIICase(mimeType->subtype, "form-data"_s)) {
            auto iterator = mimeType->parameters.find<HashTranslatorASCIILiteral>("boundary"_s);
            if (iterator != mimeType->parameters.end())
                return iterator->value;
        }
        return std::nullopt;
    };

    auto form = DOMFormData::create(context, PAL::UTF8Encoding());
    auto mimeType = parseMIMEType(contentType);
    if (auto multipartBoundary = parseMultipartBoundary(mimeType)) {
        auto boundaryWithDashes = makeString("--"_s, *multipartBoundary);
        CString boundary = boundaryWithDashes.utf8();
        size_t boundaryLength = boundary.length();

        auto* currentBoundary = static_cast<const uint8_t*>(memmem(data.data(), data.size(), boundary.data(), boundaryLength));
        if (!currentBoundary)
            return nullptr;
        auto* nextBoundary = static_cast<const uint8_t*>(memmem(currentBoundary + boundaryLength, data.size() - (currentBoundary + boundaryLength - data.data()), boundary.data(), boundaryLength));
        while (nextBoundary) {
            parseMultipartPart(std::span { currentBoundary + boundaryLength, nextBoundary - currentBoundary - boundaryLength - strlen("\r\n") }, form.get());
            currentBoundary = nextBoundary;
            nextBoundary = static_cast<const uint8_t*>(memmem(nextBoundary + boundaryLength, data.size() - (nextBoundary + boundaryLength - data.data()), boundary.data(), boundaryLength));
        }
    } else if (mimeType && equalLettersIgnoringASCIICase(mimeType->type, "application"_s) && equalLettersIgnoringASCIICase(mimeType->subtype, "x-www-form-urlencoded"_s)) {
        auto dataString = String::fromUTF8(data);
        for (auto& pair : WTF::URLParser::parseURLEncodedForm(dataString))
            form->append(pair.key, pair.value);
    } else
        return nullptr;

    return form;
}

static void resolveWithTypeAndData(Ref<DeferredPromise>&& promise, FetchBodyConsumer::Type type, const String& contentType, std::span<const uint8_t> data)
{
    RefPtr context = promise->scriptExecutionContext();

    switch (type) {
    case FetchBodyConsumer::Type::ArrayBuffer:
        fulfillPromiseWithArrayBufferFromSpan(WTFMove(promise), data);
        return;
    case FetchBodyConsumer::Type::Blob:
        promise->resolveCallbackValueWithNewlyCreated<IDLInterface<Blob>>([data, &contentType](auto& context) {
            return blobFromData(&context, { data }, contentType);
        });
        return;
    case FetchBodyConsumer::Type::Bytes:
        fulfillPromiseWithUint8ArrayFromSpan(WTFMove(promise), data);
        return;
    case FetchBodyConsumer::Type::JSON:
        fulfillPromiseWithJSON(WTFMove(promise), TextResourceDecoder::textFromUTF8(data));
        return;
    case FetchBodyConsumer::Type::Text:
        promise->resolve<IDLDOMString>(TextResourceDecoder::textFromUTF8(data));
        return;
    case FetchBodyConsumer::Type::FormData:
        if (auto formData = FetchBodyConsumer::packageFormData(context.get(), contentType, data))
            promise->resolve<IDLInterface<DOMFormData>>(*formData);
        else
            promise->reject(ExceptionCode::TypeError);
        return;
    case FetchBodyConsumer::Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FetchBodyConsumer::clean()
{
    m_buffer.reset();
    if (m_formDataConsumer)
        m_formDataConsumer->cancel();
    resetConsumePromise();
    if (m_sink) {
        m_sink->clearCallback();
        return;
    }
}

void FetchBodyConsumer::resolveWithData(Ref<DeferredPromise>&& promise, const String& contentType, std::span<const uint8_t> data)
{
    resolveWithTypeAndData(WTFMove(promise), m_type, contentType, data);
}

void FetchBodyConsumer::resolveWithFormData(Ref<DeferredPromise>&& promise, const String& contentType, const FormData& formData, ScriptExecutionContext* context)
{
    if (auto sharedBuffer = formData.asSharedBuffer()) {
        resolveWithData(WTFMove(promise), contentType, sharedBuffer->makeContiguous()->span());
        return;
    }

    if (!context)
        return;

    m_formDataConsumer = makeUnique<FormDataConsumer>(formData, *context, [this, promise = WTFMove(promise), contentType, builder = SharedBufferBuilder { }](auto&& result) mutable {
        if (result.hasException()) {
            auto protectedPromise = WTFMove(promise);
            protectedPromise->reject(result.releaseException());
            return;
        }

        auto& value = result.returnValue();
        if (value.empty()) {
            auto protectedPromise = WTFMove(promise);
            auto buffer = builder.takeAsContiguous();
            resolveWithData(WTFMove(protectedPromise), contentType, buffer->span());
            return;
        }

        builder.append(value);
    });
}

void FetchBodyConsumer::consumeFormDataAsStream(const FormData& formData, FetchBodySource& source, ScriptExecutionContext* context)
{
    if (auto sharedBuffer = formData.asSharedBuffer()) {
        if (source.enqueue(ArrayBuffer::tryCreate(sharedBuffer->makeContiguous()->span())))
            source.close();
        return;
    }

    if (!context)
        return;

    m_formDataConsumer = makeUnique<FormDataConsumer>(formData, *context, [this, source = Ref { source }](auto&& result) {
        auto protectedSource = source;
        if (result.hasException()) {
            source->error(result.releaseException());
            return;
        }

        auto& value = result.returnValue();
        if (value.empty()) {
            source->close();
            return;
        }

        if (!source->enqueue(ArrayBuffer::tryCreate(value)))
            m_formDataConsumer->cancel();
    });
}

void FetchBodyConsumer::extract(ReadableStream& stream, ReadableStreamToSharedBufferSink::Callback&& callback)
{
    ASSERT(!m_sink);
    m_sink = ReadableStreamToSharedBufferSink::create(WTFMove(callback));
    m_sink->pipeFrom(stream);
}

void FetchBodyConsumer::resolve(Ref<DeferredPromise>&& promise, const String& contentType, FetchBodyOwner* owner, ReadableStream* stream)
{
    if (stream) {
        ASSERT(!m_sink);
        m_sink = ReadableStreamToSharedBufferSink::create([promise = WTFMove(promise), data = SharedBufferBuilder(), type = m_type, contentType](auto&& result) mutable {
            if (result.hasException()) {
                auto protectedPromise = WTFMove(promise);
                protectedPromise->reject(result.releaseException());
                return;
            }

            auto* chunk = result.returnValue();
            if (!chunk) {
                auto protectedPromise = WTFMove(promise);
                auto buffer = data.takeAsContiguous();
                resolveWithTypeAndData(WTFMove(protectedPromise), type, contentType, buffer->span());
                return;
            }

            data.append(*chunk);
        });
        m_sink->pipeFrom(*stream);
        return;
    }

    if (m_isLoading) {
        if (owner)
            owner->loadBody();
        setConsumePromise(WTFMove(promise));
        return;
    }

    ASSERT(m_type != Type::None);
    switch (m_type) {
    case Type::ArrayBuffer:
        fulfillPromiseWithArrayBuffer(WTFMove(promise), takeAsArrayBuffer().get());
        return;
    case Type::Blob:
        promise->resolveCallbackValueWithNewlyCreated<IDLInterface<Blob>>([this, &contentType](auto& context) {
            return takeAsBlob(&context, contentType);
        });
        return;
    case Type::Bytes: {
        RefPtr buffer = takeAsArrayBuffer();
        RefPtr view = buffer ? RefPtr { Uint8Array::create(buffer.releaseNonNull()) } : nullptr;
        fulfillPromiseWithUint8Array(WTFMove(promise), view.get());
        return;
    }
    case Type::JSON:
        fulfillPromiseWithJSON(WTFMove(promise), takeAsText());
        return;
    case Type::Text:
        promise->resolve<IDLDOMString>(takeAsText());
        return;
    case FetchBodyConsumer::Type::FormData: {
        auto buffer = takeData();
        if (auto formData = packageFormData(promise->scriptExecutionContext(), contentType, buffer ? buffer->makeContiguous()->span() : std::span<const uint8_t> { }))
            promise->resolve<IDLInterface<DOMFormData>>(*formData);
        else
            promise->reject(ExceptionCode::TypeError);
        return;
    }
    case Type::None:
        ASSERT_NOT_REACHED();
        return;
    }
}

void FetchBodyConsumer::append(const SharedBuffer& buffer)
{
    if (m_source) {
        m_source->enqueue(buffer.tryCreateArrayBuffer());
        return;
    }
    m_buffer.append(buffer);
}

void FetchBodyConsumer::setData(Ref<FragmentedSharedBuffer>&& data)
{
    m_buffer = WTFMove(data);
}

RefPtr<FragmentedSharedBuffer> FetchBodyConsumer::takeData()
{
    if (!m_buffer)
        return nullptr;
    return m_buffer.take();
}

RefPtr<JSC::ArrayBuffer> FetchBodyConsumer::takeAsArrayBuffer()
{
    return m_buffer.takeAsArrayBuffer();
}

Ref<Blob> FetchBodyConsumer::takeAsBlob(ScriptExecutionContext* context, const String& contentType)
{
    String normalizedContentType = Blob::normalizedContentType(extractMIMETypeFromMediaType(contentType));

    if (!m_buffer)
        return Blob::create(context, Vector<uint8_t>(), normalizedContentType);

    return blobFromData(context, m_buffer.take()->extractData(), normalizedContentType);
}

String FetchBodyConsumer::takeAsText()
{
    // FIXME: We could probably text decode on the fly as soon as m_type is set to JSON or Text.
    if (!m_buffer)
        return String();

    auto buffer = m_buffer.takeAsContiguous();
    auto text = TextResourceDecoder::textFromUTF8(buffer->span());
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
    if (m_buffer)
        m_source->enqueue(m_buffer.takeAsArrayBuffer());
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
            resolve(m_consumePromise.releaseNonNull(), contentType, nullptr, nullptr);
        else {
            UserGestureIndicator gestureIndicator(m_userGestureToken, UserGestureToken::GestureScope::MediaOnly, UserGestureToken::IsPropagatedFromFetch::Yes);
            resolve(m_consumePromise.releaseNonNull(), contentType, nullptr, nullptr);
        }
    }
    if (m_source) {
        m_source->close();
        m_source = nullptr;
    }
}

FetchBodyConsumer FetchBodyConsumer::clone()
{
    FetchBodyConsumer clone { m_type };
    clone.m_buffer = m_buffer;
    return clone;
}

} // namespace WebCore
