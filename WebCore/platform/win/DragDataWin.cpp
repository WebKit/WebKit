/*
 * Copyright (C) 2007, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DragData.h"

#include "ClipboardWin.h"
#include "ClipboardUtilitiesWin.h"
#include "ClipboardAccessPolicy.h"
#include "DocumentFragment.h"
#include "PlatformString.h"
#include "Markup.h"
#include "TextEncoding.h"
#include <objidl.h>
#include <shlwapi.h>
#include <wininet.h>

namespace WebCore {

PassRefPtr<Clipboard> DragData::createClipboard(ClipboardAccessPolicy policy) const
{
    return ClipboardWin::create(true, m_platformDragData, policy);
}

bool DragData::containsURL() const
{
    return SUCCEEDED(m_platformDragData->QueryGetData(urlWFormat())) 
        || SUCCEEDED(m_platformDragData->QueryGetData(urlFormat()))
        || SUCCEEDED(m_platformDragData->QueryGetData(filenameWFormat())) 
        || SUCCEEDED(m_platformDragData->QueryGetData(filenameFormat()));
}

String DragData::asURL(String* title) const
{
    bool success;
    return getURL(m_platformDragData, success, title);
}

bool DragData::containsFiles() const
{
    return SUCCEEDED(m_platformDragData->QueryGetData(cfHDropFormat()));
}

void DragData::asFilenames(Vector<String>& result) const
{
    WCHAR filename[MAX_PATH];
    
    STGMEDIUM medium;
    if (FAILED(m_platformDragData->GetData(cfHDropFormat(), &medium)))
        return;
    
    HDROP hdrop = (HDROP)GlobalLock(medium.hGlobal);
    
    if (!hdrop)
        return;

    const unsigned numFiles = DragQueryFileW(hdrop, 0xFFFFFFFF, 0, 0);
    for (unsigned i = 0; i < numFiles; i++) {
        if (!DragQueryFileW(hdrop, 0, filename, ARRAYSIZE(filename)))
            continue;
        result.append((UChar*)filename);
    }

    // Free up memory from drag
    DragFinish(hdrop);

    GlobalUnlock(medium.hGlobal);
}

bool DragData::containsPlainText() const
{
    return SUCCEEDED(m_platformDragData->QueryGetData(plainTextWFormat()))
        || SUCCEEDED(m_platformDragData->QueryGetData(plainTextFormat()));
}

String DragData::asPlainText() const
{
    bool success;
    return getPlainText(m_platformDragData, success);
}

bool DragData::containsColor() const
{
    return false;
}

bool DragData::canSmartReplace() const
{
    return SUCCEEDED(m_platformDragData->QueryGetData(smartPasteFormat())); 
}

bool DragData::containsCompatibleContent() const
{
    return containsPlainText() || containsURL() 
        || containsHTML(m_platformDragData) 
        || containsFilenames(m_platformDragData) 
        || containsColor();
}

PassRefPtr<DocumentFragment> DragData::asFragment(Document* doc) const
{     
    /*
     * Order is richest format first. On OSX this is:
     * * Web Archive
     * * Filenames
     * * HTML
     * * RTF
     * * TIFF
     * * PICT
     */
        
     if (containsFilenames(m_platformDragData))
         if (PassRefPtr<DocumentFragment> fragment = fragmentFromFilenames(doc, m_platformDragData))
             return fragment;

     if (containsHTML(m_platformDragData))
         if (PassRefPtr<DocumentFragment> fragment = fragmentFromHTML(doc, m_platformDragData))
             return fragment;

     return 0;
}

Color DragData::asColor() const
{
    return Color();
}

}

