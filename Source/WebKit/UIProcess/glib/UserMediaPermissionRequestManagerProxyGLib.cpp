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
#include <WebCore/UserMediaRequest.h>

namespace WebKit {
using namespace WebCore;

void UserMediaPermissionRequestManagerProxy::platformValidateUserMediaRequestConstraints(RealtimeMediaSourceCenter::ValidConstraintsHandler&& validHandler, RealtimeMediaSourceCenter::InvalidConstraintsHandler&& invalidHandler, WebCore::MediaDeviceHashSalts&& deviceIDHashSalts)
{
    m_page.process().connection()->sendWithAsyncReply(Messages::UserMediaCaptureManager::ValidateUserMediaRequestConstraints(m_currentUserMediaRequest->userRequest(), WTFMove(deviceIDHashSalts)), [validHandler = WTFMove(validHandler), invalidHandler = WTFMove(invalidHandler)](std::optional<String> invalidConstraint, Vector<WebCore::CaptureDevice> audioDevices, Vector<WebCore::CaptureDevice> videoDevices) mutable {
        if (invalidConstraint)
            invalidHandler(*invalidConstraint);
        else
            validHandler(WTFMove(audioDevices), WTFMove(videoDevices));
    });
}

void UserMediaPermissionRequestManagerProxy::platformGetMediaStreamDevices(CompletionHandler<void(Vector<CaptureDevice>&&)>&& completionHandler)
{
    m_page.process().connection()->sendWithAsyncReply(Messages::UserMediaCaptureManager::GetMediaStreamDevices(), WTFMove(completionHandler));
}

} // namespace WebKit
