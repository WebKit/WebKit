/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#include <kwqdebug.h>

#include <kglobal.h>
#include <kconfig.h>
#include <kcharsets.h>

#include <qdict.h>

#define Fixed MacFixed
#define Rect MacRect
#define Boolean MacBoolean
#include <Cocoa/Cocoa.h>
#undef Fixed
#undef Rect
#undef Boolean

KWQStaticStringDict *KGlobal::_stringDict = 0L;
KInstance *KGlobal::_instance = 0L;
KLocale *KGlobal::_locale = 0L;
KCharsets *KGlobal::_charsets = 0L;

class KWQStaticStringDict : public QDict<QString>
{
public:
    KWQStaticStringDict() : QDict<QString>() { };
};

KInstance *KGlobal::instance()
{
    _logNotYetImplemented();
    return 0L;
}


KCharsets *KGlobal::charsets()
{
    if (_charsets == 0L) {
        _charsets = new KCharsets();    
    }
    return _charsets;
}


KLocale *KGlobal::locale()
{
    _logNotYetImplemented();
    return 0L;
}


KStandardDirs *KGlobal::dirs()
{
    _logNotYetImplemented();
    return 0L;
}


KConfig *KGlobal::config()
{
    _logNotYetImplemented();
    static KConfig c("foo");
    return &c;
}

const QString &KGlobal::staticQString(const QString &str)
{
    if (!_stringDict) {
        _stringDict = new KWQStaticStringDict;
    }
    QString *result = _stringDict->find(str);
    if (!result)
    {
        result = new QString(str);
        _stringDict->insert(str, result);
    }
    return *result;
}
