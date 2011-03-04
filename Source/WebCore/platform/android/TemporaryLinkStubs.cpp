/*
 * Copyright 2009, The Android Open Source Project
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
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
#define LOG_TAG "WebCore"

#include "config.h"

#define ANDROID_COMPILE_HACK

#include "AXObjectCache.h"
#include "CachedPage.h"
#include "CachedResource.h"
#include "Clipboard.h"
#include "Console.h"
#include "ContextMenu.h"
#include "ContextMenuItem.h"
#include "CookieJar.h"
#include "CookieStorage.h"
#include "Cursor.h"
#include "Database.h"
#include "DocumentFragment.h"
#include "DocumentLoader.h"
#include "EditCommand.h"
#include "Editor.h"
#include "File.h"
#include "Font.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoader.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLFrameOwnerElement.h"
#include "HTMLKeygenElement.h"
#include "History.h"
#include "Icon.h"
#include "IconDatabase.h"
#include "IconLoader.h"
#include "IntPoint.h"
#include "KURL.h"
#include "Language.h"
#include "LocalizedStrings.h"
#include "MIMETypeRegistry.h"
#include "MainResourceLoader.h"
#include "Node.h"
#include "NotImplemented.h"
#include "PageCache.h"
#include "Pasteboard.h"
#include "Path.h"
#include "ResourceError.h"
#include "ResourceHandle.h"
#include "ResourceLoader.h"
#include "Screen.h"
#include "Scrollbar.h"
#include "ScrollbarTheme.h"
#include "SmartReplace.h"
#include "Widget.h"
#include <stdio.h>
#include <stdlib.h>
#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/text/CString.h>

#if USE(JSC)
#include "API/JSClassRef.h"
#include "JNIUtilityPrivate.h"
#include "JavaScriptCallFrame.h"
#include "ScriptDebugServer.h"
#endif

using namespace WebCore;

/********************************************************/
/* Completely empty stubs (mostly to allow DRT to run): */
/********************************************************/

namespace WebCore {

// This function tells the bridge that a resource was loaded from the cache and thus
// the app may update progress with the amount of data loaded.
void CheckCacheObjectStatus(CachedResourceLoader*, CachedResource*)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

// This class is used in conjunction with the File Upload form element, and
// therefore relates to the above. When a file has been selected, an icon
// representing the file type can be rendered next to the filename on the
// web page. The icon for the file is encapsulated within this class.
Icon::~Icon() { }
void Icon::paint(GraphicsContext*, const IntRect&) { }

}  // namespace WebCore

// FIXME, no support for spelling yet.
Pasteboard* Pasteboard::generalPasteboard()
{
    return new Pasteboard();
}

void Pasteboard::writeSelection(Range*, bool, Frame*)
{
    notImplemented();
}

void Pasteboard::writePlainText(const String&)
{
    notImplemented();
}

void Pasteboard::writeURL(const KURL&, const String&, Frame*)
{
    notImplemented();
}

void Pasteboard::clear()
{
    notImplemented();
}

bool Pasteboard::canSmartReplace()
{
    notImplemented();
    return false;
}

PassRefPtr<DocumentFragment> Pasteboard::documentFragment(Frame*, PassRefPtr<Range>, bool, bool&)
{
    notImplemented();
    return 0;
}

String Pasteboard::plainText(Frame*)
{
    notImplemented();
    return String();
}

Pasteboard::Pasteboard()
{
    notImplemented();
}

Pasteboard::~Pasteboard()
{
    notImplemented();
}


ContextMenu::ContextMenu()
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenu::~ContextMenu()
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenu::appendItem(ContextMenuItem&)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenu::setPlatformDescription(PlatformMenuDescription menu)
{
    ASSERT_NOT_REACHED();
    m_platformDescription = menu;
}

PlatformMenuDescription ContextMenu::platformDescription() const
{
    ASSERT_NOT_REACHED();
    return m_platformDescription;
}

ContextMenuItem::ContextMenuItem(PlatformMenuItemDescription)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenu*)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuItem::ContextMenuItem(ContextMenuItemType, ContextMenuAction, const String&, ContextMenu*)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuItem::~ContextMenuItem()
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

PlatformMenuItemDescription ContextMenuItem::releasePlatformDescription()
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return m_platformDescription;
}

ContextMenuItemType ContextMenuItem::type() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return ActionType;
}

void ContextMenuItem::setType(ContextMenuItemType)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

ContextMenuAction ContextMenuItem::action() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return ContextMenuItemTagNoAction;
}

void ContextMenuItem::setAction(ContextMenuAction)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

String ContextMenuItem::title() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return String();
}

void ContextMenuItem::setTitle(const String&)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

PlatformMenuDescription ContextMenuItem::platformSubMenu() const
{
    ASSERT_NOT_REACHED();
    notImplemented();
    return 0;
}

