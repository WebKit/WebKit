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

#ifndef JSSVGPODTypeWrapper_h
#define JSSVGPODTypeWrapper_h

#if ENABLE(SVG)

#include "Frame.h"
#include "Shared.h"
#include "SVGElement.h"

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
    virtual void commitChange(KJS::ExecState*) { }

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

    virtual void commitChange(KJS::ExecState* exec)
    {
        (m_creator->*m_setter)((PODType&)(*this));

        ASSERT(exec && exec->dynamicInterpreter());
        Frame* activeFrame = static_cast<KJS::ScriptInterpreter*>(exec->dynamicInterpreter())->frame();
        if (!activeFrame)
            return;

        SVGDocumentExtensions* extensions = (activeFrame->document() ? activeFrame->document()->accessSVGExtensions() : 0);
        if (extensions && extensions->hasGenericContext<PODTypeCreator>(m_creator)) {
            const SVGElement* context = extensions->genericContext<PODTypeCreator>(m_creator);
            ASSERT(context);

            context->notifyAttributeChange();
        }
    }

private:
    // Update callbacks
    PODTypeCreator* m_creator;
    SetterMethod m_setter;
};

template<typename PODType>
class SVGPODListItem;

template<typename PODType, typename ListType>
class JSSVGPODTypeWrapperCreatorForList : public JSSVGPODTypeWrapperCreator<PODType, SVGPODListItem<PODType> >
{
public:
    JSSVGPODTypeWrapperCreatorForList(SVGPODListItem<PODType>* creator, const ListType* list)
    : JSSVGPODTypeWrapperCreator<PODType, SVGPODListItem<PODType> >(creator,
                                                                    &SVGPODListItem<PODType>::value,
                                                                    &SVGPODListItem<PODType>::setValue)
    , m_list(list)
    { }

    virtual ~JSSVGPODTypeWrapperCreatorForList() { }

    virtual void commitChange(KJS::ExecState* exec)
    {
        // Update POD item within SVGList
        JSSVGPODTypeWrapperCreator<PODType, SVGPODListItem<PODType> >::commitChange(exec);

        // Notify owner of the list, that it's content changed
        const SVGElement* context = m_list->context();
        ASSERT(context);

        context->notifyAttributeChange();         
    }

private:
    const ListType* m_list;
};

};

#endif // ENABLE(SVG)
#endif // JSSVGPODTypeWrapper_h
