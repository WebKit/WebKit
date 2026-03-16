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

// WebKitAccessibilityTestHelper
//
// A small helper tool that calls macOS client-side accessibility APIs
// (AXUIElement) on behalf of WebKitTestRunner.
//
// Communication protocol: reads JSON commands from stdin (one per line),
// writes JSON responses to stdout (one per line).

import AccessibilityPrivate.AccessibilityPrivate
import ApplicationServices
import Foundation

// MARK: - Request / Response types

struct HelperRequest: Decodable {
    let command: String
    var token: String?
    var elementId: UInt64?
    var attribute: String?
    var startElementId: UInt64?
    var isDirectionNext: Bool?
    var resultsLimit: Int?
    var visibleOnly: Bool?
    var immediateDescendantsOnly: Bool?
    var searchKey: String?
    var searchText: String?
}

struct ElementResponse: Encodable { let elementId: UInt64 }
struct ElementArrayResponse: Encodable { let elementIds: [UInt64] }
struct StringResponse: Encodable { let value: String }
struct NumberResponse: Encodable { let value: Double }
struct BooleanResponse: Encodable { let value: Bool }
struct PointResponse: Encodable {
    let x: Double
    let y: Double
}
struct SizeResponse: Encodable {
    let width: Double
    let height: Double
}
struct ErrorResponse: Encodable { let error: String }
struct OkResponse: Encodable { let ok: Bool }

// MARK: - Element storage

var nextElementID: UInt64 = 1
var elementMap: [UInt64: AXUIElement] = [:]

func storeElement(_ element: AXUIElement) -> UInt64 {
    let id = nextElementID
    nextElementID += 1
    elementMap[id] = element
    return id
}

// MARK: - AX helpers

func createRootElement(fromTokenBase64 base64String: String) -> UInt64? {
    guard let tokenData = Data(base64Encoded: base64String) else {
        return nil
    }
    let element = unsafe _AXUIElementCreateWithRemoteToken(tokenData as CFData).takeRetainedValue()
    return storeElement(element)
}

func copyAttributeValue(elementID: UInt64, attribute: String) -> (CFTypeRef?, AXError) {
    guard let element = elementMap[elementID] else {
        return (nil, .invalidUIElement)
    }
    var value: CFTypeRef?
    let error = unsafe AXUIElementCopyAttributeValue(element, attribute as CFString, &value)
    return (value, error)
}

// MARK: - Command handling

