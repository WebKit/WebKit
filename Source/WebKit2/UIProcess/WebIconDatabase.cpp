/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WebIconDatabase.h"

#include "DataReference.h"
#include <wtf/text/WTFString.h>

namespace WebKit {

PassRefPtr<WebIconDatabase> WebIconDatabase::create(WebContext* context)
{
    return adoptRef(new WebIconDatabase(context));
}

WebIconDatabase::~WebIconDatabase()
{
}

WebIconDatabase::WebIconDatabase(WebContext* context)
    : m_webContext(context)
{
}

void WebIconDatabase::invalidate()
{
}

void WebIconDatabase::retainIconForPageURL(const String&)
{
}

void WebIconDatabase::releaseIconForPageURL(const String&)
{
}

void WebIconDatabase::setIconURLForPageURL(const String&, const String&)
{
}

void WebIconDatabase::setIconDataForIconURL(const CoreIPC::DataReference&, const String&)
{
}

void WebIconDatabase::synchronousIconDataForPageURL(const String&, CoreIPC::DataReference& iconData)
{
    iconData = CoreIPC::DataReference();
}

void WebIconDatabase::synchronousIconURLForPageURL(const String&, String& iconURL)
{
    iconURL = String();
}

void WebIconDatabase::synchronousIconDataKnownForIconURL(const String&, bool& iconDataKnown) const
{
    iconDataKnown = false;
}

void WebIconDatabase::synchronousLoadDecisionForIconURL(const String&, int& loadDecision) const
{
    loadDecision = (int)WebCore::IconLoadNo;
}

void WebIconDatabase::didReceiveMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* decoder)
{
    didReceiveWebIconDatabaseMessage(connection, messageID, decoder);
}

CoreIPC::SyncReplyMode WebIconDatabase::didReceiveSyncMessage(CoreIPC::Connection* connection, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder* decoder, CoreIPC::ArgumentEncoder* reply)
{
    return didReceiveSyncWebIconDatabaseMessage(connection, messageID, decoder, reply);
}

} // namespace WebKit
