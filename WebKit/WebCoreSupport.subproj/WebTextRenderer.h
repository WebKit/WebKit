/*	
    WebTextRenderer.h  
    Copyright 2002, Apple, Inc. All rights reserved.
*/

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
    float fixedWidth;
    BOOL isFixedPitch;
}

+ (BOOL)shouldBufferTextDrawing;

- initWithFont:(NSFont *)font;

- (float)_floatWidthForCharacters:(const UniChar *)characters stringLength:(unsigned)stringLength fromCharacterPosition: (int)pos numberOfCharacters: (int)len withPadding: (int)padding applyRounding: (BOOL)applyRounding attemptFontSubstitution: (BOOL)attemptSubstitution widths: (float *)widthBuffer fonts: (NSFont **)fontBuffer glyphs: (CGGlyph *)glyphBuffer numGlyphs: (int *)_numGlyphs letterSpacing: (int)ls wordSpacing: (int)ws;

@end
