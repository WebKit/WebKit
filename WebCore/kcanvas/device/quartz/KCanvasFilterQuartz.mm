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


#import "KCanvasFilterQuartz.h"
#import "KRenderingStyle.h"
#import "KCanvasMatrix.h"
#import "KRenderingDeviceQuartz.h"
#import "QuartzSupport.h"
#import "KWQExceptions.h"

#import <QuartzCore/QuartzCore.h>

#import <qrect.h>

#import <kxmlcore/Assertions.h>

static QString KCPreviousFilterOutputName = QString::fromLatin1("__previousOutput__");

//static CIColor *ciColor(const QColor &c)
//{
//    CGColorRef colorCG = cgColor(c);
//    CIColor *colorCI = [CIColor colorWithCGColor:colorCG];
//    CGColorRelease(colorCG);
//    return colorCI;
//}

KCanvasFilterQuartz::KCanvasFilterQuartz() : m_storedCGContext(0), m_filterCIContext(0), m_filterCGLayer(0), m_imagesByName(0)
{
	m_imagesByName = [[NSMutableDictionary alloc] init];
}

KCanvasFilterQuartz::~KCanvasFilterQuartz()
{
	[m_imagesByName release];
}

void KCanvasFilterQuartz::prepareFilter(KRenderingDeviceContext *renderingContext, const QRect &bbox)
{
    if (! bbox.isValid())
        return;
    
	KRenderingDeviceContextQuartz *quartzContext = static_cast<KRenderingDeviceContextQuartz *>(renderingContext);
	ASSERT(quartzContext);
	
	CGContextRef filterContext = quartzContext->cgContext();
	//NSLog(@"before: %p stored: %p", filterContext, m_storedCGContext);
	prepareFilter(&filterContext, bbox);
	//NSLog(@"after: %p stored: %p", filterContext, m_storedCGContext);
	quartzContext->setCGContext(filterContext);
}

void KCanvasFilterQuartz::applyFilter(KRenderingDeviceContext *renderingContext, const KCanvasCommonArgs &args, const QRect &bbox)
{
    if (! bbox.isValid())
        return;
    
	KRenderingDeviceContextQuartz *quartzContext = static_cast<KRenderingDeviceContextQuartz *>(renderingContext);
	ASSERT(quartzContext);
	if (!KRenderingDeviceQuartz::filtersEnabled()) return;
	
	CGContextRef filterContext = quartzContext->cgContext();
	//NSLog(@"2before: %p stored: %p", filterContext, m_storedCGContext);
	applyFilter(&filterContext, bbox, CGAffineTransform(args.style()->objectMatrix().qmatrix()));
	//NSLog(@"2after: %p stored: %p", filterContext, m_storedCGContext);
	quartzContext->setCGContext(filterContext);
}

void KCanvasFilterQuartz::prepareFilter(CGContextRef *context, const QRect &bbox)
{
	if (!KRenderingDeviceQuartz::filtersEnabled()) return;
	if (m_effects.isEmpty()) {
		NSLog(@"WARNING: No effects, ignoring filter (%p).", this);
		return;
	}
	ASSERT(m_storedCGContext == NULL);
	ASSERT(context != NULL);
	m_storedCGContext = *context;
		
	// get a CIContext, and CGLayer for drawing in.
        bool useSoftware = ! KRenderingDeviceQuartz::hardwareRenderingEnabled();
        NSDictionary *options = nil;
        
        if (useSoftware) {
            options = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithBool:YES], kCIContextUseSoftwareRenderer, nil];
        }
        
	m_filterCIContext = [[CIContext contextWithCGContext:m_storedCGContext options:options] retain];
        
	m_filterCGLayer = [m_filterCIContext createCGLayerWithSize:CGRect(bbox).size info:NULL];
	
	// replace the current context with our new one.
	*context = CGLayerGetContext(m_filterCGLayer);
	
	CGContextConcatCTM(*context,CGAffineTransformMakeTranslation(float(-1 * bbox.x()),float(-1 * bbox.y())));
}

CGRect KCanvasFilterQuartz::filterBBoxForItemBBox(CGRect itemBBox, CGAffineTransform currentCTM) const
{
	// FIXME: hack for now
	CGRect filterBBox = CGRect(filterRect());
	if(filterBoundingBoxMode())
	{
		//NSLog(@"relative bbox: %@", NSStringFromRect(*(NSRect *)&filterBBox));
		filterBBox =
			CGRectMake((filterBBox.origin.x/100.f * itemBBox.size.width), 
						(filterBBox.origin.y/100.f * itemBBox.size.height), 
					   (filterBBox.size.width/100.f * itemBBox.size.width), 
					   (filterBBox.size.height/100.f * itemBBox.size.height));
		//NSLog(@"scaled bbox: %@", NSStringFromRect(*(NSRect *)&filterBBox));
	} else {
		//filterBBox = CGRectApplyAffineTransform(filterBBox, currentCTM);
	}
	return filterBBox;
}

