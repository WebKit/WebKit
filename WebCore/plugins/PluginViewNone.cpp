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

using namespace WTF;

namespace WebCore {

void PluginView::setFocus()
{
}

void PluginView::show()
{
}

void PluginView::hide()
{
}

void PluginView::paint(GraphicsContext*, const IntRect&)
{
}

void PluginView::handleKeyboardEvent(KeyboardEvent*)
{
}

void PluginView::handleMouseEvent(MouseEvent*)
{
}

void PluginView::setParent(ScrollView*)
{
}

void PluginView::setNPWindowRect(const IntRect&)
{
}

NPError PluginView::handlePostReadFile(Vector<char>&, uint32, const char*)
{
    return 0;
}

NPError PluginView::getValue(NPNVariable, void*)
{
    return 0;
}

#if ENABLE(NETSCAPE_PLUGIN_API)
NPError PluginView::getValueStatic(NPNVariable variable, void* value)
{
    return 0;
}
#endif

void PluginView::invalidateRect(NPRect*)
{
}

void PluginView::invalidateRect(const IntRect&)
{
}

void PluginView::invalidateRegion(NPRegion)
{
}

void PluginView::forceRedraw()
{
}

bool PluginView::platformStart()
{
    return true;
}

void PluginView::platformDestroy()
{
}

void PluginView::setParentVisible(bool)
{
}

void PluginView::updatePluginWidget()
{
}

void PluginView::halt()
{
}

void PluginView::restart()
{
}

} // namespace WebCore
