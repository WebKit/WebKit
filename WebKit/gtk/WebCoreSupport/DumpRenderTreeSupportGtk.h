/*
 *  Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#ifndef DumpRenderTreeSupportGtk_h
#define DumpRenderTreeSupportGtk_h

#include "JSStringRef.h"

class DumpRenderTreeSupportGtk {

public:
    DumpRenderTreeSupportGtk();
    ~DumpRenderTreeSupportGtk();

    static void setDumpRenderTreeModeEnabled(bool);
    static bool dumpRenderTreeModeEnabled();

    static void setLinksIncludedInFocusChain(bool);
    static bool linksIncludedInFocusChain();
    static JSValueRef nodesFromRect(JSContextRef context, JSValueRef value, int x, int y, unsigned top, unsigned right, unsigned bottom, unsigned left, bool ignoreClipping);

private:
    static bool s_drtRun;
    static bool s_linksIncludedInTabChain;
};

#endif
