Title: Overview
Slug: overview

WebKitGTK is a GObject-based library that provides a GTK widget
to interact with the WebKit engine. It can be used to write a
variety of apps, from web browers to news readers to rich text
editors.

WebkitGTK is distributed under the [BSD][bsd] and [LGPL-2.1][lgpl-2.1]
licenses.

Besides offering a C API for app developers, WebKitGTK also
provides bindings to many other programming languages using
GObject Introspection, including Python, Rust, JavaScript, and
more.

The main widget is [class@WebKit.WebView], which can be added
to any GTK container or window like a regular widget.
[class@WebKit.WebView] will then render the web page in the
area allocated to it.

Most other objects offered by WebKitGTK provide extra controls
to [class@WebKit.WebView]. For example, the [class@WebKit.Settings]
object can be used to enable or disable features of the web view.


[bsd]: https://github.com/WebKit/WebKit/blob/main/Source/WebCore/LICENSE-APPLE
[lgpl-2.1]: https://www.gnu.org/licenses/old-licenses/lgpl-2.1.en.html
