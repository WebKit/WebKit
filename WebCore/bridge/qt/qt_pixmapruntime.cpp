/*
 * Copyright (C) 2009 Nokia Corporation and/or its subsidiary(-ies)
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
 *
 */
#include "config.h"
#include "qt_pixmapruntime.h"

#include "CachedImage.h"
#include "DOMWindow.h"
#include "HTMLImageElement.h"
#include "HTMLNames.h"
#include "JSDOMBinding.h"
#include "JSDOMWindow.h"
#include "JSGlobalObject.h"
#include "JSHTMLImageElement.h"
#include "JSLock.h"
#include "ObjectPrototype.h"
#include "StillImageQt.h"
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QPixmap>
#include <QVariant>
#include <runtime_object.h>
#include <runtime_root.h>

using namespace WebCore;
namespace JSC {

namespace Bindings {

class QtPixmapClass : public Class {
public:
    QtPixmapClass();
    virtual MethodList methodsNamed(const Identifier&, Instance*) const;
    virtual Field* fieldNamed(const Identifier&, Instance*) const;
};


class QtPixmapWidthField : public Field {
public:
    static const char* name() { return "width"; }
    virtual JSValue valueFromInstance(ExecState* exec, const Instance* pixmap) const
    {
        return jsNumber(exec, static_cast<const QtPixmapInstance*>(pixmap)->width());
    }
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue) const {}
};
class QtPixmapHeightField : public Field {
public:
    static const char* name() { return "height"; }
    virtual JSValue valueFromInstance(ExecState* exec, const Instance* inst) const
    {
        return jsNumber(exec, static_cast<const QtPixmapInstance*>(inst)->height());
    }
    virtual void setValueToInstance(ExecState*, const Instance*, JSValue) const {}
};

class QtPixmapRuntimeMethod : public Method {
public:
    virtual int numParameters() const
    {
        return 0;
    }
    virtual JSValue invoke(ExecState* exec, QVariant&, PassRefPtr<RootObject> root, QtPixmapInstance* inst) = 0;

};

class QtPixmapCreateElementMethod : public QtPixmapRuntimeMethod {
public:
    static const char* name() { return "toHTMLImageElement"; }
    JSValue invoke(ExecState* exec, QVariant& v, PassRefPtr<RootObject> root, QtPixmapInstance*)
    {
        QPixmap pxm;
        if (v.type() == static_cast<QVariant::Type>(qMetaTypeId<QImage>())) {
            pxm = QPixmap::fromImage(v.value<QImage>());
            v = QVariant::fromValue<QPixmap>(pxm);
        } else
            pxm = v.value<QPixmap>();

        Document* document = 0;
        JSDOMGlobalObject* global = static_cast<JSDOMGlobalObject*>(root->globalObject());
        if (global) {
            DOMWindow* dWindow = toDOMWindow(global);
            if (dWindow)
                document = dWindow->document();
        }

        if (document) {
            PassRefPtr<StillImage> img = WebCore::StillImage::create(pxm);
            RefPtr<HTMLImageElement> image = new HTMLImageElement(HTMLNames::imgTag, document);
            image->setCachedImage(new CachedImage(img.get()));
            toJS(exec, global, document);
            return asObject(toJS(exec, global, image.release()));
        }
        return jsUndefined();
    }

};

class QtPixmapToDataUrlMethod : public QtPixmapRuntimeMethod {
public:
    static const char* name() { return "toDataUrl"; }
    JSValue invoke(ExecState* exec, QVariant& v, PassRefPtr<RootObject> root, QtPixmapInstance*)
    {
        QImage image;
        // for getting the data url, we always prefer the image.
        if (v.type() == static_cast<QVariant::Type>(qMetaTypeId<QPixmap>())) {
            image = v.value<QPixmap>().toImage();
            v = QVariant::fromValue<QImage>(image);
        } else
            image = v.value<QImage>();
        QByteArray ba;
        QBuffer b(&ba);
        image.save(&b, "PNG");
        const QString b64 = QString("data:image/png;base64,") + ba.toBase64();
        const UString ustring((UChar*)b64.utf16(), b64.length());
        return jsString(exec, ustring);
    }

};

class QtPixmapToStringMethod : public QtPixmapRuntimeMethod {
    public:
    static const char* name() { return "toString"; }
    JSValue invoke(ExecState* exec, QVariant& v, PassRefPtr<RootObject> root, QtPixmapInstance* inst)
    {
        return inst->valueOf(exec);
    }

};

struct QtPixmapMetaData {
    QtPixmapToDataUrlMethod toDataUrlMethod;
    QtPixmapCreateElementMethod createElementMethod;
    QtPixmapToStringMethod toStringMethod;
    QtPixmapHeightField heightField;
    QtPixmapWidthField widthField;
    QtPixmapClass cls;
} qt_pixmap_metaData;

// Derived RuntimeObject
class QtPixmapRuntimeObjectImp : public RuntimeObjectImp {
public:
    QtPixmapRuntimeObjectImp(ExecState*, PassRefPtr<Instance>);

