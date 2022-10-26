/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebViewController.h"

#import "SettingsViewController.h"
#import "TabViewController.h"
#import <WebKit/WKNavigation.h>
#import <WebKit/WKNavigationDelegate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebView.h>
#import <WebKit/WKWebViewConfiguration.h>

@implementation NSURL (BundleURLMethods)
+ (NSURL *)__bundleURLForFileURL:(NSURL *)url bundle:(NSBundle *)bundle
{
    if (![url.scheme isEqualToString:@"file"])
        return nil;
    NSString *resourcePath = [bundle.resourcePath stringByAppendingString:@"/"];
    if (![url.path hasPrefix:resourcePath])
        return nil;
    NSURLComponents *bundleComponents = [[NSURLComponents alloc] init];
    bundleComponents.scheme = @"bundle";
    bundleComponents.path = [url.path substringFromIndex:resourcePath.length];
    return [bundleComponents.URL copy];
}

+ (NSURL *)__fileURLForBundleURL:(NSURL *)url bundle:(NSBundle *)bundle
{
    if (![url.scheme isEqualToString:@"bundle"])
        return nil;
    return [bundle.resourceURL URLByAppendingPathComponent:url.path];
}
@end

@interface WebViewController () <WKNavigationDelegate> {
    WKWebView *_currentWebView;
}
- (WKWebView *)createWebView;
- (void)removeWebView:(WKWebView *)webView;
- (void)setCurrentWebView:(WKWebView *)webView;
@end

void* EstimatedProgressContext = &EstimatedProgressContext;
void* TitleContext = &TitleContext;
void* URLContext = &URLContext;

@implementation WebViewController

- (void)viewDidLoad
{
    [super viewDidLoad];
    self.webViews = [[NSMutableArray alloc] initWithCapacity:1];
    self.tabViewController = [self.storyboard instantiateViewControllerWithIdentifier:@"idTabViewController"];
    self.tabViewController.parent = self;
    self.tabViewController.modalPresentationStyle = UIModalPresentationPopover;
    
    self.settingsViewController = [self.storyboard instantiateViewControllerWithIdentifier:@"idSettingsViewController"];
    self.settingsViewController.parent = self;
    self.settingsViewController.modalPresentationStyle = UIModalPresentationPopover;

    WKWebView *webView = [self createWebView];
    [webView loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:[self.settingsViewController defaultURL]]]];
    [self setCurrentWebView:webView];
}


- (void)didReceiveMemoryWarning
{
    [super didReceiveMemoryWarning];
}

#pragma mark Actions

- (IBAction)reload:(id)sender
{
    [self.currentWebView reload];
}

- (IBAction)goBack:(id)sender
{
    [self.currentWebView goBack];
}

- (IBAction)goForward:(id)sender
{
    [self.currentWebView goForward];
}

- (IBAction)urlFieldEditingBegan:(id)sender
{
    self.urlField.selectedTextRange = [self.urlField textRangeFromPosition:self.urlField.beginningOfDocument toPosition:self.urlField.endOfDocument];
}

- (IBAction)navigateTo:(id)sender
{
    [self.urlField resignFirstResponder];
    NSString* requestedDestination = self.urlField.text;
    if ([requestedDestination rangeOfString:@"^[\\p{Alphabetic}]+:" options:(NSRegularExpressionSearch | NSCaseInsensitiveSearch | NSAnchoredSearch)].location == NSNotFound)
        requestedDestination = [@"http://" stringByAppendingString:requestedDestination];
    NSURL* requestedURL = [NSURL URLWithString:requestedDestination];
    if ([requestedURL.scheme isEqualToString:@"bundle"]) {
        NSBundle *frameworkBundle = [NSBundle bundleForClass:[WebViewController class]];
        requestedURL = [NSURL __fileURLForBundleURL:requestedURL bundle:frameworkBundle];
        [self.currentWebView loadFileURL:requestedURL allowingReadAccessToURL:frameworkBundle.resourceURL];
    }
    [self.currentWebView loadRequest:[NSURLRequest requestWithURL:requestedURL]];
}

