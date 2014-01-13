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
 
#import "WebDatabaseManagerClient.h"

#if ENABLE(SQL_DATABASE)

#import "WebDatabaseManagerPrivate.h"
#import "WebSecurityOriginInternal.h"
#import <wtf/MainThread.h>
#import <wtf/RetainPtr.h>
#import <WebCore/DatabaseTracker.h>
#import <WebCore/SecurityOrigin.h>

#if PLATFORM(IOS)
#import <WebCore/WebCoreThread.h>
#endif

using namespace WebCore;

#if PLATFORM(IOS)
static CFStringRef WebDatabaseOriginWasAddedNotification = CFSTR("com.apple.MobileSafariSettings.WebDatabaseOriginWasAddedNotification");
static CFStringRef WebDatabaseWasDeletedNotification = CFSTR("com.apple.MobileSafariSettings.WebDatabaseWasDeletedNotification");
static CFStringRef WebDatabaseOriginWasDeletedNotification = CFSTR("com.apple.MobileSafariSettings.WebDatabaseOriginWasDeletedNotification");
#endif

WebDatabaseManagerClient* WebDatabaseManagerClient::sharedWebDatabaseManagerClient()
{
    static WebDatabaseManagerClient* sharedClient = new WebDatabaseManagerClient();
    return sharedClient;
}

#if PLATFORM(IOS)
static void onNewDatabaseOriginAdded(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT(observer);
    
    WebDatabaseManagerClient* client = reinterpret_cast<WebDatabaseManagerClient*>(observer);
    client->newDatabaseOriginWasAdded();
}

static void onDatabaseDeleted(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT(observer);
    
    WebDatabaseManagerClient* client = reinterpret_cast<WebDatabaseManagerClient*>(observer);
    client->databaseWasDeleted();
}

static void onDatabaseOriginDeleted(CFNotificationCenterRef, void* observer, CFStringRef, const void*, CFDictionaryRef)
{
    ASSERT(observer);
    
    WebDatabaseManagerClient* client = reinterpret_cast<WebDatabaseManagerClient*>(observer);
    client->databaseOriginWasDeleted();
}
#endif

WebDatabaseManagerClient::WebDatabaseManagerClient()
#if PLATFORM(IOS)
    : m_isHandlingNewDatabaseOriginNotification(false)
    , m_isHandlingDeleteDatabaseNotification(false)
    , m_isHandlingDeleteDatabaseOriginNotification(false)
#endif
{
#if PLATFORM(IOS)
    CFNotificationCenterRef center = CFNotificationCenterGetDarwinNotifyCenter(); 
    CFNotificationCenterAddObserver(center, this, onNewDatabaseOriginAdded, WebDatabaseOriginWasAddedNotification, 0, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, onDatabaseDeleted, WebDatabaseWasDeletedNotification, 0, CFNotificationSuspensionBehaviorDeliverImmediately);
    CFNotificationCenterAddObserver(center, this, onDatabaseOriginDeleted, WebDatabaseOriginWasDeletedNotification, 0, CFNotificationSuspensionBehaviorDeliverImmediately);    
#endif
}

WebDatabaseManagerClient::~WebDatabaseManagerClient()
{
#if PLATFORM(IOS)
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetDarwinNotifyCenter(), this, 0, 0);
#endif
}

class DidModifyOriginData {
    WTF_MAKE_NONCOPYABLE(DidModifyOriginData);
public:
    static void dispatchToMainThread(WebDatabaseManagerClient* client, SecurityOrigin* origin)
    {
        DidModifyOriginData* context = new DidModifyOriginData(client, origin->isolatedCopy());
        callOnMainThread(&DidModifyOriginData::dispatchDidModifyOriginOnMainThread, context);
    }

private:
    DidModifyOriginData(WebDatabaseManagerClient* client, PassRefPtr<SecurityOrigin> origin)
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

    WebDatabaseManagerClient* client;
    RefPtr<SecurityOrigin> origin;
};

