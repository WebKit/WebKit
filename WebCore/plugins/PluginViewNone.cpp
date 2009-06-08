/*
 * Copyright (C) 2008 Kevin Ollivier <kevino@theolliviers.com> All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PluginView.h"

#include "NotImplemented.h"
#include "PluginPackage.h"

using namespace WTF;

namespace WebCore {

void PluginView::setFocus()
{
    notImplemented();
}

void PluginView::show()
{
    notImplemented();
}

void PluginView::hide()
{
    notImplemented();
}

void PluginView::paint(GraphicsContext*, const IntRect&)
{
    notImplemented();
}

void PluginView::handleKeyboardEvent(KeyboardEvent*)
{
    notImplemented();
}

void PluginView::handleMouseEvent(MouseEvent*)
{
    notImplemented();
}

void PluginView::setParent(ScrollView*)
{
    notImplemented();
}

void PluginView::setNPWindowRect(const IntRect&)
{
    notImplemented();
}

void PluginView::stop()
{
    notImplemented();
}

const char* PluginView::userAgent()
{
    notImplemented();
    return 0;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
const char* PluginView::userAgentStatic()
{
    notImplemented();
    return 0;
}
#endif

NPError PluginView::handlePostReadFile(Vector<char>&, uint32, const char*)
{
    notImplemented();

    return 0;
}

NPError PluginView::getValue(NPNVariable, void*)
{
    notImplemented();
    return 0;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    notImplemented();
    return 0;
}
#endif

void PluginView::invalidateRect(NPRect*)
{
    notImplemented();
}

void PluginView::invalidateRect(const IntRect&)
{
    notImplemented();
}

void PluginView::invalidateRegion(NPRegion)
{
    notImplemented();
}

void PluginView::forceRedraw()
{
    notImplemented();
}

PluginView::~PluginView()
{
    notImplemented();
}

void PluginView::init()
{
    notImplemented();
}

void PluginView::setParentVisible(bool)
{
    notImplemented();
}

void PluginView::updatePluginWidget()
{
    notImplemented();
}

} // namespace WebCore
