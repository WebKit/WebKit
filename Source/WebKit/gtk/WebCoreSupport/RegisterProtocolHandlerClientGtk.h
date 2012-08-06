/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef RegisterProtocolHandlerClientGtk_h
#define RegisterProtocolHandlerClientGtk_h

#if ENABLE(REGISTER_PROTOCOL_HANDLER)
#include "RegisterProtocolHandlerClient.h"

#include <wtf/PassOwnPtr.h>

namespace WebKit {

class RegisterProtocolHandlerClient : public WebCore::RegisterProtocolHandlerClient {
public:
    static PassOwnPtr<RegisterProtocolHandlerClient> create();

    ~RegisterProtocolHandlerClient() { }

    virtual void registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title);

private:
    RegisterProtocolHandlerClient();
};

}

#endif
#endif // RegisterProtocolHandlerClientGtk_h
