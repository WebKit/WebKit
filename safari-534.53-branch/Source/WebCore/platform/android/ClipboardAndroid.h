/*
 * Copyright 2009, The Android Open Source Project
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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

#ifndef ClipboardAndroid_h
#define ClipboardAndroid_h

#include "Clipboard.h"

#include "CachedResourceClient.h"

namespace WebCore {

class CachedImage;

class ClipboardAndroid : public Clipboard, public CachedResourceClient {
public:
    ClipboardAndroid(ClipboardAccessPolicy policy, ClipboardType);
    ~ClipboardAndroid();

    void clearData(const String&);
    void clearAllData();
    String getData(const String&, bool& success) const;
    bool setData(const String&, const String&);

    // extensions beyond IE's API
    HashSet<String> types() const;
    PassRefPtr<FileList> files() const;

    void setDragImage(CachedImage*, const IntPoint&);
    void setDragImageElement(Node*, const IntPoint&);
    
    virtual DragImageRef createDragImage(IntPoint&) const;
    virtual void declareAndWriteDragImage(Element*, const KURL&, const String&, Frame*);
    virtual void writeURL(const KURL&, const String&, Frame*);
    virtual void writeRange(Range*, Frame*);
    virtual void writePlainText(const String&);

    virtual bool hasData();
};

} // namespace WebCore

#endif // ClipboardAndroid_h
