/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ModifierKeys.h"

#import <WebKit/WebKitLegacy.h>
#import <wtf/Vector.h>

#if PLATFORM(IOS_FAMILY)
#import <WebKit/KeyEventCodesIOS.h>
#import <WebKit/WebEvent.h>
#endif

struct KeyMappingEntry {
    int macKeyCode;
    int macNumpadKeyCode;
    unichar character;
    NSString *characterName;
};

@implementation ModifierKeys

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    return self;
}

- (void)dealloc
{
    [super dealloc];
}

+ (ModifierKeys *)modifierKeysWithKey:(NSString *)character modifiers:(int)modifiers keyLocation:(unsigned)keyLocation
{
    ModifierKeys *modiferKeys = [[[ModifierKeys alloc] init] autorelease];

    modiferKeys->eventCharacter = adoptNS([[NSString alloc] initWithString:character]);
    modiferKeys->keyCode = 0;
    if ([character isEqualToString:@"leftArrow"]) {
        const unichar ch = NSLeftArrowFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x7B;
    } else if ([character isEqualToString:@"rightArrow"]) {
        const unichar ch = NSRightArrowFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x7C;
    } else if ([character isEqualToString:@"upArrow"]) {
        const unichar ch = NSUpArrowFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x7E;
    } else if ([character isEqualToString:@"downArrow"]) {
        const unichar ch = NSDownArrowFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x7D;
    } else if ([character isEqualToString:@"pageUp"]) {
        const unichar ch = NSPageUpFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x74;
    } else if ([character isEqualToString:@"pageDown"]) {
        const unichar ch = NSPageDownFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x79;
    } else if ([character isEqualToString:@"home"]) {
        const unichar ch = NSHomeFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x73;
    } else if ([character isEqualToString:@"end"]) {
        const unichar ch = NSEndFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x77;
    } else if ([character isEqualToString:@"insert"]) {
        const unichar ch = NSInsertFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x72;
    } else if ([character isEqualToString:@"delete"]) {
        const unichar ch = NSDeleteFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x75;
    } else if ([character isEqualToString:@"printScreen"]) {
        const unichar ch = NSPrintScreenFunctionKey;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x0; // There is no known virtual key code for PrintScreen.
    } else if ([character isEqualToString:@"cyrillicSmallLetterA"]) {
        const unichar ch = 0x0430;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x3; // Shares key with "F" on Russian layout.
    } else if ([character isEqualToString:@"leftControl"]) {
        const unichar ch = 0xFFE3;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x3B;
    } else if ([character isEqualToString:@"leftShift"]) {
        const unichar ch = 0xFFE1;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x38;
    } else if ([character isEqualToString:@"leftAlt"]) {
        const unichar ch = 0xFFE7;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x3A;
    } else if ([character isEqualToString:@"rightControl"]) {
        const unichar ch = 0xFFE4;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x3E;
    } else if ([character isEqualToString:@"rightShift"]) {
        const unichar ch = 0xFFE2;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x3C;
    } else if ([character isEqualToString:@"rightAlt"]) {
        const unichar ch = 0xFFE8;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x3D;
    } else if ([character isEqualToString:@"escape"]) {
        const unichar ch = 0x1B;
        modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
        modiferKeys->keyCode = 0x35;
    }

    // Compare the input string with the function-key names defined by the DOM spec (i.e. "F1",...,"F24").
    // If the input string is a function-key name, set its key code.
    for (unsigned i = 1; i <= 24; i++) {
        if ([character isEqualToString:[NSString stringWithFormat:@"F%u", i]]) {
            const unichar ch = NSF1FunctionKey + (i - 1);
            modiferKeys->eventCharacter = [NSString stringWithCharacters:&ch length:1];
            switch (i) {
            case 1: modiferKeys->keyCode = 0x7A; break;
            case 2: modiferKeys->keyCode = 0x78; break;
            case 3: modiferKeys->keyCode = 0x63; break;
            case 4: modiferKeys->keyCode = 0x76; break;
            case 5: modiferKeys->keyCode = 0x60; break;
            case 6: modiferKeys->keyCode = 0x61; break;
            case 7: modiferKeys->keyCode = 0x62; break;
            case 8: modiferKeys->keyCode = 0x64; break;
            case 9: modiferKeys->keyCode = 0x65; break;
            case 10: modiferKeys->keyCode = 0x6D; break;
            case 11: modiferKeys->keyCode = 0x67; break;
            case 12: modiferKeys->keyCode = 0x6F; break;
            case 13: modiferKeys->keyCode = 0x69; break;
            case 14: modiferKeys->keyCode = 0x6B; break;
            case 15: modiferKeys->keyCode = 0x71; break;
            case 16: modiferKeys->keyCode = 0x6A; break;
            case 17: modiferKeys->keyCode = 0x40; break;
            case 18: modiferKeys->keyCode = 0x4F; break;
            case 19: modiferKeys->keyCode = 0x50; break;
            case 20: modiferKeys->keyCode = 0x5A; break;
            }
        }
    }

    // FIXME: No keyCode is set for most keys.
    if ([character isEqualToString:@"\t"])
        modiferKeys->keyCode = 0x30;
    else if ([character isEqualToString:@" "])
        modiferKeys->keyCode = 0x31;
    else if ([character isEqualToString:@"\r"])
        modiferKeys->keyCode = 0x24;
    else if ([character isEqualToString:@"\n"])
        modiferKeys->keyCode = 0x4C;
    else if ([character isEqualToString:@"\x8"])
        modiferKeys->keyCode = 0x33;
    else if ([character isEqualToString:@"a"])
        modiferKeys->keyCode = 0x00;
    else if ([character isEqualToString:@"b"])
        modiferKeys->keyCode = 0x0B;
    else if ([character isEqualToString:@"d"])
        modiferKeys->keyCode = 0x02;
    else if ([character isEqualToString:@"e"])
        modiferKeys->keyCode = 0x0E;
    else if ([character isEqualToString:@"\x1b"])
        modiferKeys->keyCode = 0x1B;
    else if ([character isEqualToString:@":"])
        modiferKeys->keyCode = 0x29;
    else if ([character isEqualToString:@";"])
        modiferKeys->keyCode = 0x29;
    else if ([character isEqualToString:@","])
        modiferKeys->keyCode = 0x2B;

    KeyMappingEntry table[] = {
        { 0x2F, 0x41, '.', nil },
        { 0,    0x43, '*', nil },
        { 0,    0x45, '+', nil },
        { 0,    0x47, NSClearLineFunctionKey, @"clear" },
        { 0x2C, 0x4B, '/', nil },
        { 0,    0x4C, 3, @"enter" },
        { 0x1B, 0x4E, '-', nil },
        { 0x18, 0x51, '=', nil },
        { 0x1D, 0x52, '0', nil },
        { 0x12, 0x53, '1', nil },
        { 0x13, 0x54, '2', nil },
        { 0x14, 0x55, '3', nil },
        { 0x15, 0x56, '4', nil },
        { 0x17, 0x57, '5', nil },
        { 0x16, 0x58, '6', nil },
        { 0x1A, 0x59, '7', nil },
        { 0x1C, 0x5B, '8', nil },
        { 0x19, 0x5C, '9', nil },
    };
    for (unsigned i = 0; i < std::size(table); ++i) {
        NSString* currentCharacterString = [NSString stringWithCharacters:&table[i].character length:1];
        if ([character isEqualToString:currentCharacterString] || [character isEqualToString:table[i].characterName]) {
            if (keyLocation == 0x03 /*DOM_KEY_LOCATION_NUMPAD*/)
                modiferKeys->keyCode = table[i].macNumpadKeyCode;
            else
                modiferKeys->keyCode = table[i].macKeyCode;
            modiferKeys->eventCharacter = currentCharacterString;
            break;
        }
    }

    modiferKeys->charactersIgnoringModifiers = adoptNS([[NSString alloc] initWithString:modiferKeys->eventCharacter.get()]);

    modiferKeys->modifierFlags = 0;

    if ([character length] == 1 && [character characterAtIndex:0] >= 'A' && [character characterAtIndex:0] <= 'Z') {
#if !PLATFORM(IOS_FAMILY)
        modiferKeys->modifierFlags |= NSEventModifierFlagShift;
#else
        modiferKeys->modifierFlags |= WebEventFlagMaskLeftShiftKey;
#endif
        modiferKeys->charactersIgnoringModifiers = [character lowercaseString];
    }

    modiferKeys->modifierFlags |= modifiers;

    if (keyLocation == 0x03 /*DOM_KEY_LOCATION_NUMPAD*/)
        modiferKeys->modifierFlags |= NSEventModifierFlagNumericPad;
    
    return modiferKeys;
}

@end
