/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TransparencyWin_h
#define TransparencyWin_h

#include <windows.h>

#include "AffineTransform.h"
#include "ImageBuffer.h"
#include "Noncopyable.h"
#include "wtf/OwnPtr.h"

class SkBitmap;
class SkCanvas;

namespace WebCore {

class GraphicsContext;
class TransparencyWin_NoLayer_Test;
class TransparencyWin_WhiteLayer_Test;
class TransparencyWin_TextComposite_Test;
class TransparencyWin_OpaqueCompositeLayer_Test;

// Helper class that abstracts away drawing ClearType text and Windows form
// controls either to the original context directly, or to an offscreen context
// that is composited later manually. This is to get around Windows' inability
// to handle the alpha channel, semitransparent text, and transformed form
// controls.
class TransparencyWin : public Noncopyable {
public:
    enum LayerMode {
        // No extra layer is created. Drawing will happen to the source.
        // Valid only with KeepTransform and ScaleTransform. The region being
        // drawn onto must be opaque, since the modified region will be forced
        // to opaque when drawing is complete.
        NoLayer,

        // Makes a temporary layer consisting of the composited layers below
        // it. This result must be opaque. When complete, the result will be
        // compared to the original, and the difference will be added to a thee
        // destination layer.
        //
        // This mode only works if the lower layers are opque (normally the
        // case for a web page) and layers are only drawn in the stack order,
        // meaning you can never draw underneath a layer.
        //
        // This doesn't technically produce the correct answer in all cases. If
        // you have an opaque base, a transparency layer, than a semitransparent
        // drawing on top, the result will actually be blended in twice. But
        // this isn't a very important case. This mode is used for form
        // controls which are always opaque except for occationally some
        // antialiasing. It means form control antialiasing will be too light in
        // some cases, but only if you have extra layers.
        OpaqueCompositeLayer,

        // Allows semitransparent text to be drawn on any background (even if it
        // is itself semitransparent), but disables ClearType.
        //
        // It makes a trmporary layer filled with white. This is composited with
        // the lower layer with a custom color applied to produce the result.
        // The caller must draw the text in black, and set the desired final
        // text color by calling setTextCompositeColor().
        //
        // Only valid with KeepTransform, which is the only mode where drawing
        // text in this fashion makes sense.
        TextComposite,

        // Makes a temporary layer filled with white. When complete, the layer
        // will be forced to be opqaue (since Windows may have messed up the
        // alpha channel) and composited down. Any areas not drawn into will
        // remain white.
        //
        // This is the mode of last resort. If the opacity of the final image
        // is unknown and we can't do the text trick (since we know its color),
        // then we have to live with potential white halos. This is used for
        // form control drawing, for example.
        WhiteLayer,
    };

    enum TransformMode {
        // There are no changes to the transform. Use this when drawing
        // horizontal text. The current transform must not have rotation.
        KeepTransform,

        // Drawing happens in an Untransformed space, and then that bitmap is
        // transformed according to the current context when it is copied down.
        // Requires that a layer be created (layer mode is not NoLayer).
        Untransform,

        // When the current transform only has a scaling factor applied and
        // you're drawing form elements, use this parameter. This will unscale
        // the coordinate space, so the OS will just draw the form controls
        // larger or smaller depending on the destination size.
        ScaleTransform,
    };

    // You MUST call init() below.
    // |region| is expressed relative to the current transformation.
    TransparencyWin();
    ~TransparencyWin();

    // Initializes the members if you use the 0-argument constructor. Don't call
    // this if you use the multiple-argument constructor.
    void init(GraphicsContext* dest,
              LayerMode layerMode,
              TransformMode transformMode,
              const IntRect& region);

    // Combines the source and destination bitmaps using the given mode.
    // Calling this function before the destructor runs is mandatory in most
    // cases, and harmless otherwise.  The mandatory cases are:
    //       (m_layerMode != NoLayer) || (m_transformMode == ScaleTransform)
    void composite();

    // Returns the context for drawing into, which may be the destination
    // context, or a temporary one.
    GraphicsContext* context() const { return m_drawContext; }

