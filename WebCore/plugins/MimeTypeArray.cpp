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
#include "MimeTypeArray.h"

#include "AtomicString.h"
#include "Frame.h"
#include "Page.h"
#include "Plugin.h"
#include "PluginData.h"

namespace WebCore {

MimeTypeArray::MimeTypeArray(Frame* frame)
    : m_frame(frame)
{
}

MimeTypeArray::~MimeTypeArray()
{
}

unsigned MimeTypeArray::length() const
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    return data->mimes().size();
}

PassRefPtr<MimeType> MimeTypeArray::item(unsigned index)
{
    PluginData* data = getPluginData();
    if (!data)
        return 0;
    const Vector<MimeClassInfo*>& mimes = data->mimes();
    if (index >= mimes.size())
        return 0;
    return MimeType::create(data, index).get();
}

bool MimeTypeArray::canGetItemsForName(const AtomicString& propertyName)
{
    PluginData *data = getPluginData();
    if (!data)
        return 0;
    const Vector<MimeClassInfo*>& mimes = data->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i]->type == propertyName)
            return true;
    }
    return false;
}

PassRefPtr<MimeType> MimeTypeArray::namedItem(const AtomicString& propertyName)
{
    PluginData *data = getPluginData();
    if (!data)
        return 0;
    const Vector<MimeClassInfo*>& mimes = data->mimes();
    for (unsigned i = 0; i < mimes.size(); ++i) {
        if (mimes[i]->type == propertyName)
            return MimeType::create(data, i).get();
    }
    return 0;
}

PluginData* MimeTypeArray::getPluginData() const
{
    if (!m_frame)
        return 0;
    Page* p = m_frame->page();
    if (!p)
        return 0;
    return p->pluginData();
}

} // namespace WebCore
