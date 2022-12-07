/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
#import <wtf/cocoa/NSURLExtras.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

static inline String rtfPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypeRTF);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyRTFPasteboardType());
#endif
}

static inline String rtfdPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypeFlatRTFD);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyRTFDPasteboardType());
#endif
}

static inline String stringPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypeText);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyStringPasteboardType());
#endif
}

static inline String urlPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypeURL);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyURLPasteboardType());
#endif
}

static inline String htmlPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypeHTML);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyHTMLPasteboardType());
#endif
}

static inline String colorPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String { UIColorPboardType };
#else
    return String(legacyColorPasteboardType());
#endif
}

static inline String pdfPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypePDF);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyPDFPasteboardType());
#endif
}

static inline String tiffPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return String(kUTTypeTIFF);
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    return String(legacyTIFFPasteboardType());
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
    , m_pasteboardName([[m_platformDragData draggingPasteboard] name])
#else
    , m_pasteboardName(Pasteboard::nameOfDragPasteboard())
#endif
{
}

DragData::DragData(const String& dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, OptionSet<DragOperation> sourceOperationMask, OptionSet<DragApplicationFlags> flags, OptionSet<DragDestinationAction> destinationActionMask, std::optional<PageIdentifier> pageID)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_platformDragData(0)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_dragDestinationActionMask(destinationActionMask)
    , m_pageID(pageID)
    , m_pasteboardName(dragStorageName)
{
}

DragData::DragData(const String& dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, const Vector<String>& fileNames, OptionSet<DragOperation> sourceOperationMask, OptionSet<DragApplicationFlags> flags, OptionSet<DragDestinationAction> destinationActionMask, std::optional<PageIdentifier> pageID)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_fileNames(fileNames)
    , m_dragDestinationActionMask(destinationActionMask)
    , m_pageID(pageID)
    , m_pasteboardName(dragStorageName)
{
}

bool DragData::containsURLTypeIdentifier() const
{
    Vector<String> types;
    auto context = createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    return types.contains(urlPasteboardType());
}
    
bool DragData::canSmartReplace() const
{
    return Pasteboard(createPasteboardContext(), m_pasteboardName).canSmartReplace();
}

bool DragData::containsColor() const
{
    Vector<String> types;
    auto context = createPasteboardContext();
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    return types.contains(colorPasteboardType());
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

Vector<String> DragData::asFilenames() const
{
    if (m_disallowFileAccess)
        return { };
    auto context = createPasteboardContext();
#if PLATFORM(MAC)
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    if (types.contains(String(legacyFilesPromisePasteboardType())))
        return fileNames();

    Vector<String> results;
    platformStrategies()->pasteboardStrategy()->getPathnamesForType(results, String(legacyFilenamesPasteboardType()), m_pasteboardName, context.get());
    return results;
#else
    return fileNames();
#endif
}

bool DragData::containsPlainText() const
{
    auto context = createPasteboardContext();
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());

    return types.contains(stringPasteboardType())
        || types.contains(rtfdPasteboardType())
        || types.contains(rtfPasteboardType())
#if PLATFORM(MAC)
        || types.contains(String(legacyFilenamesPasteboardType()))
#endif
        || platformStrategies()->pasteboardStrategy()->containsStringSafeForDOMToReadForType(urlPasteboardType(), m_pasteboardName, context.get());
}

String DragData::asPlainText() const
{
    Pasteboard pasteboard(createPasteboardContext(), m_pasteboardName);
    PasteboardPlainText text;
    pasteboard.read(text);
    String string = text.text;

    // FIXME: It's not clear this is 100% correct since we know -[NSURL URLWithString:] does not handle
    // all the same cases we handle well in the URL code for creating an NSURL.
    if (text.isURL)
        return WTF::userVisibleString([NSURL URLWithString:string]);

    // FIXME: WTF should offer a non-Mac-specific way to convert string to precomposed form so we can do it for all platforms.
    return [(NSString *)string precomposedStringWithCanonicalMapping];
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
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return types.contains(String(WebArchivePboardType))
        || types.contains(htmlPasteboardType())
        || types.contains(String(kUTTypeWebArchive))
#if PLATFORM(MAC)
        || (!m_disallowFileAccess && types.contains(String(legacyFilenamesPasteboardType())))
        || (!m_disallowFileAccess && types.contains(String(legacyFilesPromisePasteboardType())))
#endif
        || types.contains(tiffPasteboardType())
        || types.contains(pdfPasteboardType())
        || types.contains(urlPasteboardType())
        || types.contains(rtfdPasteboardType())
        || types.contains(rtfPasteboardType())
        || types.contains(String(kUTTypeUTF8PlainText))
        || types.contains(stringPasteboardType())
        || types.contains(colorPasteboardType())
        || types.contains(String(kUTTypeJPEG))
        || types.contains(String(kUTTypePNG));
ALLOW_DEPRECATED_DECLARATIONS_END
}

bool DragData::containsPromise() const
{
    if (m_disallowFileAccess)
        return false;
    auto context = createPasteboardContext();
    // FIXME: legacyFilesPromisePasteboardType() contains UTIs, not path names. Also, why do we
    // think promises should only contain one file (or UTI)?
    Vector<String> files;
#if PLATFORM(MAC)
    platformStrategies()->pasteboardStrategy()->getPathnamesForType(files, String(legacyFilesPromisePasteboardType()), m_pasteboardName, context.get());
#endif
    return files.size() == 1;
}

bool DragData::containsURL(FilenameConversionPolicy) const
{
    auto context = createPasteboardContext();
    if (platformStrategies()->pasteboardStrategy()->containsURLStringSuitableForLoading(m_pasteboardName, context.get()))
        return true;

#if PLATFORM(MAC)
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    if (types.contains(String(legacyFilesPromisePasteboardType())) && fileNames().size() == 1)
        return !![NSURL fileURLWithPath:fileNames().first()];
#endif

    return false;
}

String DragData::asURL(FilenameConversionPolicy, String* title) const
{
    auto context = createPasteboardContext();
    // FIXME: Use filenamePolicy.

    String urlTitle;
    auto urlString = platformStrategies()->pasteboardStrategy()->urlStringSuitableForLoading(m_pasteboardName, urlTitle, context.get());
    if (title)
        *title = urlTitle;

    if (!urlString.isEmpty())
        return urlString;

#if PLATFORM(MAC)
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName, context.get());
    if (types.contains(String(legacyFilesPromisePasteboardType())) && fileNames().size() == 1)
        return [URLByCanonicalizingURL([NSURL fileURLWithPath:fileNames()[0]]) absoluteString];
#endif

    return { };
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
