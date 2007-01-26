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
 *
 */

#ifndef Icon_h
#define Icon_h

#include "Shared.h"

#include <wtf/PassRefPtr.h>

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSImage;
#else
class NSImage;
#endif
#elif PLATFORM(WIN)
typedef struct HICON__* HICON;
#elif PLATFORM(QT)
#include <QIcon>
#endif

namespace WebCore {

class GraphicsContext;
class IntRect;
class String;
    
class Icon : public Shared<Icon> {
public:
    Icon();
    ~Icon();
    
    static PassRefPtr<Icon> newIconForFile(const String& filename);

    void paint(GraphicsContext*, const IntRect&);

#if PLATFORM(WIN)
    Icon(HICON);
#endif

private:
#if PLATFORM(MAC)
    NSImage* m_nsImage;
#elif PLATFORM(WIN)
    HICON m_hIcon;
#elif PLATFORM(QT)
    QIcon m_icon;
#endif
};

}

#endif