    static const ClassInfo s_info;

    static PassRefPtr<Structure> createStructure(JSValue prototype)
    {
        return Structure::create(prototype, TypeInfo(ObjectType,  StructureFlags), AnonymousSlotCount);
    }

protected:
    static const unsigned StructureFlags = RuntimeObjectImp::StructureFlags | OverridesMarkChildren;

private:
    virtual const ClassInfo* classInfo() const { return &s_info; }
};

QtPixmapRuntimeObjectImp::QtPixmapRuntimeObjectImp(ExecState* exec, PassRefPtr<Instance> instance)
    : RuntimeObjectImp(exec, WebCore::deprecatedGetDOMStructure<QtPixmapRuntimeObjectImp>(exec), instance)
{
}

const ClassInfo QtPixmapRuntimeObjectImp::s_info = { "QtPixmapRuntimeObject", &RuntimeObjectImp::s_info, 0, 0 };


QtPixmapClass::QtPixmapClass()
{
}


Class* QtPixmapInstance::getClass() const
{
    return &qt_pixmap_metaData.cls;
}

JSValue QtPixmapInstance::invokeMethod(ExecState* exec, const MethodList& methods, const ArgList& args)
{
    if (methods.size() == 1) {
        QtPixmapRuntimeMethod* mtd = static_cast<QtPixmapRuntimeMethod*>(methods[0]);        
        return mtd->invoke(exec, data, rootObject(), this);
    }
    return jsUndefined();
}

MethodList QtPixmapClass::methodsNamed(const Identifier& identifier, Instance*) const
{
    MethodList methods;
    if (identifier == QtPixmapToDataUrlMethod::name())
        methods.append(&qt_pixmap_metaData.toDataUrlMethod);
    else if (identifier == QtPixmapCreateElementMethod::name())
        methods.append(&qt_pixmap_metaData.createElementMethod);
    else if (identifier == QtPixmapToStringMethod::name())
        methods.append(&qt_pixmap_metaData.toStringMethod);
    return methods;
}

Field* QtPixmapClass::fieldNamed(const Identifier& identifier, Instance*) const
{
    if (identifier == QtPixmapWidthField::name())
        return &qt_pixmap_metaData.widthField;
    if (identifier == QtPixmapHeightField::name())
        return &qt_pixmap_metaData.heightField;
    return 0;
}

void QtPixmapInstance::getPropertyNames(ExecState*exec, PropertyNameArray& arr)
{
    arr.add(Identifier(exec, UString(QtPixmapToDataUrlMethod::name())));
    arr.add(Identifier(exec, UString(QtPixmapCreateElementMethod::name())));
    arr.add(Identifier(exec, UString(QtPixmapToStringMethod::name())));
    arr.add(Identifier(exec, UString(QtPixmapWidthField::name())));
    arr.add(Identifier(exec, UString(QtPixmapHeightField::name())));
}

JSValue QtPixmapInstance::defaultValue(ExecState* exec, PreferredPrimitiveType ptype) const
{
    if (ptype == PreferNumber) {
        return jsBoolean(
                (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QImage>()) && !(data.value<QImage>()).isNull())
                || (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QPixmap>()) && !data.value<QPixmap>().isNull()));
    }
    if (ptype == PreferString)
        return valueOf(exec);
    return jsUndefined();
}

