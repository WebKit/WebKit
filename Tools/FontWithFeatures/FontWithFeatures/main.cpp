/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#include "FontCreator.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CoreGraphics/CoreGraphics.h>
#include <CoreServices/CoreServices.h>
#include <CoreText/CoreText.h>
#include <ImageIO/ImageIO.h>
#include <fstream>

static RetainPtr<CTFontDescriptorRef> constructFontWithTrueTypeFeature(CTFontDescriptorRef fontDescriptor, int type, int selector)
{
    auto typeValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &type));
    auto selectorValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &selector));
    CFTypeRef featureDictionaryKeys[] = { kCTFontFeatureTypeIdentifierKey, kCTFontFeatureSelectorIdentifierKey };
    CFTypeRef featureDictionaryValues[] = { typeValue.get(), selectorValue.get() };
    auto featureDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureDictionaryKeys, featureDictionaryValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFTypeRef featureSettingsValues[] = { featureDictionary.get() };
    auto fontFeatureSettings = adoptCF(CFArrayCreate(kCFAllocatorDefault, featureSettingsValues, 1, &kCFTypeArrayCallBacks));

    CFTypeRef fontDescriptorKeys[] = { kCTFontFeatureSettingsAttribute };
    CFTypeRef fontDescriptorValues[] = { fontFeatureSettings.get() };
    auto fontDescriptorAttributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, fontDescriptorKeys, fontDescriptorValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    return adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, fontDescriptorAttributes.get()));
}

static RetainPtr<CTFontDescriptorRef> constructFontWithOpenTypeFeature(CTFontDescriptorRef fontDescriptor, CFStringRef feature, int value)
{
    auto featureValue = adoptCF(CFNumberCreate(kCFAllocatorDefault, kCFNumberIntType, &value));
    CFTypeRef featureDictionaryKeys[] = { kCTFontOpenTypeFeatureTag, kCTFontOpenTypeFeatureValue };
    CFTypeRef featureDictionaryValues[] = { feature, featureValue.get() };
    auto featureDictionary = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, featureDictionaryKeys, featureDictionaryValues, 2, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    CFTypeRef featureSettingsValues[] = { featureDictionary.get() };
    auto fontFeatureSettings = adoptCF(CFArrayCreate(kCFAllocatorDefault, featureSettingsValues, 1, &kCFTypeArrayCallBacks));

    CFTypeRef fontDescriptorKeys[] = { kCTFontFeatureSettingsAttribute };
    CFTypeRef fontDescriptorValues[] = { fontFeatureSettings.get() };
    auto fontDescriptorAttributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, fontDescriptorKeys, fontDescriptorValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    return adoptCF(CTFontDescriptorCreateCopyWithAttributes(fontDescriptor, fontDescriptorAttributes.get()));
}

