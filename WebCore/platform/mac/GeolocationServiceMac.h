/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
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

#ifndef GeolocationServiceMac_h
#define GeolocationServiceMac_h

#if ENABLE(GEOLOCATION)

#include "GeolocationService.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/RetainPtr.h>

#ifdef __OBJC__
@class CLLocationManager;
@class WebCoreCoreLocationObserver;
#else
class CLLocationManager;
class WebCoreCoreLocationObserver;
#endif

namespace WebCore {

class GeolocationServiceMac : public GeolocationService {
public:
    GeolocationServiceMac(GeolocationServiceClient*);
    virtual ~GeolocationServiceMac();
    
    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();

    virtual void suspend();
    virtual void resume();

    virtual Geoposition* lastPosition() const { return m_lastPosition.get(); }
    virtual PositionError* lastError() const { return m_lastError.get(); }

    void positionChanged(PassRefPtr<Geoposition>);
    void errorOccurred(PassRefPtr<PositionError>);

private:
    RetainPtr<CLLocationManager> m_locationManager;
    RetainPtr<WebCoreCoreLocationObserver> m_objcObserver;
    
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;
};
    
} // namespace WebCore

#endif // ENABLE(GEOLOCATION)

#endif // GeolocationServiceMac_h
