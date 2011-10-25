/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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
 
#import "WebDatabaseTrackerClient.h"

#if ENABLE(SQL_DATABASE)

#import "WebDatabaseManagerPrivate.h"
#import "WebSecurityOriginInternal.h"
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <WebCore/SecurityOrigin.h>

using namespace WebCore;

WebDatabaseTrackerClient* WebDatabaseTrackerClient::sharedWebDatabaseTrackerClient()
{
    static WebDatabaseTrackerClient* sharedClient = new WebDatabaseTrackerClient();
    return sharedClient;
}

WebDatabaseTrackerClient::WebDatabaseTrackerClient()
{
}

WebDatabaseTrackerClient::~WebDatabaseTrackerClient()
{
}

class DidModifyOriginData {
    WTF_MAKE_NONCOPYABLE(DidModifyOriginData);
public:
    static void dispatchToMainThread(WebDatabaseTrackerClient* client, SecurityOrigin* origin)
    {
        DidModifyOriginData* context = new DidModifyOriginData(client, origin->isolatedCopy());
        callOnMainThread(&DidModifyOriginData::dispatchDidModifyOriginOnMainThread, context);
    }

private:
    DidModifyOriginData(WebDatabaseTrackerClient* client, PassRefPtr<SecurityOrigin> origin)
        : client(client)
        , origin(origin)
    {
    }

    static void dispatchDidModifyOriginOnMainThread(void* context)
    {
        ASSERT(isMainThread());
        DidModifyOriginData* info = static_cast<DidModifyOriginData*>(context);
        info->client->dispatchDidModifyOrigin(info->origin.get());
        delete info;
    }

    WebDatabaseTrackerClient* client;
    RefPtr<SecurityOrigin> origin;
};

void WebDatabaseTrackerClient::dispatchDidModifyOrigin(SecurityOrigin* origin)
{
    if (!isMainThread()) {
        DidModifyOriginData::dispatchToMainThread(this, origin);
        return;
    }

    RetainPtr<WebSecurityOrigin> webSecurityOrigin(AdoptNS, [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin]);

    [[NSNotificationCenter defaultCenter] postNotificationName:WebDatabaseDidModifyOriginNotification 
                                                        object:webSecurityOrigin.get()];
}

void WebDatabaseTrackerClient::dispatchDidModifyDatabase(SecurityOrigin* origin, const String& databaseIdentifier)
{
    if (!isMainThread()) {
        DidModifyOriginData::dispatchToMainThread(this, origin);
        return;
    }

    RetainPtr<WebSecurityOrigin> webSecurityOrigin(AdoptNS, [[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin]);
    RetainPtr<NSDictionary> userInfo(AdoptNS, [[NSDictionary alloc] 
                                               initWithObjectsAndKeys:(NSString *)databaseIdentifier, WebDatabaseIdentifierKey, nil]);
    
    [[NSNotificationCenter defaultCenter] postNotificationName:WebDatabaseDidModifyDatabaseNotification
                                                        object:webSecurityOrigin.get()
                                                      userInfo:userInfo.get()];
}

#endif
