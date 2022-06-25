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

#include "WebKitDLL.h"
#include "MarshallingHelpers.h"

#include <WebCore/BString.h>
#include <WebCore/IntRect.h>
#include <wtf/DateMath.h>
#include <wtf/MathExtras.h>
#include <wtf/URL.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

#if USE(CF)

CFArrayCallBacks MarshallingHelpers::kIUnknownArrayCallBacks = {0, IUnknownRetainCallback, IUnknownReleaseCallback, 0, 0};
CFDictionaryValueCallBacks MarshallingHelpers::kIUnknownDictionaryValueCallBacks = {0, IUnknownRetainCallback, IUnknownReleaseCallback, 0, 0};

#endif

URL MarshallingHelpers::BSTRToKURL(BSTR urlStr)
{
    return URL { String(urlStr, SysStringLen(urlStr)) };
}

BSTR MarshallingHelpers::URLToBSTR(const URL& url)
{
    return BString(url.string()).release();
}

#if USE(CF)
RetainPtr<CFURLRef> MarshallingHelpers::PathStringToFileCFURLRef(const String& string)
{
    return adoptCF(CFURLCreateWithFileSystemPath(0, string.createCFString().get(), kCFURLWindowsPathStyle, false));
}

String MarshallingHelpers::FileCFURLRefToPathString(CFURLRef fileURL)
{
    return adoptCF(CFURLCopyFileSystemPath(fileURL, kCFURLWindowsPathStyle)).get();
}

RetainPtr<CFURLRef> MarshallingHelpers::BSTRToCFURLRef(BSTR urlStr)
{
    auto urlCFString = BSTRToCFStringRef(urlStr);
    if (!urlCFString)
        return nullptr;

    return adoptCF(CFURLCreateWithString(nullptr, urlCFString.get(), nullptr));
}

RetainPtr<CFStringRef> MarshallingHelpers::BSTRToCFStringRef(BSTR str)
{
    return adoptCF(CFStringCreateWithCharacters(0, (const UniChar*)(str ? str : TEXT("")), SysStringLen(str)));
}

RetainPtr<CFStringRef> MarshallingHelpers::LPCOLESTRToCFStringRef(LPCOLESTR str)
{
    return adoptCF(CFStringCreateWithCharacters(0, (const UniChar*)(str ? str : TEXT("")), (CFIndex)(str ? wcslen(str) : 0)));
}

BSTR MarshallingHelpers::CFStringRefToBSTR(CFStringRef str)
{
    return BString(str).release();
}

int MarshallingHelpers::CFNumberRefToInt(CFNumberRef num)
{
    int number;
    CFNumberGetValue(num, kCFNumberIntType, &number);
    return number;
}

RetainPtr<CFNumberRef> MarshallingHelpers::intToCFNumberRef(int num)
{
    return adoptCF(CFNumberCreate(0, kCFNumberSInt32Type, &num));
}
#endif

#if USE(CF)
CFAbsoluteTime MarshallingHelpers::windowsEpochAbsoluteTime()
{
    static CFAbsoluteTime windowsEpochAbsoluteTime = 0;
    if (!windowsEpochAbsoluteTime) {
        CFGregorianDate windowsEpochDate = {1899, 12, 30, 0, 0, 0.0};
        windowsEpochAbsoluteTime = CFGregorianDateGetAbsoluteTime(windowsEpochDate, 0) / secondsPerDay;
    }
    return windowsEpochAbsoluteTime;
}
#else
double MarshallingHelpers::windowsEpochAbsoluteTime()
{
    static double windowsEpochAbsoluteTime = 0;
    if (!windowsEpochAbsoluteTime) {
        SYSTEMTIME windowsEpochDate = { };
        windowsEpochDate.wYear = 1899;
        windowsEpochDate.wMonth = 12;
        windowsEpochDate.wDay = 30;

        FILETIME absoluteFileTime;
        SystemTimeToFileTime(&windowsEpochDate, &absoluteFileTime);

        ULARGE_INTEGER timeValue;
        timeValue.LowPart = absoluteFileTime.dwLowDateTime;
        timeValue.HighPart = absoluteFileTime.dwHighDateTime;

        windowsEpochAbsoluteTime = timeValue.QuadPart;
    }
    return windowsEpochAbsoluteTime;
}
#endif

