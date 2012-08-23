/*
 * Copyright (C) 2004, 2005, 2006, 2008, 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ClipboardMac.h"

#import "CachedImageClient.h"
#import "DOMElementInternal.h"
#import "DragClient.h"
#import "DragController.h"
#import "DragData.h"
#import "Editor.h"
#import "FileList.h"
#import "Frame.h"
#import "Image.h"
#import "Page.h"
#import "Pasteboard.h"
#import "PasteboardStrategy.h"
#import "PlatformStrategies.h"
#import "RenderImage.h"
#import "ScriptExecutionContext.h"
#import "SecurityOrigin.h"
#import "WebCoreSystemInterface.h"


namespace WebCore {

#if ENABLE(DRAG_SUPPORT)
PassRefPtr<Clipboard> Clipboard::create(ClipboardAccessPolicy policy, DragData* dragData, Frame* frame)
{
    return ClipboardMac::create(DragAndDrop, dragData->pasteboardName(), policy, dragData->containsFiles() ? ClipboardMac::DragAndDropFiles : ClipboardMac::DragAndDropData, frame);
}
#endif

ClipboardMac::ClipboardMac(ClipboardType clipboardType, const String& pasteboardName, ClipboardAccessPolicy policy, ClipboardContents clipboardContents, Frame *frame)
    : Clipboard(policy, clipboardType)
    , m_pasteboardName(pasteboardName)
    , m_clipboardContents(clipboardContents)
    , m_frame(frame)
{
    m_changeCount = platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName);
}

ClipboardMac::~ClipboardMac()
{
}

bool ClipboardMac::hasData()
{
    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);
    return !types.isEmpty();
}
    
static String cocoaTypeFromHTMLClipboardType(const String& type)
{
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/dnd.html#dom-datatransfer-setdata
    String qType = type.lower();

    if (qType == "text")
        qType = "text/plain";
    if (qType == "url")
        qType = "text/uri-list";

    // Ignore any trailing charset - JS strings are Unicode, which encapsulates the charset issue
    if (qType == "text/plain" || qType.startsWith("text/plain;"))
        return String(NSStringPboardType);
    if (qType == "text/uri-list")
        // special case because UTI doesn't work with Cocoa's URL type
        return String(NSURLPboardType); // note special case in getData to read NSFilenamesType

    // Blacklist types that might contain subframe information
    if (qType == "text/rtf" || qType == "public.rtf" || qType == "com.apple.traditional-mac-plain-text")
        return String();

    // Try UTI now
    String mimeType = qType;
    RetainPtr<CFStringRef> utiType(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassMIMEType, mimeType.createCFString(), NULL));
    if (utiType) {
        CFStringRef pbType = UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassNSPboardType);
        if (pbType)
            return pbType;
    }

    // No mapping, just pass the whole string though
    return qType;
}

static String utiTypeFromCocoaType(const String& type)
{
    RetainPtr<CFStringRef> typeCF = adoptCF(type.createCFString());
    RetainPtr<CFStringRef> utiType(AdoptCF, UTTypeCreatePreferredIdentifierForTag(kUTTagClassNSPboardType, typeCF.get(), 0));
    if (utiType) {
        RetainPtr<CFStringRef> mimeType(AdoptCF, UTTypeCopyPreferredTagWithClass(utiType.get(), kUTTagClassMIMEType));
        if (mimeType)
            return String(mimeType.get());
    }
    return String();
}

static void addHTMLClipboardTypesForCocoaType(Vector<String>& resultTypes, const String& cocoaType, const String& pasteboardName)
{
    // UTI may not do these right, so make sure we get the right, predictable result
    if (cocoaType == String(NSStringPboardType)) {
        resultTypes.append("text/plain");
        return;
    }
    if (cocoaType == String(NSURLPboardType)) {
        resultTypes.append("text/uri-list");
        return;
    }
    if (cocoaType == String(NSFilenamesPboardType)) {
        // If file list is empty, add nothing.
        // Note that there is a chance that the file list count could have changed since we grabbed the types array.
        // However, this is not really an issue for us doing a sanity check here.
        Vector<String> fileList;
        platformStrategies()->pasteboardStrategy()->getPathnamesForType(fileList, String(NSFilenamesPboardType), pasteboardName);
        if (!fileList.isEmpty()) {
            // It is unknown if NSFilenamesPboardType always implies NSURLPboardType in Cocoa,
            // but NSFilenamesPboardType should imply both 'text/uri-list' and 'Files'
            resultTypes.append("text/uri-list");
            resultTypes.append("Files");
        }
        return;
    }
    String utiType = utiTypeFromCocoaType(cocoaType);
    if (!utiType.isEmpty()) {
        resultTypes.append(utiType);
        return;
    }
    // No mapping, just pass the whole string though
    resultTypes.append(cocoaType);
}

void ClipboardMac::clearData(const String& type)
{
    if (policy() != ClipboardWritable)
        return;

    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    String cocoaType = cocoaTypeFromHTMLClipboardType(type);
    if (!cocoaType.isEmpty())
        platformStrategies()->pasteboardStrategy()->setStringForType("", cocoaType, m_pasteboardName);
}

void ClipboardMac::clearAllData()
{
    if (policy() != ClipboardWritable)
        return;

    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    Pasteboard pasteboard(m_pasteboardName);
    pasteboard.clear();
}

static Vector<String> absoluteURLsFromPasteboardFilenames(const String& pasteboardName, bool onlyFirstURL = false)
{
    Vector<String> fileList;
    platformStrategies()->pasteboardStrategy()->getPathnamesForType(fileList, String(NSFilenamesPboardType), pasteboardName);

    if (fileList.isEmpty())
        return fileList;

    size_t count = onlyFirstURL ? 1 : fileList.size();
    Vector<String> urls;
    for (size_t i = 0; i < count; i++) {
        NSURL *url = [NSURL fileURLWithPath:fileList[i]];
        urls.append(String([url absoluteString]));
    }
    return urls;
}

static Vector<String> absoluteURLsFromPasteboard(const String& pasteboardName, bool onlyFirstURL = false)
{
    // NOTE: We must always check [availableTypes containsObject:] before accessing pasteboard data
    // or CoreFoundation will printf when there is not data of the corresponding type.
    Vector<String> availableTypes;
    Vector<String> absoluteURLs;
    platformStrategies()->pasteboardStrategy()->getTypes(availableTypes, pasteboardName);

    // Try NSFilenamesPboardType because it contains a list
    if (availableTypes.contains(String(NSFilenamesPboardType))) {
        absoluteURLs = absoluteURLsFromPasteboardFilenames(pasteboardName, onlyFirstURL);
        if (!absoluteURLs.isEmpty())
            return absoluteURLs;
    }

    // Fallback to NSURLPboardType (which is a single URL)
    if (availableTypes.contains(String(NSURLPboardType))) {
        absoluteURLs.append(platformStrategies()->pasteboardStrategy()->stringForType(String(NSURLPboardType), pasteboardName));
        return absoluteURLs;
    }

    // No file paths on the pasteboard, return nil
    return Vector<String>();
}

String ClipboardMac::getData(const String& type) const
{
    if (policy() != ClipboardReadable || m_clipboardContents == DragAndDropFiles)
        return String();

    const String& cocoaType = cocoaTypeFromHTMLClipboardType(type);
    String cocoaValue;

    // Grab the value off the pasteboard corresponding to the cocoaType
    if (cocoaType == String(NSURLPboardType)) {
        // "url" and "text/url-list" both map to NSURLPboardType in cocoaTypeFromHTMLClipboardType(), "url" only wants the first URL
        bool onlyFirstURL = (equalIgnoringCase(type, "url"));
        Vector<String> absoluteURLs = absoluteURLsFromPasteboard(m_pasteboardName, onlyFirstURL);
        for (size_t i = 0; i < absoluteURLs.size(); i++)
            cocoaValue = i ? "\n" + absoluteURLs[i]: absoluteURLs[i];
    } else if (cocoaType == String(NSStringPboardType))
        cocoaValue = [platformStrategies()->pasteboardStrategy()->stringForType(cocoaType, m_pasteboardName) precomposedStringWithCanonicalMapping];
    else if (!cocoaType.isEmpty())
        cocoaValue = platformStrategies()->pasteboardStrategy()->stringForType(cocoaType, m_pasteboardName);

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (!cocoaValue.isEmpty() && m_changeCount == platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName)) {
        return cocoaValue;
    }

    return String();
}

bool ClipboardMac::setData(const String &type, const String &data)
{
    if (policy() != ClipboardWritable || m_clipboardContents == DragAndDropFiles)
        return false;
    // note NSPasteboard enforces changeCount itself on writing - can't write if not the owner

    const String& cocoaType = cocoaTypeFromHTMLClipboardType(type);
    String cocoaData = data;

    if (cocoaType == String(NSURLPboardType) || cocoaType == String(kUTTypeFileURL)) {
        NSURL *url = [NSURL URLWithString:cocoaData];
        if ([url isFileURL])
            return false;

        Vector<String> types;
        types.append(cocoaType);
        platformStrategies()->pasteboardStrategy()->setTypes(types, m_pasteboardName);
        platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName);

        return true;
    }

    if (!cocoaType.isEmpty()) {
        // everything else we know of goes on the pboard as a string
        Vector<String> types;
        types.append(cocoaType);
        platformStrategies()->pasteboardStrategy()->addTypes(types, m_pasteboardName);
        platformStrategies()->pasteboardStrategy()->setStringForType(cocoaData, cocoaType, m_pasteboardName);
        return true;
    }

    return false;
}

Vector<String> ClipboardMac::types() const
{
    if (policy() != ClipboardReadable && policy() != ClipboardTypesReadable)
        return Vector<String>();

    Vector<String> types;
    platformStrategies()->pasteboardStrategy()->getTypes(types, m_pasteboardName);

    // Enforce changeCount ourselves for security.  We check after reading instead of before to be
    // sure it doesn't change between our testing the change count and accessing the data.
    if (m_changeCount != platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName))
        return Vector<String>();

    Vector<String> result;
    // FIXME: This loop could be split into two stages. One which adds all the HTML5 specified types
    // and a second which adds all the extra types from the cocoa clipboard (which is Mac-only behavior).
    for (size_t i = 0; i < types.size(); i++) {
        if (types[i] == "NeXT plain ascii pasteboard type")
            continue;   // skip this ancient type that gets auto-supplied by some system conversion

        addHTMLClipboardTypesForCocoaType(result, types[i], m_pasteboardName);
    }

    return result;
}

// FIXME: We could cache the computed fileList if necessary
// Currently each access gets a new copy, setData() modifications to the
// clipboard are not reflected in any FileList objects the page has accessed and stored
PassRefPtr<FileList> ClipboardMac::files() const
{
    if (policy() != ClipboardReadable || m_clipboardContents == DragAndDropData)
        return FileList::create();

    Vector<String> absoluteURLs = absoluteURLsFromPasteboardFilenames(m_pasteboardName);

    RefPtr<FileList> fileList = FileList::create();
    for (size_t i = 0; i < absoluteURLs.size(); i++) {
        NSURL *absoluteURL = [NSURL URLWithString:absoluteURLs[i]];
        ASSERT([absoluteURL isFileURL]);
        fileList->append(File::create([absoluteURL path], File::AllContentTypes));
    }
    return fileList.release(); // We will always return a FileList, sometimes empty
}

// The rest of these getters don't really have any impact on security, so for now make no checks

void ClipboardMac::setDragImage(CachedImage* img, const IntPoint &loc)
{
    setDragImage(img, 0, loc);
}

void ClipboardMac::setDragImageElement(Node *node, const IntPoint &loc)
{
    setDragImage(0, node, loc);
}

void ClipboardMac::setDragImage(CachedImage* image, Node *node, const IntPoint &loc)
{
    if (policy() == ClipboardImageWritable || policy() == ClipboardWritable) {
        if (m_dragImage)
            m_dragImage->removeClient(this);
        m_dragImage = image;
        if (m_dragImage)
            m_dragImage->addClient(this);

        m_dragLoc = loc;
        m_dragImageElement = node;
        
        if (dragStarted() && m_changeCount == platformStrategies()->pasteboardStrategy()->changeCount(m_pasteboardName)) {
            NSPoint cocoaLoc;
            NSImage* cocoaImage = dragNSImage(cocoaLoc);
            if (cocoaImage) {
                // Dashboard wants to be able to set the drag image during dragging, but Cocoa does not allow this.
                // Instead we must drop down to the CoreGraphics API.
                wkSetDragImage(cocoaImage, cocoaLoc);

                // Hack: We must post an event to wake up the NSDragManager, which is sitting in a nextEvent call
                // up the stack from us because the CoreFoundation drag manager does not use the run loop by itself.
                // This is the most innocuous event to use, per Kristen Forster.
                NSEvent* ev = [NSEvent mouseEventWithType:NSMouseMoved location:NSZeroPoint
                    modifierFlags:0 timestamp:0 windowNumber:0 context:nil eventNumber:0 clickCount:0 pressure:0];
                [NSApp postEvent:ev atStart:YES];
            }
        }
        // Else either 1) we haven't started dragging yet, so we rely on the part to install this drag image
        // as part of getting the drag kicked off, or 2) Someone kept a ref to the clipboard and is trying to
        // set the image way too late.
    }
}
    
void ClipboardMac::writeRange(Range* range, Frame* frame)
{
    ASSERT(range);
    ASSERT(frame);
    Pasteboard pasteboard(m_pasteboardName);
    pasteboard.writeSelection(range, frame->editor()->smartInsertDeleteEnabled() && frame->selection()->granularity() == WordGranularity, frame);
}

void ClipboardMac::writePlainText(const String& text)
{
    Pasteboard pasteboard(m_pasteboardName);
    pasteboard.writePlainText(text, Pasteboard::CannotSmartReplace);
}

void ClipboardMac::writeURL(const KURL& url, const String& title, Frame* frame)
{   
    ASSERT(frame);
    ASSERT(m_pasteboardName);
    Pasteboard pasteboard(m_pasteboardName);
    pasteboard.writeURL(url, title, frame);
}
    
#if ENABLE(DRAG_SUPPORT)
void ClipboardMac::declareAndWriteDragImage(Element* element, const KURL& url, const String& title, Frame* frame)
{
    ASSERT(frame);
    if (Page* page = frame->page())
        page->dragController()->client()->declareAndWriteDragImage(m_pasteboardName, kit(element), url, title, frame);
}
#endif // ENABLE(DRAG_SUPPORT)
    
DragImageRef ClipboardMac::createDragImage(IntPoint& loc) const
{
    NSPoint nsloc = {loc.x(), loc.y()};
    DragImageRef result = dragNSImage(nsloc);
    loc = (IntPoint)nsloc;
    return result;
}
    
NSImage *ClipboardMac::dragNSImage(NSPoint& loc) const
{
    NSImage *result = nil;
    if (m_dragImageElement) {
        if (m_frame) {
            NSRect imageRect;
            NSRect elementRect;
            result = m_frame->snapshotDragImage(m_dragImageElement.get(), &imageRect, &elementRect);
            // Client specifies point relative to element, not the whole image, which may include child
            // layers spread out all over the place.
            loc.x = elementRect.origin.x - imageRect.origin.x + m_dragLoc.x();
            loc.y = elementRect.origin.y - imageRect.origin.y + m_dragLoc.y();
            loc.y = imageRect.size.height - loc.y;
        }
    } else if (m_dragImage) {
        result = m_dragImage->image()->getNSImage();
        
        loc = m_dragLoc;
        loc.y = [result size].height - loc.y;
    }
    return result;
}

}
