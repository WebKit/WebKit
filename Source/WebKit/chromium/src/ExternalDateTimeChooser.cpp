/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#if ENABLE(DATE_AND_TIME_INPUT_TYPES) && !ENABLE(INPUT_MULTIPLE_FIELDS_UI)
#include "ExternalDateTimeChooser.h"

#include "ChromeClientImpl.h"
#include "DateTimeChooserClient.h"
#include "WebDateTimeChooserCompletion.h"
#include "WebDateTimeChooserParams.h"
#include "WebViewClient.h"

using namespace WebCore;

namespace WebKit {

class WebDateTimeChooserCompletionImpl : public WebDateTimeChooserCompletion {
public:
    WebDateTimeChooserCompletionImpl(ExternalDateTimeChooser* chooser)
        : m_chooser(chooser)
    {
    }

private:
    virtual void didChooseValue(const WebString& value) OVERRIDE
    {
        m_chooser->didChooseValue(value);
        delete this;
    }

    virtual void didCancelChooser() OVERRIDE
    {
        m_chooser->didCancelChooser();
        delete this;
    }

    RefPtr<ExternalDateTimeChooser> m_chooser;
};

ExternalDateTimeChooser::~ExternalDateTimeChooser()
{
}

ExternalDateTimeChooser::ExternalDateTimeChooser(WebCore::DateTimeChooserClient* client)
    : m_client(client)
{
    ASSERT(client);
}

PassRefPtr<ExternalDateTimeChooser> ExternalDateTimeChooser::create(ChromeClientImpl* chromeClient, WebViewClient* webViewClient, WebCore::DateTimeChooserClient* client, const WebCore::DateTimeChooserParameters& parameters)
{
    ASSERT(chromeClient);
    RefPtr<ExternalDateTimeChooser> chooser = adoptRef(new ExternalDateTimeChooser(client));
    if (!chooser->openDateTimeChooser(chromeClient, webViewClient, parameters))
        chooser.clear();
    return chooser.release();
}

bool ExternalDateTimeChooser::openDateTimeChooser(ChromeClientImpl* chromeClient, WebViewClient* webViewClient, const DateTimeChooserParameters& parameters)
{
    if (!webViewClient)
        return false;

    WebDateTimeChooserParams webParams;
    webParams.type = parameters.type;
    webParams.anchorRectInScreen = chromeClient->rootViewToScreen(parameters.anchorRectInRootView);
    webParams.currentValue = parameters.currentValue;
    webParams.suggestionValues = parameters.suggestionValues;
    webParams.localizedSuggestionValues = parameters.localizedSuggestionValues;
    webParams.suggestionLabels = parameters.suggestionLabels;
    webParams.minimum = parameters.minimum;
    webParams.maximum = parameters.maximum;
    webParams.step = parameters.step;
    webParams.stepBase = parameters.stepBase;
    webParams.isRequired = parameters.required;
    webParams.isAnchorElementRTL = parameters.isAnchorElementRTL;

    WebDateTimeChooserCompletion* completion = new WebDateTimeChooserCompletionImpl(this);
    if (webViewClient->openDateTimeChooser(webParams, completion))
        return true;
    // We can't open a chooser. Calling
    // WebDateTimeChooserCompletionImpl::didCancelChooser to delete the
    // WebDateTimeChooserCompletionImpl object and deref this.
    completion->didCancelChooser();
    return false;
}

void ExternalDateTimeChooser::didChooseValue(const WebString& value)
{
    if (m_client)
        m_client->didChooseValue(value);
    // didChooseValue might run JavaScript code, and endChooser() might be
    // called. However DateTimeChooserCompletionImpl still has one reference to
    // this object.
    if (m_client)
        m_client->didEndChooser();
}

void ExternalDateTimeChooser::didCancelChooser()
{
    if (m_client)
        m_client->didEndChooser();
}

void ExternalDateTimeChooser::endChooser()
{
    DateTimeChooserClient* client = m_client;
    m_client = 0;
    client->didEndChooser();
}

}

#endif
