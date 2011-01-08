/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef GeolocationServiceQt_h
#define GeolocationServiceQt_h

#include "GeolocationService.h"
#include <QGeoPositionInfoSource>
#include <wtf/RefPtr.h>

// FIXME: Remove usage of "using namespace" in a header file.
// There is bug in qtMobility signal names are not full qualified when used with namespace
// QtMobility namespace in slots throws up error and its required to be fixed in qtmobility.
using namespace QtMobility;

namespace WebCore {

// This class provides a implementation of a GeolocationService for qtWebkit.
// It uses QtMobility (v1.0.0) location service to get positions
class GeolocationServiceQt : public QObject, GeolocationService {
    Q_OBJECT

public:
    static GeolocationService* create(GeolocationServiceClient*);

    GeolocationServiceQt(GeolocationServiceClient*);
    virtual ~GeolocationServiceQt();

    virtual bool startUpdating(PositionOptions*);
    virtual void stopUpdating();

    virtual Geoposition* lastPosition() const { return m_lastPosition.get(); }
    virtual PositionError* lastError() const { return m_lastError.get(); }

public Q_SLOTS:
    // QGeoPositionInfoSource
    void positionUpdated(const QGeoPositionInfo&);

private:
    RefPtr<Geoposition> m_lastPosition;
    RefPtr<PositionError> m_lastError;

    QtMobility::QGeoPositionInfoSource* m_location;
};

} // namespace WebCore

#endif // GeolocationServiceQt_h