void WebDatabaseManagerClient::dispatchDidModifyOrigin(SecurityOrigin* origin)
{
    if (!isMainThread()) {
        DidModifyOriginData::dispatchToMainThread(this, origin);
        return;
    }

    RetainPtr<WebSecurityOrigin> webSecurityOrigin = adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin]);

    [[NSNotificationCenter defaultCenter] postNotificationName:WebDatabaseDidModifyOriginNotification 
                                                        object:webSecurityOrigin.get()];
}

void WebDatabaseManagerClient::dispatchDidModifyDatabase(SecurityOrigin* origin, const String& databaseIdentifier)
{
    if (!isMainThread()) {
        DidModifyOriginData::dispatchToMainThread(this, origin);
        return;
    }

    RetainPtr<WebSecurityOrigin> webSecurityOrigin = adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin]);
    RetainPtr<NSDictionary> userInfo = adoptNS([[NSDictionary alloc] 
                                               initWithObjectsAndKeys:(NSString *)databaseIdentifier, WebDatabaseIdentifierKey, nil]);
    
    [[NSNotificationCenter defaultCenter] postNotificationName:WebDatabaseDidModifyDatabaseNotification
                                                        object:webSecurityOrigin.get()
                                                      userInfo:userInfo.get()];
}

#if PLATFORM(IOS)
void WebDatabaseManagerClient::dispatchDidAddNewOrigin(SecurityOrigin*)
{    
    m_isHandlingNewDatabaseOriginNotification = true;
    // Send a notification to all apps that a new origin has been added, so other apps with opened database can refresh their origin maps.
    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), WebDatabaseOriginWasAddedNotification, 0, 0, true);
}

void WebDatabaseManagerClient::dispatchDidDeleteDatabase()
{
    m_isHandlingDeleteDatabaseNotification = true;
    // Send a notification to all apps that a database has been deleted, so other apps with the deleted database open will close it properly.
    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), WebDatabaseWasDeletedNotification, 0, 0, true);
}

void WebDatabaseManagerClient::dispatchDidDeleteDatabaseOrigin()
{
    m_isHandlingDeleteDatabaseOriginNotification = true;
    // Send a notification to all apps that an origin has been deleted, so other apps can update their origin maps.
    CFNotificationCenterPostNotification(CFNotificationCenterGetDarwinNotifyCenter(), WebDatabaseOriginWasDeletedNotification, 0, 0, true);
}

void WebDatabaseManagerClient::newDatabaseOriginWasAdded()
{
    // Locks the WebThread for the rest of the run loop.
    WebThreadLock();

    // If this is the process that added the new origin, its quota map should have been updated
    // and does not need to be invalidated.
    if (m_isHandlingNewDatabaseOriginNotification) {
        m_isHandlingNewDatabaseOriginNotification = false;
        return;
    }
    
    databaseOriginsDidChange();
}

void WebDatabaseManagerClient::databaseWasDeleted()
{
    // Locks the WebThread for the rest of the run loop.
    WebThreadLock();
    
    // If this is the process that added the new origin, its quota map should have been updated
    // and does not need to be invalidated.
    if (m_isHandlingDeleteDatabaseNotification) {
        m_isHandlingDeleteDatabaseNotification = false;
        return;
    }
    
    DatabaseTracker::tracker().removeDeletedOpenedDatabases();
}

void WebDatabaseManagerClient::databaseOriginWasDeleted()
{
    // Locks the WebThread for the rest of the run loop.
    WebThreadLock();
    
    // If this is the process that added the new origin, its quota map should have been updated
    // and does not need to be invalidated.
    if (m_isHandlingDeleteDatabaseOriginNotification) {
        m_isHandlingDeleteDatabaseOriginNotification = false;
        return;
    }

    databaseOriginsDidChange();
}

void WebDatabaseManagerClient::databaseOriginsDidChange()
{
    // Send a notification (within the app) about the origins change.  If an app needs to update its UI when the origins change
    // (such as Safari Settings), it can listen for that notification.
    CFNotificationCenterPostNotification(CFNotificationCenterGetLocalCenter(), WebDatabaseOriginsDidChangeNotification, 0, 0, true);
}

#endif // PLATFORM(IOS)

#endif
