// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2005 Anders Carlsson (andersca@mac.com)
 *  Copyright (C) 2006 Apple Computer, Inc.
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

#ifndef DOMPARSER_H
#define DOMPARSER_H_

#include "kjs_dom.h"

namespace KJS {

  class DOMParserConstructorImp : public DOMObject {
  public:
    DOMParserConstructorImp(ExecState*, WebCore::Document*);
    virtual bool implementsConstruct() const;
    virtual JSObject* construct(ExecState*, const List &args);
  private:
    RefPtr<WebCore::Document> doc;
  };

  class JSDOMParser : public DOMObject {
  public:
    JSDOMParser(ExecState*, WebCore::Document*);
    virtual bool toBoolean(ExecState*) const { return true; }
    virtual const ClassInfo* classInfo() const { return &info; }
    static const ClassInfo info;
    enum { ParseFromString };

  private:
    RefPtr<WebCore::Document> doc;
    friend class DOMParserProtoFunc;
  };

} // namespace

#endif
