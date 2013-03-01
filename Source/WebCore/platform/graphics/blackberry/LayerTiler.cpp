/*
 * Copyright (C) 2011, 2012, 2013 Research In Motion Limited. All rights reserved.
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

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)

#include "LayerTiler.h"

#include "BitmapImage.h"
#include "LayerCompositingThread.h"
#include "LayerMessage.h"
#include "LayerWebKitThread.h"
#include "TextureCacheCompositingThread.h"

#include <BlackBerryPlatformGLES2Program.h>
#include <BlackBerryPlatformScreen.h>
#include <BlackBerryPlatformSettings.h>
#include <GLES2/gl2.h>

using namespace std;
using BlackBerry::Platform::Graphics::GLES2Program;

namespace WebCore {

// This is used to make the viewport as used in texture visibility calculations
// slightly larger so textures are uploaded before becoming really visible.
// Inflation is greater in Y direction (one screenful in either direction) since
// scrolling vertically is more common, and easily reaches higher velocity, than
// scrolling horizontally.
// The viewportInflation is expressed as a fraction to multiply the viewport by, and
// it is centered.
// A height of 3 means to consider one screenful above and one screenful below the
// current viewport as visible.
const FloatSize viewportInflation(1.2f, 3.0f);

static void transformPoint(float x, float y, const TransformationMatrix& m, float* result)
{
    // Squash the Z coordinate so that layers aren't clipped against the near and
    // far plane. Note that the perspective is maintained as we're still passing
    // down the W coordinate.
    result[0] = x * m.m11() + y * m.m21() + m.m41();
    result[1] = x * m.m12() + y * m.m22() + m.m42();
    result[2] = 0;
    result[3] = x * m.m14() + y * m.m24() + m.m44();
}

static IntSize defaultTileSize()
{
    static IntSize size(BlackBerry::Platform::Settings::instance()->tileSize());
    return size;
}

LayerTiler::LayerTiler(LayerWebKitThread* layer)
    : m_layer(layer)
    , m_needsRenderCount(0)
    , m_needsBacking(false)
    , m_contentsDirty(false)
    , m_tileSize(defaultTileSize())
    , m_clearTextureJobs(false)
    , m_contentsScale(0.0)
{
    ref(); // This ref() is matched by a deref in layerCompositingThreadDestroyed();
}

LayerTiler::~LayerTiler()
{
    // Someone should have called LayerTiler::deleteTextures()
    // before now. We can't call it here because we have no
    // OpenGL context.
    ASSERT(m_tiles.isEmpty());
}

void LayerTiler::layerWebKitThreadDestroyed()
{
    m_layer = 0;
}

void LayerTiler::layerCompositingThreadDestroyed(LayerCompositingThread*)
{
    ASSERT(isCompositingThread());
    deref(); // Matched by ref() in constructor;
}

void LayerTiler::setNeedsDisplay(const FloatRect& dirtyRect)
{
    m_dirtyRect.unite(dirtyRect);
    m_contentsDirty = true;
}

void LayerTiler::setNeedsDisplay()
{
    m_dirtyRect.setLocation(FloatPoint::zero());
    m_dirtyRect.setSize(m_layer->bounds());
    m_contentsDirty = true;
}

void LayerTiler::updateTextureContentsIfNeeded(double scale)
{
    updateTileSize();

    // Check if update is needed
    if (!m_contentsDirty && !needsRender())
        return;

    // There's no point in drawing contents at a higher resolution for scale
    // invariant layers.
    if (m_layer->sizeIsScaleInvariant())
        scale = 1.0;

    bool isZoomJob = false;
    if (scale != m_contentsScale) {
        // The first time around, it does not count as a zoom job.
        if (m_contentsScale)
            isZoomJob = true;
        m_contentsScale = scale;
    }

    IntRect dirtyRect = enclosingIntRect(m_dirtyRect);
    IntSize requiredTextureSize;

    if (m_layer->drawsContent()) {
        // Layer contents must be drawn into a canvas.
        IntRect untransformedDirtyRect(dirtyRect);
        IntRect boundsRect(IntPoint::zero(), m_layer->bounds());
        IntRect untransformedBoundsRect(boundsRect);
        requiredTextureSize = boundsRect.size();

        if (scale != 1.0) {
            TransformationMatrix matrix;
            matrix.scale(scale);

            dirtyRect = matrix.mapRect(untransformedDirtyRect);
            requiredTextureSize = matrix.mapRect(IntRect(IntPoint::zero(), requiredTextureSize)).size();
            boundsRect = matrix.mapRect(untransformedBoundsRect);
        }

        if (requiredTextureSize != m_pendingTextureSize)
            dirtyRect = boundsRect;
        else {
            // Clip the dirtyRect to the size of the layer to avoid drawing
            // outside the bounds of the backing texture.
            dirtyRect.intersect(boundsRect);
        }
    } else if (m_layer->contents()) {
        // Layer is a container, and it contains an Image.
        requiredTextureSize = m_layer->contents()->size();
        dirtyRect = IntRect(IntPoint::zero(), requiredTextureSize);
    }

    bool didResize = false;
    IntRect previousTextureRect(IntPoint::zero(), m_pendingTextureSize);
    if (m_pendingTextureSize != requiredTextureSize) {
        didResize = true;
        m_pendingTextureSize = requiredTextureSize;
        addTextureJob(TextureJob::resizeContents(m_pendingTextureSize));
    }

    IntRect rectForOneTile(IntPoint::zero(), tileSize());
    bool wasOneTile = rectForOneTile.contains(previousTextureRect);
    bool isOneTile = rectForOneTile.contains(IntRect(IntPoint::zero(), m_pendingTextureSize));

    HashSet<TileIndex> renderJobs;
    {
        MutexLocker locker(m_tilesMutex);

        // Add prefill jobs
        if (!dirtyRect.isEmpty()) {
            IntRect rect = BlackBerry::Platform::Settings::instance()->layerTilerPrefillRect();
            rect.intersect(dirtyRect);
            TileIndex begin = indexOfTile(rect.minXMinYCorner());
            TileIndex end = indexOfTile(rect.maxXMaxYCorner() + tileSize() - IntSize(1, 1));
            for (unsigned i = begin.i(); i < end.i(); ++i) {
                for (unsigned j = begin.j(); j < end.j(); ++j)
                    renderJobs.add(TileIndex(i, j));
            }
        }

        for (TileMap::iterator it = m_tiles.begin(); it != m_tiles.end(); ++it) {
            TileIndex index = (*it).key;
            LayerTile* tile = (*it).value;
            IntRect tileRect = rectForTile(index, requiredTextureSize);
            if (tileRect.isEmpty())
                continue;

            if (tile->needsRender()) {
                tile->setRenderPending();
                --m_needsRenderCount;
                ASSERT(m_needsRenderCount >= 0);
                renderJobs.add(index);
            } else if (m_contentsDirty) {
                IntRect localDirtyRect(dirtyRect);
                localDirtyRect.intersect(tileRect);
                if (localDirtyRect.isEmpty())
                    continue;

                // Render only visible tiles. Mask layers are a special case, because they're never considered
                // visible. Workaround this by always rendering them (a very fast operation with the
                // BlackBerry::Platform::GraphicsContext since it will just result in ref'ing the mask image).
                if (tile->isVisible() || m_layer->isMask()) {
                    renderJobs.add(index);
                    continue;
                }

                // Don't discard/dirty the tile if for some reason (e.g. prefill) a render job was already added.
                if (renderJobs.contains(index))
                    continue;

                // Avoid checkerboarding unless the layer is resized. When
                // resized, the contents are likely to change appearance, for
                // example due to aspect ratio change. However, if it is a
                // resize due to zooming, the aspect ratio and content will
                // stay the same, and we can keep the old texture content as a
                // preview.
                // FIXME: the zoom preview only works if we don't re-tile the
                // layer. We need to store texture coordinates in
                // WebCore::Texture to be able to fix that.
                if (didResize && !(isZoomJob && wasOneTile && isOneTile))
                    addTextureJob(TextureJob::discardContents(tileRect));
                else
                    addTextureJob(TextureJob::dirtyContents(tileRect));
            }
        }
    }

    // If we need display because we no longer need to be displayed, due to texture size becoming 0 x 0,
    // or if we're re-rendering the whole thing anyway, clear old texture jobs.
    if (requiredTextureSize.isEmpty() || dirtyRect == IntRect(IntPoint::zero(), requiredTextureSize))
        clearTextureJobs();

    if (renderJobs.isEmpty())
        return;

    if (Image* image = m_layer->contents()) {
        // If we need backing, we have no choice but to enforce the tile size, which could cause clipping.
        // Otherwise, don't clip - include the whole image.
        IntSize bufferSize = m_needsBacking ? tileSize() : image->size();
        if (BlackBerry::Platform::Graphics::Buffer* buffer = createBuffer(bufferSize)) {
            IntRect contentsRect(IntPoint::zero(), image->size());
            m_layer->paintContents(buffer, contentsRect, scale);

            bool isOpaque = false;
            if (image->isBitmapImage())
                isOpaque = !static_cast<BitmapImage*>(image)->currentFrameHasAlpha();
            addTextureJob(TextureJob::setContents(buffer, contentsRect, isOpaque));
        }
    } else if (m_layer->drawsContent()) {
        for (HashSet<TileIndex>::iterator it = renderJobs.begin(); it != renderJobs.end(); ++it) {
            if (BlackBerry::Platform::Graphics::Buffer* buffer = createBuffer(tileSize())) {
                IntRect tileRect = rectForTile(*it, requiredTextureSize);
                m_layer->paintContents(buffer, tileRect, scale);
                addTextureJob(TextureJob::updateContents(buffer, tileRect, m_layer->isOpaque()));
            }
        }
    }

    m_contentsDirty = false;
    m_dirtyRect = FloatRect();
}

BlackBerry::Platform::Graphics::Buffer* LayerTiler::createBuffer(const IntSize& size)
{
    BlackBerry::Platform::Graphics::BufferType bufferType = m_needsBacking ? BlackBerry::Platform::Graphics::AlwaysBacked : BlackBerry::Platform::Graphics::BackedWhenNecessary;
    BlackBerry::Platform::Graphics::Buffer* buffer = BlackBerry::Platform::Graphics::createBuffer(size, bufferType);
    return buffer;
}

void LayerTiler::addTextureJob(const TextureJob& job)
{
    m_pendingTextureJobs.append(job);
}

void LayerTiler::clearTextureJobs()
{
    // Clear any committed texture jobs on next invocation of LayerTiler::commitPendingTextureUploads().
    m_clearTextureJobs = true;

    removeUpdateContentsJobs(m_pendingTextureJobs);
}

static size_t backingSizeInBytes = 0;

void LayerTiler::willCommit()
{
    backingSizeInBytes = 0;
}

void LayerTiler::commitPendingTextureUploads(LayerCompositingThread*)
{
    if (m_clearTextureJobs) {
        removeUpdateContentsJobs(m_textureJobs);
        m_clearTextureJobs = false;
    }

    // There's no point in rendering more than the cache capacity during one frame.
    const size_t maxBackingPerFrame = textureCacheCompositingThread()->memoryLimit();

    for (Vector<TextureJob>::iterator it = m_pendingTextureJobs.begin(); it != m_pendingTextureJobs.end(); ++it) {
        TextureJob& textureJob = *it;

        // Update backing surface for backed tiles now, to avoid dropping frames during animation.
        if (textureJob.m_contents && backingSizeInBytes < maxBackingPerFrame) {
            BlackBerry::Platform::Graphics::updateBufferBackingSurface(textureJob.m_contents);
            backingSizeInBytes += BlackBerry::Platform::Graphics::bufferSizeInBytes(textureJob.m_contents);
        }

        m_textureJobs.append(textureJob);
    }
    m_pendingTextureJobs.clear();
}

void LayerTiler::layerVisibilityChanged(LayerCompositingThread*, bool visible)
{
    // For visible layers, we handle the tile-level visibility
    // in the draw loop, see LayerTiler::drawTextures().
    if (visible)
        return;

    {
        // All tiles are invisible now.
        MutexLocker locker(m_tilesMutex);

        for (TileMap::iterator it = m_tiles.begin(); it != m_tiles.end(); ++it) {
            TileIndex index = (*it).key;
            LayerTile* tile = (*it).value;
            tile->setVisible(false);
            if (tile->needsRender()) {
                tile->setNeedsRender(false);
                --m_needsRenderCount;
                ASSERT(m_needsRenderCount >= 0);
            }
        }
    }
}

void LayerTiler::uploadTexturesIfNeeded(LayerCompositingThread*)
{
    TileJobsMap tileJobsMap;
    Deque<TextureJob>::const_iterator textureJobIterEnd = m_textureJobs.end();
    for (Deque<TextureJob>::const_iterator textureJobIter = m_textureJobs.begin(); textureJobIter != textureJobIterEnd; ++textureJobIter)
        processTextureJob(*textureJobIter, tileJobsMap);

    TileJobsMap::const_iterator tileJobsIterEnd = tileJobsMap.end();
    for (TileJobsMap::const_iterator tileJobsIter = tileJobsMap.begin(); tileJobsIter != tileJobsIterEnd; ++tileJobsIter) {
        IntPoint origin = originOfTile(tileJobsIter->key);

        LayerTile* tile = m_tiles.get(tileJobsIter->key);
        if (!tile) {
            if (origin.x() >= m_requiredTextureSize.width() || origin.y() >= m_requiredTextureSize.height())
                continue;

            MutexLocker locker(m_tilesMutex);
            tile = new LayerTile();
            m_tiles.add(tileJobsIter->key, tile);
        }

        IntRect tileRect(origin, tileSize());
        tileRect.setWidth(min(m_requiredTextureSize.width() - tileRect.x(), tileRect.width()));
        tileRect.setHeight(min(m_requiredTextureSize.height() - tileRect.y(), tileRect.height()));

        performTileJob(tile, *tileJobsIter->value, tileRect);
    }

    m_textureJobs.clear();
}

void LayerTiler::processTextureJob(const TextureJob& job, TileJobsMap& tileJobsMap)
{
    if (job.m_type == TextureJob::ResizeContents) {
        IntSize pendingTextureSize = job.m_dirtyRect.size();
        if (pendingTextureSize.width() < m_requiredTextureSize.width() || pendingTextureSize.height() < m_requiredTextureSize.height())
            pruneTextures();

        m_requiredTextureSize = pendingTextureSize;
        return;
    }

    if (job.m_type == TextureJob::SetContentsToColor) {
        addTileJob(job.m_index, job, tileJobsMap);
        return;
    }

    addTileJob(indexOfTile(job.m_dirtyRect.minXMinYCorner()), job, tileJobsMap);
}

void LayerTiler::addTileJob(const TileIndex& index, const TextureJob& job, TileJobsMap& tileJobsMap)
{
    // HashMap::add always returns a valid iterator even the key already exists.
    TileJobsMap::AddResult result = tileJobsMap.add(index, &job);

    // Successfully added the new job.
    if (result.isNewEntry)
        return;

    // In this case we leave the previous job.
    if (job.m_type == TextureJob::DirtyContents && result.iterator->value->m_type == TextureJob::DiscardContents)
        return;

    // Override the previous job.
    if (result.iterator->value && result.iterator->value->m_contents)
        BlackBerry::Platform::Graphics::destroyBuffer(result.iterator->value->m_contents);

    result.iterator->value = &job;
}

void LayerTiler::performTileJob(LayerTile* tile, const TextureJob& job, const IntRect& tileRect)
{
    switch (job.m_type) {
    case TextureJob::SetContentsToColor:
        tile->setContentsToColor(job.m_color);
        return;
    case TextureJob::SetContents:
        tile->setContents(job.m_contents, tileRect, indexOfTile(tileRect.location()), job.m_isOpaque);
        {
            MutexLocker locker(m_tilesMutex);
            if (tile->renderState() == LayerTile::RenderPending)
                tile->setRenderDone();
        }
        return;
    case TextureJob::UpdateContents:
        tile->updateContents(job.m_contents, job.m_dirtyRect, tileRect, job.m_isOpaque);
        {
            MutexLocker locker(m_tilesMutex);
            if (tile->renderState() == LayerTile::RenderPending)
                tile->setRenderDone();
        }
        return;
    case TextureJob::DiscardContents:
        tile->discardContents();
        return;
    case TextureJob::DirtyContents:
        tile->setContentsDirty();
        return;
    case TextureJob::Unknown:
    case TextureJob::ResizeContents:
        ASSERT_NOT_REACHED();
        return;
    }
    ASSERT_NOT_REACHED();
}

bool LayerTiler::drawTile(LayerCompositingThread* layer, double scale, const TileIndex& index, const FloatRect& dst, const GLES2Program& program)
{
    TransformationMatrix drawTransform = layer->drawTransform();
    float vertices[4 * 4];

    LayerTile* tile = m_tiles.get(index);
    if (!tile) {
        MutexLocker locker(m_tilesMutex);
        tile = new LayerTile();
        m_tiles.add(index, tile);
    }

    // We apply the transformation by hand, since we need the z coordinate
    // as well (to do perspective correct texturing) and we don't need
    // to divide by w by hand, the GPU will do that for us
    transformPoint(dst.x(), dst.y(), drawTransform, &vertices[0]);
    transformPoint(dst.x(), dst.maxY(), drawTransform, &vertices[4]);
    transformPoint(dst.maxX(), dst.maxY(), drawTransform, &vertices[8]);
    transformPoint(dst.maxX(), dst.y(), drawTransform, &vertices[12]);

    // Inflate the rect somewhat to attempt to make textures render before they show
    // up on screen. As you can see, the rect is in normalized device coordinates.
    FloatRect rect(-viewportInflation.width(), -viewportInflation.height(), 2*viewportInflation.width(), 2*viewportInflation.height());
    FloatQuad quad(
        FloatPoint(vertices[0] / vertices[3], vertices[1] / vertices[3]),
        FloatPoint(vertices[4] / vertices[7], vertices[5] / vertices[7]),
        FloatPoint(vertices[8] / vertices[11], vertices[9] / vertices[11]),
        FloatPoint(vertices[12] / vertices[15], vertices[13] / vertices[15]));
    FloatRect boundingBox = quad.boundingBox();
    bool visible = boundingBox.intersects(rect);
    FloatRect viewport(-1, -1, 2, 2);
    unsigned char globalAlpha = static_cast<unsigned char>(layer->drawOpacity() * 255.0f);
    bool shouldDrawTile = boundingBox.intersects(viewport) && globalAlpha;

    bool wasVisible = tile->isVisible();

    if (visible) {
        if (tile->hasTexture()) {
            Texture* texture = tile->texture();
            textureCacheCompositingThread()->textureAccessed(texture);

            if (shouldDrawTile) {
                drawTransform.translate(dst.x(), dst.y());
                if (layer->contentsResolutionIndependent())
                    drawTransform.scaleNonUniform(dst.width() / m_requiredTextureSize.width(), dst.height() / m_requiredTextureSize.height());
                else
                    drawTransform.scale(1.0 / layer->contentsScale());
                drawTransform.scale(layer->sizeIsScaleInvariant() ? 1.0 / scale : 1.0);
                blitToBuffer(0, texture->textureId(), reinterpret_cast<BlackBerry::Platform::TransformationMatrix&>(drawTransform),
                    BlackBerry::Platform::Graphics::SourceOver, globalAlpha);
            }
        }

        if (tile->isDirty()) {
            MutexLocker locker(m_tilesMutex);
            tile->setVisible(visible);
            if (tile->renderState() == LayerTile::DoesNotNeedRender) {
                tile->setNeedsRender(true);
                ++m_needsRenderCount;
            }
            return true;
        }

        if (!wasVisible) {
            MutexLocker locker(m_tilesMutex);
            tile->setVisible(visible);
        }
    } else if (wasVisible) {
        MutexLocker locker(m_tilesMutex);
        tile->setVisible(visible);
        if (tile->needsRender()) {
            tile->setNeedsRender(false);
            --m_needsRenderCount;
            ASSERT(m_needsRenderCount >= 0);
        }
    }

    return false;
}

void LayerTiler::drawTextures(LayerCompositingThread* layer, double scale, const GLES2Program& program)
{
    FloatSize bounds = layer->bounds();

    if (layer->sizeIsScaleInvariant()) {
        bounds.setWidth(bounds.width() / scale);
        bounds.setHeight(bounds.height() / scale);
    }

    bool needsDisplay = false;

    if (layer->contentsResolutionIndependent()) {
        // Resolution independent layers have all the needed data in the first tile and all the others are useless.
        FloatRect dst(-bounds.width() / 2.0, -bounds.height() / 2.0, bounds.width(), bounds.height());
        needsDisplay = drawTile(layer, scale, TileIndex(0, 0), dst, program);
    } else {
        float sx = static_cast<float>(bounds.width()) / m_requiredTextureSize.width();
        float sy = static_cast<float>(bounds.height()) / m_requiredTextureSize.height();

        IntRect src(IntPoint::zero(), tileSize());
        for (src.setX(0); src.x() < m_requiredTextureSize.width(); src.setX(src.x() + src.width())) {
            for (src.setY(0); src.y() < m_requiredTextureSize.height(); src.setY(src.y() + src.height())) {
                TileIndex index = indexOfTile(src.location());

                float x = index.i() * src.width() * sx;
                float y = index.j() * src.height() * sy;
                FloatRect dst(x - bounds.width() / 2.0, y - bounds.height() / 2.0, min(bounds.width() - x, src.width() * sx), min(bounds.height() - y, src.height() * sy));

                needsDisplay |= drawTile(layer, scale, index, dst, program);
            }
        }
    }

    // If we schedule a commit, visibility will be updated, and display will
    // happen if there are any visible and dirty textures.
    if (needsDisplay)
        layer->setNeedsCommit();
}

void LayerTiler::deleteTextures(LayerCompositingThread*)
{
    // Since textures are deleted by a synchronous message
    // from WebKit thread to compositing thread, we don't need
    // any synchronization mechanism here, even though we are
    // touching some WebKit thread state.
    if (m_tiles.size()) {
        for (TileMap::iterator it = m_tiles.begin(); it != m_tiles.end(); ++it)
            (*it).value->discardContents();
        m_tiles.clear();

        m_contentsDirty = true;
    }

    // For various reasons, e.g. page cache, someone may try
    // to render us after the textures were deleted.
    m_pendingTextureSize = IntSize();
    m_requiredTextureSize = IntSize();
}

void LayerTiler::pruneTextures()
{
    // Prune tiles that are no longer needed.
    Vector<TileIndex> tilesToDelete;
    for (TileMap::iterator it = m_tiles.begin(); it != m_tiles.end(); ++it) {
        TileIndex index = (*it).key;

        IntPoint origin = originOfTile(index);
        if (origin.x() >= m_requiredTextureSize.width() || origin.y() >= m_requiredTextureSize.height())
            tilesToDelete.append(index);
    }

    {
        MutexLocker locker(m_tilesMutex);

        for (Vector<TileIndex>::iterator it = tilesToDelete.begin(); it != tilesToDelete.end(); ++it) {
            LayerTile* tile = m_tiles.take(*it);
            tile->discardContents();
            delete tile;
        }
    }
}

void LayerTiler::updateTileSize()
{
    IntSize size = m_layer->isMask() ? m_layer->bounds() : defaultTileSize();
    const IntSize maxTextureSize(2048, 2048);
    size = size.shrunkTo(maxTextureSize);

    if (m_tileSize == size || size.isEmpty())
        return;

    // Invalidate the whole layer if tile size changes.
    setNeedsDisplay();
    m_tileSize = size;
}

void LayerTiler::setNeedsBacking(bool needsBacking)
{
    if (m_needsBacking == needsBacking)
        return;

    m_needsBacking = needsBacking;
    updateTileSize();
}

void LayerTiler::scheduleCommit()
{
    ASSERT(isWebKitThread());

    if (m_layer)
        m_layer->setNeedsCommit();
}

TileIndex LayerTiler::indexOfTile(const WebCore::IntPoint& origin)
{
    int offsetX = origin.x();
    int offsetY = origin.y();
    if (offsetX)
        offsetX = offsetX / tileSize().width();
    if (offsetY)
        offsetY = offsetY / tileSize().height();
    return TileIndex(offsetX, offsetY);
}

IntPoint LayerTiler::originOfTile(const TileIndex& index)
{
    return IntPoint(index.i() * tileSize().width(), index.j() * tileSize().height());
}

IntRect LayerTiler::rectForTile(const TileIndex& index, const IntSize& bounds)
{
    IntPoint origin = originOfTile(index);
    IntSize offset(origin.x(), origin.y());
    IntSize size = tileSize().shrunkTo(bounds - offset);
    return IntRect(origin, size);
}

Texture* LayerTiler::contentsTexture(LayerCompositingThread*)
{
    ASSERT(m_tiles.size() == 1);
    if (m_tiles.size() != 1)
        return 0;

    const LayerTile* tile = m_tiles.begin()->value;

    ASSERT(tile->hasTexture());
    if (!tile->hasTexture())
        return 0;

    return tile->texture();
}

} // namespace WebCore

#endif
