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

public import CoreGraphics
public import Foundation
private import TestWebKitAPILibrary.Helpers.cocoa.TestCocoaImageUtilities

#if WTF_PLATFORM_IOS_FAMILY
public import UIKit
#else
public import AppKit
private import TestWebKitAPILibrary.Helpers.mac.AppKitSPI
#endif

/// A system appearance to draw with.
public enum Appearance: Sendable {
    /// The standard light appearance.
    case light

    /// The standard dark appearance.
    case dark
}

/// Runs a closure with the given appearance current for drawing.
///
/// - Parameters:
///   - appearance: The appearance to make current.
///   - body: The work to perform, typically sampling an appearance-sensitive image.
/// - Returns: Whatever `body` returns.
/// - Throws: Whatever `body` throws.
@MainActor
public func withAppearance<T, E: Error>(_ appearance: Appearance, _ body: () throws(E) -> T) throws(E) -> T {
    var result: Result<T, E>?

    TestCocoaImageUtilities.perform(darkAppearance: appearance == .dark) {
        result = Result { () throws(E) in try body() }
    }

    // swift-format-ignore: NeverForceUnwrap
    return try result!.get()
}

/// PNG data for an image of the given size, filled with a single color.
///
/// - Parameters:
///   - size: The size of the image, in points.
///   - color: The color to fill the image with.
/// - Returns: The encoded PNG data.
@MainActor
public func makePNGData(size: CGSize, color: CocoaColor) -> Data {
    TestCocoaImageUtilities.pngData(size: size, color: color)
}

/// The color of a single pixel of an image.
///
/// - Parameters:
///   - image: The image to sample.
///   - point: The pixel to sample, in points from the top-left corner of the image.
/// - Returns: The color of that pixel, or `nil` if the image could not be sampled or the point
///   lies outside of it.
@MainActor
public func pixelColor(of image: CocoaImage, at point: CGPoint = .zero) -> CocoaColor? {
    TestCocoaImageUtilities.pixelColor(of: image, at: point)
}

/// Whether two colors match on every sRGB component, within a tolerance.
///
/// - Parameters:
///   - color: The first color.
///   - otherColor: The second color.
///   - tolerance: The largest per-component difference that still counts as a match.
/// - Returns: Whether the colors match.
@MainActor
public func compareColors(_ color: CocoaColor?, _ otherColor: CocoaColor?, tolerance: CGFloat = 0.01) -> Bool {
    TestCocoaImageUtilities.compareColors(color, otherColor, tolerance: tolerance)
}

extension CocoaImage {
    /// Whether this image was created from a system symbol.
    public var isSymbol: Bool {
        #if WTF_PLATFORM_IOS_FAMILY
        isSymbolImage
        #else
        _isSymbolImage
        #endif
    }
}

@MainActor
private func makeBitmap(size: CGSize, draw: () -> Void) -> CGImage? {
    guard
        let width = Int(exactly: size.width.rounded(.up)), width > 0,
        let height = Int(exactly: size.height.rounded(.up)), height > 0,
        let colorSpace = CGColorSpace(name: CGColorSpace.sRGB)
    else {
        return nil
    }

    #if HAVE_CGCONTEXT_INIT_WITH_BITMAP_INFO_AND_NULLABLE_COLORSPACE
    let context = unsafe CGContext(
        data: nil,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: 0,
        space: colorSpace,
        bitmapInfo: CGBitmapInfo(alpha: .premultipliedLast, byteOrder: .order32Big)
    )
    #else
    let context = unsafe CGContext(
        data: nil,
        width: width,
        height: height,
        bitsPerComponent: 8,
        bytesPerRow: 0,
        space: colorSpace,
        bitmapInfo: CGBitmapInfo(rawValue: CGImageAlphaInfo.premultipliedLast.rawValue).union(.byteOrder32Big).rawValue
    )
    #endif

    guard let context else {
        return nil
    }

    context.translateBy(x: 0, y: CGFloat(height))
    context.scaleBy(x: 1, y: -1)

    #if WTF_PLATFORM_IOS_FAMILY
    UIGraphicsPushContext(context)
    defer { UIGraphicsPopContext() }
    #else
    NSGraphicsContext.saveGraphicsState()
    defer { NSGraphicsContext.restoreGraphicsState() }

    NSGraphicsContext.current = NSGraphicsContext(cgContext: context, flipped: true)
    #endif

    draw()

    return context.makeImage()
}

