/*	
        IFWebController.h
	Copyright 2001, Apple, Inc. All rights reserved.

        Public header file.
*/
#import <Cocoa/Cocoa.h>

#ifdef READY_FOR_PRIMETIME




/*
   ============================================================================= 

    This class provides a cover for URL-based preference items. 
*/
@interface WKPreferences
+ getPreferencesForURL: (NSURL *)url;

// Encoding that will be used in none specified on page? or in header?
+ setEncoding: (NSString *)encoding;
+ (NSString *)encoding;

// Javascript preferences
- (void)setJScriptEnabled: (BOOL)flag;
- (BOOL)jScriptEnabled;

// Java preferences
- (void)setJavaEnabled: (BOOL)flag
- (BOOL)javaEnabled;

// Document refreshes allowed
- setRefreshEnabled: (BOOL)flag;
- (BOOL)refreshEnabled;

// Plugins
- (void)setPluginsEnabled: (BOOL)flag;
- (BOOL)pluginEnabled;

// Should images be loaded.
- (void)setAutoloadImages: (BOOL)flag;
- (BOOL)autoloadImages;

/*
    Specify whether only local references ( stylesheets, images, scripts, subdocuments )
    should be loaded. ( default false - everything is loaded, if the more specific
    options allow )
    This is carried over from KDE.
*/
- (void)setOnlyLocalReferences: (BOOL)flag;
- (BOOL)onlyLocalReferences;

@end


#endif

