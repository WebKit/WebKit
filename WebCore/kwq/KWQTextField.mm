/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
#import <qwidget.h>

#import <KWQNSTextField.h>


@implementation KWQNSTextField

- initWithFrame: (NSRect) r widget: (QWidget *)w 
{
    [super initWithFrame: r];
    formatter = [[KWQNSTextFieldFormatter alloc] init];
    [self setFormatter: formatter];
    widget = w;

    [self setTarget: self];
    [self setAction: @selector(action:)];

    [self setDelegate: self];

    [[self cell] setScrollable: YES];
    
    return self;
}

- (void)action: sender
{
    if (widget)
        widget->emitAction(QObject::ACTION_TEXT_FIELD);
}

- (void)controlTextDidEndEditing:(NSNotification *)aNotification
{
    if (widget)
        widget->emitAction(QObject::ACTION_TEXT_FIELD_END_EDITING);
}

- (void)dealloc
{
    [formatter release];
    [super dealloc];
}


- (KWQNSTextFieldFormatter *)formatter
{
    return formatter;
}


- (void)setPasswordMode: (bool)flag
{
    if (flag != [formatter passwordMode]){
        if (flag == NO){
            [self setStringValue: @""];
            [secureField removeFromSuperview];
        } else {
            if (secureField == nil){
                secureField = [[NSSecureTextField alloc] initWithFrame: [self bounds]];
                [secureField setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
                [secureField setFormatter: formatter];
            } else {
                [secureField setBounds: [self bounds]];
            }
            [secureField setStringValue: @""];
            [secureField setDelegate:self];
            [self addSubview: secureField];
        }
        [formatter setPasswordMode: flag];
    }
}


- (void)setEditable:(BOOL)flag
{
    if (secureField != nil)
        [secureField setEditable: flag];
    [super setEditable: flag];
}


- (void)selectText:(id)sender
{
    if ([self passwordMode])
        [secureField selectText: sender];
}


- (BOOL)isEditable
{
    return [super isEditable];
}


- (bool)passwordMode
{
    return [formatter passwordMode];
}


- (void)setMaximumLength: (int)len
{
    NSString *oldValue, *truncatedValue;
    
    oldValue = [self stringValue];
    if ((int)[oldValue length] > len){
        truncatedValue = [oldValue substringToIndex: len];
        [self setStringValue: truncatedValue];
        [secureField setStringValue: truncatedValue];
    }
    [formatter setMaximumLength: len];
}


- (int)maximumLength
{
    return [formatter maximumLength];
}


- (bool)edited
{
    return edited;
}

- (void)setEdited:(bool)ed
{
    edited = ed;
}

-(void)textDidChange:(NSNotification *)aNotification
{
    edited = true;
}

-(NSString *)stringValue
{
    NSString *result;
    
    if ([self passwordMode]) {
        result = [secureField stringValue];
    }
    else {
        result = [super stringValue];
    }

    return result;
}

@end


@implementation KWQNSTextFieldFormatter

- init {
    maxLength = 2147483647;
    isPassword = NO;
    return [super init];
}

- (void)setPasswordMode: (bool)flag
{
    isPassword = flag;
}


- (bool)passwordMode
{
    return isPassword;
}


- (void)setMaximumLength: (int)len
{
    maxLength = len;
}


- (int)maximumLength
{
    return maxLength;
}


- (NSString *)stringForObjectValue:(id)anObject
{
    return (NSString *)anObject;
}


- (BOOL)getObjectValue:(id *)obj forString:(NSString *)string errorDescription:(NSString  **)error
{
    *obj = string;
    return YES;
}


- (BOOL)isPartialStringValid:(NSString *)partialString newEditingString:(NSString **)newString errorDescription:(NSString **)error
{
    if ((int)[partialString length] > maxLength){
        *newString = nil;
        return NO;
    }

    return YES;
}


- (NSAttributedString *)attributedStringForObjectValue:(id)anObject withDefaultAttributes:(NSDictionary *)attributes
{
    return nil;
}



@end

