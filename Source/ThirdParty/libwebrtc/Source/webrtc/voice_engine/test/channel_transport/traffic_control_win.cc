/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "webrtc/voice_engine/test/channel_transport/traffic_control_win.h"

#include <assert.h>

#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace test {

TrafficControlWindows* TrafficControlWindows::instance = NULL;
uint32_t TrafficControlWindows::refCounter = 0;

TrafficControlWindows::TrafficControlWindows(const int32_t id)
{
}

TrafficControlWindows* TrafficControlWindows::GetInstance(
    const int32_t id)
{
    if(instance != NULL)
    {
        WEBRTC_TRACE(
            kTraceDebug,
            kTraceTransport,
            id,
            "TrafficControlWindows - Returning already created object");
        refCounter++;
        return instance;
    }

    WEBRTC_TRACE(kTraceMemory, kTraceTransport, id,
                 "TrafficControlWindows - Creating new object");
    instance = new TrafficControlWindows(id);
    if(instance == NULL)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceTransport, id,
                     "TrafficControlWindows - Error allocating memory");
        return NULL;
    }

    instance->tcRegister = NULL;
    instance->tcDeregister = NULL;

    instance->tcEnumerate = NULL;
    instance->tcOpenInterface = NULL;
    instance->tcCloseInterface = NULL;

    instance->tcAddFlow = NULL;
    instance->tcDeleteFlow = NULL;

    instance->tcAddFilter = NULL;
    instance->tcDeleteFilter = NULL;

    HMODULE trafficLib = LoadLibrary(TEXT("traffic.dll"));
    if(trafficLib == NULL)
    {
        WEBRTC_TRACE(
            kTraceWarning,
            kTraceTransport,
            id,
            "TrafficControlWindows - No QOS support, LoadLibrary returned NULL,\
 last error: %d\n",
            GetLastError());
        delete instance;
        instance = NULL;
        return NULL;
    }

    instance->tcRegister = (registerFn)GetProcAddress(trafficLib,
                                                      "TcRegisterClient");
    instance->tcDeregister = (deregisterFn)GetProcAddress(trafficLib,
                                                          "TcDeregisterClient");
    instance->tcEnumerate = (enumerateFn)GetProcAddress(
        trafficLib,
        "TcEnumerateInterfaces");
    instance->tcOpenInterface = (openInterfaceFn)GetProcAddress(
        trafficLib,
        "TcOpenInterfaceW");
    instance->tcCloseInterface = (closeInterfaceFn)GetProcAddress(
        trafficLib,
        "TcCloseInterface");
    instance->tcAddFlow = (flowAddFn)GetProcAddress(trafficLib,
                                                    "TcAddFlow");
    instance->tcDeleteFlow = (flowDeleteFn)GetProcAddress(trafficLib,
                                                          "TcDeleteFlow");

    instance->tcAddFilter = (filterAddFn)GetProcAddress(trafficLib,
                                                        "TcAddFilter");
    instance->tcDeleteFilter = (filterDeleteFn)GetProcAddress(trafficLib,
                                                              "TcDeleteFilter");

    if(instance->tcRegister       == NULL ||
       instance->tcDeregister     == NULL ||
       instance->tcEnumerate      == NULL ||
       instance->tcOpenInterface  == NULL ||
       instance->tcCloseInterface == NULL ||
       instance->tcAddFlow        == NULL ||
       instance->tcAddFilter      == NULL ||
       instance->tcDeleteFlow     == NULL ||
       instance->tcDeleteFilter   == NULL)
    {
        delete instance;
        instance = NULL;
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            id,
            "TrafficControlWindows - Could not find function pointer for\
 traffic control functions");
        WEBRTC_TRACE(
            kTraceError,
            kTraceTransport,
            id,
            "Tcregister    : %x, tcDeregister: %x, tcEnumerate: %x,\
 tcOpenInterface: %x, tcCloseInterface: %x, tcAddFlow: %x, tcAddFilter: %x,\
 tcDeleteFlow: %x, tcDeleteFilter: %x",
            instance->tcRegister,
            instance->tcDeregister,
            instance->tcEnumerate,
            instance->tcOpenInterface,
            instance->tcCloseInterface,
            instance->tcAddFlow,
            instance->tcAddFilter,
            instance->tcDeleteFlow,
            instance->tcDeleteFilter );
        return NULL;
    }
    refCounter++;
    return instance;
}

