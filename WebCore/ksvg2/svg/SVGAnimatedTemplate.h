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

#ifndef KSVG_SVGAnimatedTemplate_H
#define KSVG_SVGAnimatedTemplate_H

#include "SVGStyledElementImpl.h"

namespace KSVG
{
    // SVGAnimatedTemplate
    // - lazy creation of baseVal/animVal
    //   (no more waste of mostly unused animVal variable!)
    // - not directly creatable (only by child classes)
    // - no copy ctor/no assignment operator available
    //   (-> a class for only for pointer usage)
    template<class T>
    class SVGAnimatedTemplate : public KDOM::Shared<SVGAnimatedTemplate<T> >
    {
    public:
        virtual ~SVGAnimatedTemplate()
        {
            if(m_baseVal)
                m_baseVal->deref();
            if(m_animVal)
                m_animVal->deref();
        }

        T *baseVal() const
        {
            if(!m_baseVal)
            {
                m_baseVal = create();
                m_baseVal->ref();
            }

            return m_baseVal;
        }

        void setBaseVal(T *baseVal) const
        {
            KDOM_SAFE_SET(m_baseVal, baseVal);

            if(m_context)
                m_context->notifyAttributeChange();

            setAnimVal(baseVal);
        }

        T *animVal() const
        {
            if(!m_animVal)
            {
                m_animVal = create();
                m_animVal->ref();
            }

            return m_animVal;
        }

        void setAnimVal(T *animVal) const
        {
            KDOM_SAFE_SET(m_animVal, animVal);
            
            // I think this is superfluous... -- ECS 4/25/05
            if(m_context)
                m_context->notifyAttributeChange();
        }
        
    protected:
        SVGAnimatedTemplate(const SVGStyledElementImpl *context) : KDOM::Shared<SVGAnimatedTemplate>()
        {
            m_baseVal = 0;
            m_animVal = 0;
            m_context = context;
        }

        // This methods need to be reimplemented.        
        virtual T *create() const = 0;
        virtual void assign(T *src, T *dst) const = 0;

        // Attribute notification context
        const SVGStyledElementImpl *m_context;

    private:
        SVGAnimatedTemplate(const SVGAnimatedTemplate &) { }
        SVGAnimatedTemplate<T> &operator=(const SVGAnimatedTemplate<T> &) { }
        
        mutable T *m_baseVal;
        mutable T *m_animVal;
    };
};

#endif

// vim:ts=4:noet