func handleCommand(_ request: HelperRequest) -> any Encodable {
    switch request.command {
    case "wirecheck":
        return OkResponse(ok: true)

    case "getRoot":
        guard let token = request.token else {
            return ErrorResponse(error: "missing token")
        }
        if let elementID = createRootElement(fromTokenBase64: token) {
            return ElementResponse(elementId: elementID)
        }
        return ErrorResponse(error: "failed to create root element from token")

    case "copyAttributeValueAsString":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let stringValue = value as? String {
            return StringResponse(value: stringValue)
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "copyAttributeValueAsElementArray":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let elements = value as? [AXUIElement] {
            return ElementArrayResponse(elementIds: elements.map { storeElement($0) })
        }
        if error == .success, let value, CFGetTypeID(value) == AXUIElementGetTypeID() {
            let element: AXUIElement = unsafe unsafeBitCast(value, to: AXUIElement.self)
            return ElementArrayResponse(elementIds: [storeElement(element)])
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "copyAttributeValueAsElement":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let value, CFGetTypeID(value) == AXUIElementGetTypeID() {
            let element: AXUIElement = unsafe unsafeBitCast(value, to: AXUIElement.self)
            return ElementResponse(elementId: storeElement(element))
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "copyAttributeValueAsNumber":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let number = value as? NSNumber {
            return NumberResponse(value: number.doubleValue)
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "copyAttributeValueAsBoolean":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let number = value as? NSNumber {
            return BooleanResponse(value: number.boolValue)
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "copyAttributeValueAsPoint":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let value, CFGetTypeID(value) == AXValueGetTypeID() {
            var point = CGPoint.zero
            // AXValue type verified by CFGetTypeID check above.
            unsafe AXValueGetValue(unsafe unsafeBitCast(value, to: AXValue.self), .cgPoint, &point)
            return PointResponse(x: point.x, y: point.y)
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "copyAttributeValueAsSize":
        guard let elementID = request.elementId, let attribute = request.attribute else {
            return ErrorResponse(error: "missing elementId or attribute")
        }
        let (value, error) = copyAttributeValue(elementID: elementID, attribute: attribute)
        if error == .success, let value, CFGetTypeID(value) == AXValueGetTypeID() {
            var size = CGSize.zero
            // AXValue type verified by CFGetTypeID check above.
            unsafe AXValueGetValue(unsafe unsafeBitCast(value, to: AXValue.self), .cgSize, &size)
            return SizeResponse(width: size.width, height: size.height)
        }
        return ErrorResponse(error: "AXError \(error.rawValue)")

    case "searchPredicate":
        guard let elementID = request.elementId else {
            return ErrorResponse(error: "missing elementId")
        }
        guard let element = elementMap[elementID] else {
            return ErrorResponse(error: "invalid elementId")
        }
        return handleSearchPredicate(request: request, element: element)

    case "releaseElement":
        if let elementID = request.elementId {
            elementMap.removeValue(forKey: elementID)
        }
        return OkResponse(ok: true)

    case "releaseAll":
        elementMap.removeAll()
        nextElementID = 1
        return OkResponse(ok: true)

    default:
        return ErrorResponse(error: "unknown command: \(request.command)")
    }
}

func handleSearchPredicate(request: HelperRequest, element: AXUIElement) -> any Encodable {
    let paramDict = NSMutableDictionary()

    if let startElementID = request.startElementId, startElementID != 0,
        let startElement = elementMap[startElementID]
    {
        paramDict["AXStartElement"] = startElement
    }

    let isDirectionNext = request.isDirectionNext ?? true
    paramDict["AXDirection"] = isDirectionNext ? "AXDirectionNext" : "AXDirectionPrevious"

    paramDict["AXResultsLimit"] = NSNumber(value: request.resultsLimit ?? 1)

    if let searchKey = request.searchKey {
        paramDict["AXSearchKey"] = searchKey
    }
    if let searchText = request.searchText, !searchText.isEmpty {
        paramDict["AXSearchText"] = searchText
    }

    paramDict["AXVisibleOnly"] = NSNumber(value: request.visibleOnly ?? false)
    paramDict["AXImmediateDescendantsOnly"] = NSNumber(value: request.immediateDescendantsOnly ?? false)

    var resultValue: CFTypeRef?
    let searchError = unsafe AXUIElementCopyParameterizedAttributeValue(
        element,
        "AXUIElementsForSearchPredicate" as CFString,
        paramDict as CFDictionary,
        &resultValue
    )

    if searchError != .success || resultValue == nil {
        return ErrorResponse(error: "AXError \(searchError.rawValue)")
    }

    var resultIds: [UInt64] = []
    if let resultArray = resultValue as? [Any] {
        for item in resultArray {
            if CFGetTypeID(item as CFTypeRef) == AXUIElementGetTypeID() {
                resultIds.append(storeElement(unsafe unsafeBitCast(item, to: AXUIElement.self)))
            } else if let dict = item as? NSDictionary,
                let searchResultElement = dict["AXSearchResultElement"],
                CFGetTypeID(searchResultElement as CFTypeRef) == AXUIElementGetTypeID()
            {
                resultIds.append(
                    storeElement(unsafe unsafeBitCast(searchResultElement, to: AXUIElement.self))
                )
            }
        }
    }

    return ElementArrayResponse(elementIds: resultIds)
}

// MARK: - Entry point

@main
enum WebKitAccessibilityTestHelper {
    static func main() {
        unsafe fputs("WebKitAccessibilityTestHelper: started, pid \(ProcessInfo.processInfo.processIdentifier)\n", stderr)

        // Disable stdout buffering so each response line is flushed immediately.
        unsafe setbuf(stdout, nil)

        let decoder = JSONDecoder()
        let encoder = JSONEncoder()
        encoder.outputFormatting = .sortedKeys

        while let line = readLine() {
            guard let data = line.data(using: .utf8),
                let request = try? decoder.decode(HelperRequest.self, from: data)
            else {
                if let errorData = try? encoder.encode(ErrorResponse(error: "invalid JSON")),
                    let errorString = String(data: errorData, encoding: .utf8)
                {
                    print(errorString)
                }
                continue
            }

            let response = handleCommand(request)
            if let responseData = try? encoder.encode(response),
                let responseString = String(data: responseData, encoding: .utf8)
            {
                print(responseString)
            }
        }
    }
}
