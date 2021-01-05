/*
 * Copyright 2017, Adrien Destugues, pulkomandy@pulkomandy.tk
 * Distributed under terms of the MIT license.
 */


#include "NotificationClientHaiku.h"

#include "WebPage.h"

namespace WebCore {

class NotificationClientHaiku::SynchronousListener
    : public BUrlSynchronousRequest
{
    public:
        SynchronousListener(BUrlRequest& request)
            : BUrlSynchronousRequest(request)
        {}
        virtual	~SynchronousListener() {};
        void	DataReceived(BUrlRequest*, const char* data, off_t position,
                ssize_t size) override {
            result.WriteAt(position, data, size);
        }

        const BUrlResult&  Result() const override
            { return fWrappedRequest.Result(); }
        status_t    _ProtocolLoop() override
            { return B_ERROR; }

        BMallocIO result;
};


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
    BUrlRequest* request = BUrlProtocolRoster::MakeRequest(iconURL);
    if (request) {
        SynchronousListener synchronous(*request);
        synchronous.Perform();
        synchronous.WaitUntilCompletion();

        BBitmap* bitmap = BTranslationUtils::GetBitmap(&synchronous.result);
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
