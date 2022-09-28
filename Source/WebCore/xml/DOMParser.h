/*
 *  Copyright (C) 2003, 2006 Apple Inc. All rights reserved.
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

#pragma once

#include "ExceptionOr.h"
#include "ParseFromStringOptions.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class WeakPtrImplWithEventTargetData;
class Settings;

class DOMParser : public RefCounted<DOMParser> {
public:
    static Ref<DOMParser> create(Document& contextDocument);
    ~DOMParser();

    ExceptionOr<Ref<Document>> parseFromString(const String&, const String& contentType, ParseFromStringOptions);

private:
    explicit DOMParser(Document& contextDocument);

    WeakPtr<Document, WeakPtrImplWithEventTargetData> m_contextDocument;
    const Ref<const Settings> m_settings;
};

}
