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
#import <kwqdebug.h>
#import <kcombobox.h>

#import <KWQView.h>

KComboBox::KComboBox(QWidget *parent, const char *name)
{
    _logNotYetImplemented();
}


KComboBox::KComboBox(bool rw, QWidget *parent, const char *name)
{
    _logNotYetImplemented();
}

KComboBox::~KComboBox()
{
    _logNotYetImplemented();
}

void KComboBox::doneLoading()
{
    KWQNSComboBox *comboBox = (KWQNSComboBox *)getView();
    NSMutableArray *numberedItems;
    int i, _count = [items count];
    
    [comboBox removeAllItems];

    // Hack to allow multiple items with same name.   Ugh.
    // NSPopupButton is really stupid.  It doesn't allow
    // entries with the same title, unless you explicitly
    // set the title with setTitle: after all the entries
    // have been added.
    numberedItems = [[NSMutableArray alloc] init];
    for (i = 0; i < _count; i++)
        [numberedItems addObject: [NSString stringWithFormat: @"%d", i]];

    [comboBox addItemsWithTitles: numberedItems];

    for (i = 0; i < _count; i++){
        [[comboBox itemAtIndex: i] setTitle: [items objectAtIndex: i]];
    }
    
    [numberedItems release];
}


void KComboBox::setSize(int size)
{
    NSMutableArray *newItems = [[NSMutableArray alloc] initWithCapacity: size];
    [items release];
    items = newItems;
}

