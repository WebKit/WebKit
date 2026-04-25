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

#if ENABLE_PDF_HUD

public import AppKit
public import Foundation
internal import WebKit_Internal
private import pal.spi.mac.NSImageSPI
private import wtf.SPI.darwin.OSVariantSPI

private let barVerticalOffset: CGFloat = 40
private let barHorizontalPadding: CGFloat = 16
private let barVerticalPadding: CGFloat = 8
private let barSpacing: CGFloat = 6
private let groupSpacing: CGFloat = 0
private let symbolPointSize: CGFloat = 26
private let separatorHeight: CGFloat = 28
private let buttonHorizontalPadding: CGFloat = 8
private let buttonVerticalPadding: CGFloat = 4
private let legacyBarCornerRadius: CGFloat = 12
private let legacyBarAlpha: CGFloat = 0.75
#if HAVE_LIQUID_GLASS
private let visibleAlpha: CGFloat = 1.0
#else
private let visibleAlpha: CGFloat = legacyBarAlpha
#endif
private let fadeInDuration: TimeInterval = 0.25
private let fadeOutDuration: TimeInterval = 0.5
private let autoHideDelay: TimeInterval = 3.0
private let hoverInset: CGFloat = -16.0

// FIXME: (rdar://164559261) understand/document/remove unsafety
private func isInRecoveryOS() -> Bool {
    unsafe os_variant_is_basesystem("WebKit")
}

@objc
@implementation
extension WKPDFHUDView {
    var pluginIdentifier: UInt64
    var frameIdentifier: UInt64
    weak var webView: WKWebView?
    @nonobjc
    final private var barView: NSView
    @nonobjc
    final private var stackView: NSStackView
    @nonobjc
    final private var zoomOutButton: NSButton
    @nonobjc
    final private var zoomInButton: NSButton
    @nonobjc
    final private var separatorView: NSView
    @nonobjc
    final private var openInPreviewButton: NSButton
    @nonobjc
    final private var saveButton: NSButton
    @nonobjc
    final private var isBarVisible: Bool = true
    @nonobjc
    final private var mouseMovedToHUD: Bool = false
    @nonobjc
    final private var initialHideTimerFired: Bool = false
    @nonobjc
    final private var hideTimerTask: Task<Void, Never>? = nil

    // The initializer is required to be `public`, but the class itself is `internal` so this is not API.
    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @objc(initWithFrame:pluginIdentifier:frameIdentifier:webView:)
    init(
        frame: NSRect,
        pluginIdentifier: UInt64,
        frameIdentifier: UInt64,
        webView: WKWebView?
    ) {
        self.pluginIdentifier = pluginIdentifier
        self.frameIdentifier = frameIdentifier
        self.webView = webView

        self.zoomOutButton = Self.makeButton(symbolName: "minus.magnifyingglass", accessibilityLabel: "Zoom Out")
        self.zoomInButton = Self.makeButton(symbolName: "plus.magnifyingglass", accessibilityLabel: "Zoom In")
        self.separatorView = Self.makeSeparator()
        self.openInPreviewButton = Self.makeButton(symbolName: "preview", accessibilityLabel: "Open in Preview", isPrivateSymbol: true)
        self.saveButton = Self.makeButton(symbolName: "arrow.down.circle", accessibilityLabel: "Save PDF")

        let zoomGroup = NSStackView(views: [zoomOutButton, zoomInButton])
        zoomGroup.orientation = .horizontal
        zoomGroup.spacing = groupSpacing
        zoomGroup.alignment = .centerY

        self.stackView = NSStackView(views: [zoomGroup])
        if !isInRecoveryOS() {
            stackView.addArrangedSubview(separatorView)
            let fileGroup = NSStackView(views: [openInPreviewButton, saveButton])
            fileGroup.orientation = .horizontal
            fileGroup.spacing = groupSpacing
            fileGroup.alignment = .centerY
            stackView.addArrangedSubview(fileGroup)
        }
        stackView.orientation = .horizontal
        stackView.spacing = barSpacing
        stackView.alignment = .centerY
        stackView.edgeInsets = NSEdgeInsets(
            top: barVerticalPadding,
            left: barHorizontalPadding,
            bottom: barVerticalPadding,
            right: barHorizontalPadding
        )

        self.barView = Self.makeBarView()
        stackView.translatesAutoresizingMaskIntoConstraints = false
        #if HAVE_LIQUID_GLASS
        (barView as? NSGlassEffectView)?.contentView = stackView
        #else
        barView.addSubview(stackView)
        #endif

        super.init(frame: frame)

        addSubview(barView)
        barView.centerXAnchor.constraint(equalTo: centerXAnchor).isActive = true
        barView.bottomAnchor.constraint(equalTo: bottomAnchor, constant: -barVerticalOffset).isActive = true
        stackView.topAnchor.constraint(equalTo: barView.topAnchor).isActive = true
        stackView.bottomAnchor.constraint(equalTo: barView.bottomAnchor).isActive = true
        stackView.leadingAnchor.constraint(equalTo: barView.leadingAnchor).isActive = true
        stackView.trailingAnchor.constraint(equalTo: barView.trailingAnchor).isActive = true

        zoomOutButton.target = self
        zoomOutButton.action = #selector(zoomOutAction)
        zoomInButton.target = self
        zoomInButton.action = #selector(zoomInAction)
        openInPreviewButton.target = self
        openInPreviewButton.action = #selector(openInPreviewAction)
        saveButton.target = self
        saveButton.action = #selector(saveAction)

        resetHideTimer()
    }

