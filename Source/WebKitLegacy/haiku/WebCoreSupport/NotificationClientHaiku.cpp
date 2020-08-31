/*
 * Copyright 2017, Adrien Destugues, pulkomandy@pulkomandy.tk
 * Distributed under terms of the MIT license.
 */


#include "NotificationClientHaiku.h"

#include "WebPage.h"

namespace WebCore {

BNotification
NotificationClientHaiku::fromDescriptor(Notification* descriptor)
{
    BNotification notification(B_INFORMATION_NOTIFICATION);
    notification.SetGroup("WebPositive");
    // Unfortunately, we don't get a website name or so…
    if (descriptor->body().length() > 0) {
        notification.SetTitle(descriptor->title());
        notification.SetContent(descriptor->body());
    } else {
        notification.SetContent(descriptor->title());
    }

    // TODO we should cache the data, in case the notification is re-sent
    // with some changes for an update.
    BUrl iconURL(descriptor->icon());
    BMallocIO buffer;
    BUrlRequest* request = BUrlProtocolRoster::MakeRequest(iconURL, &buffer);
    if (request) {
        thread_id thread = request->Run();
        status_t dummy;
        if (thread > B_OK)
            wait_for_thread(thread, &dummy);

        BBitmap* bitmap = BTranslationUtils::GetBitmap(&buffer);
        if (bitmap) {
            notification.SetIcon(bitmap);
            delete bitmap;
        }

        delete request;
    }

    notification.SetMessageID(descriptor->tag());

    return notification;
}

};
