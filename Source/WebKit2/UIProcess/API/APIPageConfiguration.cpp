/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "APIPageConfiguration.h"

#include "APIProcessPoolConfiguration.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebUserContentControllerProxy.h"

using namespace WebKit;

namespace API {

PageConfiguration::PageConfiguration()
{
}

PageConfiguration::~PageConfiguration()
{
}

WebProcessPool* PageConfiguration::processPool()
{
    return m_processPool.get();
}

void PageConfiguration::setProcessPool(WebProcessPool* processPool)
{
    m_processPool = processPool;
}

WebUserContentControllerProxy* PageConfiguration::userContentController()
{
    return m_userContentController.get();
}

void PageConfiguration::setUserContentController(WebUserContentControllerProxy* userContentController)
{
    m_userContentController = userContentController;
}

WebPageGroup* PageConfiguration::pageGroup()
{
    return m_pageGroup.get();
}

void PageConfiguration::setPageGroup(WebPageGroup* pageGroup)
{
    m_pageGroup = pageGroup;
}

WebPreferences* PageConfiguration::preferences()
{
    return m_preferences.get();
}

void PageConfiguration::setPreferences(WebPreferences* preferences)
{
    m_preferences = preferences;
}

WebPageProxy* PageConfiguration::relatedPage()
{
    return m_relatedPage.get();
}

void PageConfiguration::setRelatedPage(WebPageProxy* relatedPage)
{
    m_relatedPage = relatedPage;
}

WebKit::WebPageConfiguration PageConfiguration::webPageConfiguration()
{
    WebKit::WebPageConfiguration configuration;

    configuration.userContentController = userContentController();
    configuration.pageGroup = pageGroup();
    configuration.preferences = preferences();
    configuration.relatedPage = relatedPage();

    return configuration;
}

} // namespace API