- (IBAction)showTabs:(id)sender
{
    [self presentViewController:self.tabViewController animated:YES completion:nil];
    self.tabViewController.popoverPresentationController.barButtonItem = self.tabButton;
}

- (IBAction)showSettings:(id)sender
{
    [self presentViewController:self.settingsViewController animated:YES completion:nil];
    self.settingsViewController.popoverPresentationController.barButtonItem = self.settingsButton;
}

#pragma mark Public methods

@dynamic currentWebView;

- (WKWebView *)currentWebView
{
    return _currentWebView;
}

- (void)setCurrentWebView:(WKWebView *)webView
{
    [_currentWebView removeObserver:self forKeyPath:@"estimatedProgress" context:EstimatedProgressContext];
    [_currentWebView removeObserver:self forKeyPath:@"URL" context:URLContext];
    [_currentWebView removeFromSuperview];

    _currentWebView = webView;

    [_currentWebView addObserver:self forKeyPath:@"estimatedProgress" options:(NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew) context:EstimatedProgressContext];
    [_currentWebView addObserver:self forKeyPath:@"URL" options:(NSKeyValueObservingOptionInitial | NSKeyValueObservingOptionNew) context:URLContext];
    [self.webViewContainer addSubview:_currentWebView];
}

- (void)selectWebViewAtIndex:(NSUInteger)index
{
    if (index >= self.webViews.count)
        return;
    self.currentWebView = self.webViews[index];
}

- (void)removeWebViewAtIndex:(NSUInteger)index
{
    if (index >= self.webViews.count)
        return;
    [self removeWebView:self.webViews[index]];
}

- (void)addWebView
{
    self.currentWebView = [self createWebView];
}

- (NSURL *)currentURL
{
    return self.currentWebView.URL;
}

#pragma mark Internal methods

- (WKWebView *)createWebView
{
    WKWebViewConfiguration *configuration = [[WKWebViewConfiguration alloc] init];

    configuration.preferences._mockCaptureDevicesEnabled = YES;

    WKWebView *webView = [[WKWebView alloc] initWithFrame:self.webViewContainer.bounds configuration:configuration];
    webView.inspectable = YES;
    webView.navigationDelegate = self;
    webView.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
    [webView addObserver:self forKeyPath:@"title" options:NSKeyValueObservingOptionNew context:TitleContext];
    [self.webViews addObject:webView];

    [self.tabViewController.tableView reloadData];

    return webView;
}

- (void)removeWebView:(WKWebView *)webView
{
    NSUInteger index = [self.webViews indexOfObject:webView];
    if (index != NSNotFound) {
        [self.webViews[index] removeObserver:self forKeyPath:@"title" context:TitleContext];
        [self.webViews removeObjectAtIndex:index];
    } if (!self.webViews.count)
        [self createWebView];
    if (index >= self.webViews.count)
        index = self.webViews.count - 1;
    [self setCurrentWebView:self.webViews[index]];

    [self.tabViewController.tableView reloadData];
}

#pragma mark Navigation Delegate

- (void)webView:(WKWebView *)webView didFailNavigation:(null_unspecified WKNavigation *)navigation withError:(NSError *)error
{
    [webView loadHTMLString:[error description] baseURL:nil];
}

#pragma mark KVO

- (void)observeValueForKeyPath:(NSString *)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    if (context == EstimatedProgressContext) {
        float value = [[change valueForKey:NSKeyValueChangeNewKey] floatValue];
        [self.progressView setProgress:value animated:YES];
    } else if (context == URLContext) {
        NSURL *newURLValue = [change valueForKey:NSKeyValueChangeNewKey];
        if ([newURLValue isKindOfClass:[NSURL class]]) {
            if ([newURLValue.scheme isEqualToString:@"file"])
                newURLValue = [NSURL __bundleURLForFileURL:newURLValue bundle:[NSBundle bundleForClass:[WebViewController class]]];
            self.urlField.text = [newURLValue absoluteString];
        } else if ([newURLValue isKindOfClass:[NSNull class]])
            self.urlField.text = @"";
    } else if (context == TitleContext)
        [self.tabViewController.tableView reloadData];
    else
        [super observeValueForKeyPath:keyPath ofObject:object change:change context:context];
}

@end
