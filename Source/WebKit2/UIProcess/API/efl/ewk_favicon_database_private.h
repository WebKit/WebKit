/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#ifndef ewk_favicon_database_private_h
#define ewk_favicon_database_private_h

#include "WKIconDatabase.h"
#include "WKRetainPtr.h"
#include "ewk_favicon_database.h"
#include <WebKit2/WKBase.h>
#include <wtf/HashMap.h>

struct IconChangeCallbackData {
    Ewk_Favicon_Database_Icon_Change_Cb callback;
    void* userData;

    IconChangeCallbackData()
        : callback(0)
        , userData(0)
    { }

    IconChangeCallbackData(Ewk_Favicon_Database_Icon_Change_Cb _callback, void* _userData)
        : callback(_callback)
        , userData(_userData)
    { }
};

struct IconRequestCallbackData {
    Ewk_Favicon_Database_Async_Icon_Get_Cb callback;
    void* userData;
    Evas* evas;

    IconRequestCallbackData()
        : callback(0)
        , userData(0)
        , evas(0)
    { }

    IconRequestCallbackData(Ewk_Favicon_Database_Async_Icon_Get_Cb _callback, void* _userData, Evas* _evas)
        : callback(_callback)
        , userData(_userData)
        , evas(_evas)
    { }
};

typedef HashMap<Ewk_Favicon_Database_Icon_Change_Cb, IconChangeCallbackData> ChangeListenerMap;
typedef Vector<IconRequestCallbackData> PendingIconRequestVector;
typedef HashMap<String /* pageURL */, PendingIconRequestVector> PendingIconRequestMap;

class Ewk_Favicon_Database {
public:
    WKRetainPtr<WKIconDatabaseRef> wkIconDatabase;
    ChangeListenerMap changeListeners;
    PendingIconRequestMap iconRequests;

    static PassOwnPtr<Ewk_Favicon_Database> create(WKIconDatabaseRef iconDatabaseRef)
    {
        return adoptPtr(new Ewk_Favicon_Database(iconDatabaseRef));
    }

private:
    explicit Ewk_Favicon_Database(WKIconDatabaseRef iconDatabaseRef);
};

#endif // ewk_favicon_database_private_h
