/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#if SVG_SUPPORT
#import "KCanvasFilterQuartz.h"
#import "KCanvasRenderingStyle.h"
#import "KCanvasMatrix.h"
#import "KRenderingDeviceQuartz.h"
#import "QuartzSupport.h"
#import "KWQExceptions.h"
#import "WKDiffuseLightingFilter.h"
#import "WKSpecularLightingFilter.h"
#import "WKSpotLightFilter.h"
#import "WKPointLightFilter.h"
#import "WKDistantLightFilter.h"
#import "WKNormalMapFilter.h"
#import "WKArithmeticFilter.h"

#import <QuartzCore/QuartzCore.h>

#import "IntRect.h"
#import "CachedImage.h"

#import <kxmlcore/Assertions.h>

static QString KCPreviousFilterOutputName = "__previousOutput__";

static inline CIColor *ciColor(const Color &c)
{
    CGColorRef colorCG = cgColor(c);
    CIColor *colorCI = [CIColor colorWithCGColor:colorCG];
    CGColorRelease(colorCG);
    return colorCI;
}

static inline CIVector *ciVector(KCanvasPoint3F point)
{
    return [CIVector vectorWithX:point.x() Y:point.y() Z:point.z()];
}

static inline CIVector *ciVector(FloatPoint point)
{
    return [CIVector vectorWithX:point.x() Y:point.y()];
}

KCanvasFilterQuartz::KCanvasFilterQuartz() : m_filterCIContext(0), m_filterCGLayer(0), m_imagesByName(0)
{
    m_imagesByName = [[NSMutableDictionary alloc] init];
}

KCanvasFilterQuartz::~KCanvasFilterQuartz()
{
    [m_imagesByName release];
}

void KCanvasFilterQuartz::prepareFilter(const FloatRect &bbox)
{
    if (bbox.isEmpty() || !KRenderingDeviceQuartz::filtersEnabled())
        return;

    if (m_effects.isEmpty())
        return;
    
    CGContextRef cgContext = static_cast<KRenderingDeviceQuartz*>(QPainter::renderingDevice())->currentCGContext();
    
    // get a CIContext, and CGLayer for drawing in.
    bool useSoftware = ! KRenderingDeviceQuartz::hardwareRenderingEnabled();
    NSDictionary *contextOptions = nil;
    
    if (useSoftware)
        contextOptions = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:YES], kCIContextUseSoftwareRenderer, nil];
    
    m_filterCIContext = [[CIContext contextWithCGContext:cgContext options:contextOptions] retain];
    m_filterCGLayer = [m_filterCIContext createCGLayerWithSize:CGRect(bbox).size info:NULL];
    
    KRenderingDeviceContext *filterContext = new KRenderingDeviceContextQuartz(CGLayerGetContext(m_filterCGLayer));
    QPainter::renderingDevice()->pushContext(filterContext);
    
    filterContext->concatCTM(QMatrix().translate(-1.0f * bbox.x(), -1.0f * bbox.y()));
}

void KCanvasFilterQuartz::applyFilter(const FloatRect &bbox)
{
    if (bbox.isEmpty() || !KRenderingDeviceQuartz::filtersEnabled() || m_effects.isEmpty())
        return;

    // restore the previous context, delete the filter context.
    delete (QPainter::renderingDevice()->popContext());

    // actually apply the filter effects
    CIImage *inputImage = [CIImage imageWithCGLayer:m_filterCGLayer];
    NSArray *filterStack = getCIFilterStack(inputImage);
    if ([filterStack count]) {
        CIImage *outputImage = [[filterStack lastObject] valueForKey:@"outputImage"];
        if (outputImage) {
            CGRect filterRect = CGRect(filterBBoxForItemBBox(bbox));
            CGRect translated = filterRect;
            CGPoint bboxOrigin = CGRect(bbox).origin;
            CGRect sourceRect = CGRectIntersection(translated,[outputImage extent]);
            
            CGPoint destOrigin = sourceRect.origin;
            destOrigin.x += bboxOrigin.x;
            destOrigin.y += bboxOrigin.y;
            
            [m_filterCIContext drawImage:outputImage atPoint:destOrigin fromRect:sourceRect];
        }
    }
    
    CGLayerRelease(m_filterCGLayer);
    [m_filterCIContext release];
}

