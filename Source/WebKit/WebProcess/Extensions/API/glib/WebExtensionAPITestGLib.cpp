/*
 * Copyright (C) 2026 Igalia S.L.
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

#include "config.h"
#include "WebExtensionAPITest.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "WebExtensionUtilities.h"
#include <WebCore/NotImplemented.h>

namespace WebKit {

JSValueRef WebExtensionAPITest::assertRejects(JSContextRef context, JSValueRef, JSValueRef, StringImpl*)
{
    notImplemented();
    return toJSRejectedPromise(context, "test.assertRejects()"_si, nullString(), "it is not implemented"_si);
}

JSValueRef WebExtensionAPITest::assertResolves(JSContextRef context, JSValueRef, StringImpl*)
{
    notImplemented();
    return toJSRejectedPromise(context, "test.assertResolves()"_si, nullString(), "it is not implemented"_si);
}

JSValueRef WebExtensionAPITest::addTest(JSContextRef context, JSValueRef, RefPtr<StringImpl> callingAPIName)
{
    notImplemented();
    return toJSRejectedPromise(context, callingAPIName, nullString(), "it is not implemented"_si);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
