/*
 *  Copyright (C) 2026 Igalia. S.L. . All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include "config.h"
#include <wtf/NotificationPoint.h>

#include <wtf/DispatchExtras.h>

namespace WTF {

std::optional<dispatch_queue_t> NotificationPoint::s_dispatchQueue;

static NotificationPoint::Error fromPlatformStatus(int status)
{
    switch (status) {
    case NOTIFY_STATUS_OK:
        RELEASE_ASSERT_NOT_REACHED();
    case NOTIFY_STATUS_INVALID_NAME:
    case NOTIFY_STATUS_INVALID_TOKEN:
    case NOTIFY_STATUS_INVALID_PORT:
    case NOTIFY_STATUS_INVALID_FILE:
    case NOTIFY_STATUS_INVALID_SIGNAL:
    case NOTIFY_STATUS_INVALID_REQUEST:
    case NOTIFY_STATUS_NULL_INPUT:
        return NotificationPoint::Error::InvalidArgument;
    case NOTIFY_STATUS_NOT_AUTHORIZED:
        return NotificationPoint::Error::PermissionError;
    case NOTIFY_STATUS_OPT_DISABLE:
        return NotificationPoint::Error::Other;
    case NOTIFY_STATUS_SERVER_NOT_FOUND:
        return NotificationPoint::Error::ConnectionError;
    default:
        return NotificationPoint::Error::Other;
    }
}

NotificationPoint::NotificationPoint(String key, int token)
    : m_token(token)
    , m_key(key)
{
}

const String& NotificationPoint::key() const
{
    return m_key;
}

Expected<RefPtr<NotificationPoint>, NotificationPoint::Error> NotificationPoint::create(ASCIILiteral path, Function<void()>&& callback)
{
    return createWithName("com.apple.WebKit"_s, path, WTF::move(callback));
}

Expected<RefPtr<NotificationPoint>, NotificationPoint::Error> NotificationPoint::createWithName(ASCIILiteral name, ASCIILiteral path, Function<void()>&& callback)
{
    auto key = makeString(name, '.', path);
    int token = NOTIFY_TOKEN_INVALID, ret;
    if (callback) {
        __block auto capturedCallback = WTF::move(callback);
        dispatch_queue_t queue = s_dispatchQueue ? *s_dispatchQueue : mainDispatchQueueSingleton();
        ret = notify_register_dispatch(key.utf8().data(), &token, queue, ^(int)
            {
                capturedCallback();
            });
    } else
        ret = notify_register_check(key.utf8().data(), &token);
    if (ret)
        return makeUnexpected(fromPlatformStatus(ret));
    return adoptRef(new NotificationPoint(key, token));
}

NotificationPoint::~NotificationPoint()
{
    int ret = notify_cancel(m_token);
    RELEASE_ASSERT(!ret);
}

Expected<bool, NotificationPoint::Error> NotificationPoint::getAndClearNotificationsPosted()
{
    int value;
    auto status = notify_check(m_token, &value);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return value;
}

bool NotificationPoint::isLive() const
{
    return m_token > 0;
}

Expected<uint64_t, NotificationPoint::Error> NotificationPoint::getState()
{
    uint64_t state;
    auto status = notify_get_state(m_token, &state);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return state;
}

Expected<uint64_t, NotificationPoint::Error> NotificationPoint::getState(ASCIILiteral name, String path)
{
    int token;
    auto key = makeString(name, '.', path);
    auto status = notify_register_check(key.utf8().data(), &token);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    uint64_t state;
    status = notify_get_state(token, &state);
    notify_cancel(token);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return state;
}

Expected<void, NotificationPoint::Error> NotificationPoint::setState(uint64_t state)
{
    auto status = notify_set_state(m_token, state);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return { };
}

Expected<void, NotificationPoint::Error> NotificationPoint::setState(ASCIILiteral name, String path, uint64_t state)
{
    int token;
    auto key = makeString(name, '.', path);
    auto status = notify_register_check(key.utf8().data(), &token);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    status = notify_set_state(token, state);
    notify_cancel(token);
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return { };
}

Expected<void, NotificationPoint::Error> NotificationPoint::notify()
{
    auto status = notify_post(m_key.utf8().data());
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return { };
}

Expected<void, NotificationPoint::Error> NotificationPoint::notify(ASCIILiteral name, String path)
{
    auto key = makeString(name, '.', path);
    auto status = notify_post(key.utf8().data());
    if (status != NOTIFY_STATUS_OK)
        return makeUnexpected(fromPlatformStatus(status));
    return { };

}

bool NotificationPoint::testWTFisEmpty()
{
    return true;
}

};
