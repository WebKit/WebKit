/*
 * Copyright (C) 2020 Jan-Michael Brummer <jan.brummer@tabos.org>
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

#include "config.h"
#include <wtf/glib/ChassisType.h>

#include <mutex>
#include <wtf/Optional.h>
#include <wtf/glib/GUniquePtr.h>

namespace WTF {

static Optional<ChassisType> readMachineInfoChassisType()
{
    GUniqueOutPtr<char> buffer;
    GUniqueOutPtr<GError> error;
    if (!g_file_get_contents("/etc/machine-info", &buffer.outPtr(), nullptr, &error.outPtr())) {
        if (!g_error_matches(error.get(), G_FILE_ERROR, G_FILE_ERROR_NOENT))
            g_warning("Could not open /etc/machine-info: %s", error->message);
        return WTF::nullopt;
    }

    GUniquePtr<char*> split(g_strsplit(buffer.get(), "\n", -1));
    for (int i = 0; split.get()[i]; ++i) {
        if (g_str_has_prefix(split.get()[i], "CHASSIS=")) {
            char* chassis = split.get()[i] + 8;

            GUniquePtr<char> unquoted(g_shell_unquote(chassis, &error.outPtr()));
            if (error)
                g_warning("Could not unquote chassis type %s: %s", chassis, error->message);

            if (!strcmp(unquoted.get(), "tablet") || !strcmp(unquoted.get(), "handset"))
                return ChassisType::Mobile;

            return ChassisType::Desktop;
        }
    }

    return WTF::nullopt;
}

static Optional<ChassisType> readDMIChassisType()
{
    GUniqueOutPtr<char> buffer;
    GUniqueOutPtr<GError> error;
    if (g_file_get_contents("/sys/class/dmi/id/chassis_type", &buffer.outPtr(), nullptr, &error.outPtr())) {
        int type = strtol(buffer.get(), nullptr, 10);

        // See the SMBIOS Specification 3.0 section 7.4.1 for details about the values listed here:
        // https://www.dmtf.org/sites/default/files/standards/documents/DSP0134_3.0.0.pdf
        switch (type) {
        case 0x3: /* Desktop */
        case 0x4: /* Low Profile Desktop */
        case 0x6: /* Mini Tower */
        case 0x7: /* Tower */
        case 0x8: /* Portable */
        case 0x9: /* Laptop */
        case 0xA: /* Notebook */
        case 0xE: /* Sub Notebook */
        case 0x11: /* Main Server Chassis */
        case 0x1C: /* Blade */
        case 0x1D: /* Blade Enclosure */
        case 0x1F: /* Convertible */
        case 0x20: /* Detachable */
            return ChassisType::Desktop;

        case 0xB: /* Hand Held */
        case 0x1E: /* Tablet */
            return ChassisType::Mobile;
        }
    } else if (!g_error_matches(error.get(), G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning("Could not open /sys/class/dmi/id/chassis_type: %s", error->message);

    return WTF::nullopt;
}

static Optional<ChassisType> readACPIChassisType()
{
    GUniqueOutPtr<char> buffer;
    GUniqueOutPtr<GError> error;
    if (g_file_get_contents("/sys/firmware/acpi/pm_profile", &buffer.outPtr(), nullptr, &error.outPtr())) {
        int type = strtol(buffer.get(), nullptr, 10);

        // See the ACPI 5.0 Spec Section 5.2.9.1 for details:
        // http://www.acpi.info/DOWNLOADS/ACPIspec50.pdf
        switch (type) {
        case 1: /* Desktop */
        case 2: /* Mobile */
        case 3: /* Workstation */
        case 4: /* Enterprise Server */
        case 5: /* SOHO Server */
        case 6: /* Appliance PC */
        case 7: /* Performance Server */
            return ChassisType::Desktop;

        case 8: /* Tablet */
            return ChassisType::Mobile;
        }
    } else if (!g_error_matches(error.get(), G_FILE_ERROR, G_FILE_ERROR_NOENT))
        g_warning("Could not open /sys/firmware/acpi/pm_profile: %s", error->message);

    return WTF::nullopt;
}

ChassisType chassisType()
{
    static ChassisType chassisType;
    static std::once_flag initializeChassis;
    std::call_once(initializeChassis, [] {
        Optional<ChassisType> optionalChassisType = readMachineInfoChassisType();
        if (!optionalChassisType)
            optionalChassisType = readDMIChassisType();
        if (!optionalChassisType)
            optionalChassisType = readACPIChassisType();
        chassisType = optionalChassisType.valueOr(ChassisType::Desktop);
    });

    return chassisType;
}

} // namespace WTF
