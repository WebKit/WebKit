/*
 * Copyright (C) 2007,2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import <WebCore/DatabaseManagerClient.h>

class WebDatabaseManagerClient final : public WebCore::DatabaseManagerClient {
public:
    static WebDatabaseManagerClient* sharedWebDatabaseManagerClient();
    
    virtual ~WebDatabaseManagerClient();
    void dispatchDidModifyOrigin(const WebCore::SecurityOriginData&) final;
    void dispatchDidModifyDatabase(const WebCore::SecurityOriginData&, const WTF::String& databaseIdentifier) final;

#if PLATFORM(IOS_FAMILY)
    void dispatchDidAddNewOrigin() final;
    void dispatchDidDeleteDatabase() final;
    void dispatchDidDeleteDatabaseOrigin() final;
    void newDatabaseOriginWasAdded();
    void databaseWasDeleted();
    void databaseOriginWasDeleted();
#endif

private:
    WebDatabaseManagerClient();

#if PLATFORM(IOS_FAMILY)
    void databaseOriginsDidChange();

    bool m_isHandlingNewDatabaseOriginNotification { false };
    bool m_isHandlingDeleteDatabaseNotification { false };
    bool m_isHandlingDeleteDatabaseOriginNotification { false };
#endif
};
