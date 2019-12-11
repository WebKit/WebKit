//
//  DefaultPolicyDelegate.m
//  DumpRenderTree
//
//  Created by Anders Carlsson on 7/9/13.
//
//

#import "config.h"
#import "DefaultPolicyDelegate.h"

#import "DumpRenderTree.h"
#import "TestRunner.h"
#import <WebKit/WebPolicyDelegatePrivate.h>
#import <WebKit/WebViewPrivate.h>

@implementation DefaultPolicyDelegate

- (void)webView:(WebView *)webView decidePolicyForNavigationAction:(NSDictionary *)actionInformation request:(NSURLRequest *)request frame:(WebFrame *)frame decisionListener:(id <WebPolicyDecisionListener>)listener
{
    if ([WebView _canHandleRequest:request]) {
        if (![frame frameElement])
            gTestRunner->willNavigate();
        [listener use];
        return;
    }

    WebNavigationType navType = (WebNavigationType)[[actionInformation objectForKey:WebActionNavigationTypeKey] intValue];
    if (static_cast<unsigned>(navType) == static_cast<unsigned>(WebNavigationTypePlugInRequest)) {
        if (![frame frameElement])
            gTestRunner->willNavigate();
        [listener use];
        return;
    }

    // The default WebKit policy delegate passes the URL along to -[NSWorkspace openURL:] here,
    // but we don't want to do that so we just ignore the navigation completely.
    [listener ignore];
}

@end
