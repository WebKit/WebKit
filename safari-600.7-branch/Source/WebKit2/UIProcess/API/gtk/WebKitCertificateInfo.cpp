/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
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
#include "WebKitCertificateInfo.h"

#include "WebKitCertificateInfoPrivate.h"
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION: WebKitCertificateInfo
 * @Short_description: Boxed type to encapsulate TLS certificate information
 * @Title: WebKitCertificateInfo
 * @See_also: #WebKitWebView, #WebKitWebContext
 *
 * When a client loads a page over HTTPS where there is an underlying TLS error
 * WebKit will fire a #WebKitWebView::load-failed-with-tls-errors signal with a
 * #WebKitCertificateInfo and the host of the failing URI.
 *
 * To handle this signal asynchronously you should make a copy of the
 * #WebKitCertificateInfo with webkit_certificate_info_copy().
 */

G_DEFINE_BOXED_TYPE(WebKitCertificateInfo, webkit_certificate_info, webkit_certificate_info_copy, webkit_certificate_info_free)

const CertificateInfo& webkitCertificateInfoGetCertificateInfo(WebKitCertificateInfo* info)
{
    ASSERT(info);
    return info->certificateInfo;
}

/**
 * webkit_certificate_info_copy:
 * @info: a #WebKitCertificateInfo
 *
 * Make a copy of the #WebKitCertificateInfo.
 *
 * Returns: (transfer full): A copy of passed in #WebKitCertificateInfo.
 *
 * Since: 2.4
 */
WebKitCertificateInfo* webkit_certificate_info_copy(WebKitCertificateInfo* info)
{
    g_return_val_if_fail(info, 0);

    WebKitCertificateInfo* copy = g_slice_new0(WebKitCertificateInfo);
    new (copy) WebKitCertificateInfo(info);
    return copy;
}

/**
 * webkit_certificate_info_free:
 * @info: a #WebKitCertificateInfo
 *
 * Free the #WebKitCertificateInfo.
 *
 * Since: 2.4
 */
void webkit_certificate_info_free(WebKitCertificateInfo* info)
{
    g_return_if_fail(info);

    info->~WebKitCertificateInfo();
    g_slice_free(WebKitCertificateInfo, info);
}

/**
 * webkit_certificate_info_get_tls_certificate:
 * @info: a #WebKitCertificateInfo
 *
 * Get the #GTlsCertificate associated with this
 * #WebKitCertificateInfo.
 *
 * Returns: (transfer none): The certificate of @info.
 *
 * Since: 2.4
 */
GTlsCertificate* webkit_certificate_info_get_tls_certificate(WebKitCertificateInfo *info)
{
    g_return_val_if_fail(info, 0);

    return info->certificateInfo.certificate();
}

/**
 * webkit_certificate_info_get_tls_errors:
 * @info: a #WebKitCertificateInfo
 *
 * Get the #GTlsCertificateFlags verification status associated with this
 * #WebKitCertificateInfo.
 *
 * Returns: The verification status of @info.
 *
 * Since: 2.4
 */
GTlsCertificateFlags webkit_certificate_info_get_tls_errors(WebKitCertificateInfo *info)
{
    g_return_val_if_fail(info, static_cast<GTlsCertificateFlags>(0));

    return info->certificateInfo.tlsErrors();
}
