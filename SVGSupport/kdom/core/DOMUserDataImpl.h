/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KDOM_DOMUserDataImpl_H
#define KDOM_DOMUserDataImpl_H

#include <kdom/Shared.h>

namespace KJS
{
    class Value;
    class UString;
    class ExecState;
};

namespace KDOM
{
    // Introduced in DOM Level 3:
    class DOMUserDataImpl : public Shared<DOMUserDataImpl>
    {
    public:
        DOMUserDataImpl();
        DOMUserDataImpl(bool b);
        virtual ~DOMUserDataImpl();

        // Internal
        void setUserData(void *userData);
        void *userData() const;

    private:
        void *m_userData;
    };

    // template specialization - for comparing with boolean

/* FIXME - Think about this in the new ecma system
    template<>
    class DOMBridge<DOMUserData> : public KJS::ObjectImp
    {
    public:
        // Example: T=Element, it's impl class is called ElementImpl, and a synonym for it is Element::Private
        DOMBridge(KJS::ExecState *exec, DOMUserDataImpl *impl) : KJS::ObjectImp(DOMUserData(impl).prototype(exec)), m_impl(impl)
        {
            Shared *sharedImpl = dynamic_cast<Shared *>(m_impl);
            if(sharedImpl)
                sharedImpl->ref();
        }

        ~DOMBridge()
        {
            Shared *sharedImpl = dynamic_cast<Shared *>(m_impl);
            if(sharedImpl)
                sharedImpl->deref();
        }

        DOMUserDataImpl *impl() const { return m_impl; }

        virtual KJS::ValueImp *get(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
        {
            kdDebug(26004) << "DOMBridge::get(), " << propertyName.qstring() << " Name: " << classInfo()->className << " Object: " << this->m_impl << endl;

            // Look for standard properties (e.g. those in the hashtables)
            DOMUserData obj(m_impl);
            KJS::ValueImp *val = obj.get(exec, propertyName, this);

            if(val->type() != KJS::UndefinedType)
                return val;

            // Not found -> forward to ObjectImp.
            val = KJS::ObjectImp::get(exec, propertyName);
            if(val->type() == KJS::UndefinedType)
                kdDebug(26004) << "WARNING: " << propertyName.qstring() << " not found in... Name: " << classInfo()->className << " Object: " << m_impl << " on line : " << exec->context().curStmtFirstLine() << endl;

            return val;
        }

        virtual bool hasProperty(KJS::ExecState *exec, const KJS::Identifier &propertyName) const
        {
            kdDebug(26004) << "DOMBridge::hasProperty(), " << propertyName.qstring() << " Name: " << classInfo()->className << " Object: " << m_impl << endl;

            DOMUserData obj(m_impl);
            if(obj.hasProperty(exec, propertyName))
                return true;

            return KJS::ObjectImp::hasProperty(exec, propertyName);
        }

        virtual const KJS::ClassInfo *classInfo() const { return &DOMUserData::s_classInfo; }
        virtual KJS::ValueImp *toPrimitive(KJS::ExecState *, KJS::Type = KJS::UndefinedType) const
        {
            kdDebug() << k_funcinfo << endl;
            if(m_impl && (*((bool *)(m_impl->userData()))))
                return KJS::String("true");
            else
                return KJS::String("false");
        }
        virtual bool toBoolean(KJS::ExecState *) const
        {
            kdDebug() << k_funcinfo << endl;
            if(m_impl && (*((bool *)(m_impl->userData()))))
                return true;
            else
                return false;
        }

    protected:
        DOMUserDataImpl *m_impl;
    };
*/
};

#endif

// vim:ts=4:noet
