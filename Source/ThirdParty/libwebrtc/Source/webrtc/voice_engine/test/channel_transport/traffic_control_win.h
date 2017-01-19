/*
 *  Copyright (c) 2011 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#ifndef WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_TRAFFIC_CONTROL_WINDOWS_H_
#define WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_TRAFFIC_CONTROL_WINDOWS_H_

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Disable deprication warning from traffic.h
#pragma warning(disable : 4995)

#include <windows.h>
#include <qos.h>
#include <ntddndis.h>
#include <traffic.h>

#include "webrtc/system_wrappers/include/trace.h"

namespace webrtc {
namespace test {
void MyClNotifyHandler(HANDLE ClRegCtx, HANDLE ClIfcCtx, ULONG Event,
                       HANDLE SubCode, ULONG BufSize, PVOID Buffer);


typedef ULONG (WINAPI *registerFn)(ULONG, HANDLE, PTCI_CLIENT_FUNC_LIST,
                                   PHANDLE);
typedef ULONG (WINAPI *deregisterFn)(HANDLE);
typedef ULONG (WINAPI *enumerateFn)(HANDLE, PULONG, PTC_IFC_DESCRIPTOR);
typedef ULONG (WINAPI *openInterfaceFn)(LPWSTR, HANDLE, HANDLE, PHANDLE);
typedef ULONG (WINAPI *closeInterfaceFn)(HANDLE);
typedef ULONG (WINAPI *flowAddFn)(HANDLE, HANDLE, ULONG, PTC_GEN_FLOW, PHANDLE);
typedef ULONG (WINAPI *filterAddFn)(HANDLE, PTC_GEN_FILTER, PHANDLE);
typedef ULONG (WINAPI *flowDeleteFn)(HANDLE);
typedef ULONG (WINAPI *filterDeleteFn)(HANDLE);

class TrafficControlWindows
{
 public:
    // Factory method. Constructor disabled.
    static TrafficControlWindows* GetInstance(const int32_t id);
    static void Release(TrafficControlWindows* gtc);

    ULONG TcRegisterClient(ULONG TciVersion, HANDLE ClRegCtx,
                           PTCI_CLIENT_FUNC_LIST ClientHandlerList,
                           PHANDLE pClientHandle);

    ULONG TcDeregisterClient(HANDLE clientHandle);

    ULONG TcEnumerateInterfaces(HANDLE ClientHandle, PULONG pBufferSize,
                                PTC_IFC_DESCRIPTOR interfaceBuffer);

    ULONG TcOpenInterfaceW(LPWSTR pInterfaceName, HANDLE ClientHandle,
                           HANDLE ClIfcCtx, PHANDLE pIfcHandle);

    ULONG TcCloseInterface(HANDLE IfcHandle);

    ULONG TcAddFlow(HANDLE IfcHandle, HANDLE ClFlowCtx, ULONG Flags,
                    PTC_GEN_FLOW pGenericFlow, PHANDLE pFlowHandle);

    ULONG TcAddFilter(HANDLE FlowHandle, PTC_GEN_FILTER pGenericFilter,
                      PHANDLE pFilterHandle);

    ULONG TcDeleteFlow(HANDLE FlowHandle);
    ULONG TcDeleteFilter(HANDLE FilterHandle);
private:
    TrafficControlWindows(const int32_t id);
    TCI_CLIENT_FUNC_LIST QoSFunctions;

    static TrafficControlWindows* instance;

    registerFn tcRegister;
    deregisterFn tcDeregister;

    enumerateFn tcEnumerate;
    openInterfaceFn tcOpenInterface;
    closeInterfaceFn tcCloseInterface;

    flowAddFn tcAddFlow;
    flowDeleteFn tcDeleteFlow;

    filterAddFn tcAddFilter;
    filterDeleteFn tcDeleteFilter;

    static uint32_t refCounter;
};

}  // namespace test
}  // namespace webrtc

#endif  // WEBRTC_VOICE_ENGINE_TEST_CHANNEL_TRANSPORT_TRAFFIC_CONTROL_WINDOWS_H_
