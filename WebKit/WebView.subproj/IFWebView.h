/*	
        WKWebView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/WKWebDataSource.h>
#import <WebKit/WKWebController.h>

#ifdef READY_FOR_PRIMETIME
/*
    Typical usage of an WKWebView.
    
    NSURL *url = [NSURL URLWithString: @"http://www.apple.com"];
    ...
    WKWebDataSource *dataSource = [[WKWebDataSource alloc] initWithURL: url];
    WKWebView *view = [[WKWebView alloc] initWithFrame: myFrame];
    [view setDataSource: dataSource];
    [[view dataSource] startLoading];
    ...
    or
    ...
    WKWebDataSource *dataSource = [[WKWebDataSource alloc] initWithURL: url];
    WKWebView *view = [[WKWebView alloc] initWithFrame: myFrame DataSource: url];
    [[view dataSource] startLoading];
    ...
    or
    ...
    WKWebView *view = [[WKWebView alloc] initWithFrame: myFrame url: url];
    [[view dataSource] startLoading];
    
    What is the behaviour of the view after it has been initialized and -startLoading is called?
    
        1.  When the data source is set during (i.e. -setDataSource:) -locationWillChange will be sent
            to the view's controller.  It may veto by returning NO.  Note that if the convience initializers
            are used no controller will have been set, and thus no chance to veto will be provided.
            
        2.  The view will do nothing until receipt of it's first -receivedDataForURL: message
            from it's data source.  Thus the view will not change it's content before users have
            a chance to cancel slow URLs.  
                        
            During this time, if -stopLoading is called on the data source, loading will 
            abort.  If the loading is stopped the contents of the window will be unchanged.
            [This implies that the data source should revert to the previous data source.
            Will that be problematic?.]  The controller will receive a -loadingCancelled message
            if the load is aborted during this period.
            
            Controllers should initiate progress indicators at this point (how?).
        
        3.  After receipt of it first -receivedDataForURL: it will clear it's contents
            and perform it's first layout.  At this point a loadingStarted message will
            be sent to the client.
            
        4.  Upon every subsequent receipts of -finishedReceivingDataForURL: messages it
            will perform additional layouts.  Note that these may be coalesced, depending
            on the interval the messages are received.  During this time the load may
            be stopped.  The layout may be incomplete if the load is stopped before all
            the referenced documents are loaded.  If the layout is stopped during this
            period the controller will received a -loadingStopped message.
            
        5.  When -documentReceived is received the loading and initial layout is complete.
            Controllers should terminate progress indicators at this point.  At this point
            controller will receive a -loadingFinished message.

    What is the behavior of the view when a link is clicked?
    
        See above.  The behavior is exactly as above.  
        
        
   ***
  
   The control behavior and view/model interdependencies of WK are manifested by several 
   protocols (so far: WKLoadHandler, WKScriptContextHandler, WKFrameSetHandler).  This 
   behavior is encapsulated in a WKWebController class.  Behavior that may be extended/overriden 
   is described with several protocols:  WKLocationChangeHandler and WKContextMenuHandler.
   WKWebController also implements these protocols.
   
   ***   
*/
@interface WKWebView : NSView
{
@private
    id _viewPrivate;
}

- initWithFrame: (NSRect)frame;
- initWithFrame: (NSRect)frame dataSource: (NSWebPageDataSource *)dataSource;
- initWithFrame: (NSRect)frame url: (NSURL *)url;

- (NSWebPageDataSource *)dataSource;
- (void)setDataSource: (NSWebPageDataSource *)ds;

// Either creates a new datasource or reuses the same datasource for
// URLs that are qualified, i.e. URLs with an anchor target.
- (BOOL)setURL: (NSURL *)url;

// This method should not be public until we have a more completely
// understood way to subclass WKWebView.
- (void)layout;

- (void)stopAnimations;

// Font API
- (void)setFontSizes: (NSArray *)sizes;
- (NSArray *)fontSize;
- (void)resetFontSizes;
- (void)setStandardFont: (NSSFont *)font;
- (NSFont *)standardFont;
- (void)setFixedFont: (NSSFont *)font;
- (NSFont *)fixedFont;

// Drag and drop links and images.  Others?
- (void)setDragFromEnabled: (BOOL)flag;
- (BOOL)dragFromEnabled;

- (void)setDragToEnabled: (BOOL)flag;
- (BOOL)dragToEnabled;

#ifdef HAVE_WKCONTROLLER
- (void)setController: (WKWebController *)controller;
#else
- (void)setLocationChangeHandler: (WKLocationChangeHandler *)client;
- (void)setContextMenuHandler: (WKContextMenuHandler *)handler;
#endif

- (void)setEnableContextMenus: (BOOL)flag;
- (BOOL)contextMenusEnabled;
- (void)setDefaultContextMenu: (NSMenu *)menu;
- (NSMenu *)defaultContextMenu;

// MCJ thinks we need high level find API.
@end



/*
    Other areas still to consider:

        ALT image text and link URLs
            Should this be built-in?  or able to be overriden?
            
        node events
		
	client access to form elements for auto-completion, passwords
        
        selection
            Selection on data source is reflected in view.
            Does the view also need a cover selection API?
            
        subclassing of WKWebView
*/

#endif

