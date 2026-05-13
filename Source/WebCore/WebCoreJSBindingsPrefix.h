/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if defined(HAVE_CONFIG_H) && HAVE_CONFIG_H && defined(BUILDING_WITH_CMAKE)
#include "cmakeconfig.h"
#endif

#include <wtf/Platform.h>

#if !OS(DARWIN)
// On non-Apple, PCH chaining is not used; include the base prefix directly so
// this PCH is self-contained and all required types and macros are available.
#include "WebCorePrefix.h"
#endif

#ifdef __cplusplus
#undef new
#undef delete

#include <JavaScriptCore/HeapAnalyzer.h>
#include <JavaScriptCore/JSCJSValueInlines.h>
#include <JavaScriptCore/JSGlobalObject.h>
#include <JavaScriptCore/JSObject.h>
#include <JavaScriptCore/JSStringInlines.h>
#include <JavaScriptCore/Lookup.h>
#include <JavaScriptCore/SubspaceInlines.h>

#include <pal/SessionID.h>
#include <wtf/PointerPreparations.h>
#include <wtf/Range.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>
#if PLATFORM(COCOA)
#include <wtf/spi/cocoa/SecuritySPI.h>
#endif

#include "ActiveDOMObject.h"
#include "BlobData.h"
#include "BlobDataFileReference.h"
#include "CacheValidation.h"
#include "CertificateInfo.h"
#include "ContentSecurityPolicyResponseHeaders.h"
#include "ContextDestructionObserverInlines.h"
#include "DOMHighResTimeStamp.h"
#include "Document.h"
#include "EventTargetInlines.h"
#include "ExtendedDOMClientIsoSubspaces.h"
#include "ExtendedDOMIsoSubspaces.h"
#include "FetchOptions.h"
#include "FetchOptionsCache.h"
#include "FetchOptionsCredentials.h"
#include "FetchOptionsDestination.h"
#include "FetchOptionsMode.h"
#include "FetchOptionsRedirect.h"
#include "FormData.h"
#include "FrameLoaderTypes.h"
#include "HTTPHeaderMap.h"
#include "HTTPHeaderNames.h"
#include "ImageBitmap.h"
#include "JSDOMAttribute.h"
#include "JSDOMBinding.h"
#include "JSDOMConstructorBase.h"
#include "JSDOMConvertBase.h"
#include "JSDOMConvertDictionary.h"
#include "JSDOMConvertInterface.h"
#include "JSDOMConvertNumbers.h"
#include "JSDOMConvertStrings.h"
#include "JSDOMExceptionHandling.h"
#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMOperation.h"
#include "JSDOMWrapperCache.h"
#include "JSValueInWrappedObject.h"
#include "NetworkLoadMetrics.h"
#include "Node.h"
#include "ParsedContentRange.h"
#include "PolicyContainer.h"
#include "ResourceLoadPriority.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SecurityOrigin.h"
#include "TrustedFonts.h"
#include "WebCoreJSClientData.h"
#include "WebCoreOpaqueRoot.h"
#include "WebKitFontFamilyNames.h"
#include "WindowOrWorkerGlobalScope.h"

#define new ("if you use new/delete make sure to include config.h at the top of the file"())
#define delete ("if you use new/delete make sure to include config.h at the top of the file"())
#endif