void TrafficControlWindows::Release(TrafficControlWindows* gtc)
{
    if (0 == refCounter)
    {
        WEBRTC_TRACE(kTraceError, kTraceTransport, -1,
                     "TrafficControlWindows - Cannot release, refCounter is 0");
        return;
    }
    if (NULL == gtc)
    {
        WEBRTC_TRACE(kTraceDebug, kTraceTransport, -1,
                     "TrafficControlWindows - Not releasing, gtc is NULL");
        return;
    }

    WEBRTC_TRACE(kTraceDebug, kTraceTransport, -1,
                 "TrafficControlWindows - Releasing object");
    refCounter--;
    if ((0 == refCounter) && instance)
    {
        WEBRTC_TRACE(kTraceMemory, kTraceTransport, -1,
                     "TrafficControlWindows - Deleting object");
        delete instance;
        instance = NULL;
    }
}

ULONG TrafficControlWindows::TcRegisterClient(
    ULONG TciVersion,
    HANDLE ClRegCtx,
    PTCI_CLIENT_FUNC_LIST ClientHandlerList,
    PHANDLE pClientHandle)
{
    assert(tcRegister != NULL);

    return tcRegister(TciVersion, ClRegCtx, ClientHandlerList, pClientHandle);
}

ULONG TrafficControlWindows::TcDeregisterClient(HANDLE clientHandle)
{
    assert(tcDeregister != NULL);

    return tcDeregister(clientHandle);
}


ULONG TrafficControlWindows::TcEnumerateInterfaces(
    HANDLE ClientHandle,
    PULONG pBufferSize,
    PTC_IFC_DESCRIPTOR interfaceBuffer)
{
    assert(tcEnumerate != NULL);

    return tcEnumerate(ClientHandle, pBufferSize, interfaceBuffer);
}


ULONG TrafficControlWindows::TcOpenInterfaceW(LPWSTR pInterfaceName,
                                              HANDLE ClientHandle,
                                              HANDLE ClIfcCtx,
                                              PHANDLE pIfcHandle)
{
    assert(tcOpenInterface != NULL);

    return tcOpenInterface(pInterfaceName, ClientHandle, ClIfcCtx, pIfcHandle);

}

ULONG TrafficControlWindows::TcCloseInterface(HANDLE IfcHandle)
{
    assert(tcCloseInterface != NULL);

    return tcCloseInterface(IfcHandle);
}

ULONG TrafficControlWindows::TcAddFlow(HANDLE IfcHandle, HANDLE ClFlowCtx,
                                       ULONG  Flags, PTC_GEN_FLOW pGenericFlow,
                                       PHANDLE pFlowHandle)
{
    assert(tcAddFlow != NULL);
    return tcAddFlow(IfcHandle, ClFlowCtx, Flags, pGenericFlow, pFlowHandle);
}

ULONG TrafficControlWindows::TcAddFilter(HANDLE FlowHandle,
                                         PTC_GEN_FILTER pGenericFilter,
                                         PHANDLE pFilterHandle)
{
    assert(tcAddFilter != NULL);
    return tcAddFilter(FlowHandle, pGenericFilter, pFilterHandle);
}

ULONG TrafficControlWindows::TcDeleteFlow(HANDLE FlowHandle)
{
    assert(tcDeleteFlow != NULL);
    return tcDeleteFlow(FlowHandle);

}

ULONG TrafficControlWindows::TcDeleteFilter(HANDLE FilterHandle)
{
    assert(tcDeleteFilter != NULL);
    return tcDeleteFilter(FilterHandle);
}

void CALLBACK MyClNotifyHandler(HANDLE ClRegCtx, HANDLE ClIfcCtx, ULONG Event,
                                HANDLE SubCode, ULONG BufSize, PVOID Buffer)
{
}

}  // namespace test
}  // namespace webrtc
