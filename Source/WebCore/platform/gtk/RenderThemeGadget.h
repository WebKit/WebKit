/*
 * Copyright (C) 2016 Igalia S.L.
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

#pragma once

#if !USE(GTK4)

#include "Color.h"
#include "IntSize.h"
#include <gtk/gtk.h>
#include <wtf/OptionSet.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {
class FloatRect;

class RenderThemeGadget {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(RenderThemeGadget);
public:
    enum class Type {
        Generic,
        Scrollbar,
    };

    struct Info {
        Type type;
        const char* name;
        Vector<const char*> classList;
    };

    static std::unique_ptr<RenderThemeGadget> create(const Info&, RenderThemeGadget* parent = nullptr, const Vector<RenderThemeGadget::Info> siblings = Vector<RenderThemeGadget::Info>(), unsigned position = 0);
    RenderThemeGadget(const Info&, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position);
    virtual ~RenderThemeGadget();

    virtual IntSize preferredSize() const;
    virtual bool render(cairo_t*, const FloatRect&, FloatRect* = nullptr);
    virtual IntSize minimumSize() const;

    GtkBorder contentsBox() const;
    Color color() const;
    Color backgroundColor() const;
    double opacity() const;

    GtkStyleContext* context() const { return m_context.get(); }

    GtkStateFlags state() const;
    void setState(GtkStateFlags);

protected:
    GtkBorder marginBox() const;
    GtkBorder borderBox() const;
    GtkBorder paddingBox() const;

    GRefPtr<GtkStyleContext> m_context;
};

class RenderThemeBoxGadget final : public RenderThemeGadget {
public:
    RenderThemeBoxGadget(const RenderThemeGadget::Info&, GtkOrientation, const Vector<RenderThemeGadget::Info> children, RenderThemeGadget* parent = nullptr);

    IntSize preferredSize() const override;

    RenderThemeGadget* child(unsigned index) const { return m_children[index].get(); }

private:
    Vector<std::unique_ptr<RenderThemeGadget>> m_children;
    GtkOrientation m_orientation { GTK_ORIENTATION_HORIZONTAL };
};

class RenderThemeScrollbarGadget final : public RenderThemeGadget {
public:
    RenderThemeScrollbarGadget(const Info&, RenderThemeGadget* parent, const Vector<RenderThemeGadget::Info> siblings, unsigned position);

    enum class Steppers {
        Backward = 1 << 0,
        Forward = 1 << 1,
        SecondaryBackward = 1 << 2,
        SecondaryForward = 1 << 3
    };
    OptionSet<Steppers> steppers() const { return m_steppers; };

    void renderStepper(cairo_t*, const FloatRect&, RenderThemeGadget*, GtkOrientation, Steppers);

private:
    OptionSet<Steppers> m_steppers;
};

} // namespace WebCore

#endif // !USE(GTK4)
