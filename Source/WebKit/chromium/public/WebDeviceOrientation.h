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
namespace WebCore { class DeviceOrientationData; }
#endif

namespace WebKit {

class WebDeviceOrientation {
public:
    WebDeviceOrientation()
        : m_isNull(true)
        , m_canProvideAlpha(false)
        , m_alpha(0)
        , m_canProvideBeta(false)
        , m_beta(0)
        , m_canProvideGamma(false)
        , m_gamma(0)
        , m_canProvideAbsolute(false)
        , m_absolute(false)
    {
    }

    static WebDeviceOrientation nullOrientation() { return WebDeviceOrientation(); }

    void setNull(bool isNull) { m_isNull = isNull; }
    bool isNull() const { return m_isNull; }

    void setAlpha(double alpha)
    {
        m_canProvideAlpha = true;
        m_alpha = alpha;
    }
    bool canProvideAlpha() const { return m_canProvideAlpha; }
    double alpha() const { return m_alpha; }

    void setBeta(double beta)
    {
        m_canProvideBeta = true;
        m_beta = beta;
    }
    bool canProvideBeta() const { return m_canProvideBeta; }
    double beta() const { return m_beta; }

    void setGamma(double gamma)
    {
        m_canProvideGamma = true;
        m_gamma = gamma;
    }
    bool canProvideGamma() const { return m_canProvideGamma; }
    double gamma() const { return m_gamma; }

    void setAbsolute(bool absolute)
    {
        m_canProvideAbsolute = true;
        m_absolute = absolute;
    }
    bool canProvideAbsolute() const {return m_canProvideAbsolute; }
    bool absolute() const { return m_absolute; }

#if WEBKIT_IMPLEMENTATION
    WebDeviceOrientation(const WebCore::DeviceOrientationData*);
    operator WTF::PassRefPtr<WebCore::DeviceOrientationData>() const;
#endif

private:
    bool m_isNull;
    bool m_canProvideAlpha;
    double m_alpha;
    bool m_canProvideBeta;
    double m_beta;
    bool m_canProvideGamma;
    double m_gamma;
    bool m_canProvideAbsolute;
    bool m_absolute;
};

} // namespace WebKit

#endif // WebDeviceOrientation_h
