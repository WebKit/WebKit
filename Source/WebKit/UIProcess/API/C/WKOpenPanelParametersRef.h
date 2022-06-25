/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#ifndef WKOpenPanelParametersRef_h
#define WKOpenPanelParametersRef_h

#include <WebKit/WKBase.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKTypeID WKOpenPanelParametersGetTypeID(void);

WK_EXPORT bool WKOpenPanelParametersGetAllowsDirectories(WKOpenPanelParametersRef parameters);
WK_EXPORT bool WKOpenPanelParametersGetAllowsMultipleFiles(WKOpenPanelParametersRef parameters);

WK_EXPORT WKArrayRef WKOpenPanelParametersCopyAcceptedMIMETypes(WKOpenPanelParametersRef parameters);
WK_EXPORT WKArrayRef WKOpenPanelParametersCopyAcceptedFileExtensions(WKOpenPanelParametersRef parameters);

WK_EXPORT WKArrayRef WKOpenPanelParametersCopyAllowedMIMETypes(WKOpenPanelParametersRef parameters);

/* DEPRECATED - Please use WKOpenPanelParametersGetCaptureEnabled() instead. */
WK_EXPORT WKStringRef WKOpenPanelParametersCopyCapture(WKOpenPanelParametersRef parameters);

WK_EXPORT bool WKOpenPanelParametersGetMediaCaptureType(WKOpenPanelParametersRef parametersRef);

WK_EXPORT WKArrayRef WKOpenPanelParametersCopySelectedFileNames(WKOpenPanelParametersRef parametersRef);

#ifdef __cplusplus
}
#endif

#endif /* WKOpenPanelParametersRef_h */
