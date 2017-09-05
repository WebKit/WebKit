/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include "SystemProxyWin.h"

#include <WebCore/CurlContext.h>

bool WindowsSystemProxy::getSystemHttpProxy(char* buffer, int bufferLen, int* port)
{
    std::unique_ptr<TCHAR[]> tRegBuffer = std::make_unique<TCHAR[]>(bufferLen);
    std::unique_ptr<TCHAR[]> tHost = std::make_unique<TCHAR[]>(bufferLen);
    if (!tRegBuffer || !tHost)
        return false;
    DWORD type;
    DWORD size;
    HKEY key;

    LONG ret = RegOpenKeyEx(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        0,
        KEY_READ,
        &key);
    if (ret != ERROR_SUCCESS)
        return false;

    size = bufferLen - 1;
    ret = RegQueryValueEx(key,
        L"ProxyServer",
        nullptr,
        &type,
        (LPBYTE)tRegBuffer.get(),
        &size);

    if (ret != ERROR_SUCCESS)
        return false;

    if (!parseProxyString(tRegBuffer.get(), tHost.get(), bufferLen, port))
        return false;

    wcstombs(buffer, tHost.get(), bufferLen);
    buffer[bufferLen-1] = '\0';
    return true;
}

void WindowsSystemProxy::setCurlHttpProxy(char* proxy, int port)
{
    WebCore::CurlContext::singleton().setProxyInfo(proxy, port);
}

void WindowsSystemProxy::setCurlHttpProxy()
{
    char proxy[ProxyServerNameLength];
    int port;
    if (getSystemHttpProxy(proxy, ProxyServerNameLength, &port))
        setCurlHttpProxy(proxy, port);
}

bool WindowsSystemProxy::parseProxyString(const TCHAR* regProxyString, TCHAR* hostString, int hostStringLen, int* port)
{
    const TCHAR* found = wcschr(regProxyString, L':');
    if (!found)
        return false;

    int len = found - regProxyString;
    if (len >= hostStringLen)
        return false;

    wcsncpy(hostString, regProxyString, hostStringLen);
    hostString[len] = L'\0';

    TCHAR* portStr = const_cast<TCHAR*>(found) + 1;
    *port = _wtoi(portStr);

    return true;
}
