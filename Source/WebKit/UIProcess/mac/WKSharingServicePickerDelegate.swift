// Copyright (C) 2015-2025 Apple Inc. All rights reserved.
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

#if ENABLE_SERVICE_CONTROLS

import AppKit
import ImageIO
import WebCore_Private
import WebKit_Internal

// FIXME: We probably need to hang on the picker itself until the context menu operation is done, and this object will probably do that.
@objc
@implementation
extension WKSharingServicePickerDelegate {
    @nonobjc
    private static let sharedInstance = WKSharingServicePickerDelegate()

    @objc(sharedSharingServicePickerDelegate)
    class var shared: WKSharingServicePickerDelegate {
        sharedInstance
    }

    @nonobjc
    private var menuProxy = WebKit.WeakPtrWebContextMenuProxyMac()

    var picker: NSSharingServicePicker?

    var filtersEditingServices = false

    var handlesEditingReplacement = false

    var sourceFrame: NSRect = .zero

    var attachmentID: String?

    override init() {
        super.init()
    }

    func setMenuProxy(_ menuProxy: WebKit.WebContextMenuProxyMac) {
        #if compiler(>=6.5)
        self.menuProxy = WebKit.WeakPtrWebContextMenuProxyMac(menuProxy)
        #else
        self.menuProxy = WebKit.CxxInteropSupport.makeWeakPtr(menuProxy)
        #endif
    }

    func removeBackground() {
        menuProxy.get()?.removeBackgroundFromControlledImage()
    }

    func sharingServicePicker(
        _ sharingServicePicker: NSSharingServicePicker,
        sharingServicesForItems items: [Any],
        mask: NSSharingServiceMask,
        proposedSharingServices proposedServices: [NSSharingService]
    ) -> [NSSharingService] {
        guard filtersEditingServices else {
            return proposedServices
        }

        return proposedServices.filter { $0.type != .editor }
    }

    // MARK: NSSharingServicePickerDelegate

    @objc(sharingServicePicker:delegateForSharingService:)
    func sharingServicePicker(
        _ sharingServicePicker: NSSharingServicePicker,
        delegateFor sharingService: NSSharingService
    ) -> (any NSSharingServiceDelegate)? {
        self
    }

    // MARK: NSSharingServiceDelegate

    @objc(sharingService:sourceFrameOnScreenForShareItem:)
    func sharingService(_ sharingService: NSSharingService, sourceFrameOnScreenForShareItem item: Any) -> NSRect {
        sourceFrame
    }

    @objc(sharingService:willShareItems:)
    func sharingService(_ sharingService: NSSharingService, willShareItems items: [Any]) {
        menuProxy.get()?.clearServicesMenu()
    }

    @objc(sharingService:didShareItems:)
    func sharingService(_ sharingService: NSSharingService, didShareItems items: [Any]) {
        // We only care about what item was shared if we were interested in editor services
        // (i.e., if we plan on replacing the selection or controlled image with the returned item)
        guard handlesEditingReplacement, let item = items.first else {
            return
        }

        let types: [String]
        let data: Data

        switch item {
        case let attributedString as NSAttributedString:
            let range = NSRange(location: 0, length: attributedString.length)
            data = attributedString.rtfd(from: range, documentAttributes: [:]) ?? Data()
            types = [NSPasteboard.PasteboardType.rtfd.rawValue, WebCore.legacyRTFDPasteboardTypeSingleton()]

        case let imageData as Data:
            guard let source = CGImageSourceCreateWithData(imageData as CFData, nil),
                CGImageSourceCreateImageAtIndex(source, 0, nil) != nil
            else {
                return
            }

            data = imageData
            types = [NSPasteboard.PasteboardType.tiff.rawValue]

        case let itemProvider as NSItemProvider:
            updateAttachment(with: itemProvider)
            return

        default:
            WebKit.LogChannel.ContextMenu().debugLog.error("sharingService:didShareItems: - Unknown item type returned")
            return
        }

        // FIXME: We should adopt replaceSelectionWithAttributedString instead of bouncing through the (fake) pasteboard.
        guard let page = menuProxy.get()?.page() else {
            return
        }

        var pasteboardTypes = WebKit.WebPageProxy.PasteboardTypeNames()
        for type in types {
            pasteboardTypes.append(consuming: WTF.String(type))
        }

        // FIXME: Pass `data.span` directly once rdar://193716908 is fixed. Swift rejects a span of a `let` that's
        // initialized later, as `data` is above.
        let pasteboardData = data
        page.replaceSelectionWithPasteboardData(pasteboardTypes, pasteboardData.span)
    }

    @objc(sharingService:sourceWindowForShareItems:sharingContentScope:)
    func sharingService(
        _ sharingService: NSSharingService,
        sourceWindowForShareItems items: [Any],
        sharingContentScope: UnsafeMutablePointer<NSSharingService.SharingContentScope>
    ) -> NSWindow? {
        menuProxy.get()?.window()
    }

    @nonobjc
    private final func updateAttachment(with itemProvider: NSItemProvider) {
        guard let itemUTI = itemProvider.registeredTypeIdentifiers.first else {
            return
        }

        // Another services menu can replace the menu proxy and attachment before the data loads,
        // so hold on to the ones this item was shared from.
        #if compiler(>=6.5)
        let page = WebKit.WeakPtrWebPageProxy(menuProxy.get()?.page())
        #else
        let page = WebKit.CxxInteropSupport.makeWeakPtr(menuProxy.get()?.page())
        #endif
        let attachmentID = self.attachmentID

        Task.immediateOnMainActor {
            let data = await withCheckedContinuation { continuation in
                _ = itemProvider.loadDataRepresentation(forTypeIdentifier: itemUTI) { data, error in
                    continuation.resume(returning: error == nil ? data : nil)
                }
            }

            guard let data, let page = page.get(), let attachmentID,
                let attachment = page.attachmentForIdentifier(WTF.String(attachmentID)).get()
            else {
                return
            }

            (attachment.wrapper() as? _WKAttachment)?.setData(data, newContentType: itemUTI)
            page.didInvalidateDataForAttachment(attachment)
        }
    }
}

#endif // ENABLE_SERVICE_CONTROLS
