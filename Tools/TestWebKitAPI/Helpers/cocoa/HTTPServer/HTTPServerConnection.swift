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
import Network

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

    func send(_ data: some DataProtocol) async throws {
        try await withCheckedThrowingContinuation { (continuation: CheckedContinuation<Void, any Error>) in
            send(
                content: data,
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
