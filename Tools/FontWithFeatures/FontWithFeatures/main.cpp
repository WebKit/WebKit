//
//  main.cpp
//  FontWithFeatures
//
//  Created by Litherum on 9/15/15.
//  Copyright © 2015 Litherum. All rights reserved.
//

#include "FontCreator.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreServices/CoreServices.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <fstream>

void drawTextWithFeature(CGContextRef context, CTFontDescriptorRef fontDescriptor, CFStringRef feature, int value, CGPoint location)
{
    CGFloat fontSize = 25;
    CGContextSetTextMatrix(context, CGAffineTransformScale(CGAffineTransformIdentity, 1, 1));
    CGContextSetTextPosition(context, location.x, location.y);

    CFNumberRef featureValue = CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value);
    CFTypeRef featureDictionaryKeys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef featureDictionaryValues[] = { feature, featureValue };
    CFDictionaryRef featureDictionary = CFDictionaryCreate(kCFAllocatorDefault, featureDictionaryKeys, featureDictionaryValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(featureValue);

    CFTypeRef featureSettingsValues[] = { featureDictionary };
    CFArrayRef fontFeatureSettings = CFArrayCreate(kCFAllocatorDefault, featureSettingsValues, 1, &kCFTypeArrayCallBacks);
    CFRelease(featureDictionary);

    CFTypeRef fontDescriptorKeys[] = { kCTFontFeatureSettingsAttribute };
    CFTypeRef fontDescriptorValues[] = { fontFeatureSettings };
    CFDictionaryRef fontDescriptorAttributes = CFDictionaryCreate(kCFAllocatorDefault, fontDescriptorKeys, fontDescriptorValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFRelease(fontFeatureSettings);

    CTFontDescriptorRef modifiedFontDescriptor = CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, fontDescriptorAttributes);
    CFRelease(fontDescriptorAttributes);

    CTFontRef font = CTFontCreateWithFontDescriptor(modifiedFontDescriptor, fontSize, nullptr);
    CFRelease(modifiedFontDescriptor);

    CFMutableStringRef string = CFStringCreateMutable(kCFAllocatorDefault, 0);
    CFStringAppend(string, feature);
    CFStringAppend(string, value ? CFSTR("  (on)") : CFSTR(" (off)"));
    CFStringAppend(string, CFSTR(": ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));

    CGColorRef red = CGColorCreateGenericRGB(1, 0, 0, 1);
    CFTypeRef lineKeys[] = { kCTForegroundColorAttributeName };
    CFTypeRef lineValues[] = { red };
    CFDictionaryRef lineAttributes = CFDictionaryCreate(kCFAllocatorDefault, lineKeys, lineValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CGColorRelease(red);

    CFAttributedStringRef attributedString = CFAttributedStringCreate(kCFAllocatorDefault, string, lineAttributes);
    CFRelease(lineAttributes);
    CFRelease(string);

    CFMutableAttributedStringRef mutableAttributedString = CFAttributedStringCreateMutableCopy(kCFAllocatorDefault, 0, attributedString);
    CFRelease(attributedString);

    CTFontRef monospaceFont = CTFontCreateWithName(CFSTR("Courier"), fontSize, nullptr);
    CFAttributedStringSetAttribute(mutableAttributedString, CFRangeMake(0, 12), kCTFontAttributeName, monospaceFont);
    CFRelease(monospaceFont);

    CFAttributedStringSetAttribute(mutableAttributedString, CFRangeMake(12, 52), kCTFontAttributeName, font);
    CFRelease(font);

    CTLineRef line = CTLineCreateWithAttributedString(mutableAttributedString);
    CFRelease(mutableAttributedString);

    CTLineDraw(line, context);
    CFRelease(line);
}

int main(int argc, const char * argv[])
{
    size_t width = 2000;
    size_t height = 2000;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate(nullptr, width, height, 8, width * 4, colorSpace, kCGImageAlphaNoneSkipLast);
    CGColorSpaceRelease(colorSpace);
    const std::vector<uint8_t> fontVector = generateFont();
    std::ofstream outputFile("/Volumes/Data/home/mmaxfield/tmp/output.otf", std::ios::out | std::ios::binary);
    for (uint8_t b : fontVector)
        outputFile << b;
    outputFile.close();
    
    CFDataRef fontData = CFDataCreate(kCFAllocatorDefault, fontVector.data(), fontVector.size());
    CTFontDescriptorRef fontDescriptor = CTFontManagerCreateFontDescriptorFromData(fontData);
    CFRelease(fontData);

    CFTypeRef featureValues[] = { CFSTR("liga"), CFSTR("clig"), CFSTR("dlig"), CFSTR("hlig"), CFSTR("calt"), CFSTR("subs"), CFSTR("sups"), CFSTR("smcp"), CFSTR("c2sc"), CFSTR("pcap"), CFSTR("c2pc"), CFSTR("unic"), CFSTR("titl"), CFSTR("onum"), CFSTR("pnum"), CFSTR("tnum"), CFSTR("frac"), CFSTR("afrc"), CFSTR("ordn"), CFSTR("zero"), CFSTR("hist"), CFSTR("jp78"), CFSTR("jp83"), CFSTR("jp90"), CFSTR("jp04"), CFSTR("smpl"), CFSTR("trad"), CFSTR("fwid"), CFSTR("pwid"), CFSTR("ruby") };
    CFArrayRef features = CFArrayCreate(kCFAllocatorDefault, featureValues, 30, &kCFTypeArrayCallBacks);

    for (CFIndex i = 0; i < CFArrayGetCount(features); ++i) {
        drawTextWithFeature(context, fontDescriptor, static_cast<CFStringRef>(CFArrayGetValueAtIndex(features, i)), 1, CGPointMake(25, 1950 - 50 * i));
        drawTextWithFeature(context, fontDescriptor, static_cast<CFStringRef>(CFArrayGetValueAtIndex(features, i)), 0, CGPointMake(25, 1925 - 50 * i));
    }

    CFRelease(features);
    CFRelease(fontDescriptor);
    CGImageRef image = CGBitmapContextCreateImage(context);
    CGContextRelease(context);
    CFURLRef url = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/Volumes/Data/home/mmaxfield/tmp/output.png"), kCFURLPOSIXPathStyle, FALSE);
    CGImageDestinationRef imageDestination = CGImageDestinationCreateWithURL(url, kUTTypePNG, 1, nullptr);
    CFRelease(url);
    CGImageDestinationAddImage(imageDestination, image, nullptr);
    CGImageRelease(image);
    CGImageDestinationFinalize(imageDestination);
    CFRelease(imageDestination);
    return 0;
}
