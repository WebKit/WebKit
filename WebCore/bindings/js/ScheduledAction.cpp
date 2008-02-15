/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (C) 2006 Jon Shier (jshier@iastate.edu)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reseved.
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
#include "ScheduledAction.h"

#include "CString.h"
#include "Chrome.h"
#include "DOMWindow.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "Page.h"
#include "kjs_proxy.h"
#include "kjs_window.h"

using namespace KJS;

namespace WebCore {

ScheduledAction::ScheduledAction(JSValue* func, const List& args)
    : m_func(func)
{
    List::const_iterator end = args.end();
    for (List::const_iterator it = args.begin(); it != end; ++it)
        m_args.append(*it);
}


void ScheduledAction::execute(Window* window)
{
    RefPtr<Frame> frame = window->impl()->frame();
    if (!frame)
        return;

    if (!frame->scriptProxy()->isEnabled())
        return;

    KJSProxy* scriptProxy = frame->scriptProxy();
    Window* globalObject = scriptProxy->globalObject();

    scriptProxy->setProcessingTimerCallback(true);

    if (JSValue* func = m_func.get()) {
        JSLock lock;
        if (func->isObject() && static_cast<JSObject*>(func)->implementsCall()) {
            ExecState* exec = window->globalExec();
            ASSERT(window == globalObject);

            List args;
            size_t size = m_args.size();
            for (size_t i = 0; i < size; ++i)
                args.append(m_args[i]);

            globalObject->startTimeoutCheck();
            static_cast<JSObject*>(func)->call(exec, window, args);
            globalObject->stopTimeoutCheck();
            if (exec->hadException()) {
                JSObject* exception = exec->exception()->toObject(exec);
                exec->clearException();
                String message = exception->get(exec, exec->propertyNames().message)->toString(exec);
                int lineNumber = exception->get(exec, "line")->toInt32(exec);
                if (Interpreter::shouldPrintExceptions())
                    printf("(timer):%s\n", message.utf8().data());
                if (Page* page = frame->page())
                    page->chrome()->addMessageToConsole(JSMessageSource, ErrorMessageLevel, message, lineNumber, String());
            }
        }
    } else
        frame->loader()->executeScript(m_code);

    // Update our document's rendering following the execution of the timeout callback.
    // FIXME: Why not use updateDocumentsRendering to update rendering of all documents?
    // FIXME: Is this really the right point to do the update? We need a place that works
    // for all possible entry points that might possibly execute script, but this seems
    // to be a bit too low-level.
    if (Document* doc = frame->document())
        doc->updateRendering();

    scriptProxy->setProcessingTimerCallback(false);
}

} // namespace WebCore
