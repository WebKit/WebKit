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
#include <khtmlview.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

// Voodoo required to get compiler to compile correctly.
#undef DEBUG
#import <Cocoa/Cocoa.h>

//
// Create and display our widget.
//

/*
    This program illustrates the canonical method for
    creating a WebPageView.  It currently use kde and Qt
    API to create a web page view.  Eventually it will
    use WebPageView and WebKit API.
    
    The following methods will eventually create a
    web page view.
    
    url = [NSURL URLWithString: @"http://www.apple.com"];
    wpd = [[NSWebPageDocument alloc] initWithURL: url];
    wpv = [[NSWebPageView alloc] initWithFrame: NSMakeRect (0,0,500,500) document: wpd]
    
*/

int main( int argc, char **argv )
{
    QApplication app( argc, argv );

    // This will eventually be replaced with a NSURL.
    KURL url = "http://www.apple.com";
    
    // Use KHTMLPart as the model for the view.  This will eventually be covered
    // by WebPageDocument.
    // [[WebPageDocument alloc] initWithURL: (NSURL *)url];
    KHTMLPart *w = new KHTMLPart();
    w->openURL (url);

    // Create the KHTMLView.  This will eventually be covered by the
    // WebPageView. 
    // [[WebPageView alloc] initWithFrame: (NSRect)rect document: (WebPageDocument *)doc]
    KHTMLView   *htmlView = new KHTMLView (w, 0);
    
    htmlView->resize(500, 400);
 
    app.setMainWidget( htmlView );
    htmlView->show();

    return app.exec();
}
