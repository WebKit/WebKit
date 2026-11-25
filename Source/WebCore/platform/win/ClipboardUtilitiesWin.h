/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
class DocumentFragment;

UINT customDataClipboardFormat();

HGLOBAL createGlobalData(StringImpl*);
HGLOBAL createGlobalData(const Vector<char>&);
HGLOBAL createGlobalData(const URL& url, StringImpl* title);
HGLOBAL createGlobalData(std::span<const uint8_t>);

FORMATETC* urlWFormat();
FORMATETC* urlFormat();
FORMATETC* plainTextWFormat();
FORMATETC* plainTextFormat();
FORMATETC* filenameWFormat();
FORMATETC* filenameFormat();
FORMATETC* htmlFormat();
FORMATETC* cfHDropFormat();
FORMATETC* smartPasteFormat();
FORMATETC* fileDescriptorFormat();
FORMATETC* fileContentFormatZero();

void markupToCFHTML(StringImpl* markup, StringImpl* srcURL, Vector<char>& result);

void replaceNewlinesWithWindowsStyleNewlines(RefPtr<StringImpl>&);
void replaceNBSPWithSpace(RefPtr<StringImpl>&);

bool containsFilenames(const IDataObject*);
bool containsFilenames(const DragDataMap*);
bool containsHTML(IDataObject*);
bool containsHTML(const DragDataMap*);

RefPtr<DocumentFragment> fragmentFromFilenames(Document*, const IDataObject*);
RefPtr<DocumentFragment> fragmentFromFilenames(Document*, const DragDataMap*);
RefPtr<DocumentFragment> fragmentFromHTML(Document*, IDataObject*);
RefPtr<DocumentFragment> fragmentFromHTML(Document*, const DragDataMap*);
Ref<DocumentFragment> fragmentFromCFHTML(Document*, StringImpl* cfhtml);

RefPtr<StringImpl> getURL(IDataObject*, DragData::FilenameConversionPolicy, RefPtr<StringImpl>* title = nullptr);
RefPtr<StringImpl> getURL(const DragDataMap*, DragData::FilenameConversionPolicy, RefPtr<StringImpl>* title = nullptr);
RefPtr<StringImpl> getPlainText(IDataObject*);
RefPtr<StringImpl> getPlainText(const DragDataMap*);
RefPtr<StringImpl> getTextHTML(IDataObject*);
RefPtr<StringImpl> getTextHTML(const DragDataMap*);
RefPtr<StringImpl> getCFHTML(IDataObject*);
RefPtr<StringImpl> getCFHTML(const DragDataMap*);

void getClipboardData(IDataObject*, FORMATETC* fetc, Vector<RefPtr<StringImpl>>& dataStrings);
void setClipboardData(IDataObject*, UINT format, const Vector<RefPtr<StringImpl>>& dataStrings);
void getFileDescriptorData(IDataObject*, int& size, RefPtr<StringImpl>& pathname);
void getFileContentData(IDataObject*, int size, void* dataBlob);
void setFileDescriptorData(IDataObject*, int size, StringImpl* pathname);
void setFileContentData(IDataObject*, int size, void* dataBlob);

} // namespace WebCore

#endif // ClipboardUtilitiesWin_h
