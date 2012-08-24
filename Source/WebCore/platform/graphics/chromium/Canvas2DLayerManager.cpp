/*
Copyright (C) 2012 Google Inc. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1.  Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
2.  Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#include "config.h"

#include "Canvas2DLayerManager.h"

#include <wtf/StdLibExtras.h>

namespace {
enum {
    DefaultMaxBytesAllocated = 64*1024*1024,
    DefaultTargetBytesAllocated = 16*1024*1024,
};
}

namespace WebCore {

Canvas2DLayerManager::Canvas2DLayerManager()
    : m_bytesAllocated(0)
    , m_maxBytesAllocated(DefaultMaxBytesAllocated)
    , m_targetBytesAllocated(DefaultTargetBytesAllocated)
{
}

Canvas2DLayerManager::~Canvas2DLayerManager()
{
    ASSERT(!m_bytesAllocated);
    ASSERT(!m_layerList.head());
}

void Canvas2DLayerManager::init(size_t maxBytesAllocated, size_t targetBytesAllocated)
{
    ASSERT(maxBytesAllocated >= targetBytesAllocated);
    m_maxBytesAllocated = maxBytesAllocated;
    m_targetBytesAllocated = targetBytesAllocated;
}

Canvas2DLayerManager& Canvas2DLayerManager::get()
{
    DEFINE_STATIC_LOCAL(Canvas2DLayerManager, manager, ());
    return manager;
}

void Canvas2DLayerManager::layerDidDraw(Canvas2DLayerBridge* layer)
{
    if (isInList(layer)) {
        if (layer != m_layerList.head()) {
            m_layerList.remove(layer);
            m_layerList.push(layer); // Set as MRU
        }
    } else
        addLayerToList(layer); 
}

void Canvas2DLayerManager::addLayerToList(Canvas2DLayerBridge* layer)
{
    ASSERT(!isInList(layer));
    m_bytesAllocated += layer->bytesAllocated();
    m_layerList.push(layer); // Set as MRU
}

void Canvas2DLayerManager::layerAllocatedStorageChanged(Canvas2DLayerBridge* layer, intptr_t deltaBytes)
{
    if (!isInList(layer))
        addLayerToList(layer);
    else {
        ASSERT((intptr_t)m_bytesAllocated + deltaBytes > 0); 
        m_bytesAllocated = (intptr_t)m_bytesAllocated + deltaBytes;
    }
    freeMemoryIfNecessary();
}

void Canvas2DLayerManager::layerToBeDestroyed(Canvas2DLayerBridge* layer)
{
    if (isInList(layer))
        removeLayerFromList(layer);
}

void Canvas2DLayerManager::freeMemoryIfNecessary() 
{
    if (m_bytesAllocated > m_maxBytesAllocated) {
        // Pass 1: Free memory from caches
        Canvas2DLayerBridge* layer = m_layerList.tail(); // LRU
        while (m_bytesAllocated > m_targetBytesAllocated && layer) {
            layer->freeMemoryIfPossible(m_bytesAllocated - m_targetBytesAllocated);
            layer = layer->prev();
        }

        // Pass 2: Flush canvases
        Canvas2DLayerBridge* leastRecentlyUsedLayer = m_layerList.tail();
        while (m_bytesAllocated > m_targetBytesAllocated && leastRecentlyUsedLayer) {
            leastRecentlyUsedLayer->flush();
            leastRecentlyUsedLayer->freeMemoryIfPossible(~0);
            removeLayerFromList(leastRecentlyUsedLayer);
            leastRecentlyUsedLayer = m_layerList.tail();
        }
    }
}

void Canvas2DLayerManager::removeLayerFromList(Canvas2DLayerBridge* layer)
{
    ASSERT(isInList(layer));
    m_bytesAllocated -= layer->bytesAllocated();
    m_layerList.remove(layer);
    layer->setNext(0);
    layer->setPrev(0);
}

bool Canvas2DLayerManager::isInList(Canvas2DLayerBridge* layer)
{
    return layer->prev() || m_layerList.head() == layer;
}

}
