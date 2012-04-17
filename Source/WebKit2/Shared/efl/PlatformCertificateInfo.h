/*
   Copyright (C) 2012 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef PlatformCertificateInfo_h
#define PlatformCertificateInfo_h

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include <WebCore/ResourceResponse.h>

namespace WebKit {

class PlatformCertificateInfo {
public:
    PlatformCertificateInfo()
    {
    }

    explicit PlatformCertificateInfo(const WebCore::ResourceResponse&)
    {
    }

    void encode(CoreIPC::ArgumentEncoder*) const
    {
    }

    static bool decode(CoreIPC::ArgumentDecoder*, PlatformCertificateInfo&)
    {
        return true;
    }
};

} // namespace WebKit

#endif // PlatformCertificateInfo_h
