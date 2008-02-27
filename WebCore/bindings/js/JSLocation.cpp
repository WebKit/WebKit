/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reseved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301
 *  USA
 */

#include "config.h"
#include "JSLocation.h"

#include "DOMWindow.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

#include "JSLocation.lut.h"

using namespace KJS;

namespace WebCore {

const ClassInfo JSLocation::info = { "Location", 0, &JSLocationTable };

/*
@begin JSLocationTable 12
  assign        WebCore::jsLocationProtoFuncAssign        DontDelete|Function 1
  hash          WebCore::JSLocation::Hash                 DontDelete
  host          WebCore::JSLocation::Host                 DontDelete
  hostname      WebCore::JSLocation::Hostname             DontDelete
  href          WebCore::JSLocation::Href                 DontDelete
  pathname      WebCore::JSLocation::Pathname             DontDelete
  port          WebCore::JSLocation::Port                 DontDelete
  protocol      WebCore::JSLocation::Protocol             DontDelete
  search        WebCore::JSLocation::Search               DontDelete
  toString      WebCore::jsLocationProtoFuncToString      DontEnum|DontDelete|Function 0
  replace       WebCore::jsLocationProtoFuncReplace       DontDelete|Function 1
  reload        WebCore::jsLocationProtoFuncReload        DontDelete|Function 0
@end
*/

JSLocation::JSLocation(JSObject* /*prototype*/, Frame* frame)
    : DOMObject(jsNull()) // FIXME: this needs to take a real prototype
    , m_frame(frame)
{
}

JSValue* JSLocation::getValueProperty(ExecState* exec, int token) const
{
  const KURL& url = m_frame->loader()->url();
  switch (token) {
  case Hash:
    return jsString(url.ref().isNull() ? "" : "#" + url.ref());
  case Host: {
    // Note: this is the IE spec. The NS spec swaps the two, it says
    // "The hostname property is the concatenation of the host and port properties, separated by a colon."
    // Bleh.
    UString str = url.host();
    if (url.port())
        str += ":" + String::number((int)url.port());
    return jsString(str);
  }
  case Hostname:
    return jsString(url.host());
  case Href:
    if (!url.hasPath())
      return jsString(url.prettyURL() + "/");
    return jsString(url.prettyURL());
  case Pathname:
    return jsString(url.path().isEmpty() ? "/" : url.path());
  case Port:
    return jsString(url.port() ? String::number((int)url.port()) : "");
  case Protocol:
    return jsString(url.protocol() + ":");
  case Search:
    return jsString(url.query());
  default:
    ASSERT_NOT_REACHED();
    return jsUndefined();
  }
}

bool JSLocation::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (customGetOwnPropertySlot(exec, propertyName, slot))
        return true;
    return getStaticPropertySlot<JSLocation, JSObject>(exec, &JSLocationTable, this, propertyName, slot);
}

bool JSLocation::customGetOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // When accessing Location cross-domain, functions are always the native built-in ones.
    // See JSDOMWindow::customGetOwnPropertySlot for additional details.

    // Our custom code is only needed to implement the Window cross-domain scheme, so if access is
    // allowed, return false so the normal lookup will take place.
    String message;
    if (allowsAccessFromFrame(exec, m_frame, message))
        return false;

    // Check for the few functions that we allow, even when called cross-domain.
    const HashEntry* entry = Lookup::findEntry(&JSLocationTable, propertyName);
    if (entry && (entry->attr & Function)
            && (entry->value.functionValue == jsLocationProtoFuncReplace
                || entry->value.functionValue == jsLocationProtoFuncReload
                || entry->value.functionValue == jsLocationProtoFuncAssign)) {
        slot.setStaticEntry(this, entry, nonCachingStaticFunctionGetter);
        return true;
    }
    // FIXME: Other implementers of the Window cross-domain scheme (Window, History) allow toString,
    // but for now we have decided not to, partly because it seems silly to return "[Object Location]" in
    // such cases when normally the string form of Location would be the URL.

    printErrorMessageForFrame(m_frame, message);
    slot.setUndefined(this);
    return true;
}

