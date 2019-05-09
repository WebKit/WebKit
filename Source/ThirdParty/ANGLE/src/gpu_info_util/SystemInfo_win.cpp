//
// Copyright (c) 2013-2017 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// SystemInfo_win.cpp: implementation of the Windows-specific parts of SystemInfo.h

#include "gpu_info_util/SystemInfo_internal.h"

#include "common/debug.h"
#include "common/string_utils.h"

// Windows.h needs to be included first
#include <windows.h>

#if defined(GPU_INFO_USE_SETUPAPI)
// Remove parts of commctrl.h that have compile errors
#    define NOTOOLBAR
#    define NOTOOLTIPS
#    include <cfgmgr32.h>
#    include <setupapi.h>
#elif defined(GPU_INFO_USE_DXGI)
#    include <d3d10.h>
#    include <dxgi.h>
#else
#    error "SystemInfo_win needs at least GPU_INFO_USE_SETUPAPI or GPU_INFO_USE_DXGI defined"
#endif

#include <array>
#include <sstream>

namespace angle
{

namespace
{

// Returns the CM device ID of the primary GPU.
std::string GetPrimaryDisplayDeviceId()
{
    DISPLAY_DEVICEA displayDevice;
    displayDevice.cb = sizeof(DISPLAY_DEVICEA);

    for (int i = 0; EnumDisplayDevicesA(nullptr, i, &displayDevice, 0); ++i)
    {
        if (displayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            return displayDevice.DeviceID;
        }
    }

    return "";
}

#if defined(GPU_INFO_USE_SETUPAPI)

std::string GetRegistryStringValue(HKEY key, const char *valueName)
{
    std::array<char, 255> value;
    DWORD valueSize = sizeof(value);
    if (RegQueryValueExA(key, valueName, nullptr, nullptr, reinterpret_cast<LPBYTE>(value.data()),
                         &valueSize) == ERROR_SUCCESS)
    {
        return value.data();
    }
    return "";
}

// Gathers information about the devices from the registry. The reason why we aren't using
// a dedicated API such as DXGI is that we need information like the driver vendor and date.
// DXGI doesn't provide a way to know the device registry key from an IDXGIAdapter.
bool GetDevicesFromRegistry(std::vector<GPUDeviceInfo> *devices)
{
    // Display adapter class GUID from
    // https://msdn.microsoft.com/en-us/library/windows/hardware/ff553426%28v=vs.85%29.aspx
    GUID displayClass = {
        0x4d36e968, 0xe325, 0x11ce, {0xbf, 0xc1, 0x08, 0x00, 0x2b, 0xe1, 0x03, 0x18}};

    HDEVINFO deviceInfo = SetupDiGetClassDevsW(&displayClass, nullptr, nullptr, DIGCF_PRESENT);

    if (deviceInfo == INVALID_HANDLE_VALUE)
    {
        return false;
    }

    // This iterates over the devices of the "Display adapter" class
    DWORD deviceIndex = 0;
    SP_DEVINFO_DATA deviceData;
    deviceData.cbSize = sizeof(deviceData);
    while (SetupDiEnumDeviceInfo(deviceInfo, deviceIndex++, &deviceData))
    {
        // The device and vendor IDs can be gathered directly, but information about the driver
        // requires some registry digging
        char fullDeviceID[MAX_DEVICE_ID_LEN];
        if (CM_Get_Device_IDA(deviceData.DevInst, fullDeviceID, MAX_DEVICE_ID_LEN, 0) != CR_SUCCESS)
        {
            continue;
        }

        GPUDeviceInfo device;

        if (!CMDeviceIDToDeviceAndVendorID(fullDeviceID, &device.vendorId, &device.deviceId))
        {
            continue;
        }

        // The driver key will end with something like {<displayClass>}/<4 digit number>.
        std::array<WCHAR, 255> value;
        if (!SetupDiGetDeviceRegistryPropertyW(deviceInfo, &deviceData, SPDRP_DRIVER, nullptr,
                                               reinterpret_cast<PBYTE>(value.data()), sizeof(value),
                                               nullptr))
        {
            continue;
        }

        std::wstring driverKey = L"System\\CurrentControlSet\\Control\\Class\\";
        driverKey += value.data();

        HKEY key;
        if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, driverKey.c_str(), 0, KEY_QUERY_VALUE, &key) !=
            ERROR_SUCCESS)
        {
            continue;
        }

