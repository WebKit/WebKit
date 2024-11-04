/*
 * Copyright (C) 2022 Sony Interactive Entertainment Inc.
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
#include "WebSocketTaskCurl.h"

#include "NetworkProcess.h"
#include "NetworkSessionCurl.h"
#include "NetworkSocketChannel.h"
#include <WebCore/AuthenticationChallenge.h>
#include <WebCore/ClientOrigin.h>
#include <WebCore/CurlStreamScheduler.h>
#include <WebCore/WebSocketHandshake.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebKit {
WTF_MAKE_TZONE_ALLOCATED_IMPL(WebSocketTask);

WebSocketTask::WebSocketTask(NetworkSocketChannel& channel, WebPageProxyIdentifier webProxyPageID, const WebCore::ResourceRequest& request, const String& protocol, const WebCore::ClientOrigin& clientOrigin)
    : m_channel(channel)
    , m_webProxyPageID(webProxyPageID)
    , m_request(request.isolatedCopy())
    , m_protocol(protocol)
    , m_scheduler(WebCore::CurlContext::singleton().streamScheduler())
{
    // We use topOrigin in case of service worker websocket connections, for which pageID does not link to a real page.
    // In that case, let's only call the callback for same origin loads.
    if (clientOrigin.topOrigin == clientOrigin.clientOrigin)
        m_topOrigin = clientOrigin.topOrigin;

    auto localhostAlias = WebCore::CurlStream::LocalhostAlias::Disable;
    if (networkSession() && networkSession()->networkProcess().localhostAliasesForTesting().contains<StringViewHashTranslator>(m_request.url().host()))
        localhostAlias = WebCore::CurlStream::LocalhostAlias::Enable;

    m_streamID = m_scheduler.createStream(request.url(), *this, WebCore::CurlStream::ServerTrustEvaluation::Enable, localhostAlias);
    channel.didSendHandshakeRequest(WebCore::ResourceRequest(m_request));
}

WebSocketTask::~WebSocketTask()
{
    destructStream();
}

Ref<NetworkSocketChannel> WebSocketTask::protectedChannel() const
{
    return m_channel.get();
}

void WebSocketTask::sendString(std::span<const uint8_t> utf8, CompletionHandler<void()>&& callback)
{
    if (m_state == State::Opened) {
        if (!sendFrame(WebCore::WebSocketFrame::OpCodeText, utf8))
            didFail("Failed to send WebSocket frame."_s);
    }
    callback();
}

void WebSocketTask::sendData(std::span<const uint8_t> data, CompletionHandler<void()>&& callback)
{
    if (m_state == State::Opened) {
        if (!sendFrame(WebCore::WebSocketFrame::OpCodeBinary, data))
            didFail("Failed to send WebSocket frame."_s);
    }
    callback();
}

void WebSocketTask::close(int32_t code, const String& reason)
{
    if (m_state == State::Closed)
        return;

    if (m_state == State::Connecting || m_state == State::Handshaking) {
        didClose(WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeAbnormalClosure, { });
        return;
    }

    sendClosingHandshakeIfNeeded(code, reason);
}

void WebSocketTask::cancel()
{

}

void WebSocketTask::resume()
{

}

NetworkSessionCurl* WebSocketTask::networkSession()
{
    return static_cast<NetworkSessionCurl*>(protectedChannel()->session());
}

void WebSocketTask::didOpen(WebCore::CurlStreamID)
{
    if (m_state != State::Connecting)
        return;

    m_state = State::Handshaking;

    m_handshake = makeUnique<WebCore::WebSocketHandshake>(m_request.url(), m_protocol, m_request.httpUserAgent(), m_request.httpHeaderField(WebCore::HTTPHeaderName::Origin), m_request.allowCookies(), false);
    m_handshake->reset();
    m_handshake->addExtensionProcessor(m_deflateFramer.createExtensionProcessor());

    CString cookieHeader;

    if (m_request.allowCookies()) {
        if (auto* storageSession = networkSession() ? networkSession()->networkStorageSession() : nullptr) {
            auto includeSecureCookies = m_request.url().protocolIs("wss"_s) ? WebCore::IncludeSecureCookies::Yes : WebCore::IncludeSecureCookies::No;
            auto cookieHeaderField = storageSession->cookieRequestHeaderFieldValue(m_request.firstPartyForCookies(), WebCore::SameSiteInfo::create(m_request), m_request.url(), std::nullopt, std::nullopt, includeSecureCookies, WebCore::ApplyTrackingPrevention::Yes, WebCore::ShouldRelaxThirdPartyCookieBlocking::No).first;
            if (!cookieHeaderField.isEmpty())
                cookieHeader = makeString("Cookie: "_s, cookieHeaderField, "\r\n"_s).utf8();
        }
    }

    auto originalMessage = m_handshake->clientHandshakeMessage();
    auto handshakeMessageLength = originalMessage.length() + cookieHeader.length();
    auto handshakeMessage = makeUniqueArray<uint8_t>(handshakeMessageLength);

    memcpy(handshakeMessage.get(), originalMessage.data(), originalMessage.length());
    if (!cookieHeader.isNull() && cookieHeader.length()) {
        memcpy(handshakeMessage.get() + originalMessage.length() - 2, cookieHeader.data(), cookieHeader.length());
        memcpy(handshakeMessage.get() + handshakeMessageLength - 2, "\r\n", 2);
    }

    m_scheduler.send(m_streamID, WTFMove(handshakeMessage), handshakeMessageLength);
}

void WebSocketTask::didReceiveData(WebCore::CurlStreamID, const WebCore::SharedBuffer& buffer)
{
    if (m_state == State::Connecting || m_state == State::Closed)
        return;

    if (!buffer.size()) {
        didClose(WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeAbnormalClosure, { });
        return;
    }

    if (m_shouldDiscardReceivedData || m_receivedClosingHandshake)
        return;

    if (!appendReceivedBuffer(buffer)) {
        didFail("Ran out of memory while receiving WebSocket data."_s);
        return;
    }

    auto validateResult = validateOpeningHandshake();
    if (!validateResult.has_value()) {
        didFail(String(validateResult.error()));
        return;
    }
    if (!validateResult.value())
        return;

    auto frameResult = receiveFrames([this, weakThis = WeakPtr { *this }](WebCore::WebSocketFrame::OpCode opCode, std::span<const uint8_t> data) {
        if (!weakThis)
            return;

        switch (opCode) {
        case WebCore::WebSocketFrame::OpCodeText:
            {
                String message = data.size() ? String::fromUTF8(data) : emptyString();
                if (!message.isNull())
                    protectedChannel()->didReceiveText(message);
                else
                    didFail("Could not decode a text frame as UTF-8."_s);
            }
            break;

        case WebCore::WebSocketFrame::OpCodeBinary:
            protectedChannel()->didReceiveBinaryData(data);
            break;

        case WebCore::WebSocketFrame::OpCodeClose:
            if (!data.size())
                m_closeEventCode = WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeNoStatusRcvd;
            else if (data.size() == 1) {
                m_closeEventCode = WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeAbnormalClosure;
                didFail("Received a broken close frame containing an invalid size body."_s);
                return;
            } else {
                auto highByte = static_cast<unsigned char>(data[0]);
                auto lowByte = static_cast<unsigned char>(data[1]);
                m_closeEventCode = highByte << 8 | lowByte;
                if (m_closeEventCode == WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeNoStatusRcvd
                    || m_closeEventCode == WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeAbnormalClosure
                    || m_closeEventCode == WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeTLSHandshake) {
                    m_closeEventCode = WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeAbnormalClosure;
                    didFail("Received a broken close frame containing a reserved status code."_s);
                    return;
                }
            }
            if (data.size() >= 3)
                m_closeEventReason = String::fromUTF8({ &data[2], data.size() - 2 });
            else
                m_closeEventReason = emptyString();

            m_receivedClosingHandshake = true;
            sendClosingHandshakeIfNeeded(m_closeEventCode, m_closeEventReason);
            didClose(m_closeEventCode, m_closeEventReason);
            break;

        case WebCore::WebSocketFrame::OpCodePing:
            if (!sendFrame(WebCore::WebSocketFrame::OpCodePong, data))
                didFail("Failed to send WebSocket frame."_s);
            break;

        case WebCore::WebSocketFrame::OpCodeContinuation:
        case WebCore::WebSocketFrame::OpCodePong:
        case WebCore::WebSocketFrame::OpCodeInvalid:
            break;
        }
    });

    if (frameResult) {
        didFail(String(*frameResult));
        return;
    }
}

void WebSocketTask::didFail(WebCore::CurlStreamID, CURLcode errorCode, WebCore::CertificateInfo&& certificateInfo)
{
    auto reason = makeString("WebSocket network error: error code "_s, static_cast<uint32_t>(errorCode));

    if (errorCode == CURLE_PEER_FAILED_VERIFICATION && networkSession()) {
        RELEASE_ASSERT(m_state == State::Connecting);
        destructStream();

        tryServerTrustEvaluation({ m_request.url(), WTFMove(certificateInfo), WebCore::ResourceError(errorCode, m_request.url()) }, WTFMove(reason));
        return;
    }

    didFail(WTFMove(reason));
}

void WebSocketTask::tryServerTrustEvaluation(WebCore::AuthenticationChallenge&& challenge, String&& errorReason)
{
    networkSession()->didReceiveChallenge(*this, WTFMove(challenge), [this, errorReason = WTFMove(errorReason)](WebKit::AuthenticationChallengeDisposition disposition, const WebCore::Credential& credential) mutable {
        if (disposition == AuthenticationChallengeDisposition::UseCredential && !credential.isEmpty()) {
            auto localhostAlias = WebCore::CurlStream::LocalhostAlias::Disable;
            if (networkSession() && networkSession()->networkProcess().localhostAliasesForTesting().contains<StringViewHashTranslator>(m_request.url().host()))
                localhostAlias = WebCore::CurlStream::LocalhostAlias::Enable;

            m_streamID = m_scheduler.createStream(m_request.url(), *this, WebCore::CurlStream::ServerTrustEvaluation::Disable, localhostAlias);
        } else
            didFail(WTFMove(errorReason));
    });
}

bool WebSocketTask::appendReceivedBuffer(const WebCore::SharedBuffer& buffer)
{
    size_t newBufferSize = m_receiveBuffer.size() + buffer.size();
    if (newBufferSize < m_receiveBuffer.size())
        return false;

    m_receiveBuffer.append(buffer.span());
    return true;
}

void WebSocketTask::skipReceivedBuffer(size_t length)
{
    memmove(m_receiveBuffer.data(), m_receiveBuffer.data() + length, m_receiveBuffer.size() - length);
    m_receiveBuffer.shrink(m_receiveBuffer.size() - length);
}

Expected<bool, String> WebSocketTask::validateOpeningHandshake()
{
    if (m_didCompleteOpeningHandshake)
        return true;

    if (m_state != State::Handshaking || !m_handshake || m_handshake->mode() != WebCore::WebSocketHandshake::Incomplete) {
        m_handshake = nullptr;
        return makeUnexpected("Unexpected handshakeing condition"_s);
    }

    auto headerLength = m_handshake->readServerHandshake(m_receiveBuffer.span());
    if (headerLength <= 0)
        return false;

    skipReceivedBuffer(headerLength);

    if (m_handshake->mode() != WebCore::WebSocketHandshake::Connected) {
        auto reason = m_handshake->failureReason();
        m_handshake = nullptr;
        return makeUnexpected(reason);
    }

    auto serverSetCookie = m_handshake->serverSetCookie();
    if (!serverSetCookie.isEmpty()) {
        if (auto* storageSession = networkSession() ? networkSession()->networkStorageSession() : nullptr)
            storageSession->setCookiesFromHTTPResponse(m_request.firstPartyForCookies(), m_request.url(), serverSetCookie);
    }

    m_state = State::Opened;
    m_didCompleteOpeningHandshake = true;

    Ref channel = m_channel.get();
    channel->didConnect(m_handshake->serverWebSocketProtocol(), m_handshake->acceptedExtensions());
    channel->didReceiveHandshakeResponse(WebCore::ResourceResponse(m_handshake->serverHandshakeResponse()));

    m_handshake = nullptr;
    return true;
}

std::optional<String> WebSocketTask::receiveFrames(Function<void(WebCore::WebSocketFrame::OpCode, std::span<const uint8_t>)>&& callback)
{
    if (m_state != State::Opened && m_state != State::Closing)
        return std::nullopt;

    while (m_receiveBuffer.size() && !m_shouldDiscardReceivedData && !m_receivedClosingHandshake) {
        WebCore::WebSocketFrame frame;
        const uint8_t* frameEnd;
        String errorString;
        auto parseResult = WebCore::WebSocketFrame::parseFrame(reinterpret_cast<uint8_t*>(m_receiveBuffer.data()), m_receiveBuffer.size(), frame, frameEnd, errorString);
        if (parseResult == WebCore::WebSocketFrame::FrameIncomplete)
            return std::nullopt;
        if (parseResult == WebCore::WebSocketFrame::FrameError)
            return errorString;

        auto inflateResult = m_deflateFramer.inflate(frame);
        if (!inflateResult->succeeded())
            return inflateResult->failureReason();

        if (auto validateResult = validateFrame(frame))
            return *validateResult;

        if (!frame.final || frame.opCode == WebCore::WebSocketFrame::OpCodeContinuation) {
            if (frame.opCode != WebCore::WebSocketFrame::OpCodeContinuation) {
                m_hasContinuousFrame = true;
                m_continuousFrameOpCode = frame.opCode;
            }

            m_continuousFrameData.append(frame.payload);

            if (frame.final) {
                callback(m_continuousFrameOpCode, m_continuousFrameData.span());
                m_hasContinuousFrame = false;
                m_continuousFrameData.clear();
            }
        } else
            callback(frame.opCode, frame.payload);

        if (!m_receiveBuffer.isEmpty())
            skipReceivedBuffer(frameEnd - m_receiveBuffer.data());
    }

    return std::nullopt;
}

std::optional<String> WebSocketTask::validateFrame(const WebCore::WebSocketFrame& frame)
{
    if (WebCore::WebSocketFrame::isReservedOpCode(frame.opCode))
        return makeString("Unrecognized frame opcode: "_s, static_cast<unsigned>(frame.opCode));

    if (frame.reserved2 || frame.reserved3)
        return makeString("One or more reserved bits are on: reserved2 = "_s, static_cast<unsigned>(frame.reserved2), ", reserved3 = "_s, static_cast<unsigned>(frame.reserved3));

    if (frame.masked)
        return  "A server must not mask any frames that it sends to the client."_s;

    // All control frames must not be fragmented.
    if (WebCore::WebSocketFrame::isControlOpCode(frame.opCode) && !frame.final)
        return makeString("Received fragmented control frame: opcode = "_s, static_cast<unsigned>(frame.opCode));

    // All control frames must have a payload of 125 bytes or less, which means the frame must not contain
    // the "extended payload length" field.
    if (WebCore::WebSocketFrame::isControlOpCode(frame.opCode) && WebCore::WebSocketFrame::needsExtendedLengthField(frame.payload.size()))
        return makeString("Received control frame having too long payload: "_s, frame.payload.size(), " bytes"_s);

    // A new data frame is received before the previous continuous frame finishes.
    // Note that control frames are allowed to come in the middle of continuous frames.
    if (m_hasContinuousFrame && frame.opCode != WebCore::WebSocketFrame::OpCodeContinuation && !WebCore::WebSocketFrame::isControlOpCode(frame.opCode))
        return "Received new data frame but previous continuous frame is unfinished."_s;

    // An unexpected continuation frame is received without any leading frame.
    if (!m_hasContinuousFrame && frame.opCode == WebCore::WebSocketFrame::OpCodeContinuation)
        return "Received unexpected continuation frame."_s;

    return std::nullopt;
}

void WebSocketTask::sendClosingHandshakeIfNeeded(int32_t code, const String& reason)
{
    if (m_didSendClosingHandshake)
        return;

    Vector<uint8_t> buf;
    if (!m_receivedClosingHandshake && code != WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeNotSpecified) {
        unsigned char highByte = static_cast<unsigned short>(code) >> 8;
        unsigned char lowByte = static_cast<unsigned short>(code);
        buf.append(static_cast<char>(highByte));
        buf.append(static_cast<char>(lowByte));
        buf.append(reason.utf8().span());
    }

    if (!sendFrame(WebCore::WebSocketFrame::OpCodeClose, buf.span()))
        didFail("Failed to send WebSocket frame."_s);

    m_state = State::Closing;
    m_didSendClosingHandshake = true;
}

bool WebSocketTask::sendFrame(WebCore::WebSocketFrame::OpCode opCode, std::span<const uint8_t> data)
{
    if (m_didSendClosingHandshake)
        return true;

    WebCore::WebSocketFrame frame(opCode, true, false, true, data);

    auto deflateResult = m_deflateFramer.deflate(frame);
    if (!deflateResult->succeeded()) {
        didFail(deflateResult->failureReason());
        return false;
    }

    Vector<uint8_t> frameData;
    frame.makeFrameData(frameData);

    auto buffer = makeUniqueArray<uint8_t>(frameData.size());
    memcpy(buffer.get(), frameData.data(), frameData.size());

    m_scheduler.send(m_streamID, WTFMove(buffer), frameData.size());
    return true;
}

void WebSocketTask::didFail(String&& reason)
{
    if (m_receivedDidFail)
        return;

    m_receivedDidFail = true;

    // Hybi-10 specification explicitly states we must not continue to handle incoming data
    // once the WebSocket connection is failed (section 7.1.7).
    m_shouldDiscardReceivedData = true;
    if (!m_receiveBuffer.isEmpty())
        skipReceivedBuffer(m_receiveBuffer.size()); // Save memory.
    m_deflateFramer.didFail();
    m_hasContinuousFrame = false;
    m_continuousFrameData.clear();

    protectedChannel()->didReceiveMessageError(WTFMove(reason));
    didClose(WebCore::ThreadableWebSocketChannel::CloseEventCode::CloseEventCodeAbnormalClosure, { });
}

void WebSocketTask::didClose(int32_t code, const String& reason)
{
    destructStream();

    if (m_state == State::Closed)
        return;

    m_state = State::Closed;

    callOnMainRunLoop([weakThis = WeakPtr { *this }, code, reason] {
        if (weakThis)
            weakThis->protectedChannel()->didClose(code, reason);
    });
}

void WebSocketTask::destructStream()
{
    if (isStreamInvalidated())
        return;

    m_scheduler.destroyStream(m_streamID);
    m_streamID = WebCore::invalidCurlStreamID;
}

} // namespace WebKit
