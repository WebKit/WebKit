/*
 *  HIWebViewPriv.h
 *  Synergy
 *
 *  Created by Ed Voas on Tue Feb 11 2003.
 *  Copyright (c) 2003 __MyCompanyName__. All rights reserved.
 *
 */
 
#ifndef WebKit_HIWEBVIEWPRIV
#define WebKit_HIWEBVIEWPRIV

#include <Carbon/Carbon.h>
#include <AppKit/AppKit.h>
#include "HIWebView.h"

#ifdef __cplusplus
extern "C" {
#endif

extern NSView*		HIWebViewGetNSView( HIViewRef inView );

#ifdef __cplusplus
}
#endif

#endif // WebKit_HIWEBVIEWPRIV