void KCanvasFilterQuartz::applyFilter(CGContextRef *context, const QRect &bbox, CGAffineTransform objectTransform)
{
	if (m_effects.isEmpty()) return; // We log during "prepare"
	ASSERT(m_storedCGContext);
	ASSERT(context);
	//CGContextRef layerContext = *context;
	
	//NSLog(@"apply filter in context: %p to context: %p bbox: %@", *context,
	//	m_storedCGContext, NSStringFromRect(NSRect(bbox)));
	
	// swap contexts back
	*context = m_storedCGContext;
	m_storedCGContext = NULL;

	// actually apply the filter effects
	CIImage *inputImage = [CIImage imageWithCGLayer:m_filterCGLayer];
	NSArray *filterStack = getCIFilterStack(inputImage);
	if ([filterStack count]) {
		CIImage *outputImage = [[filterStack lastObject] valueForKey:@"outputImage"];
		if (!outputImage) {
			NSLog(@"Failed to get ouputImage from filter stack, can't draw.");
			return;
		}
		
		// actually draw the result to the original context
		
		CGRect filterRect = filterBBoxForItemBBox(CGRect(bbox), objectTransform);
		CGRect translated = filterRect;
		CGPoint bboxOrigin = CGRect(bbox).origin;
//		translated.origin.x += bboxOrigin.x;
//		translated.origin.y += bboxOrigin.y;
		CGRect sourceRect = CGRectIntersection(translated,[outputImage extent]);
		// FIXME: objectTransform seems like a hack.
		
		CGPoint destOrigin = sourceRect.origin;
		destOrigin.x += bboxOrigin.x;
		destOrigin.y += bboxOrigin.y;
		
		//CGRect extent = [outputImage extent];
		//NSLog(@"Drawing from rect: \n%@ \n at point: \n%@ \nextent: \n%@ \nmatrix: \n%@", NSStringFromRect(*(NSRect *)&sourceRect), NSStringFromPoint(*(NSPoint *)&destOrigin), NSStringFromRect(*(NSRect *)&extent), CFStringFromCGAffineTransform(objectTransform));
		//outputImage = [outputImage imageByApplyingTransform:objectTransform];
		[m_filterCIContext drawImage:outputImage atPoint:destOrigin fromRect:sourceRect];
	} else
		NSLog(@"No filter stack, can't draw filter.");
	// cleanup
	CGLayerRelease(m_filterCGLayer);
	//CGContextRelease(layerContext); // I don't seem to need to do this...
	[m_filterCIContext release];
}

NSArray *KCanvasFilterQuartz::getCIFilterStack(CIImage *inputImage) {
	// iterate over each of the filter elements
	// passing each the necessary inputs...
	
	NSMutableArray *filterEffects = [NSMutableArray array];
	
	QValueListIterator<KCanvasFilterEffect *> it = m_effects.begin();
	QValueListIterator<KCanvasFilterEffect *> end = m_effects.end();
	
	// convert to an NSArray of CIFilters
	setImageForName(inputImage,QString::fromLatin1("SourceGraphic")); // input
	for (;it != end; it++) {
		CIFilter *filter = (*it)->getCIFilter(this);
		if (filter) [filterEffects addObject:filter];
		else NSLog(@"Failed to build filter for element: %p.", (*it));
	}
	if ([filterEffects count] != m_effects.count())
		NSLog(@"WARNING: Built stack of only %i filters from %i filter elements", [filterEffects count], m_effects.count());
	[m_imagesByName removeAllObjects]; // clean up before next time.
	
	return filterEffects;
}

CIImage *KCanvasFilterQuartz::imageForName(const QString &name) const
{
	NSString *imageName = name.getNSString();
	CIImage *foundImage = [m_imagesByName valueForKey:imageName];
	if (!foundImage && ![imageName isEqualToString:@"SourceAlpha"])
		NSLog(@"Failed to find image for name: %@", imageName);
	return foundImage;
}

void KCanvasFilterQuartz::setImageForName(CIImage *image, const QString &name)
{
	//NSLog(@"Setting image for name: %@", name.getNSString());
	[m_imagesByName setValue:image forKey:name.getNSString()];
}

