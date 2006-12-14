/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef JSSVGPODTypeWrapper_H
#define JSSVGPODTypeWrapper_H

#ifdef SVG_SUPPORT

#include "Shared.h"
#include <wtf/Assertions.h>

namespace WebCore {

template<typename PODType>
class JSSVGPODTypeWrapper : public Shared<JSSVGPODTypeWrapper<PODType> >
{
public:
    JSSVGPODTypeWrapper(const PODType& type)
    : m_podType(type)
    { }

    virtual ~JSSVGPODTypeWrapper() { }

    operator PODType&() { return m_podType; }

    // Implemented by JSSVGPODTypeWrapperCreator    
    virtual void commitChange() { }

private:
    PODType m_podType;
};

template<typename PODType, typename PODTypeCreator>
class JSSVGPODTypeWrapperCreator : public JSSVGPODTypeWrapper<PODType>
{
public:
    typedef PODType (PODTypeCreator::*GetterMethod)() const;
    typedef void (PODTypeCreator::*SetterMethod)(PODType);

    JSSVGPODTypeWrapperCreator(PODTypeCreator* creator, GetterMethod getter, SetterMethod setter)
    : JSSVGPODTypeWrapper<PODType>((creator->*getter)())
    , m_creator(creator)
    , m_setter(setter)
    { }

    virtual ~JSSVGPODTypeWrapperCreator() { }

    virtual void commitChange() { (m_creator->*m_setter)((PODType&)(*this)); }

private:
    // Update callbacks
    PODTypeCreator* m_creator;
    SetterMethod m_setter;
};

};

#endif // SVG_SUPPORT
#endif // JSSVGPODTypeWrapper_H
