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

#include <qapplication.h>
#include <qpalette.h>

#import "_KWQOwner.h"

// FIXME: 
static QPalette DEFAULT_PALETTE = QPalette();

QPalette QApplication::palette(const QWidget *p)
{
    return DEFAULT_PALETTE;
}


QWidget *QApplication::desktop()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


int QApplication::startDragDistance()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QSize QApplication::globalStrut()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QApplication::setOverrideCursor(const QCursor &c)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


void QApplication::restoreOverrideCursor()
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QApplication::sendEvent(QObject *o, QEvent *e)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}

void QApplication::sendPostedEvents(QObject *receiver, int event_type)
{
     NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QApplication::QApplication( int &argc, char **argv)
{
    _initialize();
}


void QApplication::_initialize(){
    NSDictionary *info;
    NSString *principalClassName;
    NSString *mainNibFile;

    globalPool = [[NSAutoreleasePool allocWithZone:NULL] init];
    info = [[NSBundle mainBundle] infoDictionary];
    principalClassName = [info objectForKey:@"NSPrincipalClass"];
    mainNibFile = [info objectForKey:@"NSMainNibFile"];

    if (principalClassName) {
        Class principalClass = NSClassFromString(principalClassName);
	if (principalClass) {
            application = [principalClass sharedApplication];
	    if (![NSBundle loadNibNamed: mainNibFile owner: application]) {
                NSLog (@"ERROR:  QApplication::_initialize() unable to load %@\n", mainNibFile, nil);
            }
        }
    }	
    
    //  Force linkage
    [_KWQOwner class];
}


QApplication::~QApplication()
{
    [globalPool release];
}


void QApplication::setMainWidget(QWidget *w)
{
    NSRect b = [((_KWQOwner *)application)->containerView bounds];
    
    if (application == nil){
        NSLog (@"ERROR: QApplication::setMainWidget() application not set.\n");
        return;
    }
    if (w == 0){
        NSLog (@"ERROR: QApplication::setMainWidget() widget not valid.\n");
        return;
    }
    
    NSScrollView *sv = [[NSScrollView alloc] initWithFrame: NSMakeRect (0,0,b.size.width,b.size.height)];
    [sv setAutoresizingMask: NSViewWidthSizable | NSViewHeightSizable];
    [sv setHasVerticalScroller: YES];
    [sv setHasHorizontalScroller: YES];
    [sv setDocumentView: w->getView()];
    
    [((_KWQOwner *)application)->window setOpaque: FALSE];
    [((_KWQOwner *)application)->window setAlphaValue: (float)0.8];
    
     
    [((_KWQOwner *)application)->containerView addSubview: sv];
}


int QApplication::exec()
{
    if (application == nil){
        NSLog (@"ERROR: QApplication::exec() application not set.\n");
        return 0;
    }
    [application run];
    return 1;
}

