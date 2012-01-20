/*
 Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)

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
 the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TextureMapperNode.h"

#if USE(ACCELERATED_COMPOSITING)

#include "GraphicsLayerTextureMapper.h"
#include "MathExtras.h"

namespace {
    static const float gTileDimension = 1024.0;
}

namespace WebCore {

TextureMapperNode* toTextureMapperNode(GraphicsLayer* layer)
{
    return layer ? toGraphicsLayerTextureMapper(layer)->node() : 0;
}

TextureMapperNode* TextureMapperNode::rootLayer()
{
    if (m_effectTarget)
        return m_effectTarget->rootLayer();
    if (m_parent)
        return m_parent->rootLayer();
    return this;
}

void TextureMapperNode::setTransform(const TransformationMatrix& matrix)
{
    m_transform.setLocalTransform(matrix);
}

void TextureMapperNode::computeTransformsRecursive()
{
    if (m_size.isEmpty() && m_state.masksToBounds)
        return;

    // Compute transforms recursively on the way down to leafs.
    TransformationMatrix parentTransform;
    if (m_parent)
        parentTransform = m_parent->m_transform.combinedForChildren();
    else if (m_effectTarget)
        parentTransform = m_effectTarget->m_transform.combined();
    m_transform.combineTransforms(parentTransform);

    m_state.visible = m_state.backfaceVisibility || m_transform.combined().inverse().m33() >= 0;

    if (m_parent && m_parent->m_state.preserves3D)
        m_centerZ = m_transform.combined().mapPoint(FloatPoint3D(m_size.width() / 2, m_size.height() / 2, 0)).z();

    if (m_state.maskLayer)
        m_state.maskLayer->computeTransformsRecursive();
    if (m_state.replicaLayer)
        m_state.replicaLayer->computeTransformsRecursive();
    for (int i = 0; i < m_children.size(); ++i)
        m_children[i]->computeTransformsRecursive();

    // Reorder children if needed on the way back up.
    if (m_state.preserves3D)
        sortByZOrder(m_children, 0, m_children.size());
}

void TextureMapperNode::computeTiles()
{
#if USE(TILED_BACKING_STORE)
    if (m_state.tileOwnership == ExternallyManagedTiles)
        return;
#endif
    if (m_currentContent.contentType == HTMLContentType && !m_state.drawsContent) {
        m_ownedTiles.clear();
        return;
    }
    Vector<FloatRect> tilesToAdd;
    Vector<int> tilesToRemove;
    const int TileEraseThreshold = 6;
    FloatSize size = contentSize();
    FloatRect contentRect(0, 0, size.width(), size.height());

    for (float y = 0; y < contentRect.height(); y += gTileDimension) {
        for (float x = 0; x < contentRect.width(); x += gTileDimension) {
            FloatRect tileRect(x, y, gTileDimension, gTileDimension);
            tileRect.intersect(contentRect);
            tilesToAdd.append(tileRect);
        }
    }

    for (int i = m_ownedTiles.size() - 1; i >= 0; --i) {
        const FloatRect oldTile = m_ownedTiles[i].rect;
        bool found = false;

        for (int j = tilesToAdd.size() - 1; j >= 0; --j) {
            const FloatRect newTile = tilesToAdd[j];
            if (oldTile != newTile)
                continue;

            found = true;
            tilesToAdd.remove(j);
            break;
        }

        if (!found)
            tilesToRemove.append(i);
    }

    for (size_t i = 0; i < tilesToAdd.size(); ++i) {
        if (!tilesToRemove.isEmpty()) {
            OwnedTile& tile = m_ownedTiles[tilesToRemove[0]];
            tilesToRemove.remove(0);
            tile.rect = tilesToAdd[i];
            tile.needsReset = true;
            continue;
        }

        OwnedTile tile;
        tile.rect = tilesToAdd[i];
        tile.needsReset = true;
        m_ownedTiles.append(tile);
    }

    for (size_t i = 0; i < tilesToRemove.size() && m_ownedTiles.size() > TileEraseThreshold; ++i)
        m_ownedTiles.remove(tilesToRemove[i]);
}

void TextureMapperNode::renderContent(TextureMapper* textureMapper, GraphicsLayer* layer)
{
#if USE(TILED_BACKING_STORE)
    if (m_state.tileOwnership == ExternallyManagedTiles)
        return;
#endif

    if (m_size.isEmpty() || !layer || (!m_state.drawsContent && m_currentContent.contentType == HTMLContentType)) {
        m_ownedTiles.clear();
        return;
    }

    if (!textureMapper)
        return;

    // FIXME: Add directly composited images.
    FloatRect dirtyRect = m_currentContent.needsDisplay ? entireRect() : m_currentContent.needsDisplayRect;

    for (size_t tileIndex = 0; tileIndex < m_ownedTiles.size(); ++tileIndex) {
        OwnedTile& tile = m_ownedTiles[tileIndex];
        FloatRect rect = dirtyRect;
        if (!tile.texture)
            tile.texture = textureMapper->createTexture();
        RefPtr<BitmapTexture>& texture = tile.texture;
        IntSize tileSize = enclosingIntRect(tile.rect).size();

        if (tile.needsReset || texture->contentSize() != tileSize || !texture->isValid()) {
            tile.needsReset = false;
            texture->reset(tileSize, m_currentContent.contentType == DirectImageContentType ? false : m_state.contentsOpaque);
            rect = tile.rect;
        }

        IntRect contentRect = enclosingIntRect(tile.rect);
        contentRect.intersect(enclosingIntRect(rect));
        if (contentRect.isEmpty())
            continue;

        FloatRect contentRectInTileCoordinates = contentRect;
        FloatPoint offset(-tile.rect.x(), -tile.rect.y());
        contentRectInTileCoordinates.move(offset.x(), offset.y());

        {
            GraphicsContext context(texture->beginPaint(enclosingIntRect(contentRectInTileCoordinates)));
            context.setImageInterpolationQuality(textureMapper->imageInterpolationQuality());
            context.setTextDrawingMode(textureMapper->textDrawingMode());
            context.translate(offset.x(), offset.y());
            FloatRect scaledContentRect(contentRect);
            if (m_currentContent.contentType == DirectImageContentType)
                context.drawImage(m_currentContent.image.get(), ColorSpaceDeviceRGB, IntPoint(0, 0));
            else
                layer->paintGraphicsLayerContents(context, enclosingIntRect(scaledContentRect));
            texture->endPaint();
        }
    }

    m_currentContent.needsDisplay = false;
    m_currentContent.needsDisplayRect = IntRect();
}

void TextureMapperNode::paint()
{
    if (m_size.isEmpty())
        return;

    computeTransformsRecursive();

    TextureMapperPaintOptions opt;
    opt.textureMapper = m_textureMapper;
    opt.textureMapper->bindSurface(0);
    paintRecursive(opt);
}

FloatRect TextureMapperNode::targetRectForTileRect(const FloatRect& targetRect, const FloatRect& tileRect) const
{
    return FloatRect(
                targetRect.x() + (tileRect.x() - targetRect.x()),
                targetRect.y() + (tileRect.y() - targetRect.y()),
                tileRect.width(),
                tileRect.height());
}

void TextureMapperNode::paintSelf(const TextureMapperPaintOptions& options)
{
    if (m_size.isEmpty() || (!m_state.drawsContent && m_currentContent.contentType == HTMLContentType))
        return;

    float opacity = options.opacity;
    BitmapTexture* mask = options.mask;
    FloatRect targetRect = this->targetRect();

    // We apply the following transform to compensate for painting into a surface, and then apply the offset so that the painting fits in the target rect.
    TransformationMatrix transform =
            TransformationMatrix(options.transform)
            .multiply(m_transform.combined())
            .translate(options.offset.width(), options.offset.height());

#if USE(TILED_BACKING_STORE)
    Vector<ExternallyManagedTile> tilesToPaint;

    if (m_state.tileOwnership == ExternallyManagedTiles) {
        // Sort tiles, with current scale factor last.
        Vector<ExternallyManagedTile*> tiles;
        HashMap<int, ExternallyManagedTile>::iterator end = m_externallyManagedTiles.end();
        for (HashMap<int, ExternallyManagedTile>::iterator it = m_externallyManagedTiles.begin(); it != end; ++it) {
            if (!it->second.frontBuffer.texture)
                continue;

            if (it->second.scale == m_state.contentScale) {
                tiles.append(&it->second);
                continue;
            }

            // This creates a transitional effect looks good only when there's no transparency, so we only use it when the opacity for the layer is above a certain threshold.
            if (opacity > 0.95)
                tiles.prepend(&it->second);
        }

        for (int i = 0; i < tiles.size(); ++i) {
            ExternallyManagedTile& tile = *tiles[i];
            FloatRect rect = tile.frontBuffer.targetRect;
            BitmapTexture& texture = *tile.frontBuffer.texture;
            options.textureMapper->drawTexture(texture, rect, transform, opacity, mask);
        }
        return;
    }
#endif

    // Now we paint owned tiles, if we're in OwnedTileMode.
    for (size_t i = 0; i < m_ownedTiles.size(); ++i) {
        BitmapTexture* texture = m_ownedTiles[i].texture.get();
        const FloatRect rect = targetRectForTileRect(targetRect, m_ownedTiles[i].rect);
        options.textureMapper->drawTexture(*texture, rect, transform, opacity, mask);
    }

    if (m_currentContent.contentType == MediaContentType && m_currentContent.media)
        m_currentContent.media->paintToTextureMapper(options.textureMapper, targetRect, transform, opacity, mask);
}

int TextureMapperNode::compareGraphicsLayersZValue(const void* a, const void* b)
{
    TextureMapperNode* const* nodeA = static_cast<TextureMapperNode* const*>(a);
    TextureMapperNode* const* nodeB = static_cast<TextureMapperNode* const*>(b);
    return int(((*nodeA)->m_centerZ - (*nodeB)->m_centerZ) * 1000);
}

void TextureMapperNode::sortByZOrder(Vector<TextureMapperNode* >& array, int first, int last)
{
    qsort(array.data(), array.size(), sizeof(TextureMapperNode*), compareGraphicsLayersZValue);
}

void TextureMapperNode::paintSelfAndChildren(const TextureMapperPaintOptions& options)
{
    bool hasClip = m_state.masksToBounds && !m_children.isEmpty();
    if (hasClip)
        options.textureMapper->beginClip(TransformationMatrix(options.transform).multiply(m_transform.combined()), FloatRect(0, 0, m_size.width(), m_size.height()));

    paintSelf(options);

    for (int i = 0; i < m_children.size(); ++i)
        m_children[i]->paintRecursive(options);

    if (hasClip)
        options.textureMapper->endClip();
}

IntRect TextureMapperNode::intermediateSurfaceRect()
{
    // FIXME: Add an inverse transform to LayerTransform.
    return intermediateSurfaceRect(m_transform.combined().inverse());
}

IntRect TextureMapperNode::intermediateSurfaceRect(const TransformationMatrix& matrix)
{
    IntRect rect;
    TransformationMatrix localTransform = TransformationMatrix(matrix).multiply(m_transform.combined());
    rect = enclosingIntRect(localTransform.mapRect(entireRect()));
    if (!m_state.masksToBounds && !m_state.maskLayer) {
        for (int i = 0; i < m_children.size(); ++i)
            rect.unite(m_children[i]->intermediateSurfaceRect(matrix));
    }

    if (m_state.replicaLayer)
        rect.unite(m_state.replicaLayer->intermediateSurfaceRect(matrix));

    return rect;
}

bool TextureMapperNode::shouldPaintToIntermediateSurface() const
{
    bool hasOpacity = m_opacity < 0.99;
    bool hasChildren = !m_children.isEmpty();
    bool hasReplica = !!m_state.replicaLayer;
    bool hasMask = !!m_state.maskLayer;

    // We don't use two-pass blending for preserves-3d, that's in sync with Safari.
    if (m_state.preserves3D)
        return false;

    // We should use an intermediate surface when blending several items with an ancestor opacity.
    // Tested by compositing/reflections/reflection-opacity.html
    if (hasOpacity && (hasChildren || hasReplica))
        return true;

    // We should use an intermediate surface with a masked ancestor.
    // In the case of replicas the mask is applied before replicating.
    // Tested by compositing/masks/masked-ancestor.html
    if (hasMask && hasChildren && !hasReplica)
        return true;

    return false;
}

bool TextureMapperNode::isVisible() const
{
    if (m_size.isEmpty() && (m_state.masksToBounds || m_state.maskLayer || m_children.isEmpty()))
        return false;
    if (!m_state.visible || m_opacity < 0.01)
        return false;
    return true;
}

void TextureMapperNode::paintSelfAndChildrenWithReplica(const TextureMapperPaintOptions& options)
{
    if (m_state.replicaLayer) {
        TextureMapperPaintOptions replicaOptions(options);
        // We choose either the content's mask or the replica's mask.
        // FIXME: blend the two if both exist.
        if (m_state.replicaLayer->m_state.maskLayer)
            replicaOptions.mask = m_state.replicaLayer->m_state.maskLayer->texture();

        replicaOptions.transform
                  .multiply(m_state.replicaLayer->m_transform.combined())
                  .multiply(m_transform.combined().inverse());
        paintSelfAndChildren(replicaOptions);
    }

    paintSelfAndChildren(options);
}

void TextureMapperNode::paintRecursive(const TextureMapperPaintOptions& options)
{
    if (!isVisible())
        return;

    float opacity = options.opacity * m_opacity;
    RefPtr<BitmapTexture> maskTexture = m_state.maskLayer ? m_state.maskLayer->texture() : 0;

    TextureMapperPaintOptions paintOptions(options);
    paintOptions.mask = maskTexture.get();
    IntRect surfaceRect;

    RefPtr<BitmapTexture> surface;

    if (!shouldPaintToIntermediateSurface()) {
        paintOptions.opacity = opacity;
        paintSelfAndChildrenWithReplica(paintOptions);
        return;
    }

    // Prepare a surface to paint into.
    // We paint into the surface ignoring the opacity/transform of the current layer.
    surfaceRect = intermediateSurfaceRect();
    surface = options.textureMapper->acquireTextureFromPool(surfaceRect.size());
    options.textureMapper->bindSurface(surface.get());
    paintOptions.opacity = 1;

    // We have to use combinedForChildren() and not combined(), otherwise preserve-3D doesn't work.
    paintOptions.transform = m_transform.combinedForChildren().inverse();
    paintOptions.offset = -IntSize(surfaceRect.x(), surfaceRect.y());

    paintSelfAndChildrenWithReplica(paintOptions);

    // If we painted the replica, the mask is already applied so we don't need to paint it again.
    if (m_state.replicaLayer)
        maskTexture = 0;

    options.textureMapper->bindSurface(options.surface);
    TransformationMatrix targetTransform =
            TransformationMatrix(options.transform)
                .multiply(m_transform.combined())
                .translate(options.offset.width(), options.offset.height());
    options.textureMapper->drawTexture(*surface.get(), surfaceRect, targetTransform, opacity, maskTexture.get());
}

TextureMapperNode::~TextureMapperNode()
{
    for (int i = m_children.size() - 1; i >= 0; --i)
        m_children[i]->m_parent = 0;

    if (m_parent)
        m_parent->m_children.remove(m_parent->m_children.find(this));
}

#if USE(TILED_BACKING_STORE)
int TextureMapperNode::createContentsTile(float scale)
{
    static int nextID = 0;
    int id = ++nextID;
    m_externallyManagedTiles.add(id, ExternallyManagedTile(scale));
    m_state.contentScale = scale;
    return id;
}

void TextureMapperNode::removeContentsTile(int id)
{
    m_externallyManagedTiles.remove(id);
}

void TextureMapperNode::purgeNodeTexturesRecursive()
{
    m_externallyManagedTiles.clear();

    for (int i = m_children.size() - 1; i >= 0; --i)
        m_children[i]->purgeNodeTexturesRecursive();
}

void TextureMapperNode::setTileBackBufferTextureForDirectlyCompositedImage(int id, const IntRect& sourceRect, const FloatRect& targetRect, BitmapTexture* texture)
{
    HashMap<int, ExternallyManagedTile>::iterator it = m_externallyManagedTiles.find(id);

    if (it == m_externallyManagedTiles.end())
        return;

    ExternallyManagedTile& tile = it->second;

    tile.backBuffer.sourceRect = sourceRect;
    tile.backBuffer.targetRect = targetRect;
    tile.backBuffer.texture = texture;
    tile.isBackBufferUpdated = true;
    tile.isDirectlyCompositedImage = true;
}

void TextureMapperNode::clearAllDirectlyCompositedImageTiles()
{
    for (HashMap<int, ExternallyManagedTile>::iterator it = m_externallyManagedTiles.begin(); it != m_externallyManagedTiles.end(); ++it) {
        if (it->second.isDirectlyCompositedImage)
            m_externallyManagedTiles.remove(it);
    }
}

void TextureMapperNode::setContentsTileBackBuffer(int id, const IntRect& sourceRect, const IntRect& targetRect, void* bits, BitmapTexture::PixelFormat format)
{
    ASSERT(m_textureMapper);

    HashMap<int, ExternallyManagedTile>::iterator it = m_externallyManagedTiles.find(id);
    if (it == m_externallyManagedTiles.end())
        return;

    ExternallyManagedTile& tile = it->second;

    tile.backBuffer.sourceRect = sourceRect;
    tile.backBuffer.targetRect = FloatRect(targetRect);
    tile.backBuffer.targetRect.scale(1.0 / tile.scale);

    if (!tile.backBuffer.texture)
        tile.backBuffer.texture = m_textureMapper->createTexture();
    tile.backBuffer.texture->reset(sourceRect.size(), false);
    tile.backBuffer.texture->updateContents(format, sourceRect, bits);
    tile.isBackBufferUpdated = true;
}

void TextureMapperNode::swapContentsBuffers()
{
    HashMap<int, ExternallyManagedTile>::iterator end = m_externallyManagedTiles.end();
    for (HashMap<int, ExternallyManagedTile>::iterator it = m_externallyManagedTiles.begin(); it != end; ++it) {
        ExternallyManagedTile& tile = it->second;
        if (!tile.isBackBufferUpdated)
            continue;
        tile.isBackBufferUpdated = false;
        ExternallyManagedTileBuffer swapBuffer = tile.frontBuffer;
        tile.frontBuffer = tile.backBuffer;
        tile.backBuffer = swapBuffer;
    }
}
#endif

void TextureMapperNode::syncCompositingState(GraphicsLayerTextureMapper* graphicsLayer, int options)
{
    syncCompositingState(graphicsLayer, rootLayer()->m_textureMapper, options);
}

void TextureMapperNode::syncCompositingStateSelf(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper* textureMapper)
{
    int changeMask = graphicsLayer->changeMask();
    const TextureMapperNode::ContentData& pendingContent = graphicsLayer->pendingContent();
#if USE(TILED_BACKING_STORE)
    swapContentsBuffers();
#endif
    if (changeMask == NoChanges && graphicsLayer->m_animations.isEmpty() && pendingContent.needsDisplayRect.isEmpty() && !pendingContent.needsDisplay)
        return;

    if (m_currentContent.contentType == HTMLContentType && (changeMask & ParentChange)) {
        TextureMapperNode* newParent = toTextureMapperNode(graphicsLayer->parent());
        if (newParent != m_parent) {
            // Remove node from current from child list first.
            if (m_parent) {
                size_t index = m_parent->m_children.find(this);
                m_parent->m_children.remove(index);
                m_parent = 0;
            }
            // Set new node parent and add node to the parents child list.
            if (newParent) {
                m_parent = newParent;
                m_parent->m_children.append(this);
            }
        }
    }

    if (changeMask & ChildrenChange) {
        // Clear children parent pointer to avoid unsync and crash on node delete.
        for (size_t i = 0; i < m_children.size(); i++)
            m_children[i]->m_parent = 0;

        m_children.clear();
        for (size_t i = 0; i < graphicsLayer->children().size(); ++i) {
            TextureMapperNode* child = toTextureMapperNode(graphicsLayer->children()[i]);
            if (!child)
                continue;
            m_children.append(child);
            child->m_parent = this;
        }
    }

    if (changeMask & (SizeChange | ContentsRectChange)) {
        FloatSize wantedSize(graphicsLayer->size().width(), graphicsLayer->size().height());
        if (wantedSize.isEmpty() && pendingContent.contentType == HTMLContentType)
            wantedSize = FloatSize(graphicsLayer->contentsRect().width(), graphicsLayer->contentsRect().height());

        if (wantedSize != m_size)
            m_ownedTiles.clear();

        m_size = wantedSize;
    }

    if (changeMask & MaskLayerChange) {
       if (TextureMapperNode* layer = toTextureMapperNode(graphicsLayer->maskLayer()))
           layer->m_effectTarget = this;
    }

    if (changeMask & ReplicaLayerChange) {
       if (TextureMapperNode* layer = toTextureMapperNode(graphicsLayer->replicaLayer()))
           layer->m_effectTarget = this;
    }

    if (changeMask & AnimationChange) {
        m_animations.clear();
        for (size_t i = 0; i < graphicsLayer->m_animations.size(); ++i)
            m_animations.append(graphicsLayer->m_animations[i]);
    }

    m_state.maskLayer = toTextureMapperNode(graphicsLayer->maskLayer());
    m_state.replicaLayer = toTextureMapperNode(graphicsLayer->replicaLayer());
    m_state.pos = graphicsLayer->position();
    m_state.anchorPoint = graphicsLayer->anchorPoint();
    m_state.size = graphicsLayer->size();
    m_state.contentsRect = graphicsLayer->contentsRect();
    m_state.transform = graphicsLayer->transform();
    m_state.contentsRect = graphicsLayer->contentsRect();
    m_state.preserves3D = graphicsLayer->preserves3D();
    m_state.masksToBounds = graphicsLayer->masksToBounds();
    m_state.drawsContent = graphicsLayer->drawsContent();
    m_state.contentsOpaque = graphicsLayer->contentsOpaque();
    m_state.backfaceVisibility = graphicsLayer->backfaceVisibility();
    m_state.childrenTransform = graphicsLayer->childrenTransform();
    m_state.opacity = graphicsLayer->opacity();

    m_currentContent.contentType = pendingContent.contentType;
    m_currentContent.image = pendingContent.image;
    m_currentContent.media = pendingContent.media;
    m_currentContent.needsDisplay = m_currentContent.needsDisplay || pendingContent.needsDisplay;
    if (!m_currentContent.needsDisplay)
        m_currentContent.needsDisplayRect.unite(pendingContent.needsDisplayRect);

    if (!hasOpacityAnimation())
        m_opacity = m_state.opacity;
    if (!hasTransformAnimation())
        m_transform.setLocalTransform(m_state.transform);

    m_transform.setPosition(m_state.pos);
    m_transform.setAnchorPoint(m_state.anchorPoint);
    m_transform.setSize(m_state.size);
    m_transform.setFlattening(!m_state.preserves3D);
    m_transform.setChildrenTransform(m_state.childrenTransform);
}

bool TextureMapperNode::descendantsOrSelfHaveRunningAnimations() const
{
    for (size_t i = 0; i < m_animations.size(); ++i) {
        if (!m_animations[i]->paused)
            return true;
    }

    for (size_t i = 0; i < m_children.size(); ++i) {
        if (m_children[i]->descendantsOrSelfHaveRunningAnimations())
            return true;
    }

    return false;
}

static double normalizedAnimationValue(double runningTime, double duration, bool alternate)
{
    if (!duration)
        return 0;
    const int loopCount = runningTime / duration;
    const double lastFullLoop = duration * double(loopCount);
    const double remainder = runningTime - lastFullLoop;
    const double normalized = remainder / duration;
    return (loopCount % 2 && alternate) ? (1 - normalized) : normalized;
}

void TextureMapperNode::applyOpacityAnimation(float fromOpacity, float toOpacity, double progress)
{
    // Optimization: special case the edge values (0 and 1).
    if (progress == 1.0)
        setOpacity(toOpacity);
    else if (!progress)
        setOpacity(fromOpacity);
    else
        setOpacity(fromOpacity + progress * (toOpacity - fromOpacity));
}

static inline double solveEpsilon(double duration)
{
    return 1.0 / (200.0 * duration);
}

static inline double solveCubicBezierFunction(double p1x, double p1y, double p2x, double p2y, double t, double duration)
{
    UnitBezier bezier(p1x, p1y, p2x, p2y);
    return bezier.solve(t, solveEpsilon(duration));
}

static inline double solveStepsFunction(int numSteps, bool stepAtStart, double t)
{
    if (stepAtStart)
        return std::min(1.0, (floor(numSteps * t) + 1) / numSteps);
    return floor(numSteps * t) / numSteps;
}

static inline float applyTimingFunction(const TimingFunction* timingFunction, float progress, double duration)
{
    if (!timingFunction)
        return progress;

    if (timingFunction->isCubicBezierTimingFunction()) {
        const CubicBezierTimingFunction* ctf = static_cast<const CubicBezierTimingFunction*>(timingFunction);
        return solveCubicBezierFunction(ctf->x1(),
                                        ctf->y1(),
                                        ctf->x2(),
                                        ctf->y2(),
                                        progress, duration);
    }

    if (timingFunction->isStepsTimingFunction()) {
        const StepsTimingFunction* stf = static_cast<const StepsTimingFunction*>(timingFunction);
        return solveStepsFunction(stf->numberOfSteps(), stf->stepAtStart(), double(progress));
    }

    return progress;
}

void TextureMapperNode::applyTransformAnimation(const TextureMapperAnimation& animation, const TransformOperations* from, const TransformOperations* to, double progress)
{
    // Optimization: special case the edge values (0 and 1).
    if (progress == 1.0 || !progress) {
        TransformationMatrix matrix;
        const TransformOperations* ops = progress ? to : from;
        ops->apply(animation.boxSize, matrix);
        setTransform(matrix);
    }

    if (!animation.listsMatch) {
        TransformationMatrix toMatrix, fromMatrix;
        to->apply(animation.boxSize, toMatrix);
        from->apply(animation.boxSize, fromMatrix);
        toMatrix.blend(fromMatrix, progress);
        setTransform(toMatrix);
        return;
    }

    TransformationMatrix matrix;

    if (!to->size()) {
        const TransformOperations* swap = to;
        to = from;
        from = swap;
    } else if (!from->size())
        progress = 1.0 - progress;

    TransformOperations blended(*to);
    for (size_t i = 0; i < animation.functionList.size(); ++i)
        blended.operations()[i]->blend(from->at(i), progress, !from->at(i))->apply(matrix, animation.boxSize);

    setTransform(matrix);
}

void TextureMapperNode::applyAnimationFrame(const TextureMapperAnimation& animation, const AnimationValue* from, const AnimationValue* to, float progress)
{
    switch (animation.keyframes.property()) {
    case AnimatedPropertyOpacity:
        applyOpacityAnimation((static_cast<const FloatAnimationValue*>(from)->value()), (static_cast<const FloatAnimationValue*>(to)->value()), progress);
        return;
    case AnimatedPropertyWebkitTransform:
        applyTransformAnimation(animation, static_cast<const TransformAnimationValue*>(from)->value(), static_cast<const TransformAnimationValue*>(to)->value(), progress);
        return;
    default:
        ASSERT_NOT_REACHED();
    }
}

void TextureMapperNode::applyAnimation(const TextureMapperAnimation& animation, double normalizedValue)
{
    // Optimization: special case the edge values (0 and 1).
    if (!normalizedValue) {
        applyAnimationFrame(animation, animation.keyframes.at(0), animation.keyframes.at(1), 0);
        return;
    }
    if (normalizedValue == 1.0) {
        applyAnimationFrame(animation, animation.keyframes.at(animation.keyframes.size() - 2), animation.keyframes.at(animation.keyframes.size() - 1), 1);
        return;
    }
    if (animation.keyframes.size() == 2) {
        normalizedValue = applyTimingFunction(animation.animation->timingFunction().get(), normalizedValue, animation.animation->duration());
        applyAnimationFrame(animation, animation.keyframes.at(0), animation.keyframes.at(1), normalizedValue);
        return;
    }

    for (size_t i = 0; i < animation.keyframes.size() - 1; ++i) {
        const AnimationValue* from = animation.keyframes.at(i);
        const AnimationValue* to = animation.keyframes.at(i + 1);
        if (from->keyTime() > normalizedValue || to->keyTime() < normalizedValue)
            continue;

        normalizedValue = (normalizedValue - from->keyTime()) / (to->keyTime() - from->keyTime());
        normalizedValue = applyTimingFunction(from->timingFunction(), normalizedValue, animation.animation->duration());
        applyAnimationFrame(animation, from, to, normalizedValue);
        break;
    }
}

bool TextureMapperNode::hasOpacityAnimation() const
{
    for (size_t i = 0; i < m_animations.size(); ++i) {
        const TextureMapperAnimation& animation = *m_animations[i].get();
        if (animation.keyframes.property() == AnimatedPropertyOpacity)
            return true;
    }
    return false;
}

bool TextureMapperNode::hasTransformAnimation() const
{
    for (size_t i = 0; i < m_animations.size(); ++i) {
        const TextureMapperAnimation& animation = *m_animations[i].get();
        if (animation.keyframes.property() == AnimatedPropertyWebkitTransform)
            return true;
    }
    return false;
}

void TextureMapperNode::syncAnimations(GraphicsLayerTextureMapper* layer)
{
    for (int i = m_animations.size() - 1; i >= 0; --i) {
        RefPtr<TextureMapperAnimation> animation = m_animations[i];

        double totalRunningTime = WTF::currentTime() - animation->startTime;
        RefPtr<Animation> anim = animation->animation;
        double normalizedValue = normalizedAnimationValue(totalRunningTime, anim->duration(), anim->direction());

        if (anim->iterationCount() != Animation::IterationCountInfinite && totalRunningTime >= anim->duration() * anim->iterationCount()) {
            // We apply an animation that very close to the edge, so that the final frame is applied, oterwise we might get, for example, an opacity of 0.01 which is still visible.
            if (anim->fillsForwards()) {
                if (animation->keyframes.property() == AnimatedPropertyWebkitTransform)
                    setTransform(m_state.transform);
                else if (animation->keyframes.property() == AnimatedPropertyOpacity)
                    setOpacity(m_state.opacity);
            }

            m_animations.remove(i);
            continue;
        }

        if (!animation->paused)
            applyAnimation(*animation.get(), normalizedValue);
    }
}

void TextureMapperNode::syncAnimationsRecursively()
{
    syncAnimations(0);

    for (int i = m_children.size() - 1; i >= 0; --i)
        m_children[i]->syncAnimationsRecursively();
}

void TextureMapperNode::syncCompositingState(GraphicsLayerTextureMapper* graphicsLayer, TextureMapper* textureMapper, int options)
{
    if (graphicsLayer && !(options & ComputationsOnly)) {
        syncCompositingStateSelf(graphicsLayer, textureMapper);
        graphicsLayer->didSynchronize();
    }

    if (graphicsLayer && m_state.maskLayer) {
        m_state.maskLayer->syncCompositingState(toGraphicsLayerTextureMapper(graphicsLayer->maskLayer()), textureMapper);

        // A mask layer has its parent's size by default, in case it's not set specifically.
        if (m_state.maskLayer->m_size.isEmpty())
            m_state.maskLayer->m_size = m_size;
    }

    if (m_state.replicaLayer)
        m_state.replicaLayer->syncCompositingState(toGraphicsLayerTextureMapper(graphicsLayer->replicaLayer()), textureMapper);

    syncAnimations(graphicsLayer);

    computeTiles();

    if (graphicsLayer)
        renderContent(textureMapper, graphicsLayer);

    if (!(options & TraverseDescendants))
        options = ComputationsOnly;

    if (graphicsLayer) {
        Vector<GraphicsLayer*> children = graphicsLayer->children();
        for (int i = children.size() - 1; i >= 0; --i) {
            TextureMapperNode* node = toTextureMapperNode(children[i]);
            if (!node)
                continue;
            node->syncCompositingState(toGraphicsLayerTextureMapper(children[i]), textureMapper, options);
        }
    } else {
        for (int i = m_children.size() - 1; i >= 0; --i)
            m_children[i]->syncCompositingState(0, textureMapper, options);
    }
}

TextureMapperAnimation::TextureMapperAnimation(const KeyframeValueList& values)
    : keyframes(values)
{
}

}

#endif
