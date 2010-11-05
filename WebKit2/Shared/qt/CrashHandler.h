/*
 * Copyright (C) 2010 University of Szeged. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CrashHandler_h
#define CrashHandler_h

#include <QList>
#include <QObject>
#include <wtf/HashSet.h>
#include <wtf/StdLibExtras.h>

namespace WebKit {

class CrashHandler : private QObject {
    Q_OBJECT
public:
    static CrashHandler* instance()
    {
        if (!theInstance)
            theInstance = new CrashHandler();
        return theInstance;
    }

    void markForDeletionOnCrash(QObject* object)
    {
        theInstance->m_objects.append(object);
    }

    void didDelete(QObject* object)
    {
        if (m_inDeleteObjects)
            return;
        theInstance->m_objects.removeOne(object);
    }

private:
    static void signalHandler(int);
    static CrashHandler* theInstance;

    CrashHandler();

    void deleteObjects();

    QList<QObject*> m_objects;
    bool m_inDeleteObjects;
};

} // namespace WebKit

#endif // CrashHandler_h
