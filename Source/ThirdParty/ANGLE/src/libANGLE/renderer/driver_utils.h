//
// Copyright (c) 2016 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

// driver_utils.h : provides more information about current driver.

#ifndef LIBANGLE_RENDERER_DRIVER_UTILS_H_
#define LIBANGLE_RENDERER_DRIVER_UTILS_H_

#include "libANGLE/angletypes.h"

namespace rx
{

enum VendorID : uint32_t
{
    VENDOR_ID_UNKNOWN = 0x0,
    VENDOR_ID_AMD     = 0x1002,
    VENDOR_ID_INTEL   = 0x8086,
    VENDOR_ID_NVIDIA  = 0x10DE,
    // This is Qualcomm PCI Vendor ID.
    // Android doesn't have a PCI bus, but all we need is a unique id.
    VENDOR_ID_QUALCOMM = 0x5143,
};

inline bool IsAMD(uint32_t vendor_id)
{
    return vendor_id == VENDOR_ID_AMD;
}

inline bool IsIntel(uint32_t vendor_id)
{
    return vendor_id == VENDOR_ID_INTEL;
}

inline bool IsNvidia(uint32_t vendor_id)
{
    return vendor_id == VENDOR_ID_NVIDIA;
}

inline bool IsQualcomm(uint32_t vendor_id)
{
    return vendor_id == VENDOR_ID_QUALCOMM;
}

// Intel
class IntelDriverVersion
{
  public:
    // Currently, We only provide the constructor with one parameter. It mainly used in Intel
    // version number on windows. If you want to use this class on other platforms, it's easy to
    // be extended.
    IntelDriverVersion(uint16_t lastPart);
    bool operator==(const IntelDriverVersion &);
    bool operator!=(const IntelDriverVersion &);
    bool operator<(const IntelDriverVersion &);
    bool operator>=(const IntelDriverVersion &);

  private:
    uint16_t mVersionPart;
};

bool IsHaswell(uint32_t DeviceId);
bool IsBroadwell(uint32_t DeviceId);
bool IsCherryView(uint32_t DeviceId);
bool IsSkylake(uint32_t DeviceId);
bool IsBroxton(uint32_t DeviceId);
bool IsKabylake(uint32_t DeviceId);

}  // namespace rx
#endif  // LIBANGLE_RENDERER_DRIVER_UTILS_H_
