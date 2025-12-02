/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import <wtf/SystemTracing.h>

#if HAVE(OS_SIGNPOST)

#import <dispatch/dispatch.h>
#import <mach/mach_time.h>
#import <wtf/ContinuousTime.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/OSObjectPtr.h>

bool WTFSignpostIndirectLoggingEnabled;

os_log_t WTFSignpostLogHandle()
{
    SUPPRESS_RETAINPTR_CTOR_ADOPT static NeverDestroyed<OSObjectPtr<os_log_t>> handle = adoptOSObject(os_log_create("com.apple.WebKit", "Signposts"));
    return handle.get().get();
}

#define WTF_SIGNPOST_EVENT_EMIT_FUNC_CASE_LABEL(emitFunc, name, timeFormat) \
    case WTFOSSignpostName ## name : \
        emitFunc(log, signpostIdentifier, #name, "pid: %d | %{public}s" timeFormat, pid, logString, timestamp); \
        break;

static void beginSignpostInterval(os_log_t log, WTFOSSignpostName signpostName, uint64_t signpostIdentifier, uint64_t timestamp, pid_t pid, const char* logString)
{
#define WTF_SIGNPOST_EVENT_BEGIN_INTERVAL_CASE_LABEL(name) \
    WTF_SIGNPOST_EVENT_EMIT_FUNC_CASE_LABEL(os_signpost_interval_begin, name, " %{signpost.description:begin_time}llu")

    switch (signpostName) {
        FOR_EACH_WTF_SIGNPOST_NAME(WTF_SIGNPOST_EVENT_BEGIN_INTERVAL_CASE_LABEL)
        default: break;
    }
}

static void endSignpostInterval(os_log_t log, WTFOSSignpostName signpostName, uint64_t signpostIdentifier, uint64_t timestamp, pid_t pid, const char* logString)
{
#define WTF_SIGNPOST_EVENT_END_INTERVAL_CASE_LABEL(name) \
    WTF_SIGNPOST_EVENT_EMIT_FUNC_CASE_LABEL(os_signpost_interval_end, name, " %{signpost.description:end_time}llu")

    switch (signpostName) {
        FOR_EACH_WTF_SIGNPOST_NAME(WTF_SIGNPOST_EVENT_END_INTERVAL_CASE_LABEL)
        default: break;
    }
}

static void emitSignpostEvent(os_log_t log, WTFOSSignpostName signpostName, uint64_t signpostIdentifier, uint64_t timestamp, pid_t pid, const char* logString)
{
#define WTF_SIGNPOST_EVENT_EMIT_CASE_LABEL(name) \
    WTF_SIGNPOST_EVENT_EMIT_FUNC_CASE_LABEL(os_signpost_event_emit, name, " %{signpost.description:event_time}llu")

    switch (signpostName) {
        FOR_EACH_WTF_SIGNPOST_NAME(WTF_SIGNPOST_EVENT_EMIT_CASE_LABEL)
        default: break;
    }
}

static void emitSignpost(os_log_t log, WTFOSSignpostType type, WTFOSSignpostName name, uint64_t signpostIdentifier, uint64_t timestamp, pid_t pid, const char* logString)
{
    switch (type) {
    case WTFOSSignpostTypeBeginInterval:
        beginSignpostInterval(log, name, signpostIdentifier, timestamp, pid, logString);
        break;
    case WTFOSSignpostTypeEndInterval:
        endSignpostInterval(log, name, signpostIdentifier, timestamp, pid, logString);
        break;
    case WTFOSSignpostTypeEmitEvent:
        emitSignpostEvent(log, name, signpostIdentifier, timestamp, pid, logString);
        break;
    default:
        break;
    }
}

bool WTFSignpostHandleIndirectLog(os_log_t log, pid_t pid, std::span<const char> nullTerminatedLogString)
{
    if (log != WTFSignpostLogHandle() || !nullTerminatedLogString.data())
        return false;

    int signpostType = 0;
    int signpostName = 0;
    uintptr_t signpostIdentifierPointer = 0;
    uint64_t timestamp = 0;
    int bytesConsumed = 0;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
    if (sscanf(nullTerminatedLogString.data(), "type=%d name=%d p=%" SCNuPTR " ts=%llu %n", &signpostType, &signpostName, &signpostIdentifierPointer, &timestamp, &bytesConsumed) != 4)
        return false;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

    if (signpostType < 0 || signpostType >= WTFOSSignpostTypeCount)
        return false;

    if (signpostName < 0 || signpostName >= WTFOSSignpostNameCount)
        return false;

    // Mix in bits from the pid, since pointers from different pids could be at the same address, causing signpost IDs to clash.
    signpostIdentifierPointer ^= pid;
    auto signpostIdentifier = os_signpost_id_make_with_pointer(log, reinterpret_cast<const void *>(signpostIdentifierPointer));

    emitSignpost(log, static_cast<WTFOSSignpostType>(signpostType), static_cast<WTFOSSignpostName>(signpostName), signpostIdentifier, timestamp, pid, nullTerminatedLogString.subspan(bytesConsumed).data());
    return true;
}

uint64_t WTFCurrentContinuousTime(Seconds deltaFromNow)
{
    return (ContinuousTime::now() + deltaFromNow).toMachContinuousTime();
}

#endif
