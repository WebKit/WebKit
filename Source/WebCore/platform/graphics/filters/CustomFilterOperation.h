/*
 * Copyright 2011 Adobe Systems Incorporated. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef CustomFilterOperation_h
#define CustomFilterOperation_h

#if ENABLE(CSS_SHADERS)
#include "CustomFilterProgram.h"
#include "FilterOperation.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

// CSS Shaders

class CustomFilterParameter;
typedef Vector<RefPtr<CustomFilterParameter> > CustomFilterParameterList;

class CustomFilterOperation : public FilterOperation {
public:
    enum MeshBoxType {
        FILTER_BOX,
        BORDER_BOX,
        PADDING_BOX,
        CONTENT_BOX
    };
    
    enum MeshType {
        ATTACHED,
        DETACHED
    };
    
    static PassRefPtr<CustomFilterOperation> create(PassRefPtr<CustomFilterProgram> program, const CustomFilterParameterList& sortedParameters, unsigned meshRows, unsigned meshColumns, MeshBoxType meshBoxType, MeshType meshType)
    {
        return adoptRef(new CustomFilterOperation(program, sortedParameters, meshRows, meshColumns, meshBoxType, meshType));
    }
    
    CustomFilterProgram* program() const { return m_program.get(); }
    
    const CustomFilterParameterList& parameters() { return m_parameters; }
    
    unsigned meshRows() const { return m_meshRows; }
    unsigned meshColumns() const { return m_meshColumns; }
    
    MeshBoxType meshBoxType() const { return m_meshBoxType; }
    MeshType meshType() const { return m_meshType; }
    
    virtual ~CustomFilterOperation();
    
private:
    virtual bool operator==(const FilterOperation& o) const
    {
        if (!isSameType(o))
            return false;

        const CustomFilterOperation* other = static_cast<const CustomFilterOperation*>(&o);
        return m_program.get() == other->m_program.get()
               && m_meshRows == other->m_meshRows
               && m_meshColumns == other->m_meshColumns
               && m_meshBoxType == other->m_meshBoxType
               && m_meshType == other->m_meshType;
    }
    
    CustomFilterOperation(PassRefPtr<CustomFilterProgram>, const CustomFilterParameterList&, unsigned meshRows, unsigned meshColumns, MeshBoxType, MeshType);
    
#ifndef NDEBUG
    bool hasSortedParameterList();
#endif

    RefPtr<CustomFilterProgram> m_program;
    CustomFilterParameterList m_parameters;
    
    unsigned m_meshRows;
    unsigned m_meshColumns;
    MeshBoxType m_meshBoxType;
    MeshType m_meshType;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS)

#endif // CustomFilterOperation_h
