/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
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

#if !defined(__WEBKIT_WEB_EXTENSION_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkit-web-extension.h> can be included directly."
#endif

#ifndef WebKitWebHitTestResult_h
#define WebKitWebHitTestResult_h

#include <glib-object.h>
#include <wpe/WebKitDefines.h>
#include <wpe/WebKitHitTestResult.h>
#include <wpe/webkitdom.h>

G_BEGIN_DECLS

#define WEBKIT_TYPE_WEB_HIT_TEST_RESULT            (webkit_web_hit_test_result_get_type())
#define WEBKIT_WEB_HIT_TEST_RESULT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_WEB_HIT_TEST_RESULT, WebKitWebHitTestResult))
#define WEBKIT_IS_WEB_HIT_TEST_RESULT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_WEB_HIT_TEST_RESULT))
#define WEBKIT_WEB_HIT_TEST_RESULT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_TYPE_WEB_HIT_TEST_RESULT, WebKitWebHitTestResultClass))
#define WEBKIT_IS_WEB_HIT_TEST_RESULT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_TYPE_WEB_HIT_TEST_RESULT))
#define WEBKIT_WEB_HIT_TEST_RESULT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_TYPE_WEB_HIT_TEST_RESULT, WebKitWebHitTestResultClass))

typedef struct _WebKitWebHitTestResult        WebKitWebHitTestResult;
typedef struct _WebKitWebHitTestResultClass   WebKitWebHitTestResultClass;
typedef struct _WebKitWebHitTestResultPrivate WebKitWebHitTestResultPrivate;

struct _WebKitWebHitTestResult {
    WebKitHitTestResult parent;

    WebKitWebHitTestResultPrivate *priv;
};

struct _WebKitWebHitTestResultClass {
    WebKitHitTestResultClass parent_class;

    void (*_webkit_reserved0) (void);
    void (*_webkit_reserved1) (void);
    void (*_webkit_reserved2) (void);
    void (*_webkit_reserved3) (void);
};

WEBKIT_API GType
webkit_web_hit_test_result_get_type (void);

WEBKIT_API WebKitDOMNode *
webkit_web_hit_test_result_get_node (WebKitWebHitTestResult *hit_test_result);

G_END_DECLS

#endif
