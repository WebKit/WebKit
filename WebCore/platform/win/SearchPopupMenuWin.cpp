/*
 * Copyright (C) 2006, 2007 Apple Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SearchPopupMenu.h"

#include "AtomicString.h"
#include <wtf/RetainPtr.h>

namespace WebCore {

SearchPopupMenu::SearchPopupMenu(PopupMenuClient* client)
    : PopupMenu(client)
{
}

bool SearchPopupMenu::enabled()
{
    return true;
}

static RetainPtr<CFStringRef> autosaveKey(const String& name)
{
    String key = "com.apple.WebKit.searchField:" + name;
    return RetainPtr<CFStringRef>(AdoptCF, key.createCFString());
}

void SearchPopupMenu::saveRecentSearches(const AtomicString& name, const Vector<String>& searchItems)
{
    if (name.isEmpty())
        return;

    RetainPtr<CFMutableArrayRef> items;

    size_t size = searchItems.size();
    if (size) {
        items.adoptCF(CFArrayCreateMutable(0, size, &kCFTypeArrayCallBacks));
        for (size_t i = 0; i < size; ++i) {
            RetainPtr<CFStringRef> item(AdoptCF, searchItems[i].createCFString());
            CFArrayAppendValue(items.get(), item.get());
        }
    }

    CFPreferencesSetAppValue(autosaveKey(name).get(), items.get(), kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
}

void SearchPopupMenu::loadRecentSearches(const AtomicString& name, Vector<String>& searchItems)
{
    if (name.isEmpty())
        return;

    searchItems.clear();
    RetainPtr<CFArrayRef> items(AdoptCF, reinterpret_cast<CFArrayRef>(CFPreferencesCopyAppValue(autosaveKey(name).get(), kCFPreferencesCurrentApplication)));

    if (!items || CFGetTypeID(items.get()) != CFArrayGetTypeID())
        return;

    size_t size = CFArrayGetCount(items.get());
    for (size_t i = 0; i < size; ++i) {
        CFStringRef item = (CFStringRef)CFArrayGetValueAtIndex(items.get(), i);
        if (CFGetTypeID(item) == CFStringGetTypeID())
            searchItems.append(item);
    }
}

}
