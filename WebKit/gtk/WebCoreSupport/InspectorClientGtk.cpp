/*
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
#include "InspectorClientGtk.h"

#include "NotImplemented.h"
#include "PlatformString.h"

using namespace WebCore;

namespace WebKit {

void InspectorClient::inspectorDestroyed()
{
    delete this;
}

Page* InspectorClient::createPage()
{
    notImplemented();
    return 0;
}

String InspectorClient::localizedStringsURL()
{
    notImplemented();
    return String();
}

void InspectorClient::showWindow()
{
    notImplemented();
}

void InspectorClient::closeWindow()
{
    notImplemented();
}

void InspectorClient::attachWindow()
{
    notImplemented();
}

void InspectorClient::detachWindow()
{
    notImplemented();
}

void InspectorClient::setAttachedWindowHeight(unsigned height)
{
    notImplemented();
}

void InspectorClient::highlight(Node* node)
{
    notImplemented();
}

void InspectorClient::hideHighlight()
{
    notImplemented();
}

void InspectorClient::inspectedURLChanged(const String&)
{
    notImplemented();
}

void InspectorClient::populateSetting(const String& key, InspectorController::Setting& setting)
{
    notImplemented();
}

void InspectorClient::storeSetting(const String& key, const InspectorController::Setting& setting)
{
    notImplemented();
}

void InspectorClient::removeSetting(const String& key)
{
    notImplemented();
}

}

