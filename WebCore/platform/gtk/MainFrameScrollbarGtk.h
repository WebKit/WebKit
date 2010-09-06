/*
 *  Copyright (C) 2010 Igalia S.L.
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


#ifndef MainFrameScrollbarGtk_h
#define MainFrameScrollbarGtk_h

#include "GRefPtrGtk.h"
#include "Scrollbar.h"
#include <wtf/PassRefPtr.h>

typedef struct _GtkAdjustment GtkAdjustment;

namespace WebCore {

class MainFrameScrollbarGtk : public Scrollbar {
public:
    ~MainFrameScrollbarGtk();
    virtual void paint(GraphicsContext*, const IntRect&);
    void detachAdjustment();
    void attachAdjustment(GtkAdjustment*);
    static PassRefPtr<MainFrameScrollbarGtk> create(ScrollbarClient*, ScrollbarOrientation, GtkAdjustment*);

protected:
    virtual void updateThumbPosition();
    virtual void updateThumbProportion();

private:
    MainFrameScrollbarGtk(ScrollbarClient*, ScrollbarOrientation, GtkAdjustment*);
    static void gtkValueChanged(GtkAdjustment*, MainFrameScrollbarGtk*);

    PlatformRefPtr<GtkAdjustment> m_adjustment;
};

}

#endif // ScrollbarGtk_h
