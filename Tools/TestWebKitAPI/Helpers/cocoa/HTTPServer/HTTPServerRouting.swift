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

struct HTTPResponseData: Sendable {
    enum Behavior: Sendable {
        case sendResponseNormally
        case terminateConnectionAfterReceivingRequest
        case neverSendResponse
    }

    var statusCode: UInt = 200
    var headerFields: [(name: String, value: String)] = []
    var body = Data()
    var behavior: Behavior = .sendResponseNormally
    var shouldRespondWith304ToConditionalRequests = false
    var headerFieldsFor304: [String: String] = [:]
}

extension HTTPResponseData {
    func serialize(includingContentLength: Bool = true) -> Data {
        var head = "HTTP/1.1 \(statusCode) \(statusText(code: statusCode))\r\n"
        if includingContentLength {
            head += "Content-Length: \(body.count)\r\n"
        }
        for (name, value) in headerFields {
            head += "\(name): \(value)\r\n"
        }
        head += "\r\n"

        var result = Data(head.utf8)
        result.append(body)
        return result
    }
}

struct HTTPRequestComponentParser: ~Copyable {
    enum Error: Swift.Error {
        case invalidHTTPVersion
        case invalidHTTPMethod
    }

    let request: Data

    private let headerLines: [Substring]
    private let bodyStart: Data.Index

    init(request: Data) {
        self.request = request

        let terminator = request.range(of: Data("\r\n\r\n".utf8))
        let headerBlock = String(decoding: request[..<(terminator?.lowerBound ?? request.endIndex)], as: UTF8.self)

        self.headerLines = headerBlock.split(separator: "\r\n", omittingEmptySubsequences: false)
        self.bodyStart = terminator?.upperBound ?? request.endIndex
    }

    var path: String {
        get throws(Self.Error) {
            guard let requestLine = headerLines.first, !requestLine.isEmpty else {
                return ""
            }

            guard let pathEnd = requestLine.range(of: " HTTP/1.1")?.lowerBound else {
                throw .invalidHTTPVersion
            }

            let pathStart: Substring.Index
            if requestLine.hasPrefix("GET ") {
                pathStart = requestLine.index(requestLine.startIndex, offsetBy: 4)
            } else if requestLine.hasPrefix("POST ") {
                pathStart = requestLine.index(requestLine.startIndex, offsetBy: 5)
            } else {
                throw .invalidHTTPMethod
            }

            return String(requestLine[pathStart..<pathEnd])
        }
    }

    var cookies: String {
        headerValue(prefix: "Cookie: ")
    }

    var authorization: String {
        headerValue(prefix: "Authorization: ")
    }

    var body: String {
        String(decoding: request[bodyStart...], as: UTF8.self)
    }

    var isConditionalRequest: Bool {
        headerLines.contains {
            $0.range(of: "if-none-match:", options: [.caseInsensitive, .anchored]) != nil
        }
    }

    private func headerValue(prefix: String) -> String {
        guard let field = headerLines.first(where: { $0.hasPrefix(prefix) }) else {
            return ""
        }

        return String(field.dropFirst(prefix.count))
    }
}

private func statusText(code: UInt) -> String {
    switch code {
    case 101: "Switching Protocols"
    case 200: "OK"
    case 204: "No Content"
    case 301: "Moved Permanently"
    case 302: "Found"
    case 303: "See Other"
    case 304: "Not Modified"
    case 401: "Unauthorized"
    case 403: "Forbidden"
    case 404: "Not Found"
    case 418: "I'm a teapot"
    case 503: "Service Unavailable"
    default:
        fatalError("Unknown status code \(code)")
    }
}
