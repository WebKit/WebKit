/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "Module.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>

typedef struct _WebKitWebExtension WebKitWebExtension;

namespace API {
class Object;
}

namespace WebKit {

class InjectedBundle;

class WebKitExtensionManager {
    WTF_MAKE_NONCOPYABLE(WebKitExtensionManager);
public:
    __attribute__((visibility("default"))) static WebKitExtensionManager& singleton();

    __attribute__((visibility("default"))) void initialize(InjectedBundle*, API::Object*);

    WebKitWebExtension* extension() const { return m_extension.get(); }

private:
    WebKitExtensionManager();

    void scanModules(const String&, Vector<String>&);
    bool initializeWebExtension(Module* extensionModule, GVariant* userData);

    Vector<Module*> m_extensionModules;
    GRefPtr<WebKitWebExtension> m_extension;

    friend NeverDestroyed<WebKitExtensionManager>;
};

} // namespace WebKit
