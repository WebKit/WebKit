/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "UserStyleSheetLoader.h"

#include "CachedCSSStyleSheet.h"
#include "DocLoader.h"
#include "Frame.h"

using namespace WebCore;

UserStyleSheetLoader::UserStyleSheetLoader(PassRefPtr<Document> document, const String& url)
    : m_document(document)
    , m_cachedSheet(m_document->docLoader()->requestUserCSSStyleSheet(url, ""))
{
    if (m_cachedSheet) {
        m_document->addPendingSheet();
        m_cachedSheet->addClient(this);
    }
}

UserStyleSheetLoader::~UserStyleSheetLoader()
{
    if (m_cachedSheet) {
        if (!m_cachedSheet->isLoaded())
            m_document->removePendingSheet();
        m_cachedSheet->removeClient(this);
    }
}

void UserStyleSheetLoader::setCSSStyleSheet(const String& /*URL*/, const String& /*charset*/, const CachedCSSStyleSheet* sheet)
{
    m_document->removePendingSheet();
    if (Frame* frame = m_document->frame())
        frame->setUserStyleSheet(sheet->sheetText());
}
