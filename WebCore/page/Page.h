// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * Copyright (C) 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef PAGE_H
#define PAGE_H

#include "PlatformString.h"
#include <kxmlcore/HashSet.h>
#include <kxmlcore/Noncopyable.h>
#include <kxmlcore/PassRefPtr.h>
#include <kxmlcore/RefPtr.h>

#if __APPLE__
#ifdef __OBJC__
@class WebCorePageBridge;
#else
class WebCorePageBridge;
#endif
#endif

namespace WebCore {

    class Frame;
    class FrameNamespace;
    class IntRect;
    
    class Page : Noncopyable {
    public:
        ~Page();

        void setMainFrame(PassRefPtr<Frame> mainFrame);
        Frame* mainFrame() const { return m_mainFrame.get(); }

        IntRect windowRect() const;
        void setWindowRect(const IntRect&);

        void setGroupName(const String&);
        String groupName() const { return m_groupName; }
        const HashSet<Page*>* frameNamespace() const;
        static const HashSet<Page*>* frameNamespace(const String&);

        void incrementFrameCount() { ++m_frameCount; }
        void decrementFrameCount() { --m_frameCount; }
        int frameCount() const { return m_frameCount; }

#if __APPLE__
        Page(WebCorePageBridge*);
        WebCorePageBridge* bridge() const { return m_bridge; }
#endif

    private:
        void init();

        RefPtr<Frame> m_mainFrame;
        int m_frameCount;
        String m_groupName;

#if __APPLE__
        WebCorePageBridge* m_bridge;
#endif
    };

} // namespace WebCore
    
#endif // PAGE_H
