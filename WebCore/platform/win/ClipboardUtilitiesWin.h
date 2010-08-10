/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#ifndef ClipboardUtilitiesWin_h
#define ClipboardUtilitiesWin_h

#include "DragData.h"
#include <windows.h>
#include <wtf/Forward.h>

namespace WebCore {

class Document;
class KURL;

HGLOBAL createGlobalData(const String&);
HGLOBAL createGlobalData(const Vector<char>&);
HGLOBAL createGlobalData(const KURL& url, const String& title);

FORMATETC* urlWFormat();
FORMATETC* urlFormat();
FORMATETC* plainTextWFormat();
FORMATETC* plainTextFormat();
FORMATETC* filenameWFormat();
FORMATETC* filenameFormat();
FORMATETC* htmlFormat();
FORMATETC* cfHDropFormat();
FORMATETC* smartPasteFormat();

void markupToCFHTML(const String& markup, const String& srcURL, Vector<char>& result);

void replaceNewlinesWithWindowsStyleNewlines(String&);
void replaceNBSPWithSpace(String&);

bool containsFilenames(const IDataObject*);
bool containsHTML(IDataObject*);

PassRefPtr<DocumentFragment> fragmentFromFilenames(Document*, const IDataObject*);
PassRefPtr<DocumentFragment> fragmentFromHTML(Document*, IDataObject*);
PassRefPtr<DocumentFragment> fragmentFromCFHTML(Document*, const String& cfhtml);

String getURL(IDataObject*, DragData::FilenameConversionPolicy, bool& success, String* title = 0);
String getPlainText(IDataObject*, bool& success);
String getTextHTML(IDataObject*, bool& success);
String getCFHTML(IDataObject*, bool& success);

} // namespace WebCore

#endif // ClipboardUtilitiesWin_h
