/*
    Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
    Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.

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

#include "config.h"
#include "PluginDebug.h"
#include "PlatformString.h"

#if !LOG_DISABLED

namespace WebCore {

static const char* const errorStrings[] = {
    "No errors occurred.", /* NPERR_NO_ERROR */
    "Error with no specific error code occurred.", /* NPERR_GENERIC_ERROR */
    "Invalid instance passed to the plug-in.", /* NPERR_INVALID_INSTANCE_ERROR */
    "Function table invalid.", /* NPERR_INVALID_FUNCTABLE_ERROR */
    "Loading of plug-in failed.", /* NPERR_MODULE_LOAD_FAILED_ERROR */
    "Memory allocation failed.", /* NPERR_OUT_OF_MEMORY_ERROR */
    "Plug-in missing or invalid.", /* NPERR_INVALID_PLUGIN_ERROR */
    "Plug-in directory missing or invalid.", /* NPERR_INVALID_PLUGIN_DIR_ERROR */
    "Versions of plug-in and Communicator do not match.", /* NPERR_INCOMPATIBLE_VERSION_ERROR */
    "Parameter missing or invalid.", /* NPERR_INVALID_PARAM */
    "URL missing or invalid.", /* NPERR_INVALID_URL */
    "File missing or invalid.", /* NPERR_FILE_NOT_FOUND */
    "Stream contains no data.", /* NPERR_NO_DATA */
    "Seekable stream expected.", /* NPERR_STREAM_NOT_SEEKABLE */
    "Unknown error code"
};

#ifdef XP_MACOSX
static const char* const drawingModels[] = {
    "NPDrawingModelQuickDraw",
    "NPDrawingModelCoreGraphics",
    "NPDrawingModelOpenGL",
    "NPDrawingModelCoreAnimation"
};

static const char* const eventModels[] = {
    "NPEventModelCarbon",
    "NPEventModelCocoa"
};
#endif //XP_MACOSX

const char* prettyNameForNPError(NPError error)
{
    return errorStrings[error];
}

#ifdef XP_MACOSX
const char* prettyNameForDrawingModel(NPDrawingModel drawingModel)
{
    return drawingModels[drawingModel];
}

const char* prettyNameForEventModel(NPEventModel eventModel)
{
    return eventModels[eventModel];
}
#endif //XP_MACOSX

CString prettyNameForNPNVariable(NPNVariable variable)
{
    switch (variable) {
    case NPNVxDisplay: return "NPNVxDisplay";
    case NPNVxtAppContext: return "NPNVxtAppContext";
    case NPNVnetscapeWindow: return "NPNVnetscapeWindow";
    case NPNVjavascriptEnabledBool: return "NPNVjavascriptEnabledBool";
    case NPNVasdEnabledBool: return "NPNVasdEnabledBool";
    case NPNVisOfflineBool: return "NPNVisOfflineBool";

    case NPNVserviceManager: return "NPNVserviceManager (not supported)";
    case NPNVDOMElement: return "NPNVDOMElement (not supported)";
    case NPNVDOMWindow: return "NPNVDOMWindow (not supported)";
    case NPNVToolkit: return "NPNVToolkit (not supported)";
    case NPNVSupportsXEmbedBool: return "NPNVSupportsXEmbedBool (not supported)";

    case NPNVWindowNPObject: return "NPNVWindowNPObject";
    case NPNVPluginElementNPObject: return "NPNVPluginElementNPObject";
    case NPNVSupportsWindowless: return "NPNVSupportsWindowless";
    case NPNVprivateModeBool: return "NPNVprivateModeBool";

#ifdef XP_MACOSX
    case NPNVpluginDrawingModel: return "NPNVpluginDrawingModel";
#ifndef NP_NO_QUICKDRAW
    case NPNVsupportsQuickDrawBool: return "NPNVsupportsQuickDrawBool";
#endif
    case NPNVsupportsCoreGraphicsBool: return "NPNVsupportsCoreGraphicsBool";
    case NPNVsupportsOpenGLBool: return "NPNVsupportsOpenGLBool";
    case NPNVsupportsCoreAnimationBool: return "NPNVsupportsCoreAnimationBool";
#ifndef NP_NO_CARBON
    case NPNVsupportsCarbonBool: return "NPNVsupportsCarbonBool";
#endif
    case NPNVsupportsCocoaBool: return "NPNVsupportsCocoaBool";
#endif

    default: return "Unknown variable";
    }
}

CString prettyNameForNPPVariable(NPPVariable variable, void* value)
{
    switch (variable) {
    case NPPVpluginNameString: return "NPPVpluginNameString";
    case NPPVpluginDescriptionString: return "NPPVpluginDescriptionString";
    case NPPVpluginWindowBool: return "NPPVpluginWindowBool";
    case NPPVpluginTransparentBool: return "NPPVpluginTransparentBool";

    case NPPVjavaClass: return "NPPVjavaClass (not supported)";
    case NPPVpluginWindowSize: return "NPPVpluginWindowSize (not supported)";
    case NPPVpluginTimerInterval: return "NPPVpluginTimerInterval (not supported)";
    case NPPVpluginScriptableInstance: return "NPPVpluginScriptableInstance (not supported)";
    case NPPVpluginScriptableIID: return "NPPVpluginScriptableIID (not supported)";
    case NPPVjavascriptPushCallerBool: return "NPPVjavascriptPushCallerBool (not supported)";
    case NPPVpluginKeepLibraryInMemory: return "NPPVpluginKeepLibraryInMemory (not supported)";
    case NPPVpluginNeedsXEmbed: return "NPPVpluginNeedsXEmbed (not supported)";

    case NPPVpluginScriptableNPObject: return "NPPVpluginScriptableNPObject";

    case NPPVformValue: return "NPPVformValue (not supported)";
    case NPPVpluginUrlRequestsDisplayedBool: return "NPPVpluginUrlRequestsDisplayedBool (not supported)";

    case NPPVpluginWantsAllNetworkStreams: return "NPPVpluginWantsAllNetworkStreams";
    case NPPVpluginCancelSrcStream: return "NPPVpluginCancelSrcStream";

#ifdef XP_MACOSX
    case NPPVpluginDrawingModel: {
        String result("NPPVpluginDrawingModel, ");
        result.append(prettyNameForDrawingModel(NPDrawingModel(uintptr_t(value))));
        return result.latin1();
    }
    case NPPVpluginEventModel: {
        String result("NPPVpluginEventModel, ");
        result.append(prettyNameForEventModel(NPEventModel(uintptr_t(value))));
        return result.latin1();
    }
    case NPPVpluginCoreAnimationLayer: return "NPPVpluginCoreAnimationLayer";
#endif

    default: return "Unknown variable";
    }
}

} // namespace WebCore

#endif // !LOG_DISABLED
