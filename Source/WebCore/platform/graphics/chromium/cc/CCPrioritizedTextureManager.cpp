/*
 * Copyright (C) 2012, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "CCPrioritizedTextureManager.h"

#include "CCPrioritizedTexture.h"
#include "CCPriorityCalculator.h"
#include "LayerRendererChromium.h"
#include "TraceEvent.h"
#include <algorithm>

using namespace std;

namespace WebCore {

CCPrioritizedTextureManager::CCPrioritizedTextureManager(size_t maxMemoryLimitBytes, int)
    : m_maxMemoryLimitBytes(maxMemoryLimitBytes)
    , m_memoryUseBytes(0)
    , m_memoryAboveCutoffBytes(0)
    , m_memoryAvailableBytes(0)
{
}

CCPrioritizedTextureManager::~CCPrioritizedTextureManager()
{
    while (m_textures.size() > 0)
        unregisterTexture(*m_textures.begin());

    // Each remaining backing is a leaked opengl texture. We don't have the allocator
    // to delete the textures at this time so clearMemory() needs to be called before this.
    while (m_backings.size() > 0)
        destroyBacking(*m_backings.begin(), 0);
}

void CCPrioritizedTextureManager::prioritizeTextures(size_t renderSurfacesMemoryBytes)
{
    TRACE_EVENT0("cc", "CCPrioritizedTextureManager::prioritizeTextures");

#if !ASSERT_DISABLED
    assertInvariants();
#endif

    // Sorting textures in this function could be replaced by a slightly
    // modified O(n) quick-select to partition textures rather than
    // sort them (if performance of the sort becomes an issue).

    TextureVector& sortedTextures = m_tempTextureVector;
    BackingVector& sortedBackings = m_tempBackingVector;
    sortedTextures.clear();
    sortedBackings.clear();

    // Copy all textures into a vector and sort them.
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it)
        sortedTextures.append(*it);
    std::sort(sortedTextures.begin(), sortedTextures.end(), compareTextures);

    m_memoryAvailableBytes = m_maxMemoryLimitBytes;
    m_priorityCutoff = CCPriorityCalculator::lowestPriority();
    bool reservedRenderSurfaces = false;
    size_t memoryBytes = 0;
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        if ((*it)->requestPriority() == CCPriorityCalculator::lowestPriority())
            break;

        // FIXME: We can make placeholder objects similar to textures to represent the render surface memory request.
        if (!reservedRenderSurfaces && CCPriorityCalculator::priorityIsLower((*it)->requestPriority(), CCPriorityCalculator::renderSurfacePriority())) {
            size_t newMemoryBytes = memoryBytes + renderSurfacesMemoryBytes;
            if (newMemoryBytes > m_memoryAvailableBytes) {
                m_priorityCutoff = (*it)->requestPriority();
                m_memoryAvailableBytes = memoryBytes;
                break;
            }
            m_memoryAvailableBytes -= renderSurfacesMemoryBytes;
            reservedRenderSurfaces = true;
        }

        size_t newMemoryBytes = memoryBytes + (*it)->memorySizeBytes();
        if (newMemoryBytes > m_memoryAvailableBytes) {
            m_priorityCutoff = (*it)->requestPriority();
            break;
        }

        memoryBytes = newMemoryBytes;
    }

    // Only allow textures if they are higher than the cutoff. All textures
    // of the same priority are accepted or rejected together, rather than
    // being partially allowed randomly.
    m_memoryAboveCutoffBytes = 0;
    for (TextureVector::iterator it = sortedTextures.begin(); it != sortedTextures.end(); ++it) {
        bool isAbovePriorityCutoff = CCPriorityCalculator::priorityIsHigher((*it)->requestPriority(), m_priorityCutoff);
        (*it)->setAbovePriorityCutoff(isAbovePriorityCutoff);
        if (isAbovePriorityCutoff)
            m_memoryAboveCutoffBytes += (*it)->memorySizeBytes();
    }
    ASSERT(m_memoryAboveCutoffBytes <= m_memoryAvailableBytes);

    // Put backings in eviction/recycling order.
    for (BackingSet::iterator it = m_backings.begin(); it != m_backings.end(); ++it)
        sortedBackings.append(*it);
    std::sort(sortedBackings.begin(), sortedBackings.end(), compareBackings);

    for (BackingVector::iterator it = sortedBackings.begin(); it != sortedBackings.end(); ++it) {
        m_backings.remove(*it);
        m_backings.add(*it);
    }

    sortedTextures.clear();
    sortedBackings.clear();

#if !ASSERT_DISABLED
    assertInvariants();
    ASSERT(memoryAboveCutoffBytes() <= maxMemoryLimitBytes());
#endif
}

void CCPrioritizedTextureManager::clearPriorities()
{
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        // FIXME: We should remove this and just set all priorities to
        //        CCPriorityCalculator::lowestPriority() once we have priorities
        //        for all textures (we can't currently calculate distances for
        //        off-screen textures).
        (*it)->setRequestPriority(CCPriorityCalculator::lingeringPriority((*it)->requestPriority()));
    }
}

bool CCPrioritizedTextureManager::requestLate(CCPrioritizedTexture* texture)
{
    // This is already above cutoff, so don't double count it's memory below.
    if (texture->isAbovePriorityCutoff())
        return true;

    if (CCPriorityCalculator::priorityIsLower(texture->requestPriority(), m_priorityCutoff))
        return false;

    size_t newMemoryBytes = m_memoryAboveCutoffBytes + texture->memorySizeBytes();
    if (newMemoryBytes > m_memoryAvailableBytes)
        return false;

    m_memoryAboveCutoffBytes = newMemoryBytes;
    texture->setAbovePriorityCutoff(true);
    if (texture->currentBacking()) {
        m_backings.remove(texture->currentBacking());
        m_backings.add(texture->currentBacking());
    }
    return true;
}

void CCPrioritizedTextureManager::acquireBackingTextureIfNeeded(CCPrioritizedTexture* texture, TextureAllocator* allocator)
{
    ASSERT(texture->isAbovePriorityCutoff());
    if (texture->currentBacking() || !texture->isAbovePriorityCutoff())
        return;

    // Find a backing below, by either recycling or allocating.
    CCPrioritizedTexture::Backing* backing = 0;

    // First try to recycle
    for (BackingSet::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->currentTexture() && (*it)->currentTexture()->isAbovePriorityCutoff())
            break;
        if ((*it)->size() == texture->size() && (*it)->format() == texture->format()) {
            backing = (*it);
            break;
        }
    }

    // Otherwise reduce memory and just allocate a new backing texures.
    if (!backing) {
        reduceMemory(m_memoryAvailableBytes - texture->memorySizeBytes(), allocator);
        backing = createBacking(texture->size(), texture->format(), allocator);
    }

    // Move the used backing texture to the end of the eviction list.
    if (backing->currentTexture())
        unlink(backing->currentTexture(), backing);
    link(texture, backing);
    m_backings.remove(backing);
    m_backings.add(backing);
}

void CCPrioritizedTextureManager::reduceMemory(size_t limitBytes, TextureAllocator* allocator)
{
    if (memoryUseBytes() <= limitBytes)
        return;
    // Destroy backings until we are below the limit,
    // or until all backings remaining are above the cutoff.
    while (memoryUseBytes() > limitBytes && m_backings.size() > 0) {
        BackingSet::iterator it = m_backings.begin();
        if ((*it)->currentTexture() && (*it)->currentTexture()->isAbovePriorityCutoff())
            break;
        destroyBacking((*it), allocator);
    }
}

void CCPrioritizedTextureManager::reduceMemory(TextureAllocator* allocator)
{
    reduceMemory(m_memoryAvailableBytes, allocator);
    ASSERT(memoryUseBytes() <= maxMemoryLimitBytes());

    // We currently collect backings from deleted textures for later recycling.
    // However, if we do that forever we will always use the max limit even if
    // we really need very little memory. This should probably be solved by reducing the
    // limit externally, but until then this just does some "clean up" of unused
    // backing textures (any more than 10%).
    size_t wastedMemory = 0;
    for (BackingSet::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->currentTexture())
            break;
        wastedMemory += (*it)->memorySizeBytes();
    }
    size_t tenPercentOfMemory = m_memoryAvailableBytes / 10;
    if (wastedMemory <= tenPercentOfMemory)
        return;
    reduceMemory(memoryUseBytes() - (wastedMemory - tenPercentOfMemory), allocator);
}

void CCPrioritizedTextureManager::clearAllMemory(TextureAllocator* allocator)
{
    // Unlink and destroy all backing textures.
    while (m_backings.size() > 0) {
        BackingSet::iterator it = m_backings.begin();
        if ((*it)->currentTexture())
            unlink((*it)->currentTexture(), (*it));
        destroyBacking((*it), allocator);
    }
}

void CCPrioritizedTextureManager::allBackingTexturesWereDeleted()
{
    // Same as clearAllMemory, except all our textures were already
    // deleted externally, so we don't delete them. Passing no
    // allocator results in leaking the (now invalid) texture ids.
    clearAllMemory(0);
}

void CCPrioritizedTextureManager::unlink(CCPrioritizedTexture* texture, CCPrioritizedTexture::Backing* backing)
{
    ASSERT(texture && backing);
    ASSERT(texture->currentBacking() == backing);
    ASSERT(backing->currentTexture() == texture);

    texture->setCurrentBacking(0);
    backing->setCurrentTexture(0);
}

void CCPrioritizedTextureManager::link(CCPrioritizedTexture* texture, CCPrioritizedTexture::Backing* backing)
{
    ASSERT(texture && backing);
    ASSERT(!texture->currentBacking());
    ASSERT(!backing->currentTexture());

    texture->setCurrentBacking(backing);
    backing->setCurrentTexture(texture);
}


void CCPrioritizedTextureManager::registerTexture(CCPrioritizedTexture* texture)
{
    ASSERT(texture);
    ASSERT(!texture->textureManager());
    ASSERT(!texture->currentBacking());
    ASSERT(m_textures.find(texture) == m_textures.end());

    texture->setManagerInternal(this);
    m_textures.add(texture);

}

void CCPrioritizedTextureManager::unregisterTexture(CCPrioritizedTexture* texture)
{
    ASSERT(texture);
    ASSERT(m_textures.find(texture) != m_textures.end());

    returnBackingTexture(texture);
    texture->setManagerInternal(0);
    m_textures.remove(texture);
    texture->setAbovePriorityCutoff(false);
}


void CCPrioritizedTextureManager::returnBackingTexture(CCPrioritizedTexture* texture)
{
    if (texture->currentBacking()) {
        // Move the backing texture to the front for eviction/recycling and unlink it.
        m_backings.remove(texture->currentBacking());
        m_backings.insertBefore(m_backings.begin(), texture->currentBacking());
        unlink(texture, texture->currentBacking());
    }
}

CCPrioritizedTexture::Backing* CCPrioritizedTextureManager::createBacking(IntSize size, GC3Denum format, TextureAllocator* allocator)
{
    ASSERT(allocator);

    unsigned textureId = allocator->createTexture(size, format);
    CCPrioritizedTexture::Backing* backing = new CCPrioritizedTexture::Backing(size, format, textureId);
    m_memoryUseBytes += backing->memorySizeBytes();
    // Put backing texture at the front for eviction, since it isn't in use yet.
    m_backings.insertBefore(m_backings.begin(), backing);
    return backing;
}

void CCPrioritizedTextureManager::destroyBacking(CCPrioritizedTexture::Backing* backing, TextureAllocator* allocator)
{
    ASSERT(backing);
    ASSERT(!backing->currentTexture() || !backing->currentTexture()->isAbovePriorityCutoff());
    ASSERT(m_backings.find(backing) != m_backings.end());

    if (allocator)
        allocator->deleteTexture(backing->textureId(), backing->size(), backing->format());
    if (backing->currentTexture())
        unlink(backing->currentTexture(), backing);
    m_memoryUseBytes -= backing->memorySizeBytes();
    m_backings.remove(backing);

    delete backing;
}


#if !ASSERT_DISABLED
void CCPrioritizedTextureManager::assertInvariants()
{
    // If we hit any of these asserts, there is a bug in this class. To see
    // where the bug is, call this function at the beginning and end of
    // every public function.

    // Backings/textures must be doubly-linked and only to other backings/textures in this manager.
    for (BackingSet::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->currentTexture()) {
            ASSERT(m_textures.find((*it)->currentTexture()) != m_textures.end());
            ASSERT((*it)->currentTexture()->currentBacking() == (*it));
        }
    }
    for (TextureSet::iterator it = m_textures.begin(); it != m_textures.end(); ++it) {
        if ((*it)->currentBacking()) {
            ASSERT(m_backings.find((*it)->currentBacking()) != m_backings.end());
            ASSERT((*it)->currentBacking()->currentTexture() == (*it));
        }
    }

    // At all times, backings that can be evicted must always come before
    // backings that can't be evicted in the backing texture list (otherwise
    // reduceMemory will not find all textures available for eviction/recycling).
    bool reachedProtected = false;
    for (BackingSet::iterator it = m_backings.begin(); it != m_backings.end(); ++it) {
        if ((*it)->currentTexture() && (*it)->currentTexture()->isAbovePriorityCutoff())
            reachedProtected = true;
        if (reachedProtected)
            ASSERT((*it)->currentTexture() && (*it)->currentTexture()->isAbovePriorityCutoff());
    }
}
#endif


} // namespace WebCore
