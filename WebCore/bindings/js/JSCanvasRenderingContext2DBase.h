/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "kjs_binding.h"

namespace WebCore {

    class CanvasRenderingContext2D;

    KJS_DEFINE_PROTOTYPE(JSCanvasRenderingContext2DBaseProto)

    class JSCanvasRenderingContext2DBase : public KJS::DOMObject {
    public:
        JSCanvasRenderingContext2DBase(KJS::ExecState*, PassRefPtr<CanvasRenderingContext2D>);
        virtual ~JSCanvasRenderingContext2DBase();
        virtual bool getOwnPropertySlot(KJS::ExecState*, const KJS::Identifier&, KJS::PropertySlot&);
        KJS::JSValue* getValueProperty(KJS::ExecState*, int token) const;
        virtual void put(KJS::ExecState*, const KJS::Identifier&, KJS::JSValue*, int attr = KJS::None);
        void putValueProperty(KJS::ExecState*, int token, KJS::JSValue*, int attr);
        virtual const KJS::ClassInfo* classInfo() const { return &info; }
        static const KJS::ClassInfo info;
        enum { StrokeStyle, FillStyle };
        enum { SetStrokeColor, SetFillColor, StrokeRect, DrawImage, DrawImageFromRect, SetShadow, CreatePattern };
        CanvasRenderingContext2D* impl() const { return m_impl.get(); }
    private:
        RefPtr<CanvasRenderingContext2D> m_impl;
    };

}