NSArray *KCanvasFilterQuartz::getCIFilterStack(CIImage *inputImage)
{
    NSMutableArray *filterEffects = [NSMutableArray array];

    QValueListIterator<KCanvasFilterEffect *> it = m_effects.begin();
    QValueListIterator<KCanvasFilterEffect *> end = m_effects.end();

    setImageForName(inputImage, "SourceGraphic"); // input
    for (;it != end; it++) {
        CIFilter *filter = (*it)->getCIFilter(this);
        if (filter)
            [filterEffects addObject:filter];
    }
    if ([filterEffects count] != m_effects.count())
    [m_imagesByName removeAllObjects]; // clean up before next time.

    return filterEffects;
}

CIImage *KCanvasFilterQuartz::imageForName(const QString& name) const
{
    return [m_imagesByName valueForKey:name.getNSString()];
}

void KCanvasFilterQuartz::setImageForName(CIImage *image, const QString &name)
{
    [m_imagesByName setValue:image forKey:name.getNSString()];
}

void KCanvasFilterQuartz::setOutputImage(const KCanvasFilterEffect *filterEffect, CIImage *output)
{
    if (!filterEffect->result().isEmpty())
        setImageForName(output, filterEffect->result());
    setImageForName(output, KCPreviousFilterOutputName);
}

