/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 
#ifndef IconDatabaseBase_h
#define IconDatabaseBase_h

#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassRefPtr.h>


namespace WebCore { 

class DocumentLoader;
class IconDatabaseClient;
class Image;
class IntSize;
class SharedBuffer;

enum IconLoadDecision {
    IconLoadYes,
    IconLoadNo,
    IconLoadUnknown
};

class IconDatabaseBase {
    WTF_MAKE_NONCOPYABLE(IconDatabaseBase);

protected:
    IconDatabaseBase() { }

public:
    virtual ~IconDatabaseBase() { }

    // Used internally by WebCore
    virtual bool isEnabled() const { return false; }
    
    virtual Image* iconForPageURL(const String&, const IntSize&) { return 0; }
    
    virtual void retainIconForPageURL(const String&) { }
    virtual void releaseIconForPageURL(const String&) { }

    virtual String iconURLForPageURL(const String&);
    virtual bool iconDataKnownForIconURL(const String&) { return false; }
    virtual IconLoadDecision loadDecisionForIconURL(const String&, DocumentLoader*) { return IconLoadNo; }
    
    virtual void setIconURLForPageURL(const String&, const String&) { }
    virtual void setIconDataForIconURL(PassRefPtr<SharedBuffer>, const String&) { }

    // Used within one or more WebKit ports.
    // We should try to remove these dependencies from the IconDatabaseBase class.
    virtual void setEnabled(bool) { }

    virtual Image* defaultIcon(const IntSize&) { return 0; }

    virtual size_t pageURLMappingCount() { return 0; }
    virtual size_t retainedPageURLCount() { return 0; }
    virtual size_t iconRecordCount() { return 0; }
    virtual size_t iconRecordCountWithData() { return 0; }

    virtual void importIconURLForPageURL(const String&, const String&) { }
    virtual void importIconDataForIconURL(PassRefPtr<SharedBuffer>, const String&) { }
    virtual bool shouldStopThreadActivity() const { return true; }

    virtual bool open(const String&) { return false; }
    virtual void close() { }
    virtual void removeAllIcons() { }

    virtual void setPrivateBrowsingEnabled(bool) { }
    virtual void setClient(IconDatabaseClient*) { }
    
    virtual bool isOpen() const { return false; }
    virtual String databasePath() const;

};

// Functions to get/set the global icon database.
IconDatabaseBase& iconDatabase();
void setGlobalIconDatabase(IconDatabaseBase*);

} // namespace WebCore

#endif // IconDatabaseBase_h
