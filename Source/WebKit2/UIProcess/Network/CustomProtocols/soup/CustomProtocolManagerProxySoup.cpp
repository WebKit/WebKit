/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "CustomProtocolManagerProxy.h"

#if ENABLE(CUSTOM_PROTOCOLS)

#include "ChildProcessProxy.h"
#include "CustomProtocolManagerMessages.h"
#include "CustomProtocolManagerProxyMessages.h"
#include <WebCore/NotImplemented.h>
#include <WebCore/ResourceRequest.h>

namespace WebKit {

CustomProtocolManagerProxy::CustomProtocolManagerProxy(ChildProcessProxy* childProcessProxy)
    : m_childProcessProxy(childProcessProxy)
{
    ASSERT(m_childProcessProxy);
    m_childProcessProxy->addMessageReceiver(Messages::CustomProtocolManagerProxy::messageReceiverName(), *this);
}

void CustomProtocolManagerProxy::startLoading(uint64_t, const WebCore::ResourceRequest&)
{
    notImplemented();
}

void CustomProtocolManagerProxy::stopLoading(uint64_t)
{
    notImplemented();
}

} // namespace WebKit

#endif // ENABLE(CUSTOM_PROTOCOLS)