    PlatformGraphicsContext* platformContext() const { return m_drawContext ? m_drawContext->platformContext() : 0; }

    // When the mode is TextComposite, this sets the color that the text will
    // get. See the enum above for more.
    void setTextCompositeColor(Color color);

    // Returns the input bounds translated into the destination space. This is
    // not necessary for KeepTransform since the rectangle will be unchanged.
    const IntRect& drawRect() { return m_drawRect; }

private:
    friend TransparencyWin_NoLayer_Test;
    friend TransparencyWin_WhiteLayer_Test;
    friend TransparencyWin_TextComposite_Test;
    friend TransparencyWin_OpaqueCompositeLayer_Test;

    class OwnedBuffers;

    void computeLayerSize();

    // Sets up a new layer, if any. setupLayer() will call the appopriate layer-
    // specific helper. Must be called after computeLayerSize();
    void setupLayer();
    void setupLayerForNoLayer();
    void setupLayerForOpaqueCompositeLayer();
    void setupLayerForTextComposite();
    void setupLayerForWhiteLayer();

    // Sets up the transformation on the newly created layer. setupTransform()
    // will call the appropriate transform-specific helper. Must be called after
    // setupLayer().
    void setupTransform(const IntRect& region);
    void setupTransformForKeepTransform(const IntRect& region);
    void setupTransformForUntransform();
    void setupTransformForScaleTransform();

    void initializeNewContext();

    void compositeOpaqueComposite();
    void compositeTextComposite();

    // Fixes the alpha channel to make the region inside m_transformedRect
    // opaque.
    void makeLayerOpaque();

    // The context our drawing will eventually end up in.
    GraphicsContext* m_destContext;

    // The original transform from the destination context.
    AffineTransform m_orgTransform;

    LayerMode m_layerMode;
    TransformMode m_transformMode;

    // The rectangle we're drawing in the destination's coordinate space
    IntRect m_sourceRect;

    // The source rectangle transformed into pixels in the final image. For
    // Untransform this has no meaning, since the destination might not be a
    // rectangle.
    IntRect m_transformedSourceRect;

    // The size of the layer we created. If there's no layer, this is the size
    // of the region we're using in the source.
    IntSize m_layerSize;

    // The rectangle we're drawing to in the draw context's coordinate space.
    // This will be the same as the source rectangle except for ScaleTransform
    // where we create a new virtual coordinate space for the layer.
    IntRect m_drawRect;

    // Points to the graphics context to draw text to, which will either be
    // the original context or the copy, depending on our mode.
    GraphicsContext* m_drawContext;

    // This flag is set when we call save() on the draw context during
    // initialization. It allows us to avoid doing an extra save()/restore()
    // when one is unnecessary.
    bool m_savedOnDrawContext;

    // Used only when m_mode = TextComposite, this is the color that the text
    // will end up being once we figure out the transparency.
    Color m_textCompositeColor;

    // Layer we're drawing to.
    ImageBuffer* m_layerBuffer;

    // When the layer type is OpaqueCompositeLayer, this will contain a copy
    // of the original contents of the m_layerBuffer before Windows drew on it.
    // It allows us to re-create what Windows did to the layer. It is an
    // SkBitmap instead of an ImageBuffer because an SkBitmap is lighter-weight
    // (ImageBuffers are also GDI surfaces, which we don't need here).
    SkBitmap* m_referenceBitmap;

    // If the given size of bitmap can be cached, they will be stored here. Both
    // the bitmap and the reference are guaranteed to be allocated if this
    // member is non-null.
    static OwnedBuffers* m_cachedBuffers;

    // If a buffer was too big to be cached, it will be created temporarily, and
    // this member tracks its scope to make sure it gets deleted. Always use
    // m_layerBuffer, which will either point to this object, or the statically
    // cached one. Don't access directly.
    OwnPtr<OwnedBuffers> m_ownedBuffers;

    // Sometimes we're asked to create layers that have negative dimensions.
    // This API is not designed to fail to initialize, so we hide the fact 
    // that they are illegal and can't be rendered (failing silently, drawing
    // nothing).
    bool m_validLayer;
};

} // namespace WebCore

#endif // TransaprencyWin_h
