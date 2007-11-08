/*
 * Copyright (C) 2007 Staikos Computing Services Inc.
 * Copyright (C) 2007 Trolltech ASA
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <QtCore/QObject>
#include <QtCore/QCoreApplication>


namespace WebCore {

class PerformFunctionEvent : public QEvent {
public:
    static const int EventType = 723;

    PerformFunctionEvent(void (*_function)());
    void (*function)();
};

class MainThreadInvoker : public QObject {
    Q_OBJECT
public:
    MainThreadInvoker();

protected:
    bool event(QEvent*);
};

PerformFunctionEvent::PerformFunctionEvent(void (*_function)())
    : QEvent(static_cast<QEvent::Type>(EventType))
    , function(_function)
{}

MainThreadInvoker::MainThreadInvoker()
{
    moveToThread(QCoreApplication::instance()->thread());
}

bool MainThreadInvoker::event(QEvent* event)
{
    if (event->type() == PerformFunctionEvent::EventType)
        static_cast<PerformFunctionEvent*>(event)->function();

    return QObject::event(event);
}

Q_GLOBAL_STATIC(MainThreadInvoker, webkit_main_thread_invoker)


void callOnMainThread(void (*functionToPerform)()) {
    if (!functionToPerform)
        return;

    QCoreApplication::postEvent(webkit_main_thread_invoker(), new PerformFunctionEvent(functionToPerform));
}

}

#include "ThreadingQt.moc"
