/*
        NSCarbonWindowContentView.m
        Application Kit
        Copyright (c) 2000-2001, Apple Computer, Inc.
        All rights reserved.
        Original Author: Mark Piccirelli
        Responsibility: Mark Piccirelli

 The class of objects that are meant to be used as _contentViews of NSCarbonWindow objects.  It's really just used to provide the context in which views that are being shown by NSViewCarbonControls can operate.
*/


#import "CarbonWindowContentView.h"
#import "CarbonWindowAdapter.h"
#import <QD/Quickdraw.h>
#import <assert.h>


// Turn off the assertions in this file.
// If this is commented out, uncomment it before committing to CVS.  M.P. Warning - 10/18/01
#undef assert
#define assert(X)


@implementation CarbonWindowContentView

@end // implementation CarbonWindowContentView
