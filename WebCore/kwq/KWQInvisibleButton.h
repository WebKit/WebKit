#ifndef KWQINVISIBLEBUTTON_H_
#define KWQINVISIBLEBUTTON_H_

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
#import <Cocoa/Cocoa.h>
#endif

#include <qptrlist.h>
#include "rendering/render_replaced.h"
#include "rendering/render_image.h"
#include "rendering/render_flow.h"
#include "rendering/render_form.h"
#include "html/html_formimpl.h"

namespace DOM {
    class HTMLGenericFormElementImpl;
};

namespace khtml {
    class RenderImageButton;
}

// class KWQInvisibleButton ================================================================

class KWQInvisibleButton {
public:

    // structs -----------------------------------------------------------------
    // typedefs ----------------------------------------------------------------

    KWQInvisibleButton(khtml::RenderImageButton *theImageButton);
    ~KWQInvisibleButton();

    // member functions --------------------------------------------------------

    void setFrameInView(int x, int y, int w, int h, KHTMLView *khtmlview);

    // operators ---------------------------------------------------------------

// protected -------------------------------------------------------------------
// private ---------------------------------------------------------------------

private:
    khtml::RenderImageButton *imageButton;
#if (defined(__APPLE__) && defined(__OBJC__) && defined(__cplusplus))
    id buttonView;
#else
    void *buttonView;
#endif

}; // class KWQInvisibleButton =============================================================

#endif
