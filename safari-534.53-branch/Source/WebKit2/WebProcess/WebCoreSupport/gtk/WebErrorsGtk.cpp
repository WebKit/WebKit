/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
 * Copyright (C) 2011 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebErrors.h"

#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>
#include <glib/gi18n-lib.h>
#include <webkit/webkiterror.h>

using namespace WebCore;

namespace WebKit {

ResourceError cancelledError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_NETWORK_ERROR), WEBKIT_NETWORK_ERROR_CANCELLED,
                         request.url().string(), _("Load request cancelled"));
}

ResourceError blockedError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT,
                         request.url().string(), _("Not allowed to use restricted network port"));
}

ResourceError cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_CANNOT_SHOW_URL,
                         request.url().string(), _("URL cannot be shown"));
}

ResourceError interruptForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE,
                         request.url().string(), _("Frame load was interrupted"));
}

ResourceError cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError(g_quark_to_string(WEBKIT_POLICY_ERROR), WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE,
                         response.url().string(), _("Content with the specified MIME type cannot be shown"));
}

ResourceError fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError(g_quark_to_string(WEBKIT_NETWORK_ERROR), WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST,
                         response.url().string(), _("File does not exist"));
}

ResourceError pluginWillHandleLoadError(const ResourceResponse& response)
{
    return ResourceError(g_quark_to_string(WEBKIT_PLUGIN_ERROR), WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD,
                         response.url().string(), _("Plugin will handle load"));
}

} // namespace WebKit
