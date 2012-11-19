/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ColorChooserUIController.h"

#if ENABLE(INPUT_TYPE_COLOR)
#include "ChromeClientImpl.h"
#include "Color.h"
#include "ColorChooserClient.h"
#include "WebColorChooser.h"
#include <public/WebColor.h>

using namespace WebCore;

namespace WebKit {


ColorChooserUIController::ColorChooserUIController(ChromeClientImpl* chromeClient, ColorChooserClient* client)
    : m_chromeClient(chromeClient)
    , m_client(client)
{
}

ColorChooserUIController::~ColorChooserUIController()
{
}

void ColorChooserUIController::openUI()
{
    openColorChooser();
}

void ColorChooserUIController::setSelectedColor(const Color& color)
{
    ASSERT(m_chooser);
    m_chooser->setSelectedColor(static_cast<WebColor>(color.rgb()));
}

void ColorChooserUIController::endChooser()
{
    if (m_chooser)
        m_chooser->endChooser();
}

void ColorChooserUIController::didChooseColor(const WebColor& color)
{
    ASSERT(m_client);
    m_client->didChooseColor(Color(static_cast<RGBA32>(color)));
}

void ColorChooserUIController::didEndChooser()
{
    ASSERT(m_client);
    m_chooser = nullptr;
    m_client->didEndChooser();
}

void ColorChooserUIController::openColorChooser()
{
    ASSERT(!m_chooser);
    m_chooser = m_chromeClient->createWebColorChooser(this, static_cast<WebColor>(m_client->currentColor().rgb()));
}

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)