static void drawText(CGContextRef context, CTFontDescriptorRef fontDescriptor, CFStringRef prefix, CGPoint location)
{
    CGContextSetTextMatrix(context, CGAffineTransformScale(CGAffineTransformIdentity, 1, 1));
    CGContextSetTextPosition(context, location.x, location.y);

    CGFloat fontSize = 25;
    auto font = adoptCF(CTFontCreateWithFontDescriptor(fontDescriptor, fontSize, nullptr));

    auto string = adoptCF(CFStringCreateMutable(kCFAllocatorDefault, 0));
    CFStringAppend(string.get(), prefix);
    CFStringAppend(string.get(), CFSTR("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz"));

    auto red = adoptCF(CGColorCreateGenericRGB(1, 0, 0, 1));
    CFTypeRef lineKeys[] = { kCTForegroundColorAttributeName };
    CFTypeRef lineValues[] = { red.get() };
    auto lineAttributes = adoptCF(CFDictionaryCreate(kCFAllocatorDefault, lineKeys, lineValues, 1, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

    auto attributedString = adoptCF(CFAttributedStringCreate(kCFAllocatorDefault, string.get(), lineAttributes.get()));

    auto mutableAttributedString = adoptCF(CFAttributedStringCreateMutableCopy(kCFAllocatorDefault, 0, attributedString.get()));

    auto monospaceFont = adoptCF(CTFontCreateWithName(CFSTR("Courier"), fontSize, nullptr));
    CFAttributedStringSetAttribute(mutableAttributedString.get(), CFRangeMake(0, CFStringGetLength(prefix)), kCTFontAttributeName, monospaceFont.get());

    CFAttributedStringSetAttribute(mutableAttributedString.get(), CFRangeMake(CFStringGetLength(prefix), CFStringGetLength(string.get()) - CFStringGetLength(prefix)), kCTFontAttributeName, font.get());

    auto line = adoptCF(CTLineCreateWithAttributedString(mutableAttributedString.get()));

    CTLineDraw(line.get(), context);
}

int main(int argc, const char * argv[])
{
    size_t width = 2500;
    size_t height = 2000;
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    auto context = adoptCF(CGBitmapContextCreate(nullptr, width, height, 8, width * 4, colorSpace, kCGImageAlphaNoneSkipLast));
    CGColorSpaceRelease(colorSpace);
    Type type = Type::TrueType;
    const std::vector<uint8_t> fontVector = generateFont(type);
    std::ofstream outputFile("/Volumes/Data/home/mmaxfield/src/WebKit/OpenSource/LayoutTests/css3/resources/FontWithFeatures.ttf", std::ios::out | std::ios::binary);
    for (uint8_t b : fontVector)
        outputFile << b;
    outputFile.close();
    
    auto fontData = adoptCF(CFDataCreate(kCFAllocatorDefault, fontVector.data(), fontVector.size()));
    auto fontDescriptor = adoptCF(CTFontManagerCreateFontDescriptorFromData(fontData.get()));

    if (type == Type::OpenType) {
        CFTypeRef featureValuesOpenType[] = { CFSTR("liga"), CFSTR("clig"), CFSTR("dlig"), CFSTR("hlig"), CFSTR("calt"), CFSTR("subs"), CFSTR("sups"), CFSTR("smcp"), CFSTR("c2sc"), CFSTR("pcap"), CFSTR("c2pc"), CFSTR("unic"), CFSTR("titl"), CFSTR("onum"), CFSTR("pnum"), CFSTR("tnum"), CFSTR("frac"), CFSTR("afrc"), CFSTR("ordn"), CFSTR("zero"), CFSTR("hist"), CFSTR("jp78"), CFSTR("jp83"), CFSTR("jp90"), CFSTR("jp04"), CFSTR("smpl"), CFSTR("trad"), CFSTR("fwid"), CFSTR("pwid"), CFSTR("ruby") };
        auto features = adoptCF(CFArrayCreate(kCFAllocatorDefault, featureValuesOpenType, 30, &kCFTypeArrayCallBacks));

        for (CFIndex i = 0; i < CFArrayGetCount(features.get()); ++i) {
            CFStringRef feature = static_cast<CFStringRef>(CFArrayGetValueAtIndex(features.get(), i));
            auto modifiedFontDescriptor = constructFontWithOpenTypeFeature(fontDescriptor.get(), feature, 1);
            auto prefix = adoptCF(CFStringCreateMutable(kCFAllocatorDefault, 0));
            CFStringAppend(prefix.get(), feature);
            CFStringAppend(prefix.get(), CFSTR("  (on): "));
            drawText(context.get(), modifiedFontDescriptor.get(), prefix.get(), CGPointMake(25, 1950 - 50 * i));

            modifiedFontDescriptor = constructFontWithOpenTypeFeature(fontDescriptor.get(), feature, 0);
            prefix = adoptCF(CFStringCreateMutable(kCFAllocatorDefault, 0));
            CFStringAppend(prefix.get(), feature);
            CFStringAppend(prefix.get(), CFSTR(" (off): "));
            drawText(context.get(), modifiedFontDescriptor.get(), prefix.get(), CGPointMake(25, 1925 - 50 * i));
        }
    } else {
        __block int i = 0;
        void (^handler)(uint16_t type, CFStringRef typeString, uint16_t selector, CFStringRef selectorString) = ^(uint16_t type, CFStringRef typeString, uint16_t selector, CFStringRef selectorString)
        {
            auto modifiedFontDescriptor = constructFontWithTrueTypeFeature(fontDescriptor.get(), type, selector);
            auto prefix = adoptCF(CFStringCreateMutable(kCFAllocatorDefault, 0));
            CFStringAppend(prefix.get(), typeString);
            CFStringAppend(prefix.get(), CFSTR(": "));
            CFStringAppend(prefix.get(), selectorString);
            CFStringAppend(prefix.get(), CFSTR(": "));
            while (CFStringGetLength(prefix.get()) < 65)
                CFStringAppend(prefix.get(), CFSTR(" "));
            drawText(context.get(), modifiedFontDescriptor.get(), prefix.get(), CGPointMake(25, 1950 - 40 * i));
            ++i;
        };
        handler(kLigaturesType, CFSTR("kLigaturesType"), kCommonLigaturesOnSelector, CFSTR("kCommonLigaturesOnSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kContextualLigaturesOnSelector, CFSTR("kContextualLigaturesOnSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kCommonLigaturesOffSelector, CFSTR("kCommonLigaturesOffSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kContextualLigaturesOffSelector, CFSTR("kContextualLigaturesOffSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kRareLigaturesOnSelector, CFSTR("kRareLigaturesOnSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kRareLigaturesOffSelector, CFSTR("kRareLigaturesOffSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kHistoricalLigaturesOnSelector, CFSTR("kHistoricalLigaturesOnSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kHistoricalLigaturesOffSelector, CFSTR("kHistoricalLigaturesOffSelector"));
        handler(kContextualAlternatesType, CFSTR("kContextualAlternatesType"), kContextualAlternatesOnSelector, CFSTR("kContextualAlternatesOnSelector"));
        handler(kContextualAlternatesType, CFSTR("kContextualAlternatesType"), kContextualAlternatesOffSelector, CFSTR("kContextualAlternatesOffSelector"));
        handler(kVerticalPositionType, CFSTR("kVerticalPositionType"), kInferiorsSelector, CFSTR("kInferiorsSelector"));
        handler(kVerticalPositionType, CFSTR("kVerticalPositionType"), kSuperiorsSelector, CFSTR("kSuperiorsSelector"));
        handler(kLowerCaseType, CFSTR("kLowerCaseType"), kLowerCaseSmallCapsSelector, CFSTR("kLowerCaseSmallCapsSelector"));
        handler(kUpperCaseType, CFSTR("kUpperCaseType"), kUpperCaseSmallCapsSelector, CFSTR("kUpperCaseSmallCapsSelector"));
        handler(kLowerCaseType, CFSTR("kLowerCaseType"), kLowerCasePetiteCapsSelector, CFSTR("kLowerCasePetiteCapsSelector"));
        handler(kUpperCaseType, CFSTR("kUpperCaseType"), kUpperCasePetiteCapsSelector, CFSTR("kUpperCasePetiteCapsSelector"));
        handler(kLetterCaseType, CFSTR("kLetterCaseType"), 14, CFSTR("14"));
        handler(kStyleOptionsType, CFSTR("kStyleOptionsType"), kTitlingCapsSelector, CFSTR("kTitlingCapsSelector"));
        handler(kNumberCaseType, CFSTR("kNumberCaseType"), kUpperCaseNumbersSelector, CFSTR("kUpperCaseNumbersSelector"));
        handler(kNumberCaseType, CFSTR("kNumberCaseType"), kLowerCaseNumbersSelector, CFSTR("kLowerCaseNumbersSelector"));
        handler(kNumberSpacingType, CFSTR("kNumberSpacingType"), kProportionalNumbersSelector, CFSTR("kProportionalNumbersSelector"));
        handler(kNumberSpacingType, CFSTR("kNumberSpacingType"), kMonospacedNumbersSelector, CFSTR("kMonospacedNumbersSelector"));
        handler(kFractionsType, CFSTR("kFractionsType"), kDiagonalFractionsSelector, CFSTR("kDiagonalFractionsSelector"));
        handler(kFractionsType, CFSTR("kFractionsType"), kVerticalFractionsSelector, CFSTR("kVerticalFractionsSelector"));
        handler(kVerticalPositionType, CFSTR("kVerticalPositionType"), kOrdinalsSelector, CFSTR("kOrdinalsSelector"));
        handler(kTypographicExtrasType, CFSTR("kTypographicExtrasType"), kSlashedZeroOnSelector, CFSTR("kSlashedZeroOnSelector"));
        handler(kLigaturesType, CFSTR("kLigaturesType"), kHistoricalLigaturesOnSelector, CFSTR("kHistoricalLigaturesOnSelector"));
        handler(kCharacterShapeType, CFSTR("kCharacterShapeType"), kJIS1978CharactersSelector, CFSTR("kJIS1978CharactersSelector"));
        handler(kCharacterShapeType, CFSTR("kCharacterShapeType"), kJIS1983CharactersSelector, CFSTR("kJIS1983CharactersSelector"));
        handler(kCharacterShapeType, CFSTR("kCharacterShapeType"), kJIS1990CharactersSelector, CFSTR("kJIS1990CharactersSelector"));
        handler(kCharacterShapeType, CFSTR("kCharacterShapeType"), kJIS2004CharactersSelector, CFSTR("kJIS2004CharactersSelector"));
        handler(kCharacterShapeType, CFSTR("kCharacterShapeType"), kSimplifiedCharactersSelector, CFSTR("kSimplifiedCharactersSelector"));
        handler(kCharacterShapeType, CFSTR("kCharacterShapeType"), kTraditionalCharactersSelector, CFSTR("kTraditionalCharactersSelector"));
        handler(kTextSpacingType, CFSTR("kTextSpacingType"), kMonospacedTextSelector, CFSTR("kMonospacedTextSelector"));
        handler(kTextSpacingType, CFSTR("kTextSpacingType"), kProportionalTextSelector, CFSTR("kProportionalTextSelector"));
        handler(kRubyKanaType, CFSTR("kRubyKanaType"), kRubyKanaOnSelector, CFSTR("kRubyKanaOnSelector"));
    }

    auto image = adoptCF(CGBitmapContextCreateImage(context.get()));
    auto url = adoptCF(CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/Volumes/Data/home/mmaxfield/tmp/output.png"), kCFURLPOSIXPathStyle, FALSE));
    auto imageDestination = adoptCF(CGImageDestinationCreateWithURL(url.get(), kUTTypePNG, 1, nullptr));
    CGImageDestinationAddImage(imageDestination.get(), image.get(), nullptr);
    CGImageDestinationFinalize(imageDestination.get());
    return 0;
}
