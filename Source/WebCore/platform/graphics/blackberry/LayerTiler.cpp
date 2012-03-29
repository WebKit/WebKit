/*
 * Copyright (C) 2011, 2012 Research In Motion Limited. All rights reserved.
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
#include "NativeImageSkia.h"
#include "TextureCacheCompositingThread.h"

#include <BlackBerryPlatformScreen.h>
#include <GLES2/gl2.h>

using namespace std;

namespace WebCore {

// This is used to make the viewport as used in texture visibility calculations
// slightly larger so textures are uploaded before becoming really visible.
const float viewportInflationFactor = 1.1f;

// The tileMultiplier indicates how many tiles will fit in the largest dimension
// of the screen, if drawn using identity transform.
// We use half the screen size as tile size, to reduce the texture upload time
// for small repaint rects. Compared to using screen size directly, this should
// make most small invalidations 16x faster, unless they're unfortunate enough
// to intersect two or more tiles, where it would be 8x-1x faster.
const int tileMultiplier = 4;

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
    static IntSize screenSize = BlackBerry::Platform::Graphics::Screen::primaryScreen()->nativeSize();
    static int dim = max(screenSize.width(), screenSize.height()) / tileMultiplier;
    return IntSize(dim, dim);
}

LayerTiler::LayerTiler(LayerWebKitThread* layer)
    : m_layer(layer)
    , m_tilingDisabled(false)
    , m_contentsDirty(false)
    , m_tileSize(defaultTileSize())
    , m_clearTextureJobs(false)
    , m_hasMissingTextures(false)
    , m_contentsScale(0.0)
{
}

LayerTiler::~LayerTiler()
{
    // Someone should have called LayerTiler::deleteTextures()
    // before now. We can't call it here because we have no
    // OpenGL context.
    ASSERT(m_tilesCompositingThread.isEmpty());
}

void LayerTiler::layerWebKitThreadDestroyed()
{
    m_layer = 0;
}

void LayerTiler::layerCompositingThreadDestroyed()
{
    ASSERT(isCompositingThread());
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

    HashSet<TileIndex> renderJobs;
    {
        MutexLocker locker(m_renderJobsMutex);
        if (!m_contentsDirty && m_renderJobs.isEmpty())
            return;
        renderJobs = m_renderJobs;
    }

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

    // If we need display because we no longer need to be displayed, due to texture size becoming 0 x 0,
    // or if we're re-rendering the whole thing anyway, clear old texture jobs.
    HashSet<TileIndex> finishedJobs;
    if (requiredTextureSize.isEmpty() || dirtyRect == IntRect(IntPoint::zero(), requiredTextureSize)) {
        {
            MutexLocker locker(m_renderJobsMutex);
            m_renderJobs.clear();
        }
        clearTextureJobs();
    } else if (!renderJobs.isEmpty()) {
        if (Image* image = m_layer->contents()) {
            bool isOpaque = false;
            if (image->isBitmapImage())
                isOpaque = !static_cast<BitmapImage*>(image)->currentFrameHasAlpha();
            if (NativeImagePtr nativeImage = image->nativeImageForCurrentFrame()) {
                SkBitmap bitmap = SkBitmap(nativeImage->bitmap());
                addTextureJob(TextureJob::setContents(bitmap, isOpaque));
            }
        } else {
            // There might still be some pending render jobs due to visibility changes.
            for (HashSet<TileIndex>::iterator it = renderJobs.begin(); it != renderJobs.end(); ++it) {
                {
                    // Check if the job has been cancelled.
                    MutexLocker locker(m_renderJobsMutex);
                    if (!m_renderJobs.contains(*it))
                        continue;
                    m_renderJobs.remove(*it);
                }

                IntRect tileRect = rectForTile(*it, requiredTextureSize);
                if (tileRect.isEmpty())
                    continue;

                bool isSolidColor = false;
                Color color;
                SkBitmap bitmap = m_layer->paintContents(tileRect, scale, &isSolidColor, &color);
                // bitmap can be null here. Make requiredTextureSize empty so that we
                // will not try to update and draw the layer.
                if (!bitmap.isNull()) {
                    if (isSolidColor)
                        addTextureJob(TextureJob::setContentsToColor(color, *it));
                    else
                        addTextureJob(TextureJob::updateContents(bitmap, tileRect, m_layer->isOpaque()));
                }

                finishedJobs.add(*it);
            }
        }
    }

    bool didResize = false;
    if (m_pendingTextureSize != requiredTextureSize) {
        didResize = true;
        m_pendingTextureSize = requiredTextureSize;
        addTextureJob(TextureJob::resizeContents(m_pendingTextureSize));
    }
    m_contentsDirty = false;
    m_dirtyRect = FloatRect();

    if (dirtyRect.isEmpty() || requiredTextureSize.isEmpty())
        return;

    if (Image* image = m_layer->contents()) {
        bool isOpaque = false;
        if (image->isBitmapImage())
            isOpaque = !static_cast<BitmapImage*>(image)->currentFrameHasAlpha();
        // No point in tiling an image layer, the image is already stored as an SkBitmap
        NativeImagePtr nativeImage = m_layer->contents()->nativeImageForCurrentFrame();
        if (nativeImage) {
            SkBitmap bitmap = SkBitmap(nativeImage->bitmap());
            addTextureJob(TextureJob::setContents(bitmap, isOpaque));
        }
        return;
    }

    IntPoint topLeft = dirtyRect.minXMinYCorner();
    IntPoint bottomRight = dirtyRect.maxXMaxYCorner(); // This is actually a pixel below and to the right of the dirtyRect.

    IntSize tileMaximumSize = tileSize();
    bool wasOneTile = m_tilesWebKitThread.size() == 1;
    bool isOneTile = m_pendingTextureSize.width() <= tileMaximumSize.width() && m_pendingTextureSize.height() <= tileMaximumSize.height();
    IntPoint origin = originOfTile(indexOfTile(topLeft));
    IntRect tileRect;
    for (tileRect.setX(origin.x()); tileRect.x() < bottomRight.x(); tileRect.setX(tileRect.x() + tileMaximumSize.width())) {
        for (tileRect.setY(origin.y()); tileRect.y() < bottomRight.y(); tileRect.setY(tileRect.y() + tileMaximumSize.height())) {
            tileRect.setWidth(min(requiredTextureSize.width() - tileRect.x(), tileMaximumSize.width()));
            tileRect.setHeight(min(requiredTextureSize.height() - tileRect.y(), tileMaximumSize.height()));

            IntRect localDirtyRect(dirtyRect);
            localDirtyRect.intersect(tileRect);
            if (localDirtyRect.isEmpty())
                continue;

            TileIndex index = indexOfTile(tileRect.location());

            // If we already did this as part of one of the render jobs due to
            // visibility changes, don't render that tile again.
            if (finishedJobs.contains(index))
                continue;

            if (!shouldPerformRenderJob(index, !isZoomJob)) {
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
                continue;
            }

            // Just in case a new job for this tile has just been inserted by compositing thread.
            removeRenderJob(index);

            // FIXME: We are always drawing whole tiles at the moment, because
            // we currently can't keep track of which part of a tile is
            // rendered and which is not. Sending only a subrectangle of a tile
            // to the compositing thread might cause it to be uploaded using
            // glTexImage, if the texture was previously evicted from the cache.
            // The result would be that a subrectangle of the tile was stretched
            // to fit the tile geometry, appearing as a glaring misrendering of
            // the web page.
            bool isSolidColor = false;
            Color color;
            SkBitmap bitmap = m_layer->paintContents(tileRect, scale, &isSolidColor, &color);
            // bitmap can be null here. Make requiredTextureSize empty so that we
            // will not try to update and draw the layer.
            if (!bitmap.isNull()) {
                if (isSolidColor)
                    addTextureJob(TextureJob::setContentsToColor(color, index));
                else
                    addTextureJob(TextureJob::updateContents(bitmap, tileRect, m_layer->isOpaque()));
            }
        }
    }
}

bool LayerTiler::shouldPerformRenderJob(const TileIndex& index, bool allowPrefill)
{
    // If the visibility information was propagated from the compositing
    // thread, use that information.
    // However, we are about to commit new layer properties that may make
    // currently hidden layers visible. To avoid false negatives, we only allow
    // the current state to be used to accept render jobs, not to reject them.
    if (m_tilesWebKitThread.get(index).isVisible())
        return true;

    // Tiles that are not currently visible on the compositing thread may still
    // deserve to be rendered if they should be prefilled...
    if (allowPrefill && !m_tilesWebKitThread.contains(index) && shouldPrefillTile(index))
        return true;

    // Or if they are visible according to the state that's about to be
    // committed. We do a visibility test using the current transform state.
    IntRect contentRect = rectForTile(index, m_pendingTextureSize);
    return m_layer->contentsVisible(contentRect);
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

void LayerTiler::commitPendingTextureUploads()
{
    if (m_clearTextureJobs) {
        removeUpdateContentsJobs(m_textureJobs);
        m_clearTextureJobs = false;
    }

    for (Vector<TextureJob>::iterator it = m_pendingTextureJobs.begin(); it != m_pendingTextureJobs.end(); ++it)
        m_textureJobs.append(*it);
    m_pendingTextureJobs.clear();
}

void LayerTiler::layerVisibilityChanged(bool visible)
{
    // For visible layers, we handle the tile-level visibility
    // in the draw loop, see LayerTiler::drawTextures().
    if (visible)
        return;

    {
        // All tiles are invisible now.
        MutexLocker locker(m_renderJobsMutex);
        m_renderJobs.clear();
    }

    for (TileMap::iterator it = m_tilesCompositingThread.begin(); it != m_tilesCompositingThread.end(); ++it) {
        TileIndex index = (*it).first;
        LayerTile* tile = (*it).second;
        tile->setVisible(false);
    }
}

void LayerTiler::uploadTexturesIfNeeded()
{
    TileJobsMap tileJobsMap;
    Deque<TextureJob>::const_iterator textureJobIterEnd = m_textureJobs.end();
    for (Deque<TextureJob>::const_iterator textureJobIter = m_textureJobs.begin(); textureJobIter != textureJobIterEnd; ++textureJobIter)
        processTextureJob(*textureJobIter, tileJobsMap);

    TileJobsMap::const_iterator tileJobsIterEnd = tileJobsMap.end();
    for (TileJobsMap::const_iterator tileJobsIter = tileJobsMap.begin(); tileJobsIter != tileJobsIterEnd; ++tileJobsIter) {
        IntPoint origin = originOfTile(tileJobsIter->first);

        LayerTile* tile = m_tilesCompositingThread.get(tileJobsIter->first);
        if (!tile) {
            if (origin.x() >= m_requiredTextureSize.width() || origin.y() >= m_requiredTextureSize.height())
                continue;
            tile = new LayerTile();
            m_tilesCompositingThread.add(tileJobsIter->first, tile);
        }

        IntRect tileRect(origin, tileSize());
        tileRect.setWidth(min(m_requiredTextureSize.width() - tileRect.x(), tileRect.width()));
        tileRect.setHeight(min(m_requiredTextureSize.height() - tileRect.y(), tileRect.height()));

        performTileJob(tile, *tileJobsIter->second, tileRect);
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

    IntSize tileMaximumSize = tileSize();
    IntPoint topLeft = job.m_dirtyRect.minXMinYCorner();
    IntPoint bottomRight = job.m_dirtyRect.maxXMaxYCorner(); // This is actually a pixel below and to the right of the dirtyRect.
    IntPoint origin = originOfTile(indexOfTile(topLeft));
    IntRect tileRect;
    for (tileRect.setX(origin.x()); tileRect.x() < bottomRight.x(); tileRect.setX(tileRect.x() + tileMaximumSize.width())) {
        for (tileRect.setY(origin.y()); tileRect.y() < bottomRight.y(); tileRect.setY(tileRect.y() + tileMaximumSize.height()))
            addTileJob(indexOfTile(tileRect.location()), job, tileJobsMap);
    }
}

void LayerTiler::addTileJob(const TileIndex& index, const TextureJob& job, TileJobsMap& tileJobsMap)
{
    // HashMap::add always returns a valid iterator even the key already exists.
    TileJobsMap::AddResult result = tileJobsMap.add(index, &job);

    // Successfully added the new job.
    if (result.isNewEntry)
        return;

    // In this case we leave the previous job.
    if (job.m_type == TextureJob::DirtyContents && result.iterator->second->m_type == TextureJob::DiscardContents)
        return;

    // Override the previous job.
    result.iterator->second = &job;
}

void LayerTiler::performTileJob(LayerTile* tile, const TextureJob& job, const IntRect& tileRect)
{
    switch (job.m_type) {
    case TextureJob::SetContentsToColor:
        tile->setContentsToColor(job.m_color);
        return;
    case TextureJob::SetContents:
        tile->setContents(job.m_contents, tileRect, indexOfTile(tileRect.location()), job.m_isOpaque);
        return;
    case TextureJob::UpdateContents:
        tile->updateContents(job.m_contents, job.m_dirtyRect, tileRect, job.m_isOpaque);
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

void LayerTiler::drawTextures(LayerCompositingThread* layer, int pos, int texCoord)
{
    drawTexturesInternal(layer, pos, texCoord, false /* drawMissing */);
}

