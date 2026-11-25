/*
 * Copyright (C) 2007-2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "DragData.h"

#if ENABLE(DRAG_SUPPORT)
#import "DeprecatedGlobalSettings.h"
#import "LegacyNSPasteboardTypes.h"
#import "MIMETypeRegistry.h"
#import "NotImplemented.h"
#import "PagePasteboardContext.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformPasteboard.h"
#import "PlatformStrategies.h"
#import "WebCoreNSURLExtras.h"
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <wtf/cocoa/NSURLExtras.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

static inline RefPtr<StringImpl> rtfPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypeRTF.identifier);
#else
    return StringImpl::create(legacyRTFPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> rtfdPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypeFlatRTFD.identifier);
#else
    return StringImpl::create(legacyRTFDPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> stringPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypeText.identifier);
#else
    return StringImpl::create(legacyStringPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> urlPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypeURL.identifier);
#else
    return StringImpl::create(legacyURLPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> htmlPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypeHTML.identifier);
#else
    return StringImpl::create(legacyHTMLPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> colorPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(protect(UIColorPboardType).get());
#else
    return StringImpl::create(legacyColorPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> pdfPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypePDF.identifier);
#else
    return StringImpl::create(legacyPDFPasteboardTypeSingleton());
#endif
}

static inline RefPtr<StringImpl> tiffPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return StringImpl::create(UTTypeTIFF.identifier);
#else
    return StringImpl::create(legacyTIFFPasteboardTypeSingleton());
#endif
}

DragData::DragData(DragDataRef data, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation> sourceOperationMask, OptionSet<DragApplicationFlags> flags, OptionSet<DragDestinationAction> destinationActionMask, std::optional<PageIdentifier> pageID)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_platformDragData(data)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_dragDestinationActionMask(destinationActionMask)
    , m_pageID(pageID)
#if PLATFORM(MAC)
    , m_pasteboardName(StringImpl::create([[data draggingPasteboard] name]))
#else
    , m_pasteboardName(Pasteboard::nameOfDragPasteboard())
#endif
{
}

DragData::DragData(StringImpl* dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation> sourceOperationMask, OptionSet<DragApplicationFlags> flags, OptionSet<DragDestinationAction> destinationActionMask, std::optional<PageIdentifier> pageID)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_dragDestinationActionMask(destinationActionMask)
    , m_pageID(pageID)
    , m_pasteboardName(dragStorageName)
{
}

DragData::DragData(StringImpl* dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, const Vector<RefPtr<StringImpl>>& fileNames, Vector<RefPtr<StringImpl>>&& promisedFileMIMETypes, OptionSet<DragOperation> sourceOperationMask, OptionSet<DragApplicationFlags> flags, OptionSet<DragDestinationAction> destinationActionMask, std::optional<PageIdentifier> pageID)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_fileNames(fileNames)
    , m_promisedFileMIMETypes(WTF::move(promisedFileMIMETypes))
    , m_dragDestinationActionMask(destinationActionMask)
    , m_pageID(pageID)
    , m_pasteboardName(dragStorageName)
{
}

bool DragData::containsURLTypeIdentifier() const
{
    Vector<RefPtr<StringImpl>> types;
    auto context = createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    return types.containsIf([&](auto& item) {
        return equal(item, urlPasteboardType());
    });
}
    
bool DragData::canSmartReplace() const
{
    return Pasteboard(createPasteboardContext(), m_pasteboardName).canSmartReplace();
}

bool DragData::shouldMatchStyleOnDrop() const
{
#if ENABLE(MULTI_REPRESENTATION_HEIC)
    Vector<RefPtr<StringImpl>> types;
    auto context = createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    return types.containsIf([&](auto& item) {
        return equal(item, "com.apple.sticker"_s);
    });
#else
    return false;
#endif
}

bool DragData::containsColor() const
{
    Vector<RefPtr<StringImpl>> types;
    auto context = createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    return types.containsIf([&](auto& item) {
        return equal(item, colorPasteboardType());
    });
}

bool DragData::containsFiles() const
{
    return !m_disallowFileAccess && numberOfFiles();
}

unsigned DragData::numberOfFiles() const
{
    if (m_disallowFileAccess)
        return 0;
    auto context = createPasteboardContext();
    return platformStrategies()->pasteboardStrategy()->getNumberOfFiles(m_pasteboardName, context.get());
}

Vector<RefPtr<StringImpl>> DragData::asFilenames() const
{
    if (m_disallowFileAccess)
        return { };
    auto context = createPasteboardContext();
#if PLATFORM(MAC)
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); }))
        return fileNames();

    Vector<RefPtr<StringImpl>> results;
    platformStrategies()->pasteboardStrategy()->getPathnamesForType(results, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get(), m_pasteboardName, context.get());
    return results;
#else
    return fileNames();
#endif
}

bool DragData::containsPlainText() const
{
    auto context = createPasteboardContext();
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());

    return types.containsIf([&](auto& item) { return equal(item, stringPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, rtfdPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, rtfPasteboardType()); })
#if PLATFORM(MAC)
        || types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()); })
#endif
        || platformStrategies()->pasteboardStrategy()->containsStringSafeForDOMToReadForType(urlPasteboardType(), m_pasteboardName, context.get());
}

RefPtr<StringImpl> DragData::asPlainText() const
{
    Pasteboard pasteboard(createPasteboardContext(), m_pasteboardName);
    PasteboardPlainText text;
    pasteboard.read(text);
    RefPtr<StringImpl> string = text.text;

    // FIXME: It's not clear this is 100% correct since we know -[NSURL URLWithString:] does not handle
    // all the same cases we handle well in the URL code for creating an NSURL.
    if (text.isURL)
        return StringImpl::create(protect(WTF::userVisibleString([NSURL URLWithString:createNSString(string).get()])).get());

    // FIXME: WTF should offer a non-Mac-specific way to convert string to precomposed form so we can do it for all platforms.
    return StringImpl::create([createNSString(string) precomposedStringWithCanonicalMapping]);
}

Color DragData::asColor() const
{
    auto context = createPasteboardContext();
    return platformStrategies()->pasteboardStrategy()->color(m_pasteboardName, context.get());
}

bool DragData::containsCompatibleContent(DraggingPurpose purpose) const
{
    if (purpose == DraggingPurpose::ForFileUpload)
        return containsFiles();

    if (purpose == DraggingPurpose::ForColorControl)
        return containsColor();

    if (purpose == DraggingPurpose::ForEditing && DeprecatedGlobalSettings::attachmentElementEnabled() && containsFiles())
        return true;

    auto context = createPasteboardContext();
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    return types.containsIf([&](auto& item) { return equal(item, webArchivePboardTypeString()); })
        || types.containsIf([&](auto& item) { return equal(item, htmlPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, StringImpl::create(UTTypeWebArchive.identifier).get()); })
#if PLATFORM(MAC)
        || (!m_disallowFileAccess && types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilenamesPasteboardTypeSingleton()).get()); }))
        || (!m_disallowFileAccess && types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); }))
#endif
        || types.containsIf([&](auto& item) { return equal(item, tiffPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, pdfPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, urlPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, rtfdPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, rtfPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, StringImpl::create(UTTypeUTF8PlainText.identifier).get()); })
        || types.containsIf([&](auto& item) { return equal(item, stringPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, colorPasteboardType()); })
        || types.containsIf([&](auto& item) { return equal(item, StringImpl::create(UTTypeJPEG.identifier).get()); })
        || types.containsIf([&](auto& item) {
            return equal(item, StringImpl::create(UTTypePNG.identifier).get());
        });
}

bool DragData::containsPromise() const
{
    return !m_disallowFileAccess && !m_promisedFileMIMETypes.isEmpty();
}

bool DragData::containsURL(FilenameConversionPolicy) const
{
    auto context = createPasteboardContext();
    if (platformStrategies()->pasteboardStrategy()->containsURLStringSuitableForLoading(m_pasteboardName, context.get()))
        return true;

#if PLATFORM(MAC)
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); }) && fileNames().size() == 1)
        return !![NSURL fileURLWithPath:createNSString(fileNames().first()).get()];
#endif

    return false;
}

RefPtr<StringImpl> DragData::asURL(FilenameConversionPolicy, RefPtr<StringImpl>* title) const
{
    auto context = createPasteboardContext();
    // FIXME: Use filenamePolicy.

    RefPtr<StringImpl> urlTitle;
    auto urlString = platformStrategies()->pasteboardStrategy()->urlStringSuitableForLoading(m_pasteboardName, urlTitle, context.get());
    if (title)
        *title = urlTitle;

    if (!isEmpty(urlString))
        return urlString;

#if PLATFORM(MAC)
    Vector<RefPtr<StringImpl>> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    if (types.containsIf([&](auto& item) { return equal(item, StringImpl::create(legacyFilesPromisePasteboardTypeSingleton()).get()); }) && fileNames().size() == 1)
        return StringImpl::create([protect(URLByCanonicalizingURL([NSURL fileURLWithPath:createNSString(fileNames()[0]).get()])) absoluteString]);
#endif

    return { };
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
