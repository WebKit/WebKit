/*	NSWebPageView.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Cocoa/Cocoa.h>

#import <WebKit/NSWebPageDataSource.h>

#ifdef READY_FOR_PRIMETIME
/*
    Typical usage of an WKWebPageView.
    
    NSURL *url = [NSURL URLWithString: @"http://www.apple.com"];
    ...
    WKWebPageDataSource *dataSource = [[WKWebPageDataSource alloc] initWithURL: url];
    WKWebPageView *view = [[WKWebPageView alloc] initWithFrame: myFrame];
    [view setClient: (WKWebClient *)myClient];
    [view setDataSource: dataSource];
    [[view dataSource] startLoading];
    ...
    or
    ...
    WKWebPageDataSource *dataSource = [[WKWebPageDataSource alloc] initWithURL: url];
    WKWebPageView *view = [[WKWebPageView alloc] initWithFrame: myFrame DataSource: url];
    [view setClient: (WKWebClient *)myClient];
    [[view dataSource] startLoading];
    ...
    or
    ...
    WKWebPageView *view = [[WKWebPageView alloc] initWithFrame: myFrame url: url];
    [view setClient: (WKWebClient *)myClient];
    [[view dataSource] startLoading];
    
    What is the behaviour of the view after it has been initialized and -startLoading is called?
    
        1.  When the data source is set during (i.e. -setDataSource:) -locationWillChange will be sent
            to the view's client.  It may veto by returning NO.  Note that if the convience initializers
            are used no client will have been set, and thus no chance to veto will be provided.
            
        2.  The view will do nothing until receipt of it's first -receivedDataForURL: message
            from it's data source.  Thus the view will not change it's content before users have
            a chance to cancel slow URLs.  
                        
            During this time, if -stopLoading is called on the data source, loading will 
            abort.  If the loading is stopped the contents of the window will be unchanged.
            [This implies that the data source should revert to the previous data source.
            Will that be problematic?.]  The client will receive a -loadingCancelled message
            if the load is aborted during this period.
            
            Client should initiate progress indicators at this point (how?).
            
        
        3.  After receipt of it first -receivedDataForURL: it will clear it's contents
            and perform it's first layout.  At this point a loadingStarted message will
            be sent to the client.
            
        4.  Upon every subsequent receipts of -finishedReceivingDataForURL: messages it
            will perform additional layouts.  Note that these may be coalesced, depending
            on the interval the messages are received.  During this time the load may
            be stopped.  The layout may be incomplete if the load is stopped before all
            the referenced documents are loaded.  If the layout is stopped during this
            period the client will received a -loadingStopped message.
            
        5.  When -documentReceived is received the loading and initial layout is complete.
            Clients should terminate progress indicators at this point.  At this point
            clients will receive a -loadingFinished message.

    What is the behavior of the view when a link is clicked?
    
        0.  See above.  The behavior is exactly as above.  
        
        
   ***
  
   The control behavior and view/model interdependencies of a WK is manifested by several 
   protocols (so far: WKWebDataSourceClient, WKScriptContext, WKFrameSetManagement).  This 
   behavior could be encapsulated in a WKController class, or it could be implemented by the 
   WKWebView.
*/
@interface NSWebPageView : NSView <WKWebDataSourceClient, WKScriptContext, WKFrameSetManagement>
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

- (void)setClient: (WKWebViewClient *)client;

// MCJ thinks we need high level find API.
@end


@protocol WKWebViewClient
- (BOOL)locationWillChange;
- (void)loadingCancelled;
- (void)loadingStopped;
- (void)loadingFinished;
- (void)loadingStarted;

// These reflect the WKWebDataSourceClient protocol
- (void)receivedDataForURL: (NSURL *)url;
- (void)finishedReceivingDataForURL: (NSURL *)url;
- documentReceived;

@end

/*
    Other areas still to consider:

		tool tips for ALT image text and link URLs
		
		node events
		
		context menus

		client access to form elements
		   for auto-completion, passwords

		authentication dialog

		authentication/security state (is page secure)
*/



#else
@interface NSWebPageView : NSView 
{
@private
    id _viewPrivate;
}
@end
#endif

