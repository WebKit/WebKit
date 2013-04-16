/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2008, 2009 Google Inc.
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

// Modified from DragDataWin.cpp to not directly call any windows methods as
// they may not be available to us in the multiprocess 

#include "config.h"
#include "DragData.h"

#include "ChromiumDataObject.h"
#include "ClipboardMimeTypes.h"
#include "Document.h"
#include "DocumentFragment.h"
#include "FileSystem.h"
#include "Frame.h"
#include "KURL.h"
#include "NotImplemented.h"
#include "PlatformString.h"
#include "PlatformSupport.h"
#include "markup.h"

namespace WebCore {

static bool containsHTML(const ChromiumDataObject* dropData)
{
    return dropData->types().contains(mimeTypeTextHTML);
}

bool DragData::containsURL(Frame*, FilenameConversionPolicy filenamePolicy) const
{
    return m_platformDragData->types().contains(mimeTypeTextURIList)
        || (filenamePolicy == ConvertFilenames && m_platformDragData->containsFilenames());
}

String DragData::asURL(Frame*, FilenameConversionPolicy filenamePolicy, String* title) const
{
    String url;
    if (m_platformDragData->types().contains(mimeTypeTextURIList))
        m_platformDragData->urlAndTitle(url, title);
    else if (filenamePolicy == ConvertFilenames && containsFiles()) {
        url = PlatformSupport::filePathToURL(PlatformSupport::getAbsolutePath(m_platformDragData->filenames()[0]));
    }
    return url;
}

bool DragData::containsFiles() const
{
    return m_platformDragData->containsFilenames();
}

unsigned DragData::numberOfFiles() const
{
    return m_platformDragData->filenames().size();
}

void DragData::asFilenames(Vector<String>& result) const
{
    const Vector<String>& filenames = m_platformDragData->filenames();
    for (size_t i = 0; i < filenames.size(); ++i)
        result.append(filenames[i]);
}

bool DragData::containsPlainText() const
{
    return m_platformDragData->types().contains(mimeTypeTextPlain);
}

String DragData::asPlainText(Frame*) const
{
    return m_platformDragData->getData(mimeTypeTextPlain);
}

bool DragData::containsColor() const
{
    notImplemented();
    return false;
}

bool DragData::canSmartReplace() const
{
    // Mimic the situations in which mac allows drag&drop to do a smart replace.
    // This is allowed whenever the drag data contains a 'range' (ie.,
    // ClipboardWin::writeRange is called).  For example, dragging a link
    // should not result in a space being added.
    return m_platformDragData->types().contains(mimeTypeTextPlain)
        && !m_platformDragData->types().contains(mimeTypeTextURIList);
}

bool DragData::containsCompatibleContent() const
{
    return containsPlainText()
        || containsURL(0)
        || containsHTML(m_platformDragData)
        || containsColor()
        || containsFiles();
}

PassRefPtr<DocumentFragment> DragData::asFragment(Frame* frame, PassRefPtr<Range>, bool, bool&) const
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

    if (containsFiles()) {
        // FIXME: Implement this.  Should be pretty simple to make some HTML
        // and call createFragmentFromMarkup.
        //if (RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(doc,
        //    ?, KURL()))
        //    return fragment;
    }

    if (m_platformDragData->types().contains(mimeTypeTextHTML)) {
        String html;
        KURL baseURL;
        m_platformDragData->htmlAndBaseURL(html, baseURL);
        if (RefPtr<DocumentFragment> fragment = createFragmentFromMarkup(frame->document(), html, baseURL, FragmentScriptingAndPluginContentNotAllowed))
            return fragment.release();
    }

    return 0;
}

Color DragData::asColor() const
{
    notImplemented();
    return Color();
}

} // namespace WebCore