static inline CIImage *alphaImageForImage(CIImage *image)
{
    CIFilter *onlyAlpha = [CIFilter filterWithName:@"CIColorMatrix"];
    float zero[4] = {0.f, 0.f, 0.f, 0.f};
    [onlyAlpha setDefaults];
    [onlyAlpha setValue:image forKey:@"inputImage"];
    [onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
    [onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
    [onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
    return [onlyAlpha valueForKey:@"outputImage"];
}

CIImage *KCanvasFilterQuartz::inputImage(const KCanvasFilterEffect *filterEffect)
{
    if (filterEffect->in().isEmpty()) {
        CIImage *inImage = imageForName(KCPreviousFilterOutputName);
        if (!inImage)
            inImage = imageForName("SourceGraphic");
        return inImage;
    } else if (filterEffect->in() == "SourceAlpha") {
        CIImage *sourceAlpha = imageForName(filterEffect->in());
        if (!sourceAlpha) {
            CIImage *sourceGraphic = imageForName("SourceGraphic");
            if (!sourceGraphic)
                return nil;
            sourceAlpha = alphaImageForImage(sourceGraphic);
            setImageForName(sourceAlpha, "SourceAlpha");
        }
        return sourceAlpha;
    }

    return imageForName(filterEffect->in());
}

#pragma mark -
#pragma mark Filter Elements

#define FE_QUARTZ_SETUP_INPUT(name) \
    CIImage *inputImage = quartzFilter->inputImage(this); \
    FE_QUARTZ_CHECK_INPUT(inputImage) \
    CIFilter *filter; \
    KWQ_BLOCK_EXCEPTIONS; \
    filter = [CIFilter filterWithName:name]; \
    [filter setDefaults]; \
    [filter setValue:inputImage forKey:@"inputImage"];

#define FE_QUARTZ_CHECK_INPUT(input) \
    if (!input) \
        return nil;

#define FE_QUARTZ_OUTPUT_RETURN \
    quartzFilter->setOutputImage(this, [filter valueForKey:@"outputImage"]); \
    return filter; \
    KWQ_UNBLOCK_EXCEPTIONS; \
    return nil;

#define FE_QUARTZ_CROP_TO_RECT(rect) \
    { \
        CIFilter *crop = [CIFilter filterWithName:@"CICrop"]; \
        [crop setDefaults]; \
        [crop setValue:[filter valueForKey:@"outputImage"] forKey:@"inputImage"]; \
        [crop setValue:[CIVector vectorWithX:rect.origin.x Y:rect.origin.y Z:rect.size.width W:rect.size.height] forKey:@"inputRectangle"]; \
        filter = crop; \
    }

CIFilter *KCanvasFEBlendQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    CIFilter *filter = nil;
    KWQ_BLOCK_EXCEPTIONS;

    switch (blendMode()) {
    case BM_NORMAL:
        // FIXME: I think this is correct....
        filter = [CIFilter filterWithName:@"CISourceOverCompositing"];
        break;
    case BM_MULTIPLY:
        filter = [CIFilter filterWithName:@"CIMultiplyBlendMode"];
        break;
    case BM_SCREEN:
        filter = [CIFilter filterWithName:@"CIScreenBlendMode"];
        break;
    case BM_DARKEN:
        filter = [CIFilter filterWithName:@"CIDarkenBlendMode"];
        break;
    case BM_LIGHTEN:
        filter = [CIFilter filterWithName:@"CILightenBlendMode"];
        break;
    default:
        ERROR("Unhandled blend mode: %i", blendMode());
        return nil;
    }

    [filter setDefaults];
    CIImage *inputImage = quartzFilter->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    [filter setValue:inputImage forKey:@"inputImage"];
    CIImage *backgroundImage = quartzFilter->imageForName(in2());
    FE_QUARTZ_CHECK_INPUT(backgroundImage);
    [filter setValue:backgroundImage forKey:@"inputBackgroundImage"];

    FE_QUARTZ_OUTPUT_RETURN;
}

#define deg2rad(d) ((d * (2.0 * M_PI))/360.0)

#define CMValuesCheck(expected, type) \
    if (values().count() != expected) { \
        NSLog(@"Error, incorrect number of values in ColorMatrix for type \"%s\", expected: %i actual: %i, ignoring filter.  Values:", type, expected, values().count()); \
        for (unsigned int x=0; x < values().count(); x++) fprintf(stderr, " %f", values()[x]); \
        fprintf(stderr, "\n"); \
        return nil; \
    }

CIFilter *KCanvasFEColorMatrixQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    CIFilter *filter = nil;
    KWQ_BLOCK_EXCEPTIONS;
    switch (type()) {
    case CMT_MATRIX:
    {
        CMValuesCheck(20, "matrix");
        filter = [CIFilter filterWithName:@"CIColorMatrix"];
        [filter setDefaults];
        QValueList<float> v = values();
        [filter setValue:[CIVector vectorWithX:v[0] Y:v[1] Z:v[2] W:v[3]] forKey:@"inputRVector"];
        [filter setValue:[CIVector vectorWithX:v[5] Y:v[6] Z:v[7] W:v[8]] forKey:@"inputGVector"];
        [filter setValue:[CIVector vectorWithX:v[10] Y:v[11] Z:v[12] W:v[13]] forKey:@"inputBVector"];
        [filter setValue:[CIVector vectorWithX:v[15] Y:v[16] Z:v[17] W:v[18]] forKey:@"inputAVector"];
        [filter setValue:[CIVector vectorWithX:v[4] Y:v[9] Z:v[14] W:v[19]] forKey:@"inputBiasVector"];
        break;
    }
    case CMT_SATURATE:
    {
        CMValuesCheck(1, "saturate");
        filter = [CIFilter filterWithName:@"CIColorControls"];
        [filter setDefaults];
        float saturation = values()[0];
        if ((saturation < 0.0) || (saturation > 3.0))
                NSLog(@"WARNING: Saturation adjustment: %f outside supported range.");
        [filter setValue:[NSNumber numberWithFloat:saturation] forKey:@"inputSaturation"];
        break;
    }
    case CMT_HUE_ROTATE:
    {
        CMValuesCheck(1, "hueRotate");
        filter = [CIFilter filterWithName:@"CIHueAdjust"];
        [filter setDefaults];
        float radians = deg2rad(values()[0]);
        [filter setValue:[NSNumber numberWithFloat:radians] forKey:@"inputAngle"];
        break;
    }
    case CMT_LUMINANCE_TO_ALPHA:
    {
        CMValuesCheck(0, "luminanceToAlpha");
        // FIXME: I bet there is an easy filter to do this.
        filter = [CIFilter filterWithName:@"CIColorMatrix"];
        [filter setDefaults];
        float zero[4] = {0.f, 0.f, 0.f, 0.f};
        float alpha[4] = { 0.2125f, 0.7154f, 0.0721f, 0.f};
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
        [filter setValue:[CIVector vectorWithValues:alpha count:4] forKey:@"inputAVector"];
        [filter setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBiasVector"];
        break;
    }
    default:
        ERROR("Unhandled ColorMatrix type: %i", type());
        return nil;
    }
    CIImage *inputImage = quartzFilter->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    [filter setValue:inputImage forKey:@"inputImage"];

    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFECompositeQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    CIFilter *filter = nil;
    KWQ_BLOCK_EXCEPTIONS;

    switch (operation()) {
    case CO_OVER:
        filter = [CIFilter filterWithName:@"CISourceOverCompositing"];
        break;
    case CO_IN:
        filter = [CIFilter filterWithName:@"CISourceInCompositing"];
        break;
    case CO_OUT:
        filter = [CIFilter filterWithName:@"CISourceOutCompositing"];
        break;
    case CO_ATOP:
        filter = [CIFilter filterWithName:@"CISourceAtopCompositing"];
        break;
    case CO_XOR:
        //FIXME: I'm not sure this is right...
        filter = [CIFilter filterWithName:@"CIExclusionBlendMode"];
        break;
    case CO_ARITHMETIC:
        [WKArithmeticFilter class];
        filter = [CIFilter filterWithName:@"WKArithmeticFilter"];
        break;
    }
    
    [filter setDefaults];
    CIImage *inputImage = quartzFilter->inputImage(this);
    CIImage *backgroundImage = quartzFilter->imageForName(in2());
    FE_QUARTZ_CHECK_INPUT(inputImage);
    FE_QUARTZ_CHECK_INPUT(backgroundImage);
    [filter setValue:inputImage forKey:@"inputImage"];
    [filter setValue:backgroundImage forKey:@"inputBackgroundImage"];
    //FIXME: this seems ugly
    if (operation() == CO_ARITHMETIC) {
        [filter setValue:[NSNumber numberWithFloat:k1()] forKey:@"inputK1"];
        [filter setValue:[NSNumber numberWithFloat:k2()] forKey:@"inputK2"];
        [filter setValue:[NSNumber numberWithFloat:k3()] forKey:@"inputK3"];
        [filter setValue:[NSNumber numberWithFloat:k4()] forKey:@"inputK4"];
    }
    FE_QUARTZ_OUTPUT_RETURN;
}

static inline CIFilter *getPointLightVectors(CIFilter * normals, CIVector * lightPosition, float surfaceScale)
{
    CIFilter *filter;
    KWQ_BLOCK_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKPointLight"];
    if (!filter)
        return nil;
    [filter setDefaults];
    [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
    [filter setValue:lightPosition forKey:@"inputLightPosition"];    
    [filter setValue:[NSNumber numberWithFloat:surfaceScale] forKey:@"inputSurfaceScale"];
    return filter; 
    KWQ_UNBLOCK_EXCEPTIONS;
    return nil;
}

static CIFilter *getLightVectors(CIFilter * normals, const KCLightSource * light, float surfaceScale)
{
    [WKDistantLightFilter class];
    [WKPointLightFilter class];
    [WKSpotLightFilter class];

    CIFilter *filter = nil;    
    KWQ_BLOCK_EXCEPTIONS;
    
    switch (light->type()) {
    case LS_DISTANT:
    {
        const KCDistantLightSource *dlight = static_cast<const KCDistantLightSource *>(light);
        
        filter = [CIFilter filterWithName:@"WKDistantLight"];
        if (!filter)
            return nil;
        [filter setDefaults];
        
        float azimuth = dlight->azimuth();
        float elevation = dlight->elevation();
        azimuth=deg2rad(azimuth);
        elevation=deg2rad(elevation);
        float Lx = cos(azimuth)*cos(elevation);
        float Ly = sin(azimuth)*cos(elevation);
        float Lz = sin(elevation);
        
        [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
        [filter setValue:[CIVector vectorWithX:Lx Y:Ly Z:Lz] forKey:@"inputLightDirection"];
        return filter;
    }
    case LS_POINT:
    {
        const KCPointLightSource *plight = static_cast<const KCPointLightSource *>(light);
        return getPointLightVectors(normals, [CIVector vectorWithX:plight->position().x() Y:plight->position().y() Z:plight->position().z()], surfaceScale);
    }
    case LS_SPOT:
    {
        const KCSpotLightSource *slight = static_cast<const KCSpotLightSource *>(light);
        filter = [CIFilter filterWithName:@"WKSpotLight"];
        if (!filter)
            return nil;
        
        CIFilter * pointLightFilter = getPointLightVectors(normals, [CIVector vectorWithX:slight->position().x() Y:slight->position().y() Z:slight->position().z()], surfaceScale);
        if (!pointLightFilter)
            return nil;
        [filter setDefaults];
        
        [filter setValue:[pointLightFilter valueForKey:@"outputImage"] forKey:@"inputLightVectors"];
        [filter setValue:[CIVector vectorWithX:slight->direction().x() Y:slight->direction().y() Z:slight->direction().z()] forKey:@"inputLightDirection"];
        [filter setValue:[NSNumber numberWithFloat:slight->specularExponent()] forKey:@"inputSpecularExponent"];
        [filter setValue:[NSNumber numberWithFloat:deg2rad(slight->limitingConeAngle())] forKey:@"inputLimitingConeAngle"];
        return filter;
    }
    }
    KWQ_UNBLOCK_EXCEPTIONS;
    return nil;
}

static CIFilter *getNormalMap(CIImage *bumpMap, float scale)
{
    [WKNormalMapFilter class];
    CIFilter *filter;
    KWQ_BLOCK_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKNormalMap"];   
    [filter setDefaults];
    
    [filter setValue:bumpMap forKey:@"inputImage"];  
    [filter setValue:[NSNumber numberWithFloat:scale] forKey:@"inputSurfaceScale"];
    return filter;
    KWQ_UNBLOCK_EXCEPTIONS;
    return nil;
}

CIFilter *KCanvasFEDiffuseLightingQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    const KCLightSource *light = lightSource();
    if (!light)
        return nil;
    
    [WKDiffuseLightingFilter class];
    
    CIFilter *filter;
    KWQ_BLOCK_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKDiffuseLighting"];
    if (!filter)
        return nil;
    
    [filter setDefaults];
    CIImage *inputImage = quartzFilter->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    CIFilter *normals = getNormalMap(inputImage, surfaceScale());
    if (!normals) 
        return nil;
    
    CIFilter *lightVectors = getLightVectors(normals, light, surfaceScale());
    if (!lightVectors) 
        return nil;
    
    [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
    [filter setValue:[lightVectors valueForKey:@"outputImage"] forKey:@"inputLightVectors"];
    [filter setValue:ciColor(lightingColor()) forKey:@"inputLightingColor"];
    [filter setValue:[NSNumber numberWithFloat:surfaceScale()] forKey:@"inputSurfaceScale"];
    [filter setValue:[NSNumber numberWithFloat:diffuseConstant()] forKey:@"inputDiffuseConstant"];
    [filter setValue:[NSNumber numberWithFloat:kernelUnitLengthX()] forKey:@"inputKernelUnitLengthX"];
    [filter setValue:[NSNumber numberWithFloat:kernelUnitLengthY()] forKey:@"inputKernelUnitLengthY"];
    
    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEFloodQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    CIFilter *filter;
    KWQ_BLOCK_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"CIConstantColorGenerator"];
    [filter setDefaults];
    CGColorRef color = cgColor(floodColor());
    CGColorRef withAlpha = CGColorCreateCopyWithAlpha(color,CGColorGetAlpha(color) * floodOpacity());
    CIColor *inputColor = [CIColor colorWithCGColor:withAlpha];
    CGColorRelease(color);
    CGColorRelease(withAlpha);
    [filter setValue:inputColor forKey:@"inputColor"];
    
    CGRect cropRect = CGRectMake(-100,-100,1000,1000); // HACK
    if (!subRegion().isEmpty())
        cropRect = subRegion();
    FE_QUARTZ_CROP_TO_RECT(cropRect);
    
    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEImageQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    if (!cachedImage())
        return nil;

    CIFilter *filter;
    KWQ_BLOCK_EXCEPTIONS;
    // FIXME: This is only partially implemented (only supports images)
    CIImage *ciImage = [CIImage imageWithCGImage:cachedImage()->image().imageRef()];
    
    // FIXME: There is probably a nicer way to perform both of these transforms.
    filter = [CIFilter filterWithName:@"CIAffineTransform"];
    [filter setDefaults];
    [filter setValue:ciImage forKey:@"inputImage"];
    
    CGAffineTransform cgTransform = CGAffineTransformMake(1,0,0,-1,0,cachedImage()->image().rect().bottom());
    NSAffineTransform *nsTransform = [NSAffineTransform transform];
    [nsTransform setTransformStruct:*((NSAffineTransformStruct *)&cgTransform)];
    [filter setValue:nsTransform forKey:@"inputTransform"];
    
    if (!subRegion().isEmpty()) {
        CIFilter *scaleImage = [CIFilter filterWithName:@"CIAffineTransform"];
        [scaleImage setDefaults];
        [scaleImage setValue:[filter valueForKey:@"outputImage"] forKey:@"inputImage"];
        
        cgTransform = CGAffineTransformMakeMapBetweenRects(CGRect(cachedImage()->image().rect()), subRegion());
        [nsTransform setTransformStruct:*((NSAffineTransformStruct *)&cgTransform)];
        [scaleImage setValue:nsTransform forKey:@"inputTransform"];
        filter = scaleImage;
    }
    
    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEGaussianBlurQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    FE_QUARTZ_SETUP_INPUT(@"CIGaussianPyramid");

    float inputRadius = stdDeviationX();
    if (inputRadius != stdDeviationY()) {
        float inputAspectRatio = stdDeviationX()/stdDeviationY();
        // FIXME: inputAspectRatio only support the range .5 to 2.0!
        [filter setValue:[NSNumber numberWithFloat:inputAspectRatio] forKey:@"inputAspectRatio"];
    }
    [filter setValue:[NSNumber numberWithFloat:inputRadius] forKey:@"inputRadius"];

    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEMergeQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    CIFilter *filter = nil;
    KWQ_BLOCK_EXCEPTIONS;
    QStringList inputs = mergeInputs();
    QValueListIterator<QString> it = inputs.begin();
    QValueListIterator<QString> end = inputs.end();

    CIImage *previousOutput = quartzFilter->inputImage(this);
    for (;it != end; it++) {
        CIImage *inputImage = quartzFilter->imageForName(*it);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    FE_QUARTZ_CHECK_INPUT(previousOutput);
        filter = [CIFilter filterWithName:@"CISourceOverCompositing"];
        [filter setDefaults];
        [filter setValue:inputImage forKey:@"inputImage"];
        [filter setValue:previousOutput forKey:@"inputBackgroundImage"];
        previousOutput = [filter valueForKey:@"outputImage"];
    }
    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEOffsetQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    FE_QUARTZ_SETUP_INPUT(@"CIAffineTransform");
    NSAffineTransform *offsetTransform = [NSAffineTransform transform];
    [offsetTransform translateXBy:dx() yBy:dy()];
    [filter setValue:offsetTransform  forKey:@"inputTransform"];
    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFESpecularLightingQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{  
    const KCLightSource *light = lightSource();
    if(!light)
        return nil;
    
    [WKSpecularLightingFilter class];  
    
    CIFilter *filter;
    KWQ_BLOCK_EXCEPTIONS;
    filter = [CIFilter filterWithName:@"WKSpecularLighting"];
    [filter setDefaults];
    CIImage *inputImage = quartzFilter->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
    CIFilter *normals = getNormalMap(inputImage, surfaceScale());
    if (!normals) 
        return nil;
    CIFilter *lightVectors = getLightVectors(normals, light, surfaceScale());
    if (!lightVectors) 
        return nil;
    [filter setValue:[normals valueForKey:@"outputImage"] forKey:@"inputNormalMap"];
    [filter setValue:[lightVectors valueForKey:@"outputImage"] forKey:@"inputLightVectors"];
    [filter setValue:ciColor(lightingColor()) forKey:@"inputLightingColor"];
    [filter setValue:[NSNumber numberWithFloat:surfaceScale()] forKey:@"inputSurfaceScale"];
    [filter setValue:[NSNumber numberWithFloat:specularConstant()] forKey:@"inputSpecularConstant"];
    [filter setValue:[NSNumber numberWithFloat:specularExponent()] forKey:@"inputSpecularExponent"];
    [filter setValue:[NSNumber numberWithFloat:kernelUnitLengthX()] forKey:@"inputKernelUnitLengthX"];
    [filter setValue:[NSNumber numberWithFloat:kernelUnitLengthY()] forKey:@"inputKernelUnitLengthY"];
    
    FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFETileQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
    FE_QUARTZ_SETUP_INPUT(@"CIAffineTile");
    FE_QUARTZ_OUTPUT_RETURN;
}
#endif // SVG_SUPPORT

