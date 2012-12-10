/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#ifndef WebTestPlugin_h
#define WebTestPlugin_h

#include "WebKit/chromium/public/WebPlugin.h"

namespace WebTestRunner {

class WebTestDelegate;

// A fake implemention of WebKit::WebPlugin for testing purposes.
//
// It uses WebGraphicsContext3D to paint a scene consisiting of a primitive
// over a background. The primitive and background can be customized using
// the following plugin parameters:
// primitive: none (default), triangle.
// background-color: black (default), red, green, blue.
// primitive-color: black (default), red, green, blue.
// opacity: [0.0 - 1.0]. Default is 1.0.
//
// Whether the plugin accepts touch events or not can be customized using the
// 'accepts-touch' plugin parameter (defaults to false).
class WebTestPlugin : public WebKit::WebPlugin {
public:
    static WebTestPlugin* create(WebKit::WebFrame*, const WebKit::WebPluginParams&, WebTestDelegate*);
    virtual ~WebTestPlugin();

    static const WebKit::WebString& mimeType();

protected:
    WebTestPlugin();
};

}

#endif // WebTestPlugin_h
