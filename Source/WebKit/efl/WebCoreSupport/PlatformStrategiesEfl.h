/*
    Copyright (C) 2012 Intel Corporation

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef PlatformStrategiesEfl_h
#define PlatformStrategiesEfl_h

#include "CookiesStrategy.h"
#include "DatabaseStrategy.h"
#include "LoaderStrategy.h"
#include "PasteboardStrategy.h"
#include "PlatformStrategies.h"
#include "PluginStrategy.h"
#include "SharedWorkerStrategy.h"
#include "StorageStrategy.h"

class PlatformStrategiesEfl : public WebCore::PlatformStrategies, private WebCore::CookiesStrategy, private WebCore::DatabaseStrategy, private WebCore::LoaderStrategy, private WebCore::PluginStrategy, private WebCore::SharedWorkerStrategy, private WebCore::StorageStrategy {
public:
    static void initialize();

private:
    PlatformStrategiesEfl();

    // WebCore::PlatformStrategies
    virtual WebCore::CookiesStrategy* createCookiesStrategy();
    virtual WebCore::DatabaseStrategy* createDatabaseStrategy();
    virtual WebCore::LoaderStrategy* createLoaderStrategy();
    virtual WebCore::PasteboardStrategy* createPasteboardStrategy();
    virtual WebCore::PluginStrategy* createPluginStrategy();
    virtual WebCore::SharedWorkerStrategy* createSharedWorkerStrategy();
    virtual WebCore::StorageStrategy* createStorageStrategy();

    // WebCore::CookiesStrategy
    virtual String cookiesForDOM(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&);
    virtual void setCookiesFromDOM(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&, const String&);
    virtual bool cookiesEnabled(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&);
    virtual String cookieRequestHeaderFieldValue(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&);
    virtual bool getRawCookies(const WebCore::NetworkStorageSession&, const WebCore::URL& firstParty, const WebCore::URL&, Vector<WebCore::Cookie>&);
    virtual void deleteCookie(const WebCore::NetworkStorageSession&, const WebCore::URL&, const String&);

    // WebCore::DatabaseStrategy
    // - Using default implementation.

    // WebCore::PluginStrategy
    virtual void refreshPlugins();
    virtual void getPluginInfo(const WebCore::Page*, Vector<WebCore::PluginInfo>&);
};

#endif // PlatformStrategiesEfl_h