void LayerTiler::drawMissingTextures(LayerCompositingThread* layer, int pos, int texCoord)
{
    drawTexturesInternal(layer, pos, texCoord, true /* drawMissing */);
}

void LayerTiler::drawTexturesInternal(LayerCompositingThread* layer, int positionLocation, int texCoordLocation, bool drawMissing)
{
    const TransformationMatrix& drawTransform = layer->drawTransform();
    IntSize bounds = layer->bounds();

    float texcoords[4 * 2] = { 0, 0,  0, 1,  1, 1,  1, 0 };
    float vertices[4 * 4];

    glVertexAttribPointer(positionLocation, 4, GL_FLOAT, GL_FALSE, 0, vertices);
    glVertexAttribPointer(texCoordLocation, 2, GL_FLOAT, GL_FALSE, 0, texcoords);

    m_hasMissingTextures = false;

    int maxw = tileSize().width();
    int maxh = tileSize().height();
    float sx = static_cast<float>(bounds.width()) / m_requiredTextureSize.width();
    float sy = static_cast<float>(bounds.height()) / m_requiredTextureSize.height();

    bool needsDisplay = false;

    bool blending = !drawMissing;

    IntRect tileRect;
    for (tileRect.setX(0); tileRect.x() < m_requiredTextureSize.width(); tileRect.setX(tileRect.x() + maxw)) {
        for (tileRect.setY(0); tileRect.y() < m_requiredTextureSize.height(); tileRect.setY(tileRect.y() + maxh)) {
            TileIndex index = indexOfTile(tileRect.location());
            LayerTile* tile = m_tilesCompositingThread.get(index);
            if (!tile) {
                tile = new LayerTile();
                m_tilesCompositingThread.add(index, tile);
            }

            float x = index.i() * maxw * sx;
            float y = index.j() * maxh * sy;
            float w = min(bounds.width() - x, maxw * sx);
            float h = min(bounds.height() - y, maxh * sy);
            float ox = x - bounds.width() / 2.0;
            float oy = y - bounds.height() / 2.0;

            // We apply the transformation by hand, since we need the z coordinate
            // as well (to do perspective correct texturing) and we don't need
            // to divide by w by hand, the GPU will do that for us
            transformPoint(ox, oy, drawTransform, &vertices[0]);
            transformPoint(ox, oy + h, drawTransform, &vertices[4]);
            transformPoint(ox + w, oy + h, drawTransform, &vertices[8]);
            transformPoint(ox + w, oy, drawTransform, &vertices[12]);

            // Inflate the rect somewhat to attempt to make textures render before they show
            // up on screen.
            float d = viewportInflationFactor;
            FloatRect rect(-d, -d, 2 * d, 2 * d);
            FloatQuad quad(FloatPoint(vertices[0] / vertices[3], vertices[1] / vertices[3]),
                           FloatPoint(vertices[4] / vertices[7], vertices[5] / vertices[7]),
                           FloatPoint(vertices[8] / vertices[11], vertices[9] / vertices[11]),
                           FloatPoint(vertices[12] / vertices[15], vertices[13] / vertices[15]));
            bool visible = quad.boundingBox().intersects(rect);

            bool wasVisible = tile->isVisible();
            tile->setVisible(visible);

            // This method is called in two passes. The first pass draws all
            // visible tiles with textures.
            // If a visible tile has no texture, set the m_hasMissingTextures
            // flag, to indicate that we need a second pass.
            // The second "drawMissing" pass draws all visible tiles without
            // textures as checkerboard.
            // However, don't draw brand new tiles as checkerboard. The checker-
            // board indicates that a tile has dirty contents, but that's not
            // the case if it's brand new.
            if (visible) {
                bool hasTexture = tile->hasTexture();
                if (!hasTexture)
                    m_hasMissingTextures = true;

                if (hasTexture && !drawMissing) {
                    Texture* texture = tile->texture();
                    if (texture->isOpaque() && layer->drawOpacity() == 1.0f && !layer->maskLayer()) {
                        if (blending) {
                            blending = false;
                            glDisable(GL_BLEND);
                        }
                    } else if (!blending) {
                        blending = true;
                        glEnable(GL_BLEND);
                        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    }

                    textureCacheCompositingThread()->textureAccessed(texture);
                    glBindTexture(GL_TEXTURE_2D, texture->textureId());
                }

                if (hasTexture != drawMissing)
                    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

                if (tile->isDirty()) {
                    addRenderJob(index);
                    needsDisplay = true;
                }
            } else if (wasVisible)
                removeRenderJob(index);
        }
    }

    // Return early for the drawMissing case, don't flag us as needing commit.
    if (drawMissing)
        return;

    // Switch on blending again (we know that drawMissing == false).
    if (!blending) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    }

    // If we schedule a commit, visibility will be updated, and display will
    // happen if there are any visible and dirty textures.
    if (needsDisplay)
        layer->setNeedsCommit();
}

