/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2010 Pawel Hajdan (phajdan.jr@chromium.org)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestRunner.h"

#include "WebAnimationController.h"
#include "WebBindings.h"
#include "WebConsoleMessage.h"
#include "WebDeviceOrientation.h"
#include "WebDeviceOrientationClientMock.h"
#include "WebDocument.h"
#include "WebElement.h"
#include "WebFindOptions.h"
#include "WebFrame.h"
#include "WebGeolocationClientMock.h"
#include "WebIDBFactory.h"
#include "WebInputElement.h"
#include "WebIntent.h"
#include "WebIntentRequest.h"
#include "WebKit.h"
#include "WebNotificationPresenter.h"
#include "WebPrintParams.h"
#include "WebScriptSource.h"
#include "WebSecurityPolicy.h"
#include "WebSettings.h"
#include "WebSurroundingText.h"
#include "WebView.h"
#include "WebWorkerInfo.h"
#include "platform/WebData.h"
#include "platform/WebSerializedScriptValue.h"
#include "platform/WebSize.h"
#include "platform/WebURL.h"
#include "v8/include/v8.h"
#include "webkit/support/webkit_support.h"
#include <algorithm>
#include <cctype>
#include <clocale>
#include <cstdlib>
#include <limits>
#include <sstream>
#include <wtf/OwnArrayPtr.h>
#include <wtf/text/WTFString.h>

#if OS(LINUX) || OS(ANDROID)
#include "linux/WebFontRendering.h"
#endif

using namespace WebCore;
using namespace WebKit;
using namespace std;

TestRunner::TestRunner()
{
}