        device.driverVersion = GetRegistryStringValue(key, "DriverVersion");
        device.driverDate    = GetRegistryStringValue(key, "DriverDate");
        device.driverVendor  = GetRegistryStringValue(key, "ProviderName");

        RegCloseKey(key);

        devices->push_back(device);
    }

    SetupDiDestroyDeviceInfoList(deviceInfo);

    return true;
}

#elif defined(GPU_INFO_USE_DXGI)

bool GetDevicesFromDXGI(std::vector<GPUDeviceInfo> *devices)
{
    IDXGIFactory *factory;
    if (!SUCCEEDED(CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void **>(&factory))))
    {
        return false;
    }

    UINT i                = 0;
    IDXGIAdapter *adapter = nullptr;
    while (factory->EnumAdapters(i++, &adapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        adapter->GetDesc(&desc);

        LARGE_INTEGER umdVersion;
        if (adapter->CheckInterfaceSupport(__uuidof(ID3D10Device), &umdVersion) ==
            DXGI_ERROR_UNSUPPORTED)
        {
            adapter->Release();
            continue;
        }

        // The UMD driver version here is the same as in the registry except for the last number.
        uint64_t intVersion = umdVersion.QuadPart;
        std::ostringstream o;

        const uint64_t kMask = 0xFF;
        o << ((intVersion >> 48) & kMask) << ".";
        o << ((intVersion >> 32) & kMask) << ".";
        o << ((intVersion >> 16) & kMask) << ".";
        o << (intVersion & kMask);

        GPUDeviceInfo device;
        device.vendorId      = desc.VendorId;
        device.deviceId      = desc.DeviceId;
        device.driverVersion = o.str();

        devices->push_back(device);

        adapter->Release();
    }

    factory->Release();

    return true;
}

#else
#    error
#endif

}  // anonymous namespace

bool GetSystemInfo(SystemInfo *info)
{
    // Get the CM device ID first so that it is returned even in error cases.
    info->primaryDisplayDeviceId = GetPrimaryDisplayDeviceId();

#if defined(GPU_INFO_USE_SETUPAPI)
    if (!GetDevicesFromRegistry(&info->gpus))
    {
        return false;
    }
#elif defined(GPU_INFO_USE_DXGI)
    if (!GetDevicesFromDXGI(&info->gpus))
    {
        return false;
    }
#else
#    error
#endif

    if (info->gpus.size() == 0)
    {
        return false;
    }

    FindPrimaryGPU(info);

    // Override the primary GPU index with what we gathered from EnumDisplayDevices
    uint32_t primaryVendorId = 0;
    uint32_t primaryDeviceId = 0;

    if (!CMDeviceIDToDeviceAndVendorID(info->primaryDisplayDeviceId, &primaryVendorId,
                                       &primaryDeviceId))
    {
        return false;
    }

    bool foundPrimary = false;
    for (size_t i = 0; i < info->gpus.size(); ++i)
    {
        if (info->gpus[i].vendorId == primaryVendorId && info->gpus[i].deviceId == primaryDeviceId)
        {
            info->primaryGPUIndex = static_cast<int>(i);
            foundPrimary          = true;
        }
    }
    ASSERT(foundPrimary);

    // nvd3d9wrap.dll is loaded into all processes when Optimus is enabled.
    HMODULE nvd3d9wrap = GetModuleHandleW(L"nvd3d9wrap.dll");
    info->isOptimus    = nvd3d9wrap != nullptr;

    return true;
}

}  // namespace angle
