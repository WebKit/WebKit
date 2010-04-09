/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "WebErrors.h"

#include <WebCore/ResourceRequest.h>
#include <WebCore/ResourceResponse.h>

using namespace WebCore;

namespace WebKit {

#define WebURLErrorDomain TEXT("CFURLErrorDomain")

enum {
    WebURLErrorUnknown =                         -1,
    WebURLErrorCancelled =                       -999,
    WebURLErrorBadURL =                          -1000,
    WebURLErrorTimedOut =                        -1001,
    WebURLErrorUnsupportedURL =                  -1002,
    WebURLErrorCannotFindHost =                  -1003,
    WebURLErrorCannotConnectToHost =             -1004,
    WebURLErrorNetworkConnectionLost =           -1005,
    WebURLErrorDNSLookupFailed =                 -1006,
    WebURLErrorHTTPTooManyRedirects =            -1007,
    WebURLErrorResourceUnavailable =             -1008,
    WebURLErrorNotConnectedToInternet =          -1009,
    WebURLErrorRedirectToNonExistentLocation =   -1010,
    WebURLErrorBadServerResponse =               -1011,
    WebURLErrorUserCancelledAuthentication =     -1012,
    WebURLErrorUserAuthenticationRequired =      -1013,
    WebURLErrorZeroByteResource =                -1014,
    WebURLErrorFileDoesNotExist =                -1100,
    WebURLErrorFileIsDirectory =                 -1101,
    WebURLErrorNoPermissionsToReadFile =         -1102,
    WebURLErrorSecureConnectionFailed =          -1200,
    WebURLErrorServerCertificateHasBadDate =     -1201,
    WebURLErrorServerCertificateUntrusted =      -1202,
    WebURLErrorServerCertificateHasUnknownRoot = -1203,
    WebURLErrorServerCertificateNotYetValid =    -1204,
    WebURLErrorClientCertificateRejected =       -1205,
    WebURLErrorClientCertificateRequired =       -1206,
    WebURLErrorCannotLoadFromNetwork =           -2000,

    // Download and file I/O errors
    WebURLErrorCannotCreateFile =                -3000,
    WebURLErrorCannotOpenFile =                  -3001,
    WebURLErrorCannotCloseFile =                 -3002,
    WebURLErrorCannotWriteToFile =               -3003,
    WebURLErrorCannotRemoveFile =                -3004,
    WebURLErrorCannotMoveFile =                  -3005,
    WebURLErrorDownloadDecodingFailedMidStream = -3006,
    WebURLErrorDownloadDecodingFailedToComplete =-3007,
};

#define WebKitErrorDomain TEXT("WebKitErrorDomain")

enum {
    WebKitErrorCannotShowMIMEType =                             100,
    WebKitErrorCannotShowURL =                                  101,
    WebKitErrorFrameLoadInterruptedByPolicyChange =             102,
    WebKitErrorCannotUseRestrictedPort =                        103,
};

enum {
    WebKitErrorCannotFindPlugIn =                               200,
    WebKitErrorCannotLoadPlugIn =                               201,
    WebKitErrorJavaUnavailable =                                202,
};

enum {
    WebKitErrorGeolocationLocationUnknown  =                    300,
};

#define WebKitErrorMIMETypeKey TEXT("WebKitErrorMIMETypeKey")
#define WebKitErrorPlugInNameKey TEXT("WebKitErrorPlugInNameKey")
#define WebKitErrorPlugInPageURLStringKey TEXT("WebKitErrorPlugInPageURLStringKey")

#define WebPOSIXErrorDomain TEXT("NSPOSIXErrorDomain")
#define WebPOSIXErrorECONNRESET 54

ResourceError cancelledError(const ResourceRequest& request)
{
    return ResourceError(String(WebURLErrorDomain), WebURLErrorCancelled, request.url().string(), String());
}

ResourceError blockedError(const ResourceRequest& request)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorCannotUseRestrictedPort, request.url().string(), String());
}

ResourceError cannotShowURLError(const ResourceRequest& request)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorCannotShowURL, request.url().string(), String());
}

ResourceError interruptForPolicyChangeError(const ResourceRequest& request)
{
    return ResourceError(String(WebKitErrorDomain), WebKitErrorFrameLoadInterruptedByPolicyChange, request.url().string(), String());
}

ResourceError cannotShowMIMETypeError(const ResourceResponse& response)
{
    return ResourceError();
}

ResourceError fileDoesNotExistError(const ResourceResponse& response)
{
    return ResourceError();
}

} // namespace WebKit
