/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */
#ifndef DeviceOrientationProviderQt_h
#define DeviceOrientationProviderQt_h

#include "DeviceOrientation.h"
#include "DeviceOrientationController.h"
#include <wtf/RefPtr.h>

#include <QRotationFilter>

namespace WebCore {

class DeviceOrientationProviderQt
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    : public QTM_NAMESPACE::QRotationFilter {
#else
    : public QRotationFilter {
#endif
public:
    DeviceOrientationProviderQt();

    void setController(DeviceOrientationController*);

#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    bool filter(QTM_NAMESPACE::QRotationReading*);
#else
    bool filter(QRotationReading*);
#endif

    void start();
    void stop();
    bool isActive() const { return m_sensor.isActive(); }
    DeviceOrientation* lastOrientation() const { return m_lastOrientation.get(); }
    bool hasAlpha() const { return m_sensor.property("hasZ").toBool(); }

private:
    RefPtr<DeviceOrientation> m_lastOrientation;
    DeviceOrientationController* m_controller;
#if QT_VERSION < QT_VERSION_CHECK(5, 0, 0)
    QTM_NAMESPACE::QRotationSensor m_sensor;
#else
    QRotationSensor m_sensor;
#endif
};

}

#endif // DeviceOrientationProviderQt_h
