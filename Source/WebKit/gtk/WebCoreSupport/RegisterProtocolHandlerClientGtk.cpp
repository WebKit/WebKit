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

#include "config.h"
#include "RegisterProtocolHandlerClientGtk.h"

#if ENABLE(REGISTER_PROTOCOL_HANDLER)

#include "NotImplemented.h"
#include <wtf/text/CString.h>

namespace WebKit {

PassOwnPtr<RegisterProtocolHandlerClient> RegisterProtocolHandlerClient::create()
{
    return adoptPtr(new RegisterProtocolHandlerClient);
}

RegisterProtocolHandlerClient::RegisterProtocolHandlerClient()
{
}

void RegisterProtocolHandlerClient::registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title) 
{ 
    notImplemented(); 
} 

}
#endif // ENABLE(REGISTER_PROTOCOL_HANDLER)
