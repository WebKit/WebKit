/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#ifndef KCONFIG_H_
#define KCONFIG_H_

#include "QString.h"
#include "KWQKHTMLSettings.h"

class QStringList;

class KWQKConfigImpl;

namespace WebCore {
    class Color;
}

class KConfig {
public:

    KConfig(const QString &n, bool bReadOnly=false, bool bUseKDEGlobals = true);
    ~KConfig();

    void setGroup(const QString &pGroup);
    
    QString readEntry(const char *pKey, const QString& aDefault=QString::null) const;
    
    int readNumEntry(const char *pKey, int nDefault=0) const;
    unsigned int readUnsignedNumEntry(const KHTMLSettings *settings, const char *pKey, unsigned int nDefault=0) const;
    
    WebCore::Color readColorEntry(const char *pKey, const WebCore::Color *pDefault=0L) const;

private:

    KConfig(const KConfig &);
    KConfig &operator=(const KConfig &);
    
    KWQKConfigImpl *impl;

};

void RefreshPlugins(bool reload);

#endif
