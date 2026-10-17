/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorBrowserAgent.h"

#include <array>
#include <cerrno>
#include <optional>
#include <span>
#include <sys/sysctl.h>
#include <unistd.h>
#include <wtf/RetainPtr.h>

#if PLATFORM(MAC)
#include <IOKit/IOKitLib.h>
#include <wtf/cf/TypeCastsCF.h>
#elif PLATFORM(IOS_FAMILY)
#include <UIKit/UIDevice.h>
#endif

namespace WebKit {

using namespace Inspector;

static Ref<Inspector::Protocol::Browser::VirtualizationEvidence> unavailableVirtualizationEvidence(const String& source, const String& description, std::optional<int> errorCode = std::nullopt)
{
    auto evidence = Inspector::Protocol::Browser::VirtualizationEvidence::create()
        .setSource(source)
        .setStatus(Inspector::Protocol::Browser::VirtualizationEvidence::Status::Unavailable)
        .setDescription(description)
        .release();
    if (errorCode)
        evidence->setErrorCode(*errorCode);
    return evidence;
}

static Ref<Inspector::Protocol::Browser::VirtualizationEvidence> failedVirtualizationEvidence(const String& source, int errorCode)
{
    String description;
    switch (errorCode) {
    case ENOENT:
    case ENOTSUP:
        description = "This OS does not expose the property."_s;
        break;
    case EACCES:
    case EPERM:
        description = "The OS denied access to the property."_s;
        break;
    default:
        description = "The OS query failed."_s;
        break;
    }
    return unavailableVirtualizationEvidence(source, description, errorCode);
}

Inspector::Protocol::ErrorStringOr<String> InspectorBrowserAgent::getHardwareId()
{
#if PLATFORM(MAC)
    auto platformExpert = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("IOPlatformExpertDevice"));
    if (!platformExpert)
        return makeUnexpected("Hardware ID is unavailable"_s);

    RetainPtr property = adoptCF(IORegistryEntryCreateCFProperty(platformExpert, CFSTR("IOPlatformUUID"), kCFAllocatorDefault, 0));
    IOObjectRelease(platformExpert);
    auto uuid = dynamic_cf_cast<CFStringRef>(property.get());
    if (!uuid || !CFStringGetLength(uuid))
        return makeUnexpected("Hardware ID is unavailable"_s);

    return String(uuid);
#elif PLATFORM(IOS_FAMILY)
    RetainPtr<NSUUID> identifier = [UIDevice currentDevice].identifierForVendor;
    if (!identifier)
        return makeUnexpected("Vendor identifier is currently unavailable"_s);

    return String(identifier.get().UUIDString);
#endif
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Browser::EnvironmentInfo>> InspectorBrowserAgent::getEnvironmentInfo()
{
    struct kinfo_proc processInfo { };
    std::array mib { CTL_KERN, KERN_PROC, KERN_PROC_PID, getpid() };
    size_t size = sizeof(processInfo);
    if (sysctl(mib.data(), mib.size(), &processInfo, &size, nullptr, 0) == -1 || size != sizeof(processInfo))
        return makeUnexpected("Unable to determine whether a native debugger is attached"_s);

    using Evidence = Inspector::Protocol::Browser::VirtualizationEvidence;
    using VirtualizationStatus = Inspector::Protocol::Browser::EnvironmentInfo::VirtualizationStatus;
    auto evidence = JSON::ArrayOf<Evidence>::create();
    std::optional<bool> isVirtualMachine;

#if PLATFORM(IOS_FAMILY_SIMULATOR)
    isVirtualMachine = true;
    evidence->addItem(Evidence::create()
        .setSource("ios-simulator"_s)
        .setStatus(Evidence::Status::Detected)
        .setDescription("This app was built for an iOS family simulator."_s)
        .release());
    evidence->addItem(unavailableVirtualizationEvidence("kern.hv_vmm_present"_s, "Skipped because the simulator's kernel query can describe the host instead of the simulated device."_s));
    evidence->addItem(unavailableVirtualizationEvidence("hw.model"_s, "Skipped because the simulator's hardware model can describe the host instead of the simulated device."_s));
#else
    evidence->addItem(Evidence::create()
        .setSource("ios-simulator"_s)
        .setStatus(Evidence::Status::NotDetected)
        .setDescription("This app was not built for an iOS family simulator. It may still run in a virtual machine."_s)
        .release());

    int kernelFlag = 0;
    size = sizeof(kernelFlag);
    if (sysctlbyname("kern.hv_vmm_present", &kernelFlag, &size, nullptr, 0) == -1)
        evidence->addItem(failedVirtualizationEvidence("kern.hv_vmm_present"_s, errno));
    else if (size != sizeof(kernelFlag) || (kernelFlag && kernelFlag != 1))
        evidence->addItem(unavailableVirtualizationEvidence("kern.hv_vmm_present"_s, "The OS returned an unexpected virtualization flag."_s));
    else {
        isVirtualMachine = !!kernelFlag;
        auto observation = Evidence::create()
            .setSource("kern.hv_vmm_present"_s)
            .setStatus(kernelFlag ? Evidence::Status::Detected : Evidence::Status::NotDetected)
            .setDescription(kernelFlag ? "The kernel reports a virtual machine."_s : "The kernel reports no virtual machine. This does not prove physical hardware."_s)
            .release();
        observation->setValue(kernelFlag ? "1"_s : "0"_s);
        evidence->addItem(WTF::move(observation));
    }

    std::array<char, 256> modelBuffer { };
    size = modelBuffer.size();
    if (sysctlbyname("hw.model", modelBuffer.data(), &size, nullptr, 0) == -1)
        evidence->addItem(failedVirtualizationEvidence("hw.model"_s, errno));
    else if (size < 2 || size > modelBuffer.size() || modelBuffer[size - 1])
        evidence->addItem(unavailableVirtualizationEvidence("hw.model"_s, "The OS returned an unexpected hardware model."_s));
    else {
        auto model = String::fromUTF8(std::span { modelBuffer }.first(size - 1));
        if (model.isNull())
            evidence->addItem(unavailableVirtualizationEvidence("hw.model"_s, "The OS returned an invalid hardware model string."_s));
        else {
            bool hasVirtualModel = model.startsWith("VirtualMac"_s);
            if (hasVirtualModel)
                isVirtualMachine = true;
            auto observation = Evidence::create()
                .setSource("hw.model"_s)
                .setStatus(hasVirtualModel ? Evidence::Status::Detected : Evidence::Status::NotDetected)
                .setDescription(hasVirtualModel ? "The hardware model identifies a virtual Mac."_s : "The hardware model has no recognized virtual Mac prefix. This does not prove physical hardware."_s)
                .release();
            observation->setValue(model);
            evidence->addItem(WTF::move(observation));
        }
    }
#endif

    auto status = !isVirtualMachine ? VirtualizationStatus::Unknown : *isVirtualMachine ? VirtualizationStatus::Detected : VirtualizationStatus::NotDetected;
    auto environmentInfo = Inspector::Protocol::Browser::EnvironmentInfo::create()
        .setIsBeingDebugged(!!(processInfo.kp_proc.p_flag & P_TRACED))
        .setVirtualizationStatus(status)
        .setVirtualizationEvidence(WTF::move(evidence))
        .release();
    if (isVirtualMachine)
        environmentInfo->setIsVirtualMachine(*isVirtualMachine);
    return environmentInfo;
}

} // namespace WebKit