void ContextMenuItem::setSubMenu(ContextMenu*)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setChecked(bool)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

void ContextMenuItem::setEnabled(bool)
{
    ASSERT_NOT_REACHED();
    notImplemented();
}

// systemBeep() is called by the Editor to indicate that there was nothing to copy, and may be called from
// other places too.
void systemBeep()
{
    notImplemented();
}

void* WebCore::Frame::dragImageForSelection()
{
    return 0;
}


WTF::String WebCore::MIMETypeRegistry::getMIMETypeForExtension(WTF::String const&)
{
    ASSERT(isMainThread());
    return WTF::String();
}

void WebCore::Pasteboard::writeImage(WebCore::Node*, WebCore::KURL const&, WTF::String const&) {}

namespace WebCore {

IntSize dragImageSize(void*)
{
    return IntSize(0, 0);
}

void deleteDragImage(void*) {}
void* createDragImageFromImage(Image*)
{
    return 0;
}

void* dissolveDragImageToFraction(void*, float)
{
    return 0;
}

void* createDragImageIconForCachedImage(CachedImage*)
{
    return 0;
}

Cursor dummyCursor;
const Cursor& zoomInCursor()
{
    return dummyCursor;
}

const Cursor& zoomOutCursor()
{
    return dummyCursor;
}

const Cursor& notAllowedCursor()
{
    return dummyCursor;
}

void* scaleDragImage(void*, FloatSize)
{
    return 0;
}

String searchMenuRecentSearchesText()
{
    return String();
}

String searchMenuNoRecentSearchesText()
{
    return String();
}

String searchMenuClearRecentSearchesText()
{
    return String();
}

Vector<String> supportedKeySizes()
{
    notImplemented();
    return Vector<String>();
}

} // namespace WebCore

namespace WebCore {
// isCharacterSmartReplaceExempt is defined in SmartReplaceICU.cpp; in theory, we could use that one
//      but we don't support all of the required icu functions
bool isCharacterSmartReplaceExempt(UChar32, bool)
{
    notImplemented();
    return false;
}

}  // WebCore

int MakeDataExecutable;

String KURL::fileSystemPath() const
{
    notImplemented();
    return String();
}


PassRefPtr<SharedBuffer> SharedBuffer::createWithContentsOfFile(const String&)
{
    notImplemented();
    return 0;
}


#if USE(JSC)
namespace JSC { namespace Bindings {
bool dispatchJNICall(ExecState*, const void* targetAppletView, jobject obj, bool isStatic, JNIType returnType, 
        jmethodID methodID, jvalue* args, jvalue& result, const char* callingURL, JSValue& exceptionDescription)
{
    notImplemented();
    return false;
}

} }  // namespace Bindings
#endif

char* dirname(const char*)
{
    notImplemented();
    return 0;
}

    // new as of SVN change 38068, Nov 5, 2008
namespace WebCore {
void prefetchDNS(const String&)
{
    notImplemented();
}

PassRefPtr<Icon> Icon::createIconForFile(const String&)
{
    notImplemented();
    return 0;
}

PassRefPtr<Icon> Icon::createIconForFiles(const Vector<String>&)
{
    notImplemented();
    return 0;
}

// ScrollbarTheme::nativeTheme() is called by RenderTextControl::calcPrefWidths()
// like this: scrollbarSize = ScrollbarTheme::nativeTheme()->scrollbarThickness();
// with this comment:
// // FIXME: We should get the size of the scrollbar from the RenderTheme instead.
// since our text control doesn't have scrollbars, the default size of 0 width should be
// ok. notImplemented() is commented out below so that we can find other unresolved
// unimplemented functions.
ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    /* notImplemented(); */
    static ScrollbarTheme theme;
    return &theme;
}

}  // namespace WebCore

AXObjectCache::~AXObjectCache()
{
    notImplemented();
}

// This value turns on or off the Mac specific Accessibility support.
bool AXObjectCache::gAccessibilityEnabled = false;
bool AXObjectCache::gAccessibilityEnhancedUserInterfaceEnabled = false;

void AXObjectCache::childrenChanged(RenderObject*)
{
    notImplemented();
}

void AXObjectCache::remove(RenderObject*)
{
    notImplemented();
}

#if USE(JSC)
using namespace JSC;


OpaqueJSClass::~OpaqueJSClass()
{
    notImplemented();
}

OpaqueJSClassContextData::~OpaqueJSClassContextData()
{
    notImplemented();
}

#endif

namespace WebCore {

void setCookieStoragePrivateBrowsingEnabled(bool)
{
    notImplemented();
}

void startObservingCookieChanges()
{
    notImplemented();
}

void stopObservingCookieChanges()
{
    notImplemented();
}

} // namespace WebCore
