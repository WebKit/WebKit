/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef LayerFilterRenderer_h
#define LayerFilterRenderer_h

#if USE(ACCELERATED_COMPOSITING) && ENABLE(CSS_FILTERS)

#include "IntRect.h"
#include "LayerData.h"
#include "OwnPtr.h"
#include "Texture.h"
#include "TransformationMatrix.h"

#include <BlackBerryPlatformGLES2Context.h>
#include <BlackBerryPlatformIntRectRegion.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class LayerCompositingThread;
class LayerFilterRenderer;
class LayerFilterRendererAction;
class LayerRendererSurface;

class Uniformf : public RefCounted<Uniformf> {
    WTF_MAKE_NONCOPYABLE(Uniformf);
public:
    virtual void apply() = 0;

protected:
    Uniformf(int location);
    const int& location() const { return m_location; }

private:
    int m_location;
};

class Uniform1f : public Uniformf {
public:
    static PassRefPtr<Uniformf> create(int location, float val);

protected:
    Uniform1f(int location, float val);

private:
    virtual void apply();
    float m_val;
};

class Uniform2f : public Uniformf {
public:
    static PassRefPtr<Uniformf> create(int location, float val0, float val1);

protected:
    Uniform2f(int location, float val0, float val1);

private:
    virtual void apply();
    float m_val[2];
};

class Uniform3f : public Uniformf {
public:
    static PassRefPtr<Uniformf> create(int location, float val0, float val1, float val2);

protected:
    Uniform3f(int location, float val0, float val1, float val2);

private:
    virtual void apply();
    float m_val[3];
};

class LayerFilterRendererAction : public RefCounted<LayerFilterRendererAction> {
public:
    static PassRefPtr<LayerFilterRendererAction> create(int programId);
        // A vector of actions must have an even count, so if you have an odd count, add a passthrough event at the end.
        // See the ping-pong note in LayerFilterRenderer::applyActions.

    bool shouldPushSnapshot() const { return m_pushSnapshot; }
    void setPushSnapshot() { m_pushSnapshot = true; }

    bool shouldPopSnapshot() const { return m_popSnapshot; }
    void setPopSnapshot() { m_popSnapshot = true; }

    void appendUniform(const RefPtr<Uniformf>& uniform) { m_uniforms.append(uniform); }
    void useActionOn(LayerFilterRenderer*);

protected:
    int m_programId;
    bool m_pushSnapshot;
    bool m_popSnapshot;

    Vector<RefPtr<Uniformf> > m_uniforms;
private:
    LayerFilterRendererAction(int programId);
};

class LayerFilterRenderer {
    WTF_MAKE_NONCOPYABLE(LayerFilterRenderer);

public:
    static PassOwnPtr<LayerFilterRenderer> create(const int& positionLocation, const int& texCoordLocation);
    void applyActions(unsigned& fbo, LayerCompositingThread*, Vector<RefPtr<LayerFilterRendererAction> >);
    Vector<RefPtr<LayerFilterRendererAction> > actionsForOperations(LayerRendererSurface*, const Vector<RefPtr<FilterOperation> >&);

    // If initialization fails, or disable() is called, this is false.
    bool isEnabled() const { return m_enabled; }
    void disable() { m_enabled = false; }

private:
    LayerFilterRenderer(const int& positionLocation, const int& texCoordLocation);
    void bindCommonAttribLocation(int location, const char* attribName);
    bool initializeSharedGLObjects();

    // See note about ping-ponging in applyActions()
    void ping(LayerRendererSurface*);
    void pong(LayerRendererSurface*);

    // This is for shadows, where we need to create a shadow, and then repaint the original image
    // on top of the shadow.
    void pushSnapshot(LayerRendererSurface*, int sourceId);
    void popSnapshot();

    bool m_enabled;

    // ESSL attributes shared with LayerRenderer - see constructor:
    const int m_positionLocation;
    const int m_texCoordLocation;

    // ESSL program object IDs:
    unsigned m_cssFilterProgramObject[LayerData::NumberOfCSSFilterShaders];

    // ESSL uniform locations:
    int m_amountLocation[LayerData::NumberOfCSSFilterShaders];
    int m_blurAmountLocation[2]; // 0 = Y, 1 = X
    int m_shadowColorLocation;
    int m_offsetLocation;

    // Textures for playing ping-pong - see note in applyActions()
    RefPtr<Texture> m_texture;
    RefPtr<Texture> m_snapshotTexture;

    friend class LayerFilterRendererAction;
};

}

#endif // USE(ACCELERATED_COMPOSITING) && ENABLE(CSS_FILTERS)

#endif // LayerFilterRenderer_h
