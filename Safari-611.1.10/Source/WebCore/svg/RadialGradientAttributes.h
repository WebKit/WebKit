/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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

#pragma once

#include "GradientAttributes.h"

namespace WebCore {

struct RadialGradientAttributes : GradientAttributes {
    RadialGradientAttributes()
        : m_cx(SVGLengthMode::Width, "50%")
        , m_cy(SVGLengthMode::Width, "50%")
        , m_r(SVGLengthMode::Width, "50%")
        , m_cxSet(false)
        , m_cySet(false)
        , m_rSet(false)
        , m_fxSet(false)
        , m_fySet(false) 
        , m_frSet(false)
    {
    }

    SVGLengthValue cx() const { return m_cx; }
    SVGLengthValue cy() const { return m_cy; }
    SVGLengthValue r() const { return m_r; }
    SVGLengthValue fx() const { return m_fx; }
    SVGLengthValue fy() const { return m_fy; }
    SVGLengthValue fr() const { return m_fr; }

    void setCx(SVGLengthValue value) { m_cx = value; m_cxSet = true; }
    void setCy(SVGLengthValue value) { m_cy = value; m_cySet = true; }
    void setR(SVGLengthValue value) { m_r = value; m_rSet = true; }
    void setFx(SVGLengthValue value) { m_fx = value; m_fxSet = true; }
    void setFy(SVGLengthValue value) { m_fy = value; m_fySet = true; }
    void setFr(SVGLengthValue value) { m_fr = value; m_frSet = true; }

    bool hasCx() const { return m_cxSet; }
    bool hasCy() const { return m_cySet; }
    bool hasR() const { return m_rSet; }
    bool hasFx() const { return m_fxSet; }
    bool hasFy() const { return m_fySet; }
    bool hasFr() const { return m_frSet; }

private:
    // Properties
    SVGLengthValue m_cx;
    SVGLengthValue m_cy;
    SVGLengthValue m_r;
    SVGLengthValue m_fx;
    SVGLengthValue m_fy;
    SVGLengthValue m_fr;

    // Property states
    bool m_cxSet : 1;
    bool m_cySet : 1;
    bool m_rSet : 1;
    bool m_fxSet : 1;
    bool m_fySet : 1;
    bool m_frSet : 1;
};

} // namespace WebCore
