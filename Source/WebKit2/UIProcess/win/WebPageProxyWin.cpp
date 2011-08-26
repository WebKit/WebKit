/*
* Copyright (C) 2010 Apple Inc. All rights reserved.
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
* THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
* THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
* PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
* BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
* CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
* SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
* INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
* CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
* ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
* THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"
#include "WebPageProxy.h"

#include "PageClient.h"
#include "WebDragSource.h"
#include "WebPageMessages.h"
#include "WebPopupMenuProxyWin.h"
#include "WebProcessProxy.h"

#include "resource.h"
#include <WebCore/BitmapInfo.h>
#include <WebCore/COMPtr.h>
#include <WebCore/ClipboardUtilitiesWin.h>
#include <WebCore/SystemInfo.h>
#include <WebCore/WCDataObject.h>
#include <WebCore/WebCoreInstanceHandle.h>
#include <tchar.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace WebKit {

static String userVisibleWebKitVersionString()
{
    LPWSTR buildNumberStringPtr;
    if (!::LoadStringW(instanceHandle(), BUILD_NUMBER, reinterpret_cast<LPWSTR>(&buildNumberStringPtr), 0) || !buildNumberStringPtr)
        return "534+";

    return buildNumberStringPtr;
}

String WebPageProxy::standardUserAgent(const String& applicationNameForUserAgent)
{
    DEFINE_STATIC_LOCAL(String, osVersion, (windowsVersionForUAString()));
    DEFINE_STATIC_LOCAL(String, webKitVersion, (userVisibleWebKitVersionString()));

    return makeString("Mozilla/5.0 (", osVersion, ") AppleWebKit/", webKitVersion, " (KHTML, like Gecko)", applicationNameForUserAgent.isEmpty() ? "" : " ", applicationNameForUserAgent);
}

void WebPageProxy::setPopupMenuSelectedIndex(int32_t selectedIndex)
{
    if (!m_activePopupMenu)
        return;

    static_cast<WebPopupMenuProxyWin*>(m_activePopupMenu.get())->setFocusedIndex(selectedIndex);
}

HWND WebPageProxy::nativeWindow() const
{
    return m_pageClient->nativeWindow();
}

void WebPageProxy::scheduleChildWindowGeometryUpdate(const WindowGeometry& geometry)
{
    m_pageClient->scheduleChildWindowGeometryUpdate(geometry);
}

void WebPageProxy::didInstallOrUninstallPageOverlay(bool didInstall)
{
    m_pageClient->didInstallOrUninstallPageOverlay(didInstall);
}

IntRect WebPageProxy::firstRectForCharacterInSelectedRange(int characterPosition)
{
    IntRect resultRect;
    process()->sendSync(Messages::WebPage::FirstRectForCharacterInSelectedRange(characterPosition), Messages::WebPage::FirstRectForCharacterInSelectedRange::Reply(resultRect), m_pageID);
    return resultRect;
}

String WebPageProxy::getSelectedText()
{
    String text;
    process()->sendSync(Messages::WebPage::GetSelectedText(), Messages::WebPage::GetSelectedText::Reply(text), m_pageID);
    return text;
}

bool WebPageProxy::gestureWillBegin(const IntPoint& point)
{
    bool canBeginPanning = false;
    process()->sendSync(Messages::WebPage::GestureWillBegin(point), Messages::WebPage::GestureWillBegin::Reply(canBeginPanning), m_pageID);
    return canBeginPanning;
}

void WebPageProxy::gestureDidScroll(const IntSize& size)
{
    process()->send(Messages::WebPage::GestureDidScroll(size), m_pageID);
}

void WebPageProxy::gestureDidEnd()
{
    process()->send(Messages::WebPage::GestureDidEnd(), m_pageID);
}

void WebPageProxy::setGestureReachedScrollingLimit(bool limitReached)
{
    m_pageClient->setGestureReachedScrollingLimit(limitReached);
}

void WebPageProxy::startDragDrop(const IntPoint& imageOrigin, const IntPoint& dragPoint, uint64_t okEffect, const HashMap<UINT, Vector<String> >& dataMap, uint64_t fileSize, const String& pathname, const SharedMemory::Handle& fileContentHandle, const IntSize& dragImageSize, const SharedMemory::Handle& dragImageHandle, bool isLinkDrag)
{
    COMPtr<WCDataObject> dataObject;
    WCDataObject::createInstance(&dataObject, dataMap);

    if (fileSize) {
        RefPtr<SharedMemory> fileContentBuffer = SharedMemory::create(fileContentHandle, SharedMemory::ReadOnly);
        setFileDescriptorData(dataObject.get(), fileSize, pathname);
        setFileContentData(dataObject.get(), fileSize, fileContentBuffer->data());
    }

    RefPtr<SharedMemory> memoryBuffer = SharedMemory::create(dragImageHandle, SharedMemory::ReadOnly);
    if (!memoryBuffer)
        return;

    RefPtr<WebDragSource> source = WebDragSource::createInstance();
    if (!source)
        return;

    COMPtr<IDragSourceHelper> helper;
    if (FAILED(::CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER, IID_IDragSourceHelper, reinterpret_cast<LPVOID*>(&helper))))
        return;

    BitmapInfo bitmapInfo = BitmapInfo::create(dragImageSize);
    void* bits;
    OwnPtr<HBITMAP> hbmp = adoptPtr(::CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, &bits, 0, 0));
    memcpy(bits, memoryBuffer->data(), memoryBuffer->size());

    SHDRAGIMAGE sdi;
    sdi.sizeDragImage.cx = bitmapInfo.bmiHeader.biWidth;
    sdi.sizeDragImage.cy = bitmapInfo.bmiHeader.biHeight;
    sdi.crColorKey = 0xffffffff;
    sdi.hbmpDragImage = hbmp.leakPtr();
    sdi.ptOffset.x = dragPoint.x() - imageOrigin.x();
    sdi.ptOffset.y = dragPoint.y() - imageOrigin.y();
    if (isLinkDrag)
        sdi.ptOffset.y = bitmapInfo.bmiHeader.biHeight - sdi.ptOffset.y;

    helper->InitializeFromBitmap(&sdi, dataObject.get());

    DWORD effect = DROPEFFECT_NONE;

    DragOperation operation = DragOperationNone;
    if (::DoDragDrop(dataObject.get(), source.get(), okEffect, &effect) == DRAGDROP_S_DROP) {
        if (effect & DROPEFFECT_COPY)
            operation = DragOperationCopy;
        else if (effect & DROPEFFECT_LINK)
            operation = DragOperationLink;
        else if (effect & DROPEFFECT_MOVE)
            operation = DragOperationMove;
    }
    POINT globalPoint;
    ::GetCursorPos(&globalPoint);
    POINT localPoint = globalPoint;
    ::ScreenToClient(m_pageClient->nativeWindow(), &localPoint);

    dragEnded(localPoint, globalPoint, operation);
}

void WebPageProxy::didChangeCompositionSelection(bool hasComposition)
{
    m_pageClient->compositionSelectionChanged(hasComposition);
}

void WebPageProxy::confirmComposition(const String& compositionString)
{
    process()->send(Messages::WebPage::ConfirmComposition(compositionString), m_pageID);
}

void WebPageProxy::setComposition(const String& compositionString, Vector<CompositionUnderline>& underlines, int cursorPosition)
{
    process()->send(Messages::WebPage::SetComposition(compositionString, underlines, cursorPosition), m_pageID);
}


} // namespace WebKit