    // swift-format-ignore: AllPublicDeclarationsHaveDocumentation
    @objc
    @available(*, unavailable)
    public required dynamic init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    @objc
    func show() {
        setBarVisible(true)
        resetHideTimer()
    }

    // For testing only.
    // swift-format-ignore: NoLeadingUnderscores
    @objc
    private func _performAction(forControl controlName: String) {
        switch controlName {
        case "minus.magnifyingglass":
            zoomOutButton.performClick(nil)
        case "plus.magnifyingglass":
            zoomInButton.performClick(nil)
        case "preview":
            openInPreviewButton.performClick(nil)
        case "arrow.down.circle":
            saveButton.performClick(nil)
        default:
            break
        }
    }

    final override func layout() {
        super.layout()
        #if HAVE_LIQUID_GLASS
        (barView as? NSGlassEffectView)?.cornerRadius = barView.bounds.height / 2
        #endif
    }

    // FIXME: (rdar://164559261) understand/document/remove unsafety
    final override func hitTest(_ point: NSPoint) -> NSView? {
        let pointInSelf = convert(point, from: unsafe superview)

        guard isBarVisible else { return webView }

        let pointInBar = barView.convert(pointInSelf, from: self)
        if barView.bounds.contains(pointInBar) {
            for button in [zoomOutButton, zoomInButton, openInPreviewButton, saveButton] {
                let pointInButton = button.convert(pointInSelf, from: self)
                if button.bounds.contains(pointInButton) && !button.isHidden {
                    return button
                }
            }
            return barView
        }

        return webView
    }

    final override func mouseMoved(with event: NSEvent) {
        let barFrame = barView.frame
        let expandedRect = convert(barFrame.insetBy(dx: hoverInset, dy: hoverInset), to: nil)

        if expandedRect.contains(event.locationInWindow) {
            setBarVisible(true)
            mouseMovedToHUD = true
        } else {
            mouseMovedToHUD = false
            if initialHideTimerFired {
                setBarVisible(false)
            }
        }
    }

    @objc
    private func zoomOutAction() {
        guard isBarVisible else { return }
        performPDFZoomOut()
        resetHideTimer()
    }

    @objc
    private func zoomInAction() {
        guard isBarVisible else { return }
        performPDFZoomIn()
        resetHideTimer()
    }

    @objc
    private func openInPreviewAction() {
        guard isBarVisible else { return }
        performPDFOpenWithPreview()
        resetHideTimer()
    }

    @objc
    private func saveAction() {
        guard isBarVisible else { return }
        performPDFSaveToPDF()
        resetHideTimer()
    }

