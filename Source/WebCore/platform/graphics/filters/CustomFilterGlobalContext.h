/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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

#ifndef CustomFilterGlobalContext_h
#define CustomFilterGlobalContext_h

#if ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)
#include "CustomFilterProgramInfo.h"
#include <wtf/HashMap.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class CustomFilterCompiledProgram;
class HostWindow;
class GraphicsContext3D;

typedef HashMap<CustomFilterProgramInfo, CustomFilterCompiledProgram*> CustomFilterCompiledProgramsMap;

class CustomFilterGlobalContext {
public:
    CustomFilterGlobalContext();
    ~CustomFilterGlobalContext();
    
    GraphicsContext3D* context() const { return m_context.get(); }
    
    void prepareContextIfNeeded(HostWindow*);

    PassRefPtr<CustomFilterCompiledProgram> getCompiledProgram(const CustomFilterProgramInfo&);
    void removeCompiledProgram(const CustomFilterCompiledProgram*);
private:
    RefPtr<GraphicsContext3D> m_context;
    CustomFilterCompiledProgramsMap m_programs;
};

} // namespace WebCore

#endif // ENABLE(CSS_SHADERS) && USE(3D_GRAPHICS)

#endif // CustomFilterGlobalContext_h
