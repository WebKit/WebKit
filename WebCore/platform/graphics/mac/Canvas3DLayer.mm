/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if USE(ACCELERATED_COMPOSITING)
#if ENABLE(3D_CANVAS)

#import "Canvas3DLayer.h"

#import "GraphicsLayer.h"
#import <QuartzCore/QuartzCore.h>
#import <OpenGL/OpenGL.h>

using namespace WebCore;

@implementation Canvas3DLayer

-(id)initWithContext:(CGLContextObj)context texture:(GLuint)texture
{
    m_contextObj = context;
    m_texture = texture;
    self = [super init];
    return self;
}

-(CGLPixelFormatObj)copyCGLPixelFormatForDisplayMask:(uint32_t)mask
{
    CGLPixelFormatAttribute attribs[] =
    {
        (CGLPixelFormatAttribute) kCGLPFAAccelerated,
        (CGLPixelFormatAttribute) kCGLPFAColorSize, (CGLPixelFormatAttribute) 32,
        (CGLPixelFormatAttribute) kCGLPFADisplayMask, (CGLPixelFormatAttribute) mask,
        (CGLPixelFormatAttribute) 0
    };
    
    CGLPixelFormatObj pixelFormatObj;
    GLint numPixelFormats;
 
    CGLChoosePixelFormat(attribs, &pixelFormatObj, &numPixelFormats);
    return pixelFormatObj;
}

-(CGLContextObj)copyCGLContextForPixelFormat:(CGLPixelFormatObj)pixelFormat
{
    CGLContextObj contextObj;
    CGLCreateContext(pixelFormat, m_contextObj, &contextObj);
    return contextObj;
}

-(void)drawInCGLContext:(CGLContextObj)glContext pixelFormat:(CGLPixelFormatObj)pixelFormat forLayerTime:(CFTimeInterval)timeInterval displayTime:(const CVTimeStamp *)timeStamp
{
    CGLSetCurrentContext(m_contextObj);
    glFinish();
    CGLSetCurrentContext(glContext);

    CGRect frame = [self frame];
        
    // draw the FBO into the layer
    glViewport(0, 0, frame.size.width, frame.size.height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(-1, 1, -1, 1, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    
    glBegin(GL_TRIANGLE_FAN);
        glTexCoord2f(0, 0);
        glVertex2f(-1, -1);
        glTexCoord2f(1, 0);
        glVertex2f(1, -1);
        glTexCoord2f(1, 1);
        glVertex2f(1, 1);
        glTexCoord2f(0, 1);
        glVertex2f(-1, 1);
    glEnd();
    
    glBindTexture(GL_TEXTURE_2D, 0);
    glDisable(GL_TEXTURE_2D);

    // Call super to finalize the drawing. By default all it does is call glFlush().
    [super drawInCGLContext:glContext pixelFormat:pixelFormat forLayerTime:timeInterval displayTime:timeStamp];
}

@end

@implementation Canvas3DLayer(WebLayerAdditions)

-(void)setLayerOwner:(GraphicsLayer*)aLayer
{
    m_layerOwner = aLayer;
}

-(GraphicsLayer*)layerOwner
{
    return m_layerOwner;
}

@end

#endif // ENABLE(3D_CANVAS)
#endif // USE(ACCELERATED_COMPOSITING)
