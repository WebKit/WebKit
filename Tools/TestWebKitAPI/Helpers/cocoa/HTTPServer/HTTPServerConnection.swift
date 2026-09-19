// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import Foundation

#if USE_APPLE_INTERNAL_SDK
@_spi(HTTP) @_spi(OHTTP) @_spi(ConnectionExperimental) import Network
#else
import Network
import Network_SPI
#endif

extension NWConnection.ContentContext {
    /// `init(response:)` is `@_spi(OHTTP)`. `Network_SPI` cannot redeclare it because an extension written
    /// outside Network mangles with an extension context, which no exported symbol matches so the public-SDK path
    /// goes through a factory bound to the real initializer instead.
    static func makeHTTPMessagingContext(response: HTTPResponse) -> NWConnection.ContentContext {
        #if USE_APPLE_INTERNAL_SDK
        NWConnection.ContentContext(response: response)
        #else
        NWConnection.ContentContext.__makeContentContext(response: response)
        #endif
    }
}

extension NWConnection {
    func receiveBytes(minimumLength: Int = 1) async -> Data {
        await withCheckedContinuation { continuation in
            receive(minimumIncompleteLength: minimumLength, maximumLength: .max) { content, _, _, error in
                let data = if let content, error == nil { content } else { Data() }
                continuation.resume(returning: data)
            }
        }
    }

    func receiveHTTPRequest() async -> Data {
        let headerTerminator = Data("\r\n\r\n".utf8)
        var buffer = Data()

        while true {
            let chunk = await receiveBytes()
            guard !chunk.isEmpty else {
                return buffer
            }

            buffer.append(chunk)

            guard let headerEnd = buffer.range(of: headerTerminator)?.upperBound else {
                continue
            }

            guard let contentLength = contentLength(in: buffer) else {
                return buffer
            }

            if buffer.count - headerEnd >= contentLength {
                return buffer
            }
        }
    }

    func receiveHTTPMessagingRequest() async -> (request: HTTPRequest, body: Data)? {
        var request: HTTPRequest? = nil
        var body = Data()

        while true {
            let (content, context, isComplete, error) = await withCheckedContinuation { continuation in
                receiveMessage { continuation.resume(returning: ($0, $1, $2, $3)) }
            }

            // Discard any headers/body already accumulated from earlier chunks: an error mid-stream (e.g. the
            // client resets the stream after sending headers but before finishing the body) must not be mistaken
            // by callers for a successfully-received request just because the path happens to already be set.
            if error != nil {
                return nil
            }

            if let httpRequest = context?.httpRequest {
                request = httpRequest
            }

            if let content {
                body.append(content)
            }

            if isComplete {
                guard let request else {
                    return nil
                }

                return (request, body)
            }
        }
    }

    func send(_ data: some DataProtocol, contentContext: NWConnection.ContentContext = .defaultMessage) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, any Error>) in
            send(
                content: data,
                contentContext: contentContext,
                completion: .contentProcessed { error in
                    if let error {
                        continuation.resume(throwing: error)
                    } else {
                        continuation.resume()
                    }
                }
            )
        }
    }

    func sendHTTPMessagingResponse(_ response: HTTPResponseData) async throws {
        let fields = response.headerFields.reduce(into: HTTPFields()) { partialResult, entry in
            guard let name = HTTPField.Name(entry.name) else {
                return
            }

            partialResult[fields: name].append(HTTPField(name: name, value: entry.value))
        }

        let httpResponse = HTTPResponse(status: .init(code: Int(response.statusCode)), headerFields: fields)
        try await send(response.body, contentContext: .makeHTTPMessagingContext(response: httpResponse))
    }

    func terminate() async {
        await withCheckedContinuation { continuation in
            // Strong capture: the handler is stored on the connection itself, so a weak
            // capture could never outlive a strong one. The cycle is broken on .cancelled.
            stateUpdateHandler = { [self] state in
                guard case .cancelled = state else { return }
                stateUpdateHandler = nil
                continuation.resume()
            }
            cancel()
        }
    }
}

private func contentLength(in buffer: Data) -> Int? {
    guard let field = buffer.range(of: Data("Content-Length: ".utf8)) else {
        return nil
    }

    let digits = buffer[field.upperBound...]
        .prefix {
            (UInt8(ascii: "0")...UInt8(ascii: "9")).contains($0)
        }

    let decodedDigits = String(decoding: digits, as: UTF8.self)
    return Int(decodedDigits)
}
