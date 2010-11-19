/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef WebGLTexture_h
#define WebGLTexture_h

#include "WebGLObject.h"

#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

class WebGLTexture : public WebGLObject {
public:
    virtual ~WebGLTexture() { deleteObject(); }

    static PassRefPtr<WebGLTexture> create(WebGLRenderingContext*);

    void setTarget(unsigned long target, int maxLevel);
    void setParameteri(unsigned long pname, int param);
    void setParameterf(unsigned long pname, float param);

    int getMinFilter() const { return m_minFilter; }

    void setLevelInfo(unsigned long target, int level, unsigned long internalFormat, int width, int height, unsigned long type);

    bool canGenerateMipmaps();
    // Generate all level information.
    void generateMipmapLevelInfo();

    unsigned long getInternalFormat(int level) const;

    // Whether width/height is NotPowerOfTwo.
    static bool isNPOT(unsigned, unsigned);

    bool isNPOT() const;
    // Determine if texture sampling should always return [0, 0, 0, 1] (OpenGL ES 2.0 Sec 3.8.2).
    bool needToUseBlackTexture() const;

    bool hasEverBeenBound() const { return object() && m_target; }

    static int computeLevelCount(int width, int height);

protected:
    WebGLTexture(WebGLRenderingContext*);

    virtual void deleteObjectImpl(Platform3DObject);

private:
    virtual bool isTexture() const { return true; }

    void update();

    int mapTargetToIndex(unsigned long);

    unsigned long m_target;

    int m_minFilter;
    int m_magFilter;
    int m_wrapS;
    int m_wrapT;

    class LevelInfo {
    public:
        LevelInfo()
            : valid(false)
            , internalFormat(0)
            , width(0)
            , height(0)
            , type(0)
        {
        }

        void setInfo(unsigned long internalFmt, int w, int h, unsigned long tp)
        {
            valid = true;
            internalFormat = internalFmt;
            width = w;
            height = h;
            type = tp;
        }

        bool valid;
        unsigned long internalFormat;
        int width;
        int height;
        unsigned long type;
    };

    Vector<Vector<LevelInfo> > m_info;

    bool m_isNPOT;
    bool m_isComplete;
    bool m_needToUseBlackTexture;
};

} // namespace WebCore

#endif // WebGLTexture_h
