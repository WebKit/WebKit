//
//  WebTextRenderer.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCore/WebCoreTextRenderer.h>

typedef struct WidthMap WidthMap;
typedef struct GlyphMap GlyphMap;

@interface WebTextRenderer : NSObject <WebCoreTextRenderer>
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

+ (BOOL)shouldBufferTextDrawing;

- initWithFont:(NSFont *)font;

@end
