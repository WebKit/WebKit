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

#if ENABLE(2022_GLIB_API)
#include "WebKitWebProcessExtension.h"
#else
#include "WebKitWebExtension.h"
typedef _WebKitWebExtension WebKitWebProcessExtension;
#endif

namespace API {
class Object;
}

namespace WebKit {

class InjectedBundle;

class WebProcessExtensionManager {
    WTF_MAKE_NONCOPYABLE(WebProcessExtensionManager);
public:
    __attribute__((visibility("default"))) static WebProcessExtensionManager& singleton();

    __attribute__((visibility("default"))) void initialize(InjectedBundle*, API::Object*);

    WebKitWebProcessExtension* extension() const { return m_extension.get(); }

private:
    WebProcessExtensionManager() = default;

    void scanModules(const String&, Vector<String>&);
    bool initializeWebProcessExtension(Module* extensionModule, GVariant* userData);

    Vector<Module*> m_extensionModules;
    GRefPtr<WebKitWebProcessExtension> m_extension;

    friend NeverDestroyed<WebProcessExtensionManager>;
};

} // namespace WebKit