void KCanvasFilterQuartz::setOutputImage(const KCanvasFilterEffect *filterEffect, CIImage *output) {
	if (!filterEffect->result().isEmpty()) {
		setImageForName(output, filterEffect->result());
	}
	setImageForName(output, KCPreviousFilterOutputName);
}

CIImage *KCanvasFilterQuartz::inputImage(const KCanvasFilterEffect *filterEffect)
{	
	// not specified
	if (filterEffect->in().isEmpty()) {
		CIImage *inImage = imageForName(KCPreviousFilterOutputName);
		if (!inImage) inImage = imageForName(QString::fromLatin1("SourceGraphic"));
		return inImage;
	} else if (filterEffect->in() == QString::fromLatin1("SourceAlpha")) {
		CIImage *sourceAlpha = imageForName(filterEffect->in());
		if (!sourceAlpha) {
			CIFilter *onlyAlpha = [CIFilter filterWithName:@"CIColorMatrix"];
			float zero[4] = {0.f, 0.f, 0.f, 0.f};
			[onlyAlpha setDefaults];
			[onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputRVector"];
			[onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputGVector"];
			[onlyAlpha setValue:[CIVector vectorWithValues:zero count:4] forKey:@"inputBVector"];
			CIImage *sourceGraphic = imageForName(QString::fromLatin1("SourceGraphic"));
            if (sourceGraphic) {
                [onlyAlpha setValue:sourceGraphic forKey:@"inputImage"];
                sourceAlpha = [onlyAlpha valueForKey:@"outputImage"];
                setImageForName(sourceAlpha,QString::fromLatin1("SourceAlpha"));
            }
		}
		return sourceAlpha;
	}
	
	// for now we just return what's in the dict
	// eventually we should be smarter.
	// and really build the alpha, etc.
	return imageForName(filterEffect->in());
	
	// SourceGraphic
	// SourceAlpha
	// BackgroundImage
	// BackgroundAlpha
	// FillPaint
	// StrokePaint
}

#pragma mark -
#pragma mark Filter Elements


#define FE_QUARTZ_SETUP_INPUT(name) \
    CIImage *inputImage = quartzFilter->inputImage(this); \
    FE_QUARTZ_CHECK_INPUT(inputImage) \
	KWQ_BLOCK_EXCEPTIONS \
	CIFilter *filter = [CIFilter filterWithName:name]; \
	[filter setDefaults]; \
	[filter setValue:inputImage forKey:@"inputImage"];

#define FE_QUARTZ_CHECK_INPUT(input) \
	if (!input) { \
		NSLog(@"ERROR: Can't apply filter (%p) w/o input image.", this); \
		return nil; \
	}

#define FE_QUARTZ_OUTPUT_RETURN \
	quartzFilter->setOutputImage(this, [filter valueForKey:@"outputImage"]); \
	return filter; \
	KWQ_UNBLOCK_EXCEPTIONS \
	return nil;

#define FE_QUARTZ_CROP_TO_RECT(rect) \
{ \
	CIFilter *crop = [CIFilter filterWithName:@"CICrop"]; \
	[crop setDefaults]; \
	[crop setValue:[filter valueForKey:@"outputImage"] forKey:@"inputImage"]; \
	[crop setValue:[CIVector vectorWithX:rect.origin.x Y:rect.origin.y Z:rect.size.width W:rect.size.height] forKey:@"inputRectangle"]; \
	filter = crop; \
}

//static CGRect getFilterEffectSubRegion(KCanvasFilterQuartz *quartzFilter, KCanvasFilterEffect *effect)
//{
//	// FIXME!
//	
//	/*
//	spec:
//	x, y, width and height default to the union (i.e., tightest fitting bounding box) of the subregions defined for all referenced nodes. If there are no referenced nodes (e.g., for 'feImage' or 'feTurbulence'), or one or more of the referenced nodes is a standard input (one of SourceGraphic, SourceAlpha, BackgroundImage, BackgroundAlpha, FillPaint or StrokePaint), or for 'feTile' (which is special because its principal function is to replicate the referenced node in X and Y and thereby produce a usually larger result), the default subregion is 0%,0%,100%,100%, where percentages are relative to the dimensions of the filter region.
//	*/
//	return CGRectZero;
//}

CIFilter *KCanvasFEBlendQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	CIFilter *filter = nil;
	KWQ_BLOCK_EXCEPTIONS
	
	switch(blendMode()) {
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
		NSLog(@"ERROR: Unhandled blend mode: %i", blendMode());
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
	KWQ_BLOCK_EXCEPTIONS
	switch(type()) {
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
		NSLog(@"ERROR: Unhandled ColorMatrix type: %i", type());
		return nil;
	}
	CIImage *inputImage = quartzFilter->inputImage(this);
    FE_QUARTZ_CHECK_INPUT(inputImage);
	[filter setValue:inputImage forKey:@"inputImage"];

	FE_QUARTZ_OUTPUT_RETURN;
}

/*
fillCIVectorAndBiasFromFunc(const KCComponentTransferFunction &func, KCChannelSelectorType color, CIVector *bias) {	
	float[4] vector = {0.f, 0.f, 0.f, 0.f};
	
	switch(func.type()) {
	case CT_IDENTITY:
		vector[color] = 1.f;
		break;
	case CT_TABLE:
		NSLog(@"Warning: Component Transfer \"table\" function not yet supported.");
		vector[color] = 1.f;
		break;
	case CT_DISCRETE:
		NSLog(@"Warning: Component Transfer \"discrete\" function not yet supported.");
		vector[color] = 1.f;
		break;
	case CT_LINEAR:
		vector[color] = func.slope;
		// FIXME: add intercept to bias.
		break;
	case CT_GAMMA:
		vector[color] = func.slope;
	}
	
	return [CIVector vectorWithValues:vector count:4];
}

CIFilter *KCanvasFEComponentTransferQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	FE_QUARTZ_SETUP_INPUT(@"CIColorMatrix");
	
	
	FE_QUARTZ_OUTPUT_RETURN;
	return nil;
}
*/


CIFilter *KCanvasFECompositeQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	CIFilter *filter = nil;
	KWQ_BLOCK_EXCEPTIONS
	switch(operation()) {
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
	case CO_ARITHMETIC:
		NSLog(@"Warning: arithmetic compositing mode not yet supported in Quartz.");
		// this would be where we use the k values...
		return nil;
	}
	
	[filter setDefaults];
	CIImage *inputImage = quartzFilter->inputImage(this);
	CIImage *backgroundImage = quartzFilter->imageForName(in2());
	FE_QUARTZ_CHECK_INPUT(inputImage);
	FE_QUARTZ_CHECK_INPUT(backgroundImage);
	[filter setValue:inputImage forKey:@"inputImage"];
	[filter setValue:backgroundImage forKey:@"inputBackgroundImage"];
	
	FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEFloodQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	CIFilter *filter = nil;
	KWQ_BLOCK_EXCEPTIONS
	filter = [CIFilter filterWithName:@"CIConstantColorGenerator"];
	[filter setDefaults];
	CGColorRef color = cgColor(floodColor());
	CGColorRef withAlpha = CGColorCreateCopyWithAlpha(color,CGColorGetAlpha(color) * floodOpacity());
	CIColor *inputColor = [CIColor colorWithCGColor:withAlpha];
	CGColorRelease(color);
	CGColorRelease(withAlpha);
	[filter setValue:inputColor forKey:@"inputColor"];
	
	FE_QUARTZ_CROP_TO_RECT(CGRectMake(0,0,1000,1000)); // HACK
	
	FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEImageQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	// FIXME: incomplete.
//	KCanvasImage *item = image();
//	// actually draw the item into an image...
//	CIImage *ciImage = [CIImage imageWithCGLayer:cgLayer];
//	quartzFilter->setOutputImage(this, ciImage);
	
	return nil;  // really want a noop filter... or better design.
}

CIFilter *KCanvasFEGaussianBlurQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	FE_QUARTZ_SETUP_INPUT(@"CIGaussianPyramid");
	
	float inputRadius = stdDeviationX();
	if (inputRadius != stdDeviationY()) {
		float inputAspectRatio = stdDeviationX()/stdDeviationY();
		if ( (inputAspectRatio < .5) || (inputAspectRatio > 2.0) )
			NSLog(@"WARNING: inputAspectRatio outside range supported by quartz for asymmetric Gaussian blurs! (x: %f  y: %f ratio: %f)", stdDeviationX(), stdDeviationY(), inputAspectRatio);
		[filter setValue:[NSNumber numberWithFloat:inputAspectRatio] forKey:@"inputAspectRatio"];
	}
	[filter setValue:[NSNumber numberWithFloat:inputRadius] forKey:@"inputRadius"];
	
	FE_QUARTZ_OUTPUT_RETURN;
}

CIFilter *KCanvasFEMergeQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	// Just stack a bunch of composite source over filters together...
	CIFilter *filter = nil;
	KWQ_BLOCK_EXCEPTIONS
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

CIFilter *KCanvasFETileQuartz::getCIFilter(KCanvasFilterQuartz *quartzFilter) const
{
	FE_QUARTZ_SETUP_INPUT(@"CIAffineTile");
	// no interesting parameters...
	FE_QUARTZ_OUTPUT_RETURN;
}