void JSLocation::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
  if (!m_frame)
    return;

  String str = value->toString(exec);
  KURL url = m_frame->loader()->url();
  bool sameDomainAccess = allowsAccessFromFrame(exec, m_frame);

  const HashEntry* entry = Lookup::findEntry(&JSLocationTable, propertyName);

  if (entry) {
      // cross-domain access to the location is allowed when assigning the whole location,
      // but not when assigning the individual pieces, since that might inadvertently
      // disclose other parts of the original location.
      if (entry->value.intValue != Href && !sameDomainAccess)
          return;

      switch (entry->value.intValue) {
      case Href: {
          // FIXME: Why isn't this security check needed for the other properties, like Host, below?
          Frame* frame = Window::retrieveActive(exec)->impl()->frame();
          if (!frame)
              return;
          if (!frame->loader()->shouldAllowNavigation(m_frame))
              return;
          url = frame->loader()->completeURL(str);
          break;
      }
      case Hash:
          if (str.startsWith("#"))
              str = str.substring(1);
          if (url.ref() == str)
              return;
          url.setRef(str);
          break;
      case Host:
          url.setHostAndPort(str);
          break;
      case Hostname:
          url.setHost(str);
          break;
      case Pathname:
          url.setPath(str);
          break;
      case Port: {
          // FIXME: Could make this a little less ugly if String provided a toUnsignedShort function.
          int port = str.toInt();
          if (port < 0 || port > 0xFFFF)
              port = 0;
          url.setPort(port);
          break;
      }
      case Protocol:
          url.setProtocol(str);
          break;
      case Search:
          url.setQuery(str);
          break;
      default:
          // Disallow changing other properties in JSLocationTable. e.g., "window.location.toString = ...".
          // <http://bugs.webkit.org/show_bug.cgi?id=12720>
          return;
      }
  } else {
      if (sameDomainAccess)
          JSObject::put(exec, propertyName, value);
      return;
  }

  Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
  if (!url.protocolIs("javascript") || sameDomainAccess) {
    bool userGesture = activeFrame->scriptProxy()->processingUserGesture();
    m_frame->loader()->scheduleLocationChange(url.string(), activeFrame->loader()->outgoingReferrer(), false, userGesture);
  }
}

bool JSLocation::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    // Only allow deleting by frames in the same origin.
    if (!allowsAccessFromFrame(exec, m_frame))
        return false;
    return Base::deleteProperty(exec, propertyName);
}

void JSLocation::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    // Only allow the location object to enumerated by frames in the same origin.
    if (!allowsAccessFromFrame(exec, m_frame))
        return;
    Base::getPropertyNames(exec, propertyNames);
}

JSValue* jsLocationProtoFuncReplace(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSLocation::info))
        return throwError(exec, TypeError);
    JSLocation* location = static_cast<JSLocation*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
    if (activeFrame) {
        if (!activeFrame->loader()->shouldAllowNavigation(frame))
            return jsUndefined();
        String str = args[0]->toString(exec);
        const Window* window = Window::retrieveWindow(frame);
        if (!protocolIs(str, "javascript") || (window && window->allowsAccessFrom(exec))) {
            bool userGesture = activeFrame->scriptProxy()->processingUserGesture();
            frame->loader()->scheduleLocationChange(activeFrame->loader()->completeURL(str).string(), activeFrame->loader()->outgoingReferrer(), true, userGesture);
        }
    }

    return jsUndefined();
}

JSValue* jsLocationProtoFuncReload(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSLocation::info))
        return throwError(exec, TypeError);
    JSLocation* location = static_cast<JSLocation*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    Window* window = Window::retrieveWindow(frame);
    if (!window->allowsAccessFrom(exec))
        return jsUndefined();

    if (!frame->loader()->url().protocolIs("javascript") || (window && window->allowsAccessFrom(exec))) {
        bool userGesture = Window::retrieveActive(exec)->impl()->frame()->scriptProxy()->processingUserGesture();
        frame->loader()->scheduleRefresh(userGesture);
    }
    return jsUndefined();
}

JSValue* jsLocationProtoFuncAssign(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSLocation::info))
        return throwError(exec, TypeError);
    JSLocation* location = static_cast<JSLocation*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();

    Frame* activeFrame = Window::retrieveActive(exec)->impl()->frame();
    if (activeFrame) {
        if (!activeFrame->loader()->shouldAllowNavigation(frame))
            return jsUndefined();
        const Window* window = Window::retrieveWindow(frame);
        String dstUrl = activeFrame->loader()->completeURL(args[0]->toString(exec)).string();
        if (!protocolIs(dstUrl, "javascript") || (window && window->allowsAccessFrom(exec))) {
            bool userGesture = activeFrame->scriptProxy()->processingUserGesture();
            // We want a new history item if this JS was called via a user gesture
            frame->loader()->scheduleLocationChange(dstUrl, activeFrame->loader()->outgoingReferrer(), false, userGesture);
        }
    }

    return jsUndefined();
}

JSValue* jsLocationProtoFuncToString(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSLocation::info))
        return throwError(exec, TypeError);
    JSLocation* location = static_cast<JSLocation*>(thisObj);
    Frame* frame = location->frame();
    if (!frame)
        return jsUndefined();
    if (!allowsAccessFromFrame(exec, frame))
        return jsUndefined();

    const KURL& url = frame->loader()->url();
    if (!url.hasPath())
        return jsString(url.prettyURL() + "/");
    return jsString(url.prettyURL());
}

} // namespace WebCore
