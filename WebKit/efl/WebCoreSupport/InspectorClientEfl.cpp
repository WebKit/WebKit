/*
 *  Copyright (C) 2008 INdT - Instituto Nokia de Tecnologia
 *  Copyright (C) 2009-2010 ProFUSION embedded systems
 *  Copyright (C) 2009-2010 Samsung Electronics
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

#include "config.h"
#include "InspectorClientEfl.h"

#include "NotImplemented.h"
#include "PlatformString.h"

using namespace WebCore;

namespace WebCore {

void InspectorClientEfl::inspectorDestroyed()
{
    delete this;
}

void InspectorClientEfl::openInspectorFrontend(InspectorController*)
{
    notImplemented();
}

void InspectorClientEfl::highlight(Node* node)
{
    notImplemented();
}

void InspectorClientEfl::hideHighlight()
{
    notImplemented();
}

void InspectorClientEfl::populateSetting(const String&, String*)
{
    notImplemented();
}

void InspectorClientEfl::storeSetting(const String&, const String&)
{
    notImplemented();
}

}
