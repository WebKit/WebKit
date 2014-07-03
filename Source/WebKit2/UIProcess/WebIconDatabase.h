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
#include "WebIconDatabaseClient.h"
#include <WebCore/IconDatabaseClient.h>
#include <WebCore/IntSize.h>
#include <WebCore/NativeImagePtr.h>

namespace API {
class Data;
}

namespace WebCore {
class IconDatabase;
class Image;
}

namespace WebKit {

class WebContext;

class WebIconDatabase : public API::ObjectImpl<API::Object::Type::IconDatabase>, private WebCore::IconDatabaseClient, private IPC::MessageReceiver {
public:
    static PassRefPtr<WebIconDatabase> create(WebContext*);
    virtual ~WebIconDatabase();

    void invalidate();
    void clearContext() { m_webContext = nullptr; }
    void setDatabasePath(const String&);
    void enableDatabaseCleanup();

    void retainIconForPageURL(const String&);
    void releaseIconForPageURL(const String&);
    void setIconURLForPageURL(const String&, const String&);
    void setIconDataForIconURL(const IPC::DataReference&, const String&);

    void synchronousIconDataForPageURL(const String&, IPC::DataReference&);
    void synchronousIconURLForPageURL(const String&, String&);
    void synchronousIconDataKnownForIconURL(const String&, bool&) const;
    void synchronousLoadDecisionForIconURL(const String&, int&) const;

    void getLoadDecisionForIconURL(const String&, uint64_t callbackID);
    void didReceiveIconForPageURL(const String&);

    WebCore::Image* imageForPageURL(const String&, const WebCore::IntSize& iconSize = WebCore::IntSize(32, 32));
    WebCore::NativeImagePtr nativeImageForPageURL(const String&, const WebCore::IntSize& iconSize = WebCore::IntSize(32, 32));
    PassRefPtr<API::Data> iconDataForPageURL(const String& pageURL);

    bool isOpen();
    bool isUrlImportCompleted();

    void removeAllIcons();
    void checkIntegrityBeforeOpening();
    void close();

    void initializeIconDatabaseClient(const WKIconDatabaseClientBase*);

    void setPrivateBrowsingEnabled(bool);

    // Called when the WebContext is through with this WebIconDatabase but the
    // WebCore::IconDatabase possibly isn't done shutting down.
    // In that case this WebIconDatabase will deref() itself when the time is right.
    void derefWhenAppropriate();

private:
    explicit WebIconDatabase(WebContext&);

    // WebCore::IconDatabaseClient
    virtual void didImportIconURLForPageURL(const String&) override;
    virtual void didImportIconDataForPageURL(const String&) override;
    virtual void didChangeIconForPageURL(const String&) override;
    virtual void didRemoveAllIcons() override;
    virtual void didFinishURLImport() override;
    virtual void didClose() override;

    // IPC::MessageReceiver
    virtual void didReceiveMessage(IPC::Connection*, IPC::MessageDecoder&) override;
    virtual void didReceiveSyncMessage(IPC::Connection*, IPC::MessageDecoder&, std::unique_ptr<IPC::MessageEncoder>&) override;

    void notifyIconDataReadyForPageURL(const String&);

    WebContext* m_webContext;

    std::unique_ptr<WebCore::IconDatabase> m_iconDatabaseImpl;
    bool m_urlImportCompleted;
    bool m_databaseCleanupDisabled;
    bool m_shouldDerefWhenAppropriate;
    HashMap<uint64_t, String> m_pendingLoadDecisionURLMap;

    WebIconDatabaseClient m_iconDatabaseClient;
};

} // namespace WebKit

#endif // WebIconDatabase_h