private func sampleColor(of bitmap: CGImage, atX x: Int, y: Int) -> CocoaColor? {
    guard
        (0..<bitmap.width).contains(x), (0..<bitmap.height).contains(y),
        let pixels = bitmap.dataProvider?.data
    else {
        return nil
    }

    let data = pixels as Data
    let offset = data.startIndex + y * bitmap.bytesPerRow + x * 4

    guard data.indices.contains(offset + 3) else {
        return nil
    }

    let alpha = CGFloat(data[offset + 3]) / 255

    guard alpha > 0 else {
        return .clear
    }

    // The bitmap is premultiplied, so divide the alpha back out of each color component.
    func component(_ value: UInt8) -> CGFloat {
        min(CGFloat(value) / 255 / alpha, 1)
    }

    return CocoaColor(
        red: component(data[offset]),
        green: component(data[offset + 1]),
        blue: component(data[offset + 2]),
        alpha: alpha
    )
}

private func sRGBComponents(of color: CocoaColor) -> [CGFloat]? {
    #if WTF_PLATFORM_IOS_FAMILY
    let cgColor = color.cgColor
    #else
    guard let cgColor = color.usingColorSpace(.sRGB)?.cgColor else {
        return nil
    }
    #endif

    guard
        let colorSpace = CGColorSpace(name: CGColorSpace.sRGB),
        let converted = cgColor.converted(to: colorSpace, intent: .defaultIntent, options: nil),
        let components = converted.components, components.count == 4
    else {
        return nil
    }

    return components
}

@objc
@implementation
extension TestCocoaImageUtilities {
    @objc(performWithDarkAppearance:block:)
    class func perform(darkAppearance: Bool, block: () -> Void) {
        #if WTF_PLATFORM_IOS_FAMILY
        let style: UIUserInterfaceStyle = darkAppearance ? .dark : .light

        UITraitCollection(userInterfaceStyle: style).performAsCurrent(block)
        #else
        let name: NSAppearance.Name = darkAppearance ? .darkAqua : .aqua

        guard let appearance = NSAppearance(named: name) else {
            preconditionFailure("Could not create the \(name.rawValue) appearance.")
        }

        appearance.performAsCurrentDrawingAppearance(block)
        #endif
    }

    class func pngData(size: CGSize, color: CocoaColor) -> Data {
        #if WTF_PLATFORM_IOS_FAMILY
        let format = UIGraphicsImageRendererFormat.preferred()
        format.scale = 1
        format.opaque = false
        format.preferredRange = .standard

        return UIGraphicsImageRenderer(size: size, format: format)
            .pngData { context in
                color.setFill()
                context.fill(CGRect(origin: .zero, size: size))
            }
        #else
        let image = NSImage(size: size)

        image.lockFocus()
        color.setFill()
        NSRect(origin: .zero, size: size).fill()
        image.unlockFocus()

        guard let cgImage = unsafe image.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
            preconditionFailure("Could not get a bitmap for a \(size) image that was just drawn.")
        }

        let representation = NSBitmapImageRep(cgImage: cgImage)
        representation.size = size

        guard let data = representation.representation(using: .png, properties: [:]) else {
            preconditionFailure("Could not encode a \(size) image that was just drawn.")
        }

        return data
        #endif
    }

    @objc(pixelColorOfImage:atPoint:)
    class func pixelColor(of image: CocoaImage, at point: CGPoint) -> CocoaColor? {
        #if WTF_PLATFORM_IOS_FAMILY
        let image = image.imageAsset?.image(with: .current) ?? image
        #endif

        guard
            let x = Int(exactly: point.x.rounded(.down)),
            let y = Int(exactly: point.y.rounded(.down)),
            let bitmap = makeBitmap(
                size: image.size,
                draw: {
                    #if WTF_PLATFORM_IOS_FAMILY
                    image.draw(at: .zero)
                    #else
                    image.draw(in: CGRect(origin: .zero, size: image.size))
                    #endif
                }
            )
        else {
            return nil
        }

        return sampleColor(of: bitmap, atX: x, y: y)
    }

    @objc(compareColor:toColor:tolerance:)
    class func compareColors(_ color: CocoaColor?, _ otherColor: CocoaColor?, tolerance: CGFloat) -> Bool {
        if color === otherColor {
            return true
        }

        guard let color, let otherColor else {
            return false
        }

        if color.isEqual(otherColor) {
            return true
        }

        guard
            let components = sRGBComponents(of: color),
            let otherComponents = sRGBComponents(of: otherColor)
        else {
            return false
        }

        return zip(components, otherComponents).allSatisfy { abs($0.0 - $0.1) < tolerance }
    }
}
