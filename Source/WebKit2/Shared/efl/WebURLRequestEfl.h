/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 */

#ifndef WebURLRequestEfl_h
#define WebURLRequestEfl_h

#include "WebURLRequest.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebURLRequestEfl : public RefCounted<WebURLRequestEfl> {
public:
    static PassRefPtr<WebURLRequestEfl> create(const WebURLRequest* request)
    {
        return adoptRef(new WebURLRequestEfl(request));
    }

    const String cookies() const;

private:
    explicit WebURLRequestEfl(const WebURLRequest*);

    const WebURLRequest* m_request;
};

} // namespace WebKit

#endif // WebURLRequestEfl_h
