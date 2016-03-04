/*
 *  Copyright (C) 2012 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef BatteryManager_h
#define BatteryManager_h

#if ENABLE(BATTERY_STATUS)

#include "ActiveDOMObject.h"
#include "BatteryStatus.h"
#include "EventTarget.h"

namespace WebCore {

class BatteryController;
class Navigator;
class ScriptExecutionContext;

class BatteryManager : public ActiveDOMObject, public RefCounted<BatteryManager>, public EventTarget {
public:
    virtual ~BatteryManager();
    static Ref<BatteryManager> create(Navigator*);

    // EventTarget implementation.
    EventTargetInterface eventTargetInterface() const override { return BatteryManagerEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }

    bool charging();
    double chargingTime();
    double dischargingTime();
    double level();

    void didChangeBatteryStatus(Event&, PassRefPtr<BatteryStatus>);
    void updateBatteryStatus(PassRefPtr<BatteryStatus>);
    void batteryControllerDestroyed() { m_batteryController = nullptr; }

    using RefCounted<BatteryManager>::ref;
    using RefCounted<BatteryManager>::deref;

protected:
    EventTargetData* eventTargetData() override { return &m_eventTargetData; }
    EventTargetData& ensureEventTargetData() override { return m_eventTargetData; }

private:
    explicit BatteryManager(Navigator*);

    // ActiveDOMObject API.
    bool canSuspendForDocumentSuspension() const override;
    void suspend(ReasonForSuspension) override;
    void resume() override;
    void stop() override;

    // EventTarget implementation.
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    const char* activeDOMObjectName() const override { return "BatteryManager"; }

    BatteryController* m_batteryController;
    EventTargetData m_eventTargetData;
    RefPtr<BatteryStatus> m_batteryStatus;
};

}

#endif // BATTERY_STATUS
#endif // BatteryManager_h

