//
//  IFTextRenderer.h
//  WebKit
//
//  Created by Darin Adler on Thu May 02 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <WebCoreTextRenderer.h>
#import <QD/ATSUnicodePriv.h>

typedef float IFGlyphWidth;

@interface IFTextRenderer : NSObject <WebCoreTextRenderer>
{
    NSFont *font;
    int ascent;
    int descent;
    int lineSpacing;
    
    ATSStyleGroupPtr styleGroup;
    ATSGlyphVector glyphVector;
    unsigned int widthCacheSize;
    IFGlyphWidth *widthCache;
    ATSGlyphRef *characterToGlyph;
    
    NSArray *substituteFontRenderers;
}

- initWithFont:(NSFont *)font;
- (NSFont *)convertCharacters: (const unichar *)characters length: (int)numCharacters glyphs: (ATSGlyphVector *)glyphs;


@end
