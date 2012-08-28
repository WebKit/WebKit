/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "UserMediaClientImpl.h"

#if ENABLE(MEDIA_STREAM)
#include "MediaStreamDescriptor.h"
#include "WebPage.h"

#include <BlackBerryPlatformWebMediaStreamDescriptor.h>
#include <BlackBerryPlatformWebUserMedia.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

using namespace BlackBerry::Platform;

namespace WebCore {

class WebUserMediaRequestClientImpl;

typedef HashMap<UserMediaRequest*, OwnPtr<WebUserMediaRequestClientImpl> > UserMediaRequestsMap;

static UserMediaRequestsMap& userMediaRequestsMap()
{
    DEFINE_STATIC_LOCAL(UserMediaRequestsMap, requests, ());
    return requests;
}

static PassRefPtr<MediaStreamSource> toMediaStreamSource(const WebMediaStreamSource& src)
{
    return MediaStreamSource::create(String::fromUTF8(src.id().c_str()), static_cast<MediaStreamSource::Type>(src.type()), String::fromUTF8(src.name().c_str()));
}

static PassRefPtr<MediaStreamDescriptor> toMediaStreamDescriptor(const WebMediaStreamDescriptor& d)
{
    MediaStreamSourceVector audioSources;
    for (size_t i = 0; i < d.audios().size(); i++) {
        RefPtr<MediaStreamSource> src = toMediaStreamSource(d.audios()[i]);
        audioSources.append(src.release());
    }

    MediaStreamSourceVector videoSources;
    for (size_t i = 0; i < d.videos().size(); i++) {
        RefPtr<MediaStreamSource> src = toMediaStreamSource(d.videos()[i]);
        videoSources.append(src.release());
    }

    return MediaStreamDescriptor::create(String::fromUTF8(d.label().c_str()), audioSources, videoSources);
}

class WebUserMediaRequestClientImpl : public WebUserMediaRequestClient {
public:
    WebUserMediaRequestClientImpl(PassRefPtr<UserMediaRequest> prpRequest)
        : m_request(prpRequest)
    {
    }

    void requestSucceeded(const WebMediaStreamDescriptor& d)
    {
        if (m_request) {
            RefPtr<MediaStreamDescriptor> descriptor = toMediaStreamDescriptor(d);
            m_request->succeed(descriptor);

            userMediaRequestsMap().remove(m_request.get());
        }
    }

    void requestFailed()
    {
        if (m_request) {
            m_request->fail();
            userMediaRequestsMap().remove(m_request.get());
        }
    }

private:
    RefPtr<UserMediaRequest> m_request;
};

UserMediaClientImpl::UserMediaClientImpl(BlackBerry::WebKit::WebPage*)
    : m_webUserMedia(0)
{
}

UserMediaClientImpl::~UserMediaClientImpl()
{
    BlackBerry::Platform::deleteGuardedObject(m_webUserMedia);
}

void UserMediaClientImpl::pageDestroyed()
{
}

void UserMediaClientImpl::requestUserMedia(PassRefPtr<UserMediaRequest> prpRequest, const MediaStreamSourceVector& audioSources, const MediaStreamSourceVector& videoSources)
{
    UserMediaRequest* request = prpRequest.get();
    OwnPtr<WebUserMediaRequestClientImpl> requestClient = adoptPtr(new WebUserMediaRequestClientImpl(prpRequest));

    webUserMedia()->requestUserMedia(WebUserMediaRequest(request->audio(), request->video(), requestClient.get()));
    userMediaRequestsMap().add(request, requestClient.release());
}

void UserMediaClientImpl::cancelUserMediaRequest(UserMediaRequest* request)
{
    UserMediaRequestsMap::iterator it = userMediaRequestsMap().find(request);
    if (it == userMediaRequestsMap().end())
        return;

    webUserMedia()->cancelUserMediaRequest(WebUserMediaRequest(request->audio(), request->video(), it->second.get()));
    userMediaRequestsMap().remove(it);
}

BlackBerry::Platform::WebUserMedia* UserMediaClientImpl::webUserMedia()
{
    if (!m_webUserMedia)
        m_webUserMedia = new WebUserMedia;
    return m_webUserMedia;
}

}
#endif
