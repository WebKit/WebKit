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

#if HAVE_MATERIAL_HOSTING

internal import WebKit_Internal

#if USE_APPLE_INTERNAL_SDK
#if canImport(UIKit)
@_weakLinked @_spi(Private) @_spi(ForUIKitOnly) internal import SwiftUI
#else
@_weakLinked @_spi(Private) @_spi(ForAppKitOnly) internal import SwiftUI
#endif
#else
internal import SwiftUI_SPI
#endif

#if canImport(UIKit)

private struct MaterialHostingContentViewWrapper: UIViewRepresentable {
    let contentView: UIView

    func makeUIView(context: Context) -> UIView {
        contentView
    }

    func updateUIView(_ uiView: UIView, context: Context) {
    }
}

#endif

protocol MaterialHostingProvider {
    associatedtype Source

    associatedtype Body: View

    @ViewBuilder
    static func view(for source: Source) -> Body
}

private struct LayerBackedMaterialHostingProvider: MaterialHostingProvider {
    static func view(for source: CALayer) -> some View {
        _CALayerView(type: CALayer.self) { layer in
            layer.addSublayer(source)
        }
    }
}

#if canImport(UIKit)

private struct ViewBackedMaterialHostingProvider: MaterialHostingProvider {
    static func view(for source: UIView) -> some View {
        MaterialHostingContentViewWrapper(contentView: source)
    }
}

#endif

private struct MaterialHostingView<P: MaterialHostingProvider>: View {
    private let content: P.Source
    private let materialEffectType: WKHostedMaterialEffectType
    private let colorScheme: WKHostedMaterialColorScheme
    private let cornerRadius: CGFloat

    #if os(macOS)
    @State
    private var shouldIncreaseContrast = false

    @State
    private var shouldReduceTransparency = false
    #endif

    static func resolvedMaterialEffect(for type: WKHostedMaterialEffectType) -> Material? {
        switch type {
        case .none:
            return nil
        case .glass:
            return ._glass(.regular)
        case .clearGlass:
            return ._glass(.clear)
        case .subduedGlass:
            return ._glass(.regular.forceSubdued())
        case .mediaControlsGlass:
            return ._glass(.regular.adaptive(false))
        case .subduedMediaControlsGlass:
            return ._glass(.avplayer.forceSubdued())
        @unknown default:
            return nil
        }
    }

    init(
        content: P.Source,
        materialEffectType: WKHostedMaterialEffectType = .none,
        colorScheme: WKHostedMaterialColorScheme = .light,
        cornerRadius: CGFloat = 0
    ) {
        self.content = content
        self.materialEffectType = materialEffectType
        self.colorScheme = colorScheme
        self.cornerRadius = cornerRadius
    }

    #if os(macOS)
    private func updateAccessibilityState() {
        shouldIncreaseContrast =
            NSWorkspace.shared
            .accessibilityDisplayShouldIncreaseContrast
        shouldReduceTransparency = NSWorkspace.shared.accessibilityDisplayShouldReduceTransparency
    }
    #endif

    var body: some View {
        let view = P.view(for: content)

        if let effect = MaterialHostingView<P>.resolvedMaterialEffect(for: materialEffectType) {
            AnyView(view.materialEffect(effect, in: .rect(cornerRadius: cornerRadius)))
                .environment(\.colorScheme, colorScheme == .light ? .light : .dark)
                #if os(macOS)
            .environment(\._accessibilityReduceTransparency, shouldReduceTransparency)
            .environment(\._colorSchemeContrast, shouldIncreaseContrast ? .increased : .standard)
            .onAppear {
                updateAccessibilityState()
            }
            .onReceive(NSWorkspace.shared.notificationCenter.publisher(for: NSWorkspace.accessibilityDisplayOptionsDidChangeNotification)) {
                _ in
                updateAccessibilityState()
            }
                #endif
        } else {
            view
        }
    }
}

extension CALayer {
    private static let materialHostingContentLayerKey = "_materialHostingContentLayerKey"

    fileprivate var materialHostingContentLayer: CALayer? {
        get {
            value(forKeyPath: Self.materialHostingContentLayerKey) as? CALayer
        }
        set {
            setValue(newValue, forKeyPath: Self.materialHostingContentLayerKey)
        }
    }
}

@objc
@implementation
extension WKMaterialHostingSupport {
    class func isMaterialHostingAvailable() -> Bool {
        guard #_hasSymbol(Material.self) else {
            return false
        }

        return true
    }

    class func hostingLayer() -> CALayer {
        let contentLayer = CALayer()

        let hostingLayer = CAHostingLayer(rootView: MaterialHostingView<LayerBackedMaterialHostingProvider>(content: contentLayer))

        hostingLayer.materialHostingContentLayer = contentLayer

        return hostingLayer
    }

    class func updateHostingLayer(
        _ layer: CALayer,
        materialEffectType: WKHostedMaterialEffectType,
        colorScheme: WKHostedMaterialColorScheme,
        cornerRadius: CGFloat
    ) {
        guard let hostingLayer = layer as? CAHostingLayer<MaterialHostingView<LayerBackedMaterialHostingProvider>> else {
            assertionFailure("updateHostingLayer should only be called with a hosting layer.")
            return
        }

        guard let contentLayer = hostingLayer.materialHostingContentLayer else {
            assertionFailure("updateHostingLayer should only be called with a hosting layer with a content sublayer.")
            return
        }

        hostingLayer.rootView = MaterialHostingView<LayerBackedMaterialHostingProvider>(
            content: contentLayer,
            materialEffectType: materialEffectType,
            colorScheme: colorScheme,
            cornerRadius: cornerRadius
        )
    }

    class func contentLayer(forMaterialHostingLayer layer: CALayer) -> CALayer? {
        layer.materialHostingContentLayer
    }

    #if canImport(UIKit)

    class func hostingView(_ contentView: UIView) -> UIView {
        _UIHostingView(rootView: MaterialHostingView<ViewBackedMaterialHostingProvider>(content: contentView))
    }

    class func updateHostingView(
        _ view: UIView,
        contentView: UIView,
        materialEffectType: WKHostedMaterialEffectType,
        colorScheme: WKHostedMaterialColorScheme,
        cornerRadius: CGFloat
    ) {
        guard let hostingView = view as? _UIHostingView<MaterialHostingView<ViewBackedMaterialHostingProvider>> else {
            return
        }

        hostingView.rootView = MaterialHostingView<ViewBackedMaterialHostingProvider>(
            content: contentView,
            materialEffectType: materialEffectType,
            colorScheme: colorScheme,
            cornerRadius: cornerRadius
        )
    }

    #endif
}

#endif
