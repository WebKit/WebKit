/*
 * Copyright (C) 2011-2025 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "WKAccessibilityWebPageObjectBase.h"

#import "WKArray.h"
#import "WKNumber.h"
#import "WKRetainPtr.h"
#import "WKSharedAPICast.h"
#import "WKString.h"
#import "WKStringCF.h"
#import "WebFrame.h"
#import "WebPage.h"
#import "WebProcess.h"
#import <WebCore/AXIsolatedObject.h>
#import <WebCore/AXObjectCache.h>
#import <WebCore/Document.h>
#import <WebCore/FrameTree.h>
#import <WebCore/LocalFrame.h>
#import <WebCore/LocalFrameView.h>
#import <WebCore/Page.h>
#import <WebCore/RemoteFrame.h>
#import <WebCore/ScrollView.h>
#import <WebCore/Scrollbar.h>
#import <WebCore/Settings.h>

namespace ax = WebCore::Accessibility;

@implementation WKAccessibilityWebPageObjectBase

- (NakedPtr<WebCore::AXObjectCache>)axObjectCache
{
    ASSERT(isMainRunLoop());

    if (!m_page)
        return nullptr;

    RefPtr page = m_page->corePage();
    if (!page)
        return nullptr;

    return page->axObjectCache();
}

- (void)enableAccessibilityForAllProcesses
{
    // Immediately enable accessibility in the current web process, otherwise this
    // will happen asynchronously and could break certain flows (e.g., attribute
    // requests).
    if (!WebCore::AXObjectCache::accessibilityEnabled())
        WebCore::AXObjectCache::enableAccessibility();

    if (RefPtr page = m_page.get())
        page->enableAccessibilityForAllProcesses();
}

- (id)accessibilityPluginObject
{
    ASSERT(isMainRunLoop());
    RetainPtr<id> axPlugin;
    callOnMainRunLoopAndWait([&axPlugin, &self] {
        if (RefPtr page = self->m_page.get()) {
            // FIXME: This is a static analysis false positive.
            SUPPRESS_UNRETAINED_ARG axPlugin = page->accessibilityObjectForMainFramePlugin();
        }
    });
    return axPlugin.autorelease();
}

- (BOOL)shouldFallbackToWebContentAXObjectForMainFramePlugin
{
    RefPtr page = m_page.get();
    return page && page->shouldFallbackToWebContentAXObjectForMainFramePlugin();
}

// Called directly by Accessibility framework.
- (id)accessibilityRootObjectWrapper
{
    return [self accessibilityRootObjectWrapper:protect([self focusedLocalFrame]).get()];
}

- (id)accessibilityRootObjectWrapper:(WebCore::LocalFrame*)frame
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop()) {
        if (RefPtr tree = m_isolatedTree.get()) {
            tree->applyPendingChanges();
            if (RefPtr root = tree->rootNode())
                return root->wrapper();
        }
    }
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

    return ax::retrieveAutoreleasedValueFromMainThread<id>([protectedSelf = retainPtr(self), protectedFrame = RefPtr { frame }] () -> RetainPtr<id> {
        if (!WebCore::AXObjectCache::accessibilityEnabled())
            [protectedSelf enableAccessibilityForAllProcesses];

        if (protectedSelf->m_hasMainFramePlugin) {
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            // Even though we want to serve the PDF plugin tree for main-frame plugins, we still need to make sure the isolated tree
            // is built, so that when text annotations are created on-the-fly as users focus on text fields,
            // isolated objects are able to be attached to those text annotation object wrappers.
            // If they aren't, we never have a backing object to serve any requests from.
            if (CheckedPtr cache = protectedSelf.get().axObjectCache.get())
                cache->buildIsolatedTreeIfNeeded();
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
            if (![protectedSelf shouldFallbackToWebContentAXObjectForMainFramePlugin])
                return [protectedSelf accessibilityPluginObject];
        }

        RefPtr frame = protectedFrame ? protectedFrame : RefPtr { [protectedSelf focusedLocalFrame] };
        if (RefPtr document = frame ? frame->document() : nullptr) {
            if (CheckedPtr cache = document->axObjectCache()) {
                if (RefPtr root = cache->rootObjectForFrame(*frame))
                    return root->wrapper();
            }
        }

        if (CheckedPtr cache = protectedSelf.get().axObjectCache.get()) {
            // It's possible we were given a null frame (this is explicitly expected when off the main-thread, since
            // we can't access the webpage off the main-thread to get a frame). Now that we are actually on the main-thread,
            // try again if necessary.
            RefPtr frame = protectedFrame ? protectedFrame : RefPtr { [protectedSelf focusedLocalFrame] };

            if (RefPtr root = frame ? cache->rootObjectForFrame(*frame) : nullptr)
                return root->wrapper();
        }

        return nil;
    });
}

- (void)setWebPage:(NakedPtr<WebKit::WebPage>)nakedPage
{
    ASSERT(isMainRunLoop());

    m_page = nakedPage.get();

    if (RefPtr webPage = nakedPage.get()) {
        m_pageID = webPage->identifier();
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
        [self setPosition:webPage->accessibilityPosition()];
        [self setSize:webPage->size()];
#endif
        RefPtr frame = dynamicDowncast<WebCore::LocalFrame>(webPage->mainFrame());
        m_hasMainFramePlugin = frame && frame->document() && frame->document()->isPluginDocument();
    } else {
        m_pageID = std::nullopt;
        m_hasMainFramePlugin = false;
    }
}

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
- (void)setPosition:(const WebCore::FloatPoint&)point
{
    ASSERT(isMainRunLoop());
    Locker locker { m_cacheLock };
    m_position = point;
}

- (void)setSize:(const WebCore::IntSize&)size
{
    ASSERT(isMainRunLoop());
    Locker locker { m_cacheLock };
    m_size = size;
}

- (void)setIsolatedTree:(Ref<WebCore::AXIsolatedTree>&&)tree
{
    ASSERT(isMainRunLoop());

    if (m_hasMainFramePlugin) {
        // Do not set the isolated tree root for main-frame plugins, as that would prevent serving the root
        // of the plugin accessiblity tree.
        return;
    }

    CheckedPtr cache = tree->axObjectCache();
    if (!cache)
        return;

    RefPtr mainFrame = m_page ? Ref { *m_page }->mainFrame() : nullptr;
    if (!mainFrame)
        return;

    // Ignore an isolated tree that's not the main frame, otherwise VoiceOver might jump directly to an iframe
    // when interacting with a page.
    if (cache->frameID() != mainFrame->frameID())
        return;

    m_isolatedTree = tree.get();
}

- (RefPtr<WebCore::AXIsolatedTree>)isolatedTree
{
    return m_isolatedTree.get();
}

- (void)setWindow:(id)window
{
    ASSERT(isMainRunLoop());
    Locker lock { m_windowLock };
    m_window = window;
}

- (void)_buildIsolatedTreeIfNeeded
{
    ensureOnMainThread([protectedSelf = RetainPtr { self }] {
        if (CheckedPtr cache = protectedSelf.get().axObjectCache.get())
            cache->buildIsolatedTreeIfNeeded();
    });
}
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)

- (void)setHasMainFramePlugin:(bool)hasPlugin
{
    ASSERT(isMainRunLoop());
    m_hasMainFramePlugin = hasPlugin;
}

- (void)setRemoteFrameOffset:(WebCore::IntPoint)offset
{
    ASSERT(isMainRunLoop());
    m_remoteFrameOffset = offset;
}

- (WebCore::IntPoint)accessibilityRemoteFrameOffset
{
    return m_remoteFrameOffset;
}

- (void)setRemoteParent:(id)parent token:(NSData *)token
{
    ASSERT(isMainRunLoop());

#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    Locker lock { m_parentLock };
#endif // ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    m_parent = parent;
    m_remoteToken = token;
}

- (NSUInteger)remoteTokenHash
{
    return [m_remoteToken.get() hash];
}

- (void)setFrameIdentifier:(const WebCore::FrameIdentifier&)frameID
{
    m_frameID = frameID;
}

- (id)accessibilityFocusedUIElement
{
    return [[self accessibilityRootObjectWrapper:protect([self focusedLocalFrame]).get()] accessibilityFocusedUIElement];
}

- (WebCore::LocalFrame *)focusedLocalFrame
{
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    if (!isMainRunLoop())
        return nullptr;
#endif
    RefPtr webPage = m_page.get();
    if (!webPage)
        return nullptr;

    if (!m_frameID)
        return dynamicDowncast<WebCore::LocalFrame>(webPage->mainFrame());

    RefPtr page = webPage->corePage();
    if (!page)
        return nullptr;
    ASSERT(page->settings().siteIsolationEnabled());

    // FIXME: This needs to be made thread safe when the isolated accessibility tree is on.
    for (auto& rootFrame : page->rootFrames()) {
        if (rootFrame->frameID() == m_frameID)
            return rootFrame.ptr();
    }

    return nullptr;
}

@end
