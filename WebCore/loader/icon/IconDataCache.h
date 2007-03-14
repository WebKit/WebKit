/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 
#ifndef IconDataCache_h
#define IconDataCache_h

#include "PlatformString.h"

namespace WebCore { 

class Image;
class IntSize;
class SharedBuffer;
class SQLDatabase;

enum ImageDataStatus {
    ImageDataStatusPresent, ImageDataStatusMissing, ImageDataStatusUnknown
};
    
class IconDataCache {
public:
    IconDataCache(const String& url); 
    ~IconDataCache();
    
    time_t getTimestamp() { return m_stamp; }
    void setTimestamp(time_t stamp) { m_stamp = stamp; }
        
    Image* getImage(const IntSize&);    
    String getIconURL() { return m_iconURL; }

    void setImageData(PassRefPtr<SharedBuffer> data);
    
    void loadImageFromResource(const char*);
    
    void writeToDatabase(SQLDatabase& db);
    
    ImageDataStatus imageDataStatus();
    
private:
    String m_iconURL;
    time_t m_stamp;
    Image* m_image;
    
    // This allows us to cache whether or not a SiteIcon has had its data set yet
    // This helps the IconDatabase know if it has to set the data on a new object or not,
    // and also to determine if the icon is missing data or if it just hasn't been brought
    // in from the DB yet
    bool m_dataSet;
    
    // FIXME - Right now WebCore::Image doesn't have a very good API for accessing multiple representations
    // Even the NSImage way of doing things that we do in WebKit isn't very clean...  once we come up with a 
    // better way of handling that, we'll likely have a map of size-to-images similar to below
    // typedef HashMap<IntSize, Image*> SizeImageMap;
    // SizeImageMap m_images;
};


} //namespace WebCore

#endif
