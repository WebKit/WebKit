/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

WebInspector.KeyboardShortcut = function()
{
};

/**
 * Constants for encoding modifier key set as a bit mask.
 * @see #_makeKeyFromCodeAndModifiers
 */
WebInspector.KeyboardShortcut.Modifiers = {
    None: 0,   // Constant for empty modifiers set.
    Shift: 1,
    Ctrl: 2,
    Alt: 4,
    Meta: 8    // Command key on Mac, Win key on other platforms.
};

WebInspector.KeyboardShortcut.KeyCodes = {
    Backspace: 8,
    Tab: 9,
    Esc: 27,
    Space: 32,
    PageUp: 33,      // also NUM_NORTH_EAST
    PageDown: 34,    // also NUM_SOUTH_EAST
    End: 35,         // also NUM_SOUTH_WEST
    Home: 36,        // also NUM_NORTH_WEST
    Left: 37,        // also NUM_WEST
    Up: 38,          // also NUM_NORTH
    Right: 39,       // also NUM_EAST
    Down: 40,        // also NUM_SOUTH
    Delete: 46,
    Zero: 48,
    F1: 112,
    F2: 113,
    F3: 114,
    F4: 115,
    F5: 116,
    F6: 117,
    F7: 118,
    F8: 119,
    F9: 120,
    F10: 121,
    F11: 122,
    F12: 123,
    Semicolon: 186,    // ;
    Plus: 187,         // +
    Comma: 188,        // ,
    Minus: 189,        // -
    Period: 190,       // .
    Slash: 191,        // /
    Apostrophe: 192,   // `
    SingleQuote: 222   // '
};

/**
 * Creates a number encoding keyCode in the lower 8 bits and modifiers mask in the higher 8 bits.
 * It is useful for matching pressed keys.
 * keyCode is the Code of the key, or a character "a-z" which is converted to a keyCode value.
 * optModifiers is an Optional list of modifiers passed as additional paramerters.
 */
WebInspector.KeyboardShortcut.makeKey = function(keyCode, optModifiers)
{
    if (typeof keyCode === "string")
        keyCode = keyCode.charCodeAt(0) - 32;
    var modifiers = WebInspector.KeyboardShortcut.Modifiers.None;
    for (var i = 1; i < arguments.length; i++)
        modifiers |= arguments[i];
    return WebInspector.KeyboardShortcut._makeKeyFromCodeAndModifiers(keyCode, modifiers);
};

WebInspector.KeyboardShortcut.makeKeyFromEvent = function(keyboardEvent)
{
    var modifiers = WebInspector.KeyboardShortcut.Modifiers.None;
    if (keyboardEvent.shiftKey)
        modifiers |= WebInspector.KeyboardShortcut.Modifiers.Shift;
    if (keyboardEvent.ctrlKey)
        modifiers |= WebInspector.KeyboardShortcut.Modifiers.Ctrl;
    if (keyboardEvent.altKey)
        modifiers |= WebInspector.KeyboardShortcut.Modifiers.Alt;
    if (keyboardEvent.metaKey)
        modifiers |= WebInspector.KeyboardShortcut.Modifiers.Meta;
    return WebInspector.KeyboardShortcut._makeKeyFromCodeAndModifiers(keyboardEvent.keyCode, modifiers);
};

WebInspector.KeyboardShortcut._makeKeyFromCodeAndModifiers = function(keyCode, modifiers)
{
    return (keyCode & 255) | (modifiers << 8);
};