    @nonobjc
    final private func setBarVisible(_ visible: Bool) {
        guard isBarVisible != visible else { return }
        isBarVisible = visible
        let duration = visible ? fadeInDuration : fadeOutDuration
        let targetAlpha = visible ? visibleAlpha : 0.0
        if visible {
            barView.isHidden = false
        }
        Task { [weak self] in
            await withCheckedContinuation { (continuation: CheckedContinuation<Void, Never>) in
                NSAnimationContext.runAnimationGroup { context in
                    context.duration = duration
                    self?.barView.animator().alphaValue = targetAlpha
                } completionHandler: {
                    continuation.resume()
                }
            }
            guard let self, !visible, !self.isBarVisible else { return }
            self.barView.isHidden = true
        }
    }

    @nonobjc
    final private func resetHideTimer() {
        hideTimerTask?.cancel()
        hideTimerTask = Task { [weak self] in
            try? await Task.sleep(for: .seconds(autoHideDelay))
            guard !Task.isCancelled else { return }
            self?.hideTimerFired()
        }
    }

    @nonobjc
    final private func hideTimerFired() {
        initialHideTimerFired = true
        if !mouseMovedToHUD {
            setBarVisible(false)
        }
    }

    @nonobjc
    private static func makeBarView() -> NSView {
        #if HAVE_LIQUID_GLASS
        let view = NSGlassEffectView()
        #if ENABLE_NSGLASSEFFECTVIEW_USES_DEFAULT_CONFIGURATION
        view.usesDefaultConfiguration = true
        #endif
        view._adaptiveAppearance = .on
        #else
        let view = NSView()
        view.wantsLayer = true
        view.layer?.backgroundColor = NSColor.black.cgColor
        view.layer?.cornerRadius = legacyBarCornerRadius
        view.layer?.cornerCurve = .circular
        view.alphaValue = legacyBarAlpha
        view.appearance = NSAppearance(named: .darkAqua)
        #endif
        view.translatesAutoresizingMaskIntoConstraints = false
        return view
    }

    @nonobjc
    private static func makeButton(symbolName: String, accessibilityLabel: String, isPrivateSymbol: Bool = false) -> NSButton {
        let image: NSImage?
        if isPrivateSymbol {
            image = NSImage(privateSystemSymbolName: symbolName, accessibilityDescription: accessibilityLabel)
        } else {
            image = NSImage(systemSymbolName: symbolName, accessibilityDescription: accessibilityLabel)
        }

        guard let image else {
            return NSButton()
        }

        let config = NSImage.SymbolConfiguration(pointSize: symbolPointSize, weight: .medium, scale: .large)
        let configuredImage = image.withSymbolConfiguration(config) ?? image

        let button = NSButton()
        button.title = ""
        button.image = configuredImage
        button.setAccessibilityLabel(accessibilityLabel)
        button.translatesAutoresizingMaskIntoConstraints = false

        let imageSize = configuredImage.size
        button.widthAnchor.constraint(equalToConstant: imageSize.width + buttonHorizontalPadding).isActive = true
        button.heightAnchor.constraint(equalToConstant: imageSize.height + buttonVerticalPadding).isActive = true

        if let cell = button.cell as? NSButtonCell {
            cell.bezelStyle = .flexiblePush
            cell.imagePosition = .imageOverlaps
            cell.imageScaling = .scaleProportionallyDown
            cell.alignment = .center
            cell.isBordered = true
            cell.showsBorderOnlyWhileMouseInside = true
            cell.highlightsBy = [.pushInCellMask, .changeBackgroundCellMask, .changeGrayCellMask]
        }

        return button
    }

    @nonobjc
    private static func makeSeparator() -> NSView {
        #if HAVE_LIQUID_GLASS
        let spacer = NSView()
        spacer.translatesAutoresizingMaskIntoConstraints = false
        spacer.widthAnchor.constraint(equalToConstant: barSpacing).isActive = true
        return spacer
        #else
        let separator = NSView()
        separator.wantsLayer = true
        separator.layer?.backgroundColor = NSColor.lightGray.cgColor
        separator.translatesAutoresizingMaskIntoConstraints = false
        separator.widthAnchor.constraint(equalToConstant: 1.5).isActive = true
        separator.heightAnchor.constraint(equalToConstant: separatorHeight).isActive = true
        return separator
        #endif
    }
}

#endif // ENABLE_PDF_HUD
