// Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if HAVE_DIGITAL_CREDENTIALS_UI

internal import IdentityDocumentServices
internal import IdentityDocumentServicesUI

extension WKIdentityDocumentPresentmentMobileDocumentElementInfo {
    convenience init(_ source: ISO18013MobileDocumentRequest.ElementInfo) {
        self.init(isRetaining: source.isRetaining)
    }
}

extension WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest {
    convenience init(_ source: ISO18013MobileDocumentRequest.DocumentRequest) {
        self.init(
            documentType: source.documentType,
            namespaces: source.namespaces.mapValues {
                $0.mapValues(WKIdentityDocumentPresentmentMobileDocumentElementInfo.init(_:))
            }
        )

        #if ENABLE_ISO18013_DOCUMENT_REQUEST_INFO
        if let requestInfo = source.requestInformation {
            self.applicationSpecificExtensions = Self.convertSendableExtensions(requestInfo.applicationSpecificExtensions)
        }
        #endif // ENABLE_ISO18013_DOCUMENT_REQUEST_INFO
    }

    #if ENABLE_ISO18013_DOCUMENT_REQUEST_INFO
    private static func convertSendableExtensions(
        _ extensions: [ISO18013MobileDocumentRequest.ApplicationSpecificExtensionKey: any Sendable]
    ) -> [String: Any] {
        var result: [String: Any] = [:]
        for (key, value) in extensions {
            result[key.rawValue] = convertSendableValue(value)
        }
        return result
    }

    private static func convertSendableValue(_ value: any Sendable) -> Any {
        // Convert Swift types to Objective-C compatible types
        switch value {
        case let string as String:
            return string as NSString
        case let number as Int:
            return NSNumber(value: number)
        case let number as Double:
            return NSNumber(value: number)
        case let number as Float:
            return NSNumber(value: number)
        case let bool as Bool:
            return NSNumber(value: bool)
        case let array as [any Sendable]:
            return array.map { Self.convertSendableValue($0) } as NSArray
        case let dict as [String: any Sendable]:
            var objcDict: [String: Any] = [:]
            for (key, val) in dict {
                objcDict[key] = Self.convertSendableValue(val)
            }
            return objcDict as NSDictionary
        default:
            return value as Any
        }
    }
    #endif // ENABLE_ISO18013_DOCUMENT_REQUEST_INFO
}

extension WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest {
    convenience init(_ source: ISO18013MobileDocumentRequest.PresentmentRequest) {
        self.init(
            documentSets: source.documentRequestSets.map {
                $0.requests.map(WKIdentityDocumentPresentmentMobileDocumentIndividualDocumentRequest.init(_:))
            },
            isMandatory: source.isMandatory
        )
    }
}

extension WKIdentityDocumentPresentmentMobileDocumentRequest {
    convenience init(_ source: ISO18013MobileDocumentRequest) {
        self.init(
            presentmentRequests: source.presentmentRequests.map(WKIdentityDocumentPresentmentMobileDocumentPresentmentRequest.init(_:)),
            authenticationCertificates: source.requestAuthentications.map {
                $0.authenticationCertificateChain.map(WKIdentityDocumentPresentmentRequestAuthenticationCertificate.init(certificate:))
            }
        )
    }
}

#endif
