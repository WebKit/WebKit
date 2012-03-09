/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef TreeSynchronizer_h
#define TreeSynchronizer_h

#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>

namespace WebCore {

class CCLayerImpl;
class LayerChromium;

class TreeSynchronizer {
WTF_MAKE_NONCOPYABLE(TreeSynchronizer);
public:
    // Accepts a LayerChromium tree and returns a reference to a CCLayerImpl tree that duplicates the structure
    // of the LayerChromium tree, reusing the CCLayerImpls in the tree provided by oldCCLayerImplRoot if possible.
    static PassOwnPtr<CCLayerImpl> synchronizeTrees(LayerChromium* layerRoot, PassOwnPtr<CCLayerImpl> oldCCLayerImplRoot);

private:
    TreeSynchronizer(); // Not instantiable.

    typedef HashMap<int, OwnPtr<CCLayerImpl> > OwnPtrCCLayerImplMap;
    typedef HashMap<int, CCLayerImpl*> RawPtrCCLayerImplMap;

    // Declared as static member functions so they can access functions on LayerChromium as a friend class.
    static PassOwnPtr<CCLayerImpl> reuseOrCreateCCLayerImpl(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium*);
    static void collectExistingCCLayerImplRecursive(OwnPtrCCLayerImplMap& oldLayers, PassOwnPtr<CCLayerImpl>);
    static PassOwnPtr<CCLayerImpl> synchronizeTreeRecursive(RawPtrCCLayerImplMap& newLayers, OwnPtrCCLayerImplMap& oldLayers, LayerChromium*);
    static void updateScrollbarLayerPointersRecursive(const RawPtrCCLayerImplMap& newLayers, LayerChromium*);
};

} // namespace WebCore

#endif // TreeSynchronizer_h
