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

#ifndef WebURLResponseEfl_h
#define WebURLResponseEfl_h

#include "WebURLResponse.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebKit {

class WebURLResponseEfl : public RefCounted<WebURLResponseEfl> {
public:
    static PassRefPtr<WebURLResponseEfl> create(const WebURLResponse* response)
    {
        return adoptRef(new WebURLResponseEfl(response));
    }

    const String& contentType() const;

private:
    explicit WebURLResponseEfl(const WebURLResponse*);

    const WebURLResponse* m_response;
};

} // namespace WebKit

#endif // WebURLResponseEfl_h
