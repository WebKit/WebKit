/*
 *  Copyright (C) 2021 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "RemoteCommandListener.h"

#if USE(GLIB) && ENABLE(MEDIA_SESSION)

namespace WebCore {

class RemoteCommandListenerGLib : public RemoteCommandListener {
public:
    static std::unique_ptr<RemoteCommandListenerGLib> create(RemoteCommandListenerClient&);
    explicit RemoteCommandListenerGLib(RemoteCommandListenerClient&);
    virtual ~RemoteCommandListenerGLib();

private:
    void updateSupportedCommands() final;

    const RemoteCommandsSet& defaultCommands();
};

}

#endif // USE(GLIB)
