//
//  IFTextRenderer.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCoreTextRenderer.h>

typedef struct WidthMap WidthMap;
typedef struct GlyphMap GlyphMap;

@interface IFTextRenderer : NSObject <WebCoreTextRenderer>
{
    int ascent;
    int descent;
    int lineSpacing;
    ATSGlyphRef spaceGlyph;
    
    struct AttributeGroup *styleGroup;

@public
    NSFont *font;
    GlyphMap *characterToGlyphMap;
    WidthMap *glyphToWidthMap;
}

- initWithFont:(NSFont *)font;

// Set applyRounding = NO to get an Cocoa equivalent width.
- (float)floatWidthForCharacters:(const unichar *)characters length:(unsigned)length applyRounding: (BOOL)applyRounding attemptFontSubstitution: (BOOL)attemptFontSubstitution;

@end
