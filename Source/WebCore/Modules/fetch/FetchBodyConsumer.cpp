/*
 * Copyright (C) 2016-2024 Apple Inc. All rights reserved.
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
#include <wtf/StdLibExtras.h>
#include <wtf/URLParser.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/ParsingUtilities.h>

namespace WebCore {

static inline Ref<Blob> blobFromData(ScriptExecutionContext* context, Vector<uint8_t>&& data, const String& contentType)
{
    return Blob::create(context, WTFMove(data), Blob::normalizedContentType(contentType));
}

// https://mimesniff.spec.whatwg.org/#http-quoted-string-token-code-point
static bool isHTTPQuotedStringTokenCodePoint(char16_t c)
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
            parameterValue = input.substring(valueBegin, position - valueBegin).trim(isASCIIWhitespaceWithoutFF<char16_t>);
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
    String input = contentType.trim(isASCIIWhitespaceWithoutFF<char16_t>);
    size_t slashIndex = input.find('/');
    if (slashIndex == notFound)
        return std::nullopt;

    String type = input.left(slashIndex);
    if (!type.length() || !isValidHTTPToken(type))
        return std::nullopt;
    
    size_t semicolonIndex = input.find(';', slashIndex);
    String subtype = input.substring(slashIndex + 1, semicolonIndex - slashIndex - 1).trim(isASCIIWhitespaceWithoutFF<char16_t>);
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
    static constexpr auto oneNewLine = "\r\n"_s;
    auto parseMultipartPart = [context] (std::span<const uint8_t> part, DOMFormData& form) -> bool {
        static constexpr auto twoNewLines = "\r\n\r\n"_span;
        size_t headerEnd = find(part, twoNewLines);
        if (headerEnd == notFound)
            return false;
        auto headerBytes = part.first(headerEnd);

        auto body = part.subspan(headerBytes.size() + twoNewLines.size());

        auto header = String::fromUTF8(headerBytes);

        constexpr auto contentDispositionCharacters = "content-disposition:"_s;
        size_t contentDispositionBegin = header.findIgnoringASCIICase(contentDispositionCharacters);
        if (contentDispositionBegin == notFound)
            return false;

        size_t contentDispositionEnd = header.find(oneNewLine, contentDispositionBegin);
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
                size_t contentTypeEnd = header.find(oneNewLine, contentTypeBegin);
                contentType = StringView(header).substring(contentTypeBegin + contentTypePrefixLength, contentTypeEnd - contentTypeBegin - contentTypePrefixLength).trim(isASCIIWhitespaceWithoutFF<char16_t>).toString();
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

        size_t currentBoundaryIndex = find(data, boundary.span());
        if (currentBoundaryIndex == notFound)
            return nullptr;
        skip(data, currentBoundaryIndex + boundaryLength);
        size_t nextBoundaryIndex;
        while ((nextBoundaryIndex = find(data, boundary.span())) != notFound) {
            parseMultipartPart(data.first(nextBoundaryIndex - oneNewLine.length()), form.get());
            currentBoundaryIndex = nextBoundaryIndex;
            skip(data, nextBoundaryIndex + boundaryLength);
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
    if (RefPtr formDataConsumer = m_formDataConsumer)
        formDataConsumer->cancel();
    resetConsumePromise();
    if (RefPtr sink = m_sink) {
        sink->clearCallback();
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

    m_formDataConsumer = FormDataConsumer::create(formData, *context, [promise = WTFMove(promise), type = m_type, contentType, builder = SharedBufferBuilder { }](auto&& result) mutable {
        if (result.hasException()) {
            auto protectedPromise = WTFMove(promise);
            protectedPromise->reject(result.releaseException());
            return false;
        }

        auto& value = result.returnValue();
        if (value.empty()) {
            auto protectedPromise = WTFMove(promise);
            auto buffer = builder.takeAsContiguous();
            resolveWithTypeAndData(WTFMove(protectedPromise), type, contentType, buffer->span());
            return false;
        }

        builder.append(value);
        return true;
    });
    protectedFormDataConsumer()->start();
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

    m_formDataConsumer = FormDataConsumer::create(formData, *context, [source = Ref { source }](auto&& result) {
        auto protectedSource = source;
        if (result.hasException()) {
            source->error(result.releaseException());
            return false;
        }

        auto& value = result.returnValue();
        if (value.empty()) {
            source->close();
            return false;
        }

        return source->enqueue(ArrayBuffer::tryCreate(value));
    });
    protectedFormDataConsumer()->start();
}

void FetchBodyConsumer::extract(ReadableStream& stream, ReadableStreamToSharedBufferSink::Callback&& callback)
{
    ASSERT(!m_sink);
    m_sink = ReadableStreamToSharedBufferSink::create(WTFMove(callback));
    protectedSink()->pipeFrom(stream);
}

void FetchBodyConsumer::resolve(Ref<DeferredPromise>&& promise, const String& contentType, FetchBodyOwner* owner, ReadableStream* stream)
{
    if (stream) {
        ASSERT(!m_sink);
        m_sink = ReadableStreamToSharedBufferSink::create([promise = WTFMove(promise), data = SharedBufferBuilder(), type = m_type, contentType](auto&& result) mutable {
            WTF::switchOn(WTFMove(result), [&](std::nullptr_t) {
                auto protectedPromise = WTFMove(promise);
                auto buffer = data.takeAsContiguous();
                resolveWithTypeAndData(WTFMove(protectedPromise), type, contentType, buffer->span());
            }, [&](std::span<const uint8_t>&& chunk) {
                data.append(chunk);
            }, [&](JSC::JSValue reason) {
                auto protectedPromise = WTFMove(promise);
                protectedPromise->rejectWithCallback([&](auto&) {
                    return reason;
                });
            }, [&](Exception&& error) {
                auto protectedPromise = WTFMove(promise);
                protectedPromise->reject(WTFMove(error));
            });
        });
        protectedSink()->pipeFrom(*stream);
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
        if (auto formData = packageFormData(promise->protectedScriptExecutionContext().get(), contentType, buffer ? buffer->makeContiguous()->span() : std::span<const uint8_t> { }))
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
    if (RefPtr source = m_source) {
        source->enqueue(buffer.tryCreateArrayBuffer());
        return;
    }
    m_buffer.append(buffer);
}

void FetchBodyConsumer::setData(Ref<FragmentedSharedBuffer>&& data)
{
    m_buffer.reset();
    m_buffer.append(WTFMove(data));
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

RefPtr<JSC::ArrayBuffer> FetchBodyConsumer::asArrayBuffer()
{
    return m_buffer.tryCreateArrayBuffer();
}

Ref<Blob> FetchBodyConsumer::takeAsBlob(ScriptExecutionContext* context, const String& contentType)
{
    String normalizedContentType = Blob::normalizedContentType(extractMIMETypeFromMediaType(contentType));

    if (!m_buffer)
        return Blob::create(context, Vector<uint8_t>(), normalizedContentType);

    // Minimise the number of DataSegments to reduce overall memory allocation.
    // We pack it in 8MiB minimum segment.
    static constexpr size_t MinimumBlobSize = 8 * 1024 * 1024;

    RefPtr buffer = m_buffer.take();
    if (buffer->size() <= MinimumBlobSize)
        return Blob::create(context, buffer->extractData(), normalizedContentType);

    size_t packedBufferSize = std::min(buffer->size(), MinimumBlobSize);
    Vector<uint8_t> packedBuffer;
    packedBuffer.reserveInitialCapacity(packedBufferSize);

    Vector<Ref<SharedBuffer>> segments;
    segments.reserveInitialCapacity(buffer->segmentsCount());
    buffer->forEachSegmentAsSharedBuffer([&](Ref<SharedBuffer>&& sharedBuffer) {
        segments.append(WTFMove(sharedBuffer));
    });
    buffer = nullptr; // So that the segments above hold each a single refcount to allow extractData to move the content.

    SharedBufferBuilder bufferBuilder;
    for (auto&& segment : segments) {
        if (segment->size() >= MinimumBlobSize) {
            if (packedBuffer.size()) {
                bufferBuilder.append(std::exchange(packedBuffer, { }));
                packedBuffer.reserveInitialCapacity(packedBufferSize);
            }
            bufferBuilder.append(WTFMove(segment));
            continue;
        }
        if (packedBuffer.size() + segment->size() <= MinimumBlobSize) {
            packedBuffer.appendVector(segment->extractData());
            continue;
        }
        ASSERT(packedBuffer.size() <= MinimumBlobSize);
        size_t leftInPacked = MinimumBlobSize - packedBuffer.size();
        auto segmentData = segment->extractData();
        packedBuffer.appendVector(segmentData.subvector(0, leftInPacked));
        bufferBuilder.append(std::exchange(packedBuffer, { }));
        packedBuffer.reserveInitialCapacity(packedBufferSize);
        packedBuffer.appendVector(segmentData.subvector(leftInPacked));
    };
    if (!packedBuffer.isEmpty())
        bufferBuilder.append(WTFMove(packedBuffer));

    return Blob::create(context, bufferBuilder.take(), normalizedContentType);
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
    m_source = source.copyRef();
    if (m_buffer)
        source->enqueue(m_buffer.takeAsArrayBuffer());
}

void FetchBodyConsumer::loadingFailed(const Exception& exception)
{
    m_isLoading = false;
    if (RefPtr consumePromise = m_consumePromise) {
        consumePromise->reject(exception);
        resetConsumePromise();
    }
    if (RefPtr source = m_source) {
        source->error(exception);
        m_source = nullptr;
    }
}

void FetchBodyConsumer::loadingSucceeded(const String& contentType)
{
    m_isLoading = false;

    if (m_consumePromise) {
        RefPtr userGestureToken = m_userGestureToken;
        if (!userGestureToken || userGestureToken->hasExpired(UserGestureToken::maximumIntervalForUserGestureForwardingForFetch()) || !userGestureToken->processingUserGesture())
            resolve(m_consumePromise.releaseNonNull(), contentType, nullptr, nullptr);
        else {
            UserGestureIndicator gestureIndicator(userGestureToken, UserGestureToken::GestureScope::MediaOnly, UserGestureToken::ShouldPropagateToMicroTask::Yes);
            resolve(m_consumePromise.releaseNonNull(), contentType, nullptr, nullptr);
        }
    }
    if (RefPtr source = m_source) {
        source->close();
        m_source = nullptr;
    }
}

UniqueRef<FetchBodyConsumer> FetchBodyConsumer::clone()
{
    auto clone = makeUniqueRef<FetchBodyConsumer>(m_type);
    clone->m_buffer = m_buffer;
    return clone;
}

bool FetchBodyConsumer::hasPendingActivity() const
{
    return (m_formDataConsumer && m_formDataConsumer->hasPendingActivity())
        || (m_sink && m_sink->hasCallback());
}

} // namespace WebCore
