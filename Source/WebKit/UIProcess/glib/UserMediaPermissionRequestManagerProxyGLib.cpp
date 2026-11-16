/*
 * Copyright (C) 2020 Igalia S.L.
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
 */

#include "config.h"
#include "UserMediaPermissionRequestManagerProxy.h"

#include "UserMediaCaptureManagerMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <WebCore/CaptureDeviceWithCapabilities.h>
#include <WebCore/ContextDestructionObserverInlines.h>
#include <WebCore/MediaConstraintType.h>
#include <WebCore/UserMediaRequest.h>

namespace IPC {
class Decoder;
template<> struct ArgumentCoder<WebCore::RealtimeMediaSourceCenter::ValidDevices> {
    static std::optional<WebCore::RealtimeMediaSourceCenter::ValidDevices> decode(Decoder&);
};
}

namespace WebKit {
using namespace WebCore;

void UserMediaPermissionRequestManagerProxy::validateUserMediaRequestConstraints(RealtimeMediaSourceCenter::ValidateHandler&& validateHandler, WebCore::MediaDeviceHashSalts&& deviceIDHashSalts)
{
    protect(m_page->legacyMainFrameProcess().connection())->sendWithAsyncReply(Messages::UserMediaCaptureManager::ValidateUserMediaRequestConstraints(m_currentUserMediaRequest->userRequest(), WTF::move(deviceIDHashSalts)), WTF::move(validateHandler));
}

void UserMediaPermissionRequestManagerProxy::platformGetMediaStreamDevices(bool revealIdsAndLabels, CompletionHandler<void(Vector<CaptureDeviceWithCapabilities>&&)>&& completionHandler)
{
    protect(m_page->legacyMainFrameProcess().connection())->sendWithAsyncReply(Messages::UserMediaCaptureManager::GetMediaStreamDevices(revealIdsAndLabels), WTF::move(completionHandler));
}

} // namespace WebKit
