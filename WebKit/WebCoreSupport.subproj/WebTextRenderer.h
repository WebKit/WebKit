/*	
    WebTextRenderer.h  
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebCore/WebCoreTextRenderer.h>
#import <QD/ATSUnicodePriv.h>

typedef struct WidthMap WidthMap;
typedef struct WidthEntry WidthEntry;
typedef struct GlyphMap GlyphMap;
typedef struct GlyphEntry GlyphEntry;
typedef struct UnicodeGlyphMap UnicodeGlyphMap;
typedef struct SubstituteFontWidthMap SubstituteFontWidthMap;
typedef struct CharacterWidthIterator CharacterWidthIterator;

/// Should be more than enough for normal usage.
#define NUM_SUBSTITUTE_FONT_MAPS	10

@interface WebTextRenderer : NSObject <WebCoreTextRenderer>
{
    int ascent;
    int descent;
    int lineSpacing;
    int lineGap;
    
    ATSStyleGroupPtr styleGroup;
    
@public
    NSFont *font;
    GlyphMap *characterToGlyphMap;			// Used for 16bit clean unicode characters.
    UnicodeGlyphMap *unicodeCharacterToGlyphMap; 	// Used for surrogates.
    WidthMap *glyphToWidthMap;

    BOOL treatAsFixedPitch;
    ATSGlyphRef spaceGlyph;
    float spaceWidth;
    float adjustedSpaceWidth;

    int numSubstituteFontWidthMaps;
    int maxSubstituteFontWidthMaps;
    SubstituteFontWidthMap *substituteFontWidthMaps;
    BOOL usingPrinterFont;
    BOOL isSmallCapsRenderer;
    
@private
    WebTextRenderer *smallCapsRenderer;
    NSFont *smallCapsFont;
    ATSUStyle _ATSUSstyle;
    BOOL ATSUStyleInitialized;
}

+ (BOOL)shouldBufferTextDrawing;

- (id)initWithFont:(NSFont *)font usingPrinterFont:(BOOL)usingPrinterFont;

@end


@interface WebTextRenderer (WebPrivate)

+ (void)_setAlwaysUseATSU:(BOOL)f;

@end

