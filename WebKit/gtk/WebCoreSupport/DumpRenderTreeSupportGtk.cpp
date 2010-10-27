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

#include "config.h"
#include "DumpRenderTreeSupportGtk.h"

#include "webkitwebview.h"
#include "webkitprivate.h"

using namespace WebCore;

bool DumpRenderTreeSupportGtk::s_drtRun = false;
bool DumpRenderTreeSupportGtk::s_linksIncludedInTabChain = true;

DumpRenderTreeSupportGtk::DumpRenderTreeSupportGtk()
{
}

DumpRenderTreeSupportGtk::~DumpRenderTreeSupportGtk()
{
}

void DumpRenderTreeSupportGtk::setDumpRenderTreeModeEnabled(bool enabled)
{
    s_drtRun = enabled;
}

bool DumpRenderTreeSupportGtk::dumpRenderTreeModeEnabled()
{
    return s_drtRun;
}
void DumpRenderTreeSupportGtk::setLinksIncludedInFocusChain(bool enabled)
{
    s_linksIncludedInTabChain = enabled;
}

bool DumpRenderTreeSupportGtk::linksIncludedInFocusChain()
{
    return s_linksIncludedInTabChain;
}

