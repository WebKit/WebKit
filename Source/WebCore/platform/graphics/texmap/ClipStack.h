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

#include "FloatRoundedRect.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TransformationMatrix.h"
#include <wtf/Vector.h>

namespace WebCore {

// Because GLSL uniform arrays need to have a defined size, we need to put a limit to the number of simultaneous
// rounded rectangle clips that we're going to allow. Currently this is defined to 10.
// This value must be kept in sync with the sizes defined in TextureMapperShaderProgram.cpp.
static const unsigned s_roundedRectMaxClips = 10;

// When converting a rounded rectangle to an array of floats, we need 12 elements. So the size of the array
// required to contain the 10 rectangles is 12 * 10 = 120.
// This value must be kept in sync with the sizes defined in TextureMapperShaderProgram.cpp.
static const unsigned s_roundedRectComponentsPerRect = 12;
static const unsigned s_roundedRectComponentsArraySize = s_roundedRectMaxClips * s_roundedRectComponentsPerRect;

// When converting a transformation matrix to an array of floats, we need 16 elements. So the size of the array
// required to contain the 10 matrices is 16 * 10 = 160.
// This value must be kept in sync with the sizes defined in TextureMapperShaderProgram.cpp.
static const unsigned s_roundedRectInverseTransformComponentsPerRect = 16;
static const unsigned s_roundedRectInverseTransformComponentsArraySize = s_roundedRectMaxClips * s_roundedRectInverseTransformComponentsPerRect;

class ClipStack {
public:
    struct State {
        explicit State(const IntRect& scissors = IntRect())
            : scissorBox(scissors)
        { }

        IntRect scissorBox;
        int stencilIndex { 1 };
        unsigned roundedRectCount { 0 };
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

    void addRoundedRect(const FloatRoundedRect&, const TransformationMatrix&);
    const float* roundedRectComponents() const { return m_roundedRectComponents.data(); }
    const float* roundedRectInverseTransformComponents() const { return m_roundedRectInverseTransformComponents.data(); }
    unsigned roundedRectCount() const { return clipState.roundedRectCount; }
    bool isRoundedRectClipEnabled() const { return !!clipState.roundedRectCount; }
    bool isRoundedRectClipAllowed() const { return clipState.roundedRectCount < s_roundedRectMaxClips; }

    void apply();
    void applyIfNeeded();

    bool isCurrentScissorBoxEmpty() const { return clipState.scissorBox.isEmpty(); }

private:
    Vector<State> clipStack;
    State clipState;
    IntSize size;
    bool clipStateDirty { false };
    YAxisMode yAxisMode { YAxisMode::Default };
    Vector<float, s_roundedRectComponentsArraySize> m_roundedRectComponents;
    Vector<float, s_roundedRectInverseTransformComponentsArraySize> m_roundedRectInverseTransformComponents;
};

} // namespace WebCore

#endif // ClipStack_h
