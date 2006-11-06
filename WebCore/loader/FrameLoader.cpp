/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameLoader.h"

#include "Chrome.h"
#include "DocumentLoader.h"
#include "Element.h"
#include "FloatRect.h"
#include "FormState.h"
#include "Frame.h"
#include "FrameLoadRequest.h"
#include "FrameLoaderClient.h"
#include "FrameTree.h"
#include "MainResourceLoader.h"
#include "Page.h"
#include "WindowFeatures.h"

namespace WebCore {

FrameLoader::FrameLoader(Frame* frame)
    : m_frame(frame)
    , m_client(0)
    , m_state(FrameStateCommittedPage)
    , m_loadType(FrameLoadTypeStandard)
    , m_delegateIsHandlingProvisionalLoadError(false)
    , m_delegateIsDecidingNavigationPolicy(false)
    , m_delegateIsHandlingUnimplementablePolicy(false)
    , m_firstLayoutDone(false)
    , m_quickRedirectComing(false)
    , m_sentRedirectNotification(false)
    , m_isStoppingLoad(false)
{
}

FrameLoader::~FrameLoader()
{
    if (m_client)
        m_client->detachFrameLoader();
}

static void setAllDefersLoading(const ResourceLoaderSet& loaders, bool defers)
{
    const ResourceLoaderSet copy = loaders;
    ResourceLoaderSet::const_iterator end = copy.end();
    for (ResourceLoaderSet::const_iterator it = copy.begin(); it != end; ++it)
        (*it)->setDefersLoading(defers);
}

void FrameLoader::setDefersLoading(bool defers)
{
    if (m_mainResourceLoader)
        m_mainResourceLoader->setDefersLoading(defers);
    setAllDefersLoading(m_subresourceLoaders, defers);
    setAllDefersLoading(m_plugInStreamLoaders, defers);
    m_client->setDefersLoading(defers);
}

Frame* FrameLoader::createWindow(const FrameLoadRequest& request, const WindowFeatures& features)
{ 
    ASSERT(!features.dialog || request.frameName().isEmpty());

    if (!request.frameName().isEmpty())
        if (Frame* frame = m_frame->tree()->find(request.frameName())) {
            if (!request.resourceRequest().url().isEmpty())
#if PLATFORM(MAC)
                frame->loader()->load(request, true, 0, 0, HashMap<String, String>());
#endif
            frame->page()->chrome()->focus();
            return frame;
        }

    // FIXME: Setting the referrer should be the caller's responsibility.
    FrameLoadRequest requestWithReferrer = request;
    requestWithReferrer.resourceRequest().setHTTPReferrer(m_frame->referrer());
    
    Page* page;
    if (features.dialog)
        page = m_frame->page()->chrome()->createModalDialog(requestWithReferrer);
    else
        page = m_frame->page()->chrome()->createWindow(requestWithReferrer);
    if (!page)
        return 0;

    Frame* frame = page->mainFrame();
    frame->tree()->setName(request.frameName());

    page->chrome()->setToolbarsVisible(features.toolBarVisible || features.locationBarVisible);
    page->chrome()->setStatusbarVisible(features.statusBarVisible);
    page->chrome()->setScrollbarsVisible(features.scrollbarsVisible);
    page->chrome()->setMenubarVisible(features.menuBarVisible);
    page->chrome()->setResizable(features.resizable);

    // 'x' and 'y' specify the location of the window, while 'width' and 'height' 
    // specify the size of the page. We can only resize the window, so 
    // adjust for the difference between the window size and the page size.

    FloatRect windowRect = page->chrome()->windowRect();
    FloatSize pageSize = page->chrome()->pageRect().size();
    if (features.xSet)
        windowRect.setX(features.x);
    if (features.ySet)
        windowRect.setY(features.y);
    if (features.widthSet)
        windowRect.setWidth(features.width + (windowRect.width() - pageSize.width()));
    if (features.heightSet)
        windowRect.setHeight(features.height + (windowRect.height() - pageSize.height()));
    page->chrome()->setWindowRect(windowRect);

    page->chrome()->show();

    return frame;
}

}