#if USE(CF)
CFAbsoluteTime MarshallingHelpers::DATEToCFAbsoluteTime(DATE date)
#else
double MarshallingHelpers::DATEToAbsoluteTime(DATE date)
#endif
{
    // <http://msdn2.microsoft.com/en-us/library/ms221627.aspx>
    // DATE: This is the same numbering system used by most spreadsheet programs,
    // although some specify incorrectly that February 29, 1900 existed, and thus
    // set January 1, 1900 to 1.0. The date can be converted to and from an MS-DOS
    // representation using VariantTimeToDosDateTime, which is discussed in
    // Conversion and Manipulation Functions.

    // CFAbsoluteTime: Type used to represent a specific point in time relative
    // to the absolute reference date of 1 Jan 2001 00:00:00 GMT.
    // Absolute time is measured by the number of seconds between the reference
    // date and the specified date. Negative values indicate dates/times before
    // the reference date. Positive values indicate dates/times after the
    // reference date.

    return round((date + windowsEpochAbsoluteTime()) * secondsPerDay);
}

#if USE(CF)
DATE MarshallingHelpers::CFAbsoluteTimeToDATE(CFAbsoluteTime absoluteTime)
#else
DATE MarshallingHelpers::absoluteTimeToDATE(double absoluteTime)
#endif
{
    return (round(absoluteTime)/secondsPerDay - windowsEpochAbsoluteTime());
}

#if USE(CF)
// utility method to store a 1-dim string vector into a newly created SAFEARRAY
SAFEARRAY* MarshallingHelpers::stringArrayToSafeArray(CFArrayRef inArray)
{
    CFIndex size = CFArrayGetCount(inArray);
    SAFEARRAY* sa = ::SafeArrayCreateVectorEx(VT_BSTR, 0, (ULONG) size, 0);
    long count = 0;
    for (CFIndex i=0; i<size; i++) {
        CFStringRef item = (CFStringRef) CFArrayGetValueAtIndex(inArray, i);
        BString bstr(item);
        ::SafeArrayPutElement(sa, &count, bstr); // SafeArrayPutElement() copies the string correctly.
        count++;
    }
    return sa;
}

// utility method to store a 1-dim int vector into a newly created SAFEARRAY
SAFEARRAY* MarshallingHelpers::intArrayToSafeArray(CFArrayRef inArray)
{
    CFIndex size = CFArrayGetCount(inArray);
    SAFEARRAY* sa = ::SafeArrayCreateVectorEx(VT_I4, 0, (ULONG) size, 0);
    long count = 0;
    for (CFIndex i=0; i<size; i++) {
        CFNumberRef item = (CFNumberRef) CFArrayGetValueAtIndex(inArray, i);
        int number = CFNumberRefToInt(item);
        ::SafeArrayPutElement(sa, &count, &number);
        count++;
    }
    return sa;
}
#endif

SAFEARRAY* MarshallingHelpers::intRectToSafeArray(const WebCore::IntRect& rect)
{
    SAFEARRAY* sa = ::SafeArrayCreateVectorEx(VT_I4, 0, 4, 0);
    long count = 0;
    int value;

    value = rect.x();
    ::SafeArrayPutElement(sa, &count, &value);
    count++;

    value = rect.y();
    ::SafeArrayPutElement(sa, &count, &value);
    count++;

    value = rect.width();
    ::SafeArrayPutElement(sa, &count, &value);
    count++;

    value = rect.height();
    ::SafeArrayPutElement(sa, &count, &value);
    count++;

    return sa;
}

