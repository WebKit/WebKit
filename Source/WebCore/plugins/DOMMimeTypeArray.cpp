/*
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "DOMMimeTypeArray.h"

#include "DOMPlugin.h"
#include "Frame.h"
#include "Page.h"
#include "PluginData.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

DOMMimeTypeArray::DOMMimeTypeArray(DOMWindow* window)
    : DOMWindowProperty(window)
{
}

DOMMimeTypeArray::~DOMMimeTypeArray() = default;

unsigned DOMMimeTypeArray::length() const
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;

    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    data->getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);
    return mimes.size();
}

RefPtr<DOMMimeType> DOMMimeTypeArray::item(unsigned index)
{
    PluginData* data = getPluginData();
    if (!data)
        return nullptr;

    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    data->getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);

    if (index >= mimes.size())
        return nullptr;
    return DOMMimeType::create(data, frame(), index);
}

RefPtr<DOMMimeType> DOMMimeTypeArray::namedItem(const AtomicString& propertyName)
{
    PluginData* data = getPluginData();
    if (!data)
        return nullptr;

    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    data->getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);
    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i].type == propertyName)
            return DOMMimeType::create(data, frame(), i);
    }
    return nullptr;
}

Vector<AtomicString> DOMMimeTypeArray::supportedPropertyNames()
{
    PluginData* data = getPluginData();
    if (!data)
        return { };

    Vector<MimeClassInfo> mimes;
    Vector<size_t> mimePluginIndices;
    data->getWebVisibleMimesAndPluginIndices(mimes, mimePluginIndices);

    Vector<AtomicString> result;
    result.reserveInitialCapacity(mimes.size());
    for (auto& info : mimes)
        result.uncheckedAppend(WTFMove(info.type));

    return result;
}

PluginData* DOMMimeTypeArray::getPluginData() const
{
    auto* frame = this->frame();
    if (!frame)
        return nullptr;

    auto* page = frame->page();
    if (!page)
        return nullptr;

    return &page->pluginData();
}

} // namespace WebCore
