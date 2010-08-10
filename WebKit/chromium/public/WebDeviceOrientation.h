/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebDeviceOrientation_h
#define WebDeviceOrientation_h

#if WEBKIT_IMPLEMENTATION
namespace WTF { template <typename T> class PassRefPtr; }
namespace WebCore { class DeviceOrientation; }
#endif

namespace WebKit {

class WebDeviceOrientation {
public:
    WebDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
        : m_isNull(false),
          m_canProvideAlpha(canProvideAlpha),
          m_alpha(alpha),
          m_canProvideBeta(canProvideBeta),
          m_beta(beta),
          m_canProvideGamma(canProvideGamma),
          m_gamma(gamma)
    {
    }

    static WebDeviceOrientation nullOrientation() { return WebDeviceOrientation(); }

    bool isNull() { return m_isNull; }
    bool canProvideAlpha() { return m_canProvideAlpha; }
    double alpha() { return m_alpha; }
    bool canProvideBeta() { return m_canProvideBeta; }
    double beta() { return m_beta; }
    bool canProvideGamma() { return m_canProvideGamma; }
    double gamma() { return m_gamma; }

#if WEBKIT_IMPLEMENTATION
    WebDeviceOrientation(const WTF::PassRefPtr<WebCore::DeviceOrientation>&);
    WebDeviceOrientation& operator=(const WTF::PassRefPtr<WebCore::DeviceOrientation>&);
    operator WTF::PassRefPtr<WebCore::DeviceOrientation>() const;
#endif

private:
    WebDeviceOrientation()
        : m_isNull(true),
          m_canProvideAlpha(false),
          m_alpha(0),
          m_canProvideBeta(false),
          m_beta(0),
          m_canProvideGamma(false),
          m_gamma(0)
    {
    }

    bool m_isNull;
    bool m_canProvideAlpha;
    double m_alpha;
    bool m_canProvideBeta;
    double m_beta;
    bool m_canProvideGamma;
    double m_gamma;
};

} // namespace WebKit

#endif // WebDeviceOrientation_h
