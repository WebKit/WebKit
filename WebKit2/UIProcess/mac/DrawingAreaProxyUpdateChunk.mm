/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#import "DrawingAreaProxyUpdateChunk.h"

#import "MessageID.h"

#import "DrawingAreaMessageKinds.h"
#import "DrawingAreaProxyMessageKinds.h"
#import "WKView.h"
#import "UpdateChunk.h"
#import "WKAPICast.h"
#import "WebCoreTypeArgumentMarshalling.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"

using namespace WebCore;

namespace WebKit {

DrawingAreaProxyUpdateChunk::DrawingAreaProxyUpdateChunk(WKView* webView)
    : DrawingAreaProxy(DrawingAreaUpdateChunkType)
    , m_isInitialized(false)
    , m_isWaitingForDidSetFrameNotification(false)
    , m_webView(webView)
{
}

DrawingAreaProxyUpdateChunk::~DrawingAreaProxyUpdateChunk()
{
}

void DrawingAreaProxyUpdateChunk::drawRectIntoContext(CGRect rect, CGContextRef context)
{
    if (!m_isInitialized) {
        setSize(IntSize([m_webView frame].size));
        m_isInitialized = true;
    }

    if (m_isWaitingForDidSetFrameNotification) {
        WebPageProxy* page = toWK([m_webView pageRef]);
        if (!page->isValid())
            return;
        
        std::auto_ptr<CoreIPC::ArgumentDecoder> arguments = page->process()->connection()->waitFor(DrawingAreaProxyMessage::DidSetFrame, page->pageID(), 0.04);
        if (arguments.get())
            didReceiveMessage(page->process()->connection(), CoreIPC::MessageID(DrawingAreaProxyMessage::DidSetFrame), *arguments.get());
    }

    if (!m_bitmapContext)
        return;

    RetainPtr<CGImageRef> image(AdoptCF, CGBitmapContextCreateImage(m_bitmapContext.get()));
    CGContextDrawImage(context, CGRectMake(0, 0, CGImageGetWidth(image.get()), CGImageGetHeight(image.get())), image.get());
}

void DrawingAreaProxyUpdateChunk::drawUpdateChunkIntoBackingStore(UpdateChunk* updateChunk)
{
    ensureBackingStore();

    RetainPtr<CGImageRef> image(updateChunk->createImage());
    const IntRect& updateChunkRect = updateChunk->rect();
    
    CGContextDrawImage(m_bitmapContext.get(), CGRectMake(updateChunkRect.x(), m_viewSize.height() - updateChunkRect.bottom(), 
                                                         updateChunkRect.width(), updateChunkRect.height()), image.get());
    [m_webView setNeedsDisplayInRect:NSRectFromCGRect(updateChunkRect)];
}

void DrawingAreaProxyUpdateChunk::ensureBackingStore()
{
    if (m_bitmapContext)
        return;

    RetainPtr<CGColorSpaceRef> colorSpace(AdoptCF, CGColorSpaceCreateDeviceRGB());
    m_bitmapContext.adoptCF(CGBitmapContextCreate(0, m_viewSize.width(), m_viewSize.height(), 8, m_viewSize.width() * 4, colorSpace.get(), kCGImageAlphaPremultipliedLast));
    
    // Flip the bitmap context coordinate system.
    CGContextTranslateCTM(m_bitmapContext.get(), 0, m_viewSize.height());
    CGContextScaleCTM(m_bitmapContext.get(), 1, -1);
}

void DrawingAreaProxyUpdateChunk::setSize(const IntSize& viewSize)
{
    m_isInitialized = true;

    WebPageProxy* page = toWK([m_webView pageRef]);
    if (!page->isValid())
        return;

    m_viewSize = viewSize;
    m_lastSetViewSize = viewSize;
    
    if (m_isWaitingForDidSetFrameNotification)
        return;
    m_isWaitingForDidSetFrameNotification = true;
    
    page->process()->responsivenessTimer()->start();
    page->process()->connection()->send(DrawingAreaMessage::SetFrame, page->pageID(), CoreIPC::In(viewSize));
}

void DrawingAreaProxyUpdateChunk::didSetSize(const IntSize& viewSize, UpdateChunk* updateChunk)
{
    ASSERT(m_isWaitingForDidSetFrameNotification);
    m_isWaitingForDidSetFrameNotification = false;

    if (viewSize != m_lastSetViewSize)
        setSize(m_lastSetViewSize);

    // Invalidate the backing store.
    m_bitmapContext = 0;
    drawUpdateChunkIntoBackingStore(updateChunk);

    WebPageProxy* page = toWK([m_webView pageRef]);
    page->process()->responsivenessTimer()->stop();
}

void DrawingAreaProxyUpdateChunk::didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID messageID, CoreIPC::ArgumentDecoder& arguments)
{
    switch (messageID.get<DrawingAreaProxyMessage::Kind>()) {
        case DrawingAreaProxyMessage::Update: {
            UpdateChunk updateChunk;
            if (!arguments.decode(updateChunk))
                return;
            drawUpdateChunkIntoBackingStore(&updateChunk);
            break;
        }
        case DrawingAreaProxyMessage::DidSetFrame: {
            IntSize viewSize;
            UpdateChunk updateChunk;
            if (!arguments.decode(CoreIPC::Out(viewSize, updateChunk)))
                return;

            didSetSize(viewSize, &updateChunk);
            break;
        }
        default:
            ASSERT_NOT_REACHED();
    }
}

} // namespace WebKit
