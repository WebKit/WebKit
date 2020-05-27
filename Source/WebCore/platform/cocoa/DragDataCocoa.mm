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
#import "LegacyNSPasteboardTypes.h"
#import "MIMETypeRegistry.h"
#import "NotImplemented.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformPasteboard.h"
#import "PlatformStrategies.h"
#import "RuntimeEnabledFeatures.h"
#import "WebCoreNSURLExtras.h"
#import <wtf/cocoa/NSURLExtras.h>

#if PLATFORM(IOS_FAMILY)
#import <MobileCoreServices/MobileCoreServices.h>
#endif

namespace WebCore {

static inline String rtfPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String(kUTTypeRTF);
#else
    return String(legacyRTFPasteboardType());
#endif
}

static inline String rtfdPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String(kUTTypeFlatRTFD);
#else
    return String(legacyRTFDPasteboardType());
#endif
}

static inline String stringPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String(kUTTypeText);
#else
    return String(legacyStringPasteboardType());
#endif
}

static inline String urlPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String(kUTTypeURL);
#else
    return String(legacyURLPasteboardType());
#endif
}

static inline String htmlPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String(kUTTypeHTML);
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
    return String(kUTTypePDF);
#else
    return String(legacyPDFPasteboardType());
#endif
}

static inline String tiffPasteboardType()
{
#if PLATFORM(IOS_FAMILY)
    return String(kUTTypeTIFF);
#else
    return String(legacyTIFFPasteboardType());
#endif
}

DragData::DragData(DragDataRef data, const IntPoint& clientPosition, const IntPoint& globalPosition, DragOperation sourceOperationMask, DragApplicationFlags flags, OptionSet<DragDestinationAction> destinationActionMask)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_platformDragData(data)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_dragDestinationActionMask(destinationActionMask)
#if PLATFORM(MAC)
    , m_pasteboardName([[m_platformDragData draggingPasteboard] name])
#else
    , m_pasteboardName(Pasteboard::nameOfDragPasteboard())
#endif
{
}

DragData::DragData(const String& dragStorageName, const IntPoint& clientPosition, const IntPoint& globalPosition, DragOperation sourceOperationMask, DragApplicationFlags flags, OptionSet<DragDestinationAction> destinationActionMask)
    : m_clientPosition(clientPosition)
    , m_globalPosition(globalPosition)
    , m_platformDragData(0)
    , m_draggingSourceOperationMask(sourceOperationMask)
    , m_applicationFlags(flags)
    , m_dragDestinationActionMask(destinationActionMask)
    , m_pasteboardName(dragStorageName)
{
}

bool DragData::containsURLTypeIdentifier() const
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    return types.contains(urlPasteboardType());
}
    
bool DragData::canSmartReplace() const
{
    return Pasteboard(m_pasteboardName).canSmartReplace();
}

bool DragData::containsColor() const
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    return types.contains(colorPasteboardType());
}

bool DragData::containsFiles() const
{
    return numberOfFiles();
}

unsigned DragData::numberOfFiles() const
{
    return platformStrategies()->pasteboardStrategy()->getNumberOfFiles(m_pasteboardName);
}

Vector<String> DragData::asFilenames() const
{
#if PLATFORM(MAC)
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    if (types.contains(String(legacyFilesPromisePasteboardType())))
        return fileNames();

    Vector<String> results;
    platformStrategies()->pasteboardStrategy()->getPathnamesForType(results, String(legacyFilenamesPasteboardType()), m_pasteboardName);
    return results;
#else
    return fileNames();
#endif
}

bool DragData::containsPlainText() const
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);

    return types.contains(stringPasteboardType())
        || types.contains(rtfdPasteboardType())
        || types.contains(rtfPasteboardType())
#if PLATFORM(MAC)
        || types.contains(String(legacyFilenamesPasteboardType()))
#endif
        || platformStrategies()->pasteboardStrategy()->containsStringSafeForDOMToReadForType(urlPasteboardType(), m_pasteboardName);
}

String DragData::asPlainText() const
{
    Pasteboard pasteboard(m_pasteboardName);
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
    return platformStrategies()->pasteboardStrategy()->color(m_pasteboardName);
}

bool DragData::containsCompatibleContent(DraggingPurpose purpose) const
{
    if (purpose == DraggingPurpose::ForFileUpload)
        return containsFiles();

    if (purpose == DraggingPurpose::ForColorControl)
        return containsColor();

    if (purpose == DraggingPurpose::ForEditing && RuntimeEnabledFeatures::sharedFeatures().attachmentElementEnabled() && containsFiles())
        return true;

    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    return types.contains(String(WebArchivePboardType))
        || types.contains(htmlPasteboardType())
        || types.contains(String(kUTTypeWebArchive))
#if PLATFORM(MAC)
        || types.contains(String(legacyFilenamesPasteboardType()))
        || types.contains(String(legacyFilesPromisePasteboardType()))
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
}

bool DragData::containsPromise() const
{
    // FIXME: legacyFilesPromisePasteboardType() contains UTIs, not path names. Also, why do we
    // think promises should only contain one file (or UTI)?
    Vector<String> files;
#if PLATFORM(MAC)
    platformStrategies()->pasteboardStrategy()->getPathnamesForType(files, String(legacyFilesPromisePasteboardType()), m_pasteboardName);
#endif
    return files.size() == 1;
}

bool DragData::containsURL(FilenameConversionPolicy) const
{
    if (platformStrategies()->pasteboardStrategy()->containsURLStringSuitableForLoading(m_pasteboardName))
        return true;

#if PLATFORM(MAC)
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    if (types.contains(String(legacyFilesPromisePasteboardType())) && fileNames().size() == 1)
        return !![NSURL fileURLWithPath:fileNames().first()];
#endif

    return false;
}

String DragData::asURL(FilenameConversionPolicy, String* title) const
{
    // FIXME: Use filenamePolicy.

    String urlTitle;
    auto urlString = platformStrategies()->pasteboardStrategy()->urlStringSuitableForLoading(m_pasteboardName, urlTitle);
    if (title)
        *title = urlTitle;

    if (!urlString.isEmpty())
        return urlString;

#if PLATFORM(MAC)
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    if (types.contains(String(legacyFilesPromisePasteboardType())) && fileNames().size() == 1)
        return [URLByCanonicalizingURL([NSURL fileURLWithPath:fileNames()[0]]) absoluteString];
#endif

    return { };
}

} // namespace WebCore

#endif // ENABLE(DRAG_SUPPORT)
