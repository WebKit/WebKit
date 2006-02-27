// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
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
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef KJS_Navigator_H
#define KJS_Navigator_H

#include <kjs/object.h>

namespace WebCore {
    class Frame;
}

namespace KJS {

  class Navigator : public JSObject {
  public:
    Navigator(ExecState *exec, WebCore::Frame *p);
    virtual bool getOwnPropertySlot(ExecState *, const Identifier&, PropertySlot&);
    JSValue *getValueProperty(ExecState *exec, int token) const;
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { AppCodeName, AppName, AppVersion, Language, UserAgent, Platform,
           _Plugins, _MimeTypes, Product, ProductSub, Vendor, CookieEnabled, JavaEnabled };
    WebCore::Frame *frame() const { return m_frame; }
  private:
    WebCore::Frame *m_frame;
  };
} // namespace

#endif
