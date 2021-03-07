/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#pragma once

#include <wtf/Forward.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/RetainPtr.h>
#endif

namespace WebCore {
class IntRect;
}

class MarshallingHelpers
{
public:
    static URL BSTRToKURL(BSTR);
    static BSTR URLToBSTR(const URL&);

#if USE(CF)
    static RetainPtr<CFURLRef> PathStringToFileCFURLRef(const WTF::String&);
    static WTF::String FileCFURLRefToPathString(CFURLRef fileURL);
    static RetainPtr<CFURLRef> BSTRToCFURLRef(BSTR);
    static RetainPtr<CFStringRef> BSTRToCFStringRef(BSTR);
    static RetainPtr<CFStringRef> LPCOLESTRToCFStringRef(LPCOLESTR);
    static BSTR CFStringRefToBSTR(CFStringRef);
    static int CFNumberRefToInt(CFNumberRef);
    static RetainPtr<CFNumberRef> intToCFNumberRef(int);
    static CFAbsoluteTime DATEToCFAbsoluteTime(DATE);
    static DATE CFAbsoluteTimeToDATE(CFAbsoluteTime);
    static SAFEARRAY* stringArrayToSafeArray(CFArrayRef);
    static SAFEARRAY* intArrayToSafeArray(CFArrayRef);
#else
    static double DATEToAbsoluteTime(DATE);
    static DATE absoluteTimeToDATE(double);
#endif

    static SAFEARRAY* intRectToSafeArray(const WebCore::IntRect&);

#if USE(CF)
    static SAFEARRAY* iunknownArrayToSafeArray(CFArrayRef);
    static RetainPtr<CFArrayRef> safeArrayToStringArray(SAFEARRAY*);
    static RetainPtr<CFArrayRef> safeArrayToIntArray(SAFEARRAY*);
    static RetainPtr<CFArrayRef> safeArrayToIUnknownArray(SAFEARRAY*);
    static const void* IUnknownRetainCallback(CFAllocatorRef, const void*);
    static void IUnknownReleaseCallback(CFAllocatorRef, const void*);
    static CFArrayCallBacks kIUnknownArrayCallBacks;
    static CFDictionaryValueCallBacks kIUnknownDictionaryValueCallBacks;
#endif

private:
#if USE(CF)
    static CFAbsoluteTime windowsEpochAbsoluteTime();
#else
    static double windowsEpochAbsoluteTime();
#endif

private:
    MarshallingHelpers();
    ~MarshallingHelpers();
};