#if USE(CF)

// utility method to store a 1-dim IUnknown* vector into a newly created SAFEARRAY
SAFEARRAY* MarshallingHelpers::iunknownArrayToSafeArray(CFArrayRef inArray)
{
    CFIndex size = CFArrayGetCount(inArray);
    SAFEARRAY* sa = ::SafeArrayCreateVectorEx(VT_UNKNOWN, 0, (ULONG) size, (LPVOID)&IID_IUnknown);
    long count = 0;
    for (CFIndex i=0; i<size; i++) {
        IUnknown* item = (IUnknown*) CFArrayGetValueAtIndex(inArray, i);
        ::SafeArrayPutElement(sa, &count, item);    // SafeArrayPutElement() adds a reference to the IUnknown added
        count++;
    }
    return sa;
}

RetainPtr<CFArrayRef> MarshallingHelpers::safeArrayToStringArray(SAFEARRAY* inArray)
{
    long lBound=0, uBound=-1;
    HRESULT hr = ::SafeArrayGetLBound(inArray, 1, &lBound);
    if (SUCCEEDED(hr))
        hr = ::SafeArrayGetUBound(inArray, 1, &uBound);
    long len = (SUCCEEDED(hr)) ? (uBound-lBound+1) : 0;
    Vector<CFStringRef> items;
    if (len > 0) {
        items.resize(len);
        for (; lBound <= uBound; lBound++) {
            BString str;
            hr = ::SafeArrayGetElement(inArray, &lBound, &str);
            items[lBound] = BSTRToCFStringRef(str).leakRef();
        }
    }
    return adoptCF(CFArrayCreate(0, (const void**)items.data(), len, &kCFTypeArrayCallBacks));
}

RetainPtr<CFArrayRef> MarshallingHelpers::safeArrayToIntArray(SAFEARRAY* inArray)
{
    long lBound=0, uBound=-1;
    HRESULT hr = ::SafeArrayGetLBound(inArray, 1, &lBound);
    if (SUCCEEDED(hr))
        hr = ::SafeArrayGetUBound(inArray, 1, &uBound);
    long len = (SUCCEEDED(hr)) ? (uBound-lBound+1) : 0;
    Vector<CFNumberRef> items;
    if (len > 0) {
        items.resize(len);
        for (; lBound <= uBound; lBound++) {
            int num = 0;
            hr = ::SafeArrayGetElement(inArray, &lBound, &num);
            items[lBound] = SUCCEEDED(hr) ? intToCFNumberRef(num).leakRef() : kCFNumberNaN;
        }
    }
    return adoptCF(CFArrayCreate(0, (const void**)items.data(), len, &kCFTypeArrayCallBacks));
}

RetainPtr<CFArrayRef> MarshallingHelpers::safeArrayToIUnknownArray(SAFEARRAY* inArray)
{
    long lBound=0, uBound=-1;
    HRESULT hr = ::SafeArrayGetLBound(inArray, 1, &lBound);
    if (SUCCEEDED(hr))
        hr = ::SafeArrayGetUBound(inArray, 1, &uBound);
    long len = (SUCCEEDED(hr)) ? (uBound-lBound+1) : 0;
    void* items = nullptr;
    hr = ::SafeArrayAccessData(inArray, &items);
    auto result = SUCCEEDED(hr) ? adoptCF(CFArrayCreate(0, (const void**) items, len, &kIUnknownArrayCallBacks)) : nullptr;
    hr = ::SafeArrayUnaccessData(inArray);
    return result;
}

const void* MarshallingHelpers::IUnknownRetainCallback(CFAllocatorRef /*allocator*/, const void* value)
{
    ((IUnknown*) value)->AddRef();
    return value;
}

void MarshallingHelpers::IUnknownReleaseCallback(CFAllocatorRef /*allocator*/, const void* value)
{
    ((IUnknown*) value)->Release();
}

#endif
