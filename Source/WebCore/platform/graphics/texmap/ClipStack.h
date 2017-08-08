/*
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2015, 2016 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef ClipStack_h
#define ClipStack_h

#include "IntRect.h"
#include "IntSize.h"
#include <wtf/Vector.h>

namespace WebCore {

class ClipStack {
public:
    struct State {
        State(const IntRect& scissors = IntRect(), int stencil = 1)
            : scissorBox(scissors)
            , stencilIndex(stencil)
        { }

        IntRect scissorBox;
        int stencilIndex;
    };

    // Y-axis should be inverted only when painting into the window.
    enum class YAxisMode {
        Default,
        Inverted,
    };

    void push();
    void pop();
    State& current() { return clipState; }

    void reset(const IntRect&, YAxisMode);
    void intersect(const IntRect&);
    void setStencilIndex(int);
    int getStencilIndex() const { return clipState.stencilIndex; }

    void apply();
    void applyIfNeeded();

    bool isCurrentScissorBoxEmpty() const { return clipState.scissorBox.isEmpty(); }

private:
    Vector<State> clipStack;
    State clipState;
    IntSize size;
    bool clipStateDirty { false };
    YAxisMode yAxisMode { YAxisMode::Default };
};

} // namespace WebCore

#endif // ClipStack_h
