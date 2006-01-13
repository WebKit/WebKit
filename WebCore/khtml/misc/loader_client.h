
#ifndef LOADER_CLIENT_H
#define LOADER_CLIENT_H

#include <qpixmap.h>
#include "dom/dom_string.h"

#ifndef KHTML_NO_XBL
namespace XBL {
    class XBLDocumentImpl;
};
#endif

namespace khtml {
    class CachedObject;
    class CachedImage;

    /**
     * @internal
     *
     * a client who wants to load stylesheets, images or scripts from the web has to
     * inherit from this class and overload one of the 3 functions
     *
     */
    class CachedObjectClient
    {
    public:
        virtual ~CachedObjectClient() { }

        // clipped pixmap (if it is not yet completely loaded,
        // size of the complete (finished loading) pixmap
        // rectangle of the part that has been loaded very recently
        // pointer to us
        // return whether we need manual update
        virtual void setPixmap(const QPixmap &, const QRect&, CachedImage *) { }
        virtual void setStyleSheet(const DOM::DOMString &/*url*/, const DOM::DOMString &/*sheet*/) { }
#ifndef KHTML_NO_XBL
        virtual void setXBLDocument(const DOM::DOMString& url, XBL::XBLDocumentImpl* doc) { }
#endif
        virtual void notifyFinished(CachedObject * /*finishedObj*/) { }
    };
};

#endif