void LayerTiler::addRenderJob(const TileIndex& index)
{
    ASSERT(isCompositingThread());

    MutexLocker locker(m_renderJobsMutex);
    m_renderJobs.add(index);
}

void LayerTiler::removeRenderJob(const TileIndex& index)
{
    MutexLocker locker(m_renderJobsMutex);
    m_renderJobs.remove(index);
}

void LayerTiler::deleteTextures()
{
    // Since textures are deleted by a synchronous message
    // from WebKit thread to compositing thread, we don't need
    // any synchronization mechanism here, even though we are
    // touching some WebKit thread state.
    m_tilesWebKitThread.clear();

    if (m_tilesCompositingThread.size()) {
        for (TileMap::iterator it = m_tilesCompositingThread.begin(); it != m_tilesCompositingThread.end(); ++it)
            (*it).second->discardContents();
        m_tilesCompositingThread.clear();

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
    for (TileMap::iterator it = m_tilesCompositingThread.begin(); it != m_tilesCompositingThread.end(); ++it) {
        TileIndex index = (*it).first;

        IntPoint origin = originOfTile(index);
        if (origin.x() >= m_requiredTextureSize.width() || origin.y() >= m_requiredTextureSize.height())
            tilesToDelete.append(index);
    }

    for (Vector<TileIndex>::iterator it = tilesToDelete.begin(); it != tilesToDelete.end(); ++it) {
        LayerTile* tile = m_tilesCompositingThread.take(*it);
        tile->discardContents();
        delete tile;
    }
}

void LayerTiler::updateTileSize()
{
    IntSize size = m_tilingDisabled ? m_layer->bounds() : defaultTileSize();
    const IntSize maxTextureSize(2048, 2048);
    size = size.shrunkTo(maxTextureSize);

    if (m_tileSize == size || size.isEmpty())
        return;

    // Invalidate the whole layer if tile size changes.
    setNeedsDisplay();
    m_tileSize = size;
}

void LayerTiler::disableTiling(bool disable)
{
    if (m_tilingDisabled == disable)
        return;

    m_tilingDisabled = disable;
    updateTileSize();
}

bool LayerTiler::shouldPrefillTile(const TileIndex& index)
{
    // For now, we use the heuristic of prefilling the first screenful of tiles.
    // This gives the correct result only for layers with identity transform,
    // which is why it's called a heuristic here. This is needed for the case
    // where the developer actually designed their web page around the use of
    // accelerated compositing, and expects even offscreen layers to have content.
    // We oblige them to some degree by prefilling a screenful of tiles.
    // This is redundant in some other scenarios, i.e. when an offscreen layer
    // is composited only because of overlapping a flash ad or something like
    // that, but we're willing to make this tradeoff.

    // Since the tileMultiplier indicates how many tiles fit on the screen,
    // the following formula can be used:
    return index.i() < static_cast<unsigned>(tileMultiplier) && index.j() < static_cast<unsigned>(tileMultiplier);
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

bool LayerTiler::hasDirtyTiles() const
{
    for (TileMap::const_iterator it = m_tilesCompositingThread.begin(); it != m_tilesCompositingThread.end(); ++it) {
        const LayerTile* tile = (*it).second;
        if (tile->isDirty())
            return true;
    }

    return false;
}

void LayerTiler::bindContentsTexture()
{
    ASSERT(m_tilesCompositingThread.size() == 1);
    if (m_tilesCompositingThread.size() != 1)
        return;

    const LayerTile* tile = m_tilesCompositingThread.begin()->second;

    ASSERT(tile->hasTexture());
    if (!tile->hasTexture())
        return;

    glBindTexture(GL_TEXTURE_2D, tile->texture()->textureId());
}

} // namespace WebCore

#endif
