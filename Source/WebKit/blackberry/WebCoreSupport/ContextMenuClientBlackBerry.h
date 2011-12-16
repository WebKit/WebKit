/*
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef ContextMenuClientBlackBerry_h
#define ContextMenuClientBlackBerry_h

#include "ContextMenuClient.h"

namespace WebCore {

class ContextMenuClientBlackBerry : public ContextMenuClient {
public:
    virtual void contextMenuDestroyed();
    virtual void* getCustomMenuFromDefaultItems(ContextMenu*);
    virtual void contextMenuItemSelected(ContextMenuItem*, const ContextMenu*);
    virtual void downloadURL(const KURL&);
    virtual void searchWithGoogle(const Frame*);
    virtual void lookUpInDictionary(Frame*);
    virtual bool isSpeaking();
    virtual void speak(const String&);
    virtual void stopSpeaking();
};

} // WebCore

#endif // ContextMenuClientBlackBerry_h
