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

#if ENABLE_SWIFTUI && ENABLE_CXX_INTEROP

import Foundation

/// An object that contains information about both the location and content of text and glyphs that the framework recognizes in an image.
public struct ImageAnalysisResult: Codable, Sendable {
    /// Information about regions of text that an image-analysis request detects.
    public struct Line: Codable, Sendable {
        /// Text recognized in an image through a text recognition request.
        public struct TextInfo: Codable, Sendable {
            /// The top candidate for recognized text.
            public let text: String

            /// The bounding region of the text.
            public let quad: Quad
        }

        /// The top candidate for recognized text.
        public let text: String

        /// The bounding region of the line.
        public let quad: Quad

        /// The pieces of text that compose this line.
        public let children: [TextInfo]
    }

    /// A type representing a bounding quadrilateral.
    public struct Quad: Codable, Sendable {
        /// The coordinates of the upper-left corner of the quadrilateral.
        public let topLeft: CGPoint

        /// The coordinates of the upper-right corner of the quadrilateral.
        public let topRight: CGPoint

        /// The coordinates of the lower-left corner of the quadrilateral.
        public let bottomLeft: CGPoint

        /// The coordinates of the lower-right corner of quadrilateral.
        public let bottomRight: CGPoint
    }

    /// The lines of text that compose this analysis.
    public let lines: [Line]
}

extension ImageAnalysisResult {
    /// Parses valid JSON data from a URL into a new `ImageAnalysisResult` value.
    ///
    /// - Parameter url: The URL to parse, which must contain valid JSON data in the correct schema.
    /// - Throws: An error if decoding fails.
    public init(parsing url: URL) throws {
        let decoder = JSONDecoder()
        self = try decoder.decode(Self.self, from: Data(contentsOf: url))
    }
}

/// Runs `body` with `-[VKCImageAnalyzer processRequest:progressHandler:completionHandler:]` swizzled
/// to  return `analysis` instead of dispatching to Vision.
///
/// The original implementation is restored when `body` returns (or throws).
///
/// - Parameters:
///   - response: The response to provide in place of the Vision framework's normal response.
///   - failureType: The type of the error of the result, if any.
///   - delay: An option duration which, if specified, delays the response from being provided by the specified duration.
///   - body: The code to execute with the replaced implementation.
public nonisolated(nonsending) func withMockedImageAnalyzer<Failure>(
    response: Result<ImageAnalysisResult, Failure>,
    failureType: Failure.Type = Failure.self,
    after delay: Duration? = nil,
    perform body: () async -> sending Void
) async where Failure: Error {
    typealias ObjCVKImageAnalysisRequestID = Int32
    typealias ObjCVKCImageAnalysis = AnyObject
    typealias ObjCCompletionHandler = @Sendable @convention(block) (ObjCVKCImageAnalysis?, NSError?) -> Void
    typealias ObjCProgressHandler = @Sendable @convention(block) (Double) -> Void
    typealias ObjCImplementation =
        @Sendable @convention(block) (AnyObject, AnyObject?, ObjCProgressHandler?, @escaping ObjCCompletionHandler) ->
        ObjCVKImageAnalysisRequestID

    guard let analyzerClass: AnyClass = TestWebKitAPI.getImageAnalyzerClass() else {
        fatalError()
    }

    let processRequest: ObjCImplementation = { _, _, _, completion in
        Task.immediate {
            if let delay {
                try? await Task.sleep(for: delay)
            }

            switch response {
            case .success(let result):
                completion(TestVKImageAnalysis(result), nil)

            case .failure(let error):
                completion(nil, error as NSError)
            }
        }

        return 0
    }

    await withSwizzledObjectiveCInstanceMethod(
        replacing: analyzerClass,
        name: NSSelectorFromString("processRequest:progressHandler:completionHandler:"),
        with: processRequest,
        perform: body
    )
}

/// Runs `body` with `-[VKCImageAnalyzer processRequest:progressHandler:completionHandler:]` swizzled
/// to  return `analysis` instead of dispatching to Vision.
///
/// The original implementation is restored when `body` returns (or throws).
///
/// - Parameters:
///   - response: The response to provide in place of the Vision framework's normal response.
///   - delay: An option duration which, if specified, delays the response from being provided by the specified duration.
///   - body: The code to execute with the replaced implementation.
public nonisolated(nonsending) func withMockedImageAnalyzer(
    response: Swift.Result<ImageAnalysisResult, Never>,
    after delay: Duration? = nil,
    perform body: () async -> sending Void
) async {
    await withMockedImageAnalyzer(response: response, failureType: Never.self, after: delay, perform: body)
}

// The following classes stand in for VKC's result types. WebKit reads them purely via
// Objective-C message sends (VKQuad / VKWKTextInfo / VKWKLineInfo / VKImageAnalysis selectors), so
// matching the selectors is sufficient; no inheritance from the real (opaque) VK types is required.

@objc(TestVKQuad)
private final class TestVKQuad: NSObject {
    @objc
    let topLeft: CGPoint
    @objc
    let topRight: CGPoint
    @objc
    let bottomLeft: CGPoint
    @objc
    let bottomRight: CGPoint

    init(_ quad: ImageAnalysisResult.Quad) {
        self.topLeft = quad.topLeft
        self.topRight = quad.topRight
        self.bottomLeft = quad.bottomLeft
        self.bottomRight = quad.bottomRight
    }
}

@objc(TestVKWKTextInfo)
private class TestVKWKTextInfo: NSObject {
    @objc
    let string: String
    @objc
    let quad: TestVKQuad

    convenience init(_ textInfo: ImageAnalysisResult.Line.TextInfo) {
        self.init(string: textInfo.text, quad: .init(textInfo.quad))
    }

    init(string: String, quad: TestVKQuad) {
        self.string = string
        self.quad = quad
    }
}

@objc(TestVKWKLineInfo)
private final class TestVKWKLineInfo: TestVKWKTextInfo {
    @objc
    let children: [TestVKWKTextInfo]

    init(_ lineInfo: ImageAnalysisResult.Line) {
        self.children = lineInfo.children.map(TestVKWKTextInfo.init)
        super.init(string: lineInfo.text, quad: .init(lineInfo.quad))
    }
}

@objc(TestVKImageAnalysis)
private final class TestVKImageAnalysis: NSObject {
    @objc
    let allLines: [TestVKWKLineInfo]

    init(_ analysis: ImageAnalysisResult) {
        self.allLines = analysis.lines.map(TestVKWKLineInfo.init)
        super.init()
    }

    init(lines: [TestVKWKLineInfo]) {
        allLines = lines
    }

    @objc(hasResultsForAnalysisTypes:)
    func hasResults(forAnalysisTypes types: UInt) -> Bool {
        !allLines.isEmpty
    }
}

#endif // ENABLE_SWIFTUI && ENABLE_CXX_INTEROP
