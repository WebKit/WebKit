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

#ifndef WebIconDatabase_h
#define WebIconDatabase_h

#include "APIObject.h"

#include "Connection.h"
#include <WebCore/IconDatabaseBase.h>
#include <wtf/Forward.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace CoreIPC {
class ArgumentDecoder;
class DataReference;
class MessageID;
}

namespace WebKit {

class WebContext;

class WebIconDatabase : public APIObject {
public:
    static const Type APIType = TypeIconDatabase;

    static PassRefPtr<WebIconDatabase> create(WebContext*);
    virtual ~WebIconDatabase();

    void invalidate();
    void clearContext() { m_webContext = 0; }

    void retainIconForPageURL(const String&);
    void releaseIconForPageURL(const String&);
    void setIconURLForPageURL(const String&, const String&);
    void setIconDataForIconURL(const CoreIPC::DataReference&, const String&);
    
    void synchronousIconDataForPageURL(const String&, CoreIPC::DataReference&);
    void synchronousIconURLForPageURL(const String&, String&);
    void synchronousIconDataKnownForIconURL(const String&, bool&) const;
    void synchronousLoadDecisionForIconURL(const String&, int&) const;

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

private:
    WebIconDatabase(WebContext*);

    virtual Type type() const { return APIType; }

    void didReceiveWebIconDatabaseMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebIconDatabaseMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    WebContext* m_webContext;
};

} // namespace WebKit

#endif // WebIconDatabase_h