JSValue QtPixmapInstance::valueOf(ExecState* exec) const
{
    const QString toStr = QString("[Qt Native Pixmap %1,%2]").arg(width()).arg(height());
    UString ustring((UChar*)toStr.utf16(), toStr.length());
    return jsString(exec, ustring);
}

QtPixmapInstance::QtPixmapInstance(PassRefPtr<RootObject> rootObj, const QVariant& d)
        :Instance(rootObj), data(d)
{
}

int QtPixmapInstance::width() const
{
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QPixmap>()))
        return data.value<QPixmap>().width();
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QImage>()))
        return data.value<QImage>().width();
    return 0;
}

int QtPixmapInstance::height() const
{
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QPixmap>()))
        return data.value<QPixmap>().height();
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QImage>()))
        return data.value<QImage>().height();
    return 0;
}

QPixmap QtPixmapInstance::toPixmap()
{
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QPixmap>()))
        return data.value<QPixmap>();
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QImage>())) {
        const QPixmap pxm = QPixmap::fromImage(data.value<QImage>());
        data = QVariant::fromValue<QPixmap>(pxm);
        return pxm;
    }
    return QPixmap();

}

QImage QtPixmapInstance::toImage()
{
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QImage>()))
        return data.value<QImage>();
    if (data.type() == static_cast<QVariant::Type>(qMetaTypeId<QPixmap>())) {
        const QImage img = data.value<QPixmap>().toImage();
        data = QVariant::fromValue<QImage>(img);
        return img;
    }
    return QImage();
}

QVariant QtPixmapInstance::variantFromObject(JSObject* object, QMetaType::Type hint)
{
    if (!object) {
        if (hint == qMetaTypeId<QPixmap>())
            return QVariant::fromValue<QPixmap>(QPixmap());
        if (hint == qMetaTypeId<QImage>())
            return QVariant::fromValue<QImage>(QImage());
    } else if (object->inherits(&JSHTMLImageElement::s_info)) {
        JSHTMLImageElement* el = static_cast<JSHTMLImageElement*>(object);
        HTMLImageElement* imageElement = static_cast<HTMLImageElement*>(el->impl());
        if (imageElement) {
            CachedImage* cImg = imageElement->cachedImage();
            if (cImg) {
                Image* img = cImg->image();
                if (img) {
                    QPixmap* pxm = img->nativeImageForCurrentFrame();
                    if (pxm) {
                        return (hint == static_cast<QMetaType::Type>(qMetaTypeId<QPixmap>()))
                              ? QVariant::fromValue<QPixmap>(*pxm)
                              : QVariant::fromValue<QImage>(pxm->toImage());
                    }
                }
            }
        }
    } else if (object->inherits(&QtPixmapRuntimeObjectImp::s_info)) {
        QtPixmapRuntimeObjectImp* imp = static_cast<QtPixmapRuntimeObjectImp*>(object);
        QtPixmapInstance* inst = static_cast<QtPixmapInstance*>(imp->getInternalInstance());
        if (inst) {
            if (hint == qMetaTypeId<QPixmap >())
                return QVariant::fromValue<QPixmap>(inst->toPixmap());
            if (hint == qMetaTypeId<QImage>())
                return QVariant::fromValue<QImage>(inst->toImage());
        }
    }
    return 0;
}
JSObject* QtPixmapInstance::createRuntimeObject(ExecState* exec, PassRefPtr<RootObject> root, const QVariant& data)
{
    JSLock lock(SilenceAssertionsOnly);
    return new(exec) QtPixmapRuntimeObjectImp(exec, new QtPixmapInstance(root, data));
}

bool QtPixmapInstance::canHandle(QMetaType::Type hint)
{
    return hint == qMetaTypeId<QImage>() || hint == qMetaTypeId<QPixmap>();
}

}

}
