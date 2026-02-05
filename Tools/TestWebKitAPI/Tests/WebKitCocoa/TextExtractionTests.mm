/*
 * Copyright (C) 2025-2026 Apple Inc. All rights reserved.
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

#import "config.h"

#import "HTTPServer.h"
#import "InstanceMethodSwizzler.h"
#import "JSHandlePlugInProtocol.h"
#import "PlatformUtilities.h"
#import "SafeBrowsingSPI.h"
#import "Test.h"
#import "TestNavigationDelegate.h"
#import "TestWKWebView.h"
#import "Utilities.h"
#import "WKWebViewConfigurationExtras.h"
#import <WebKit/WKContentWorldPrivate.h>
#import <WebKit/WKFrameInfoPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewConfigurationPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKContentWorldConfiguration.h>
#import <WebKit/_WKFrameTreeNode.h>
#import <WebKit/_WKJSHandle.h>
#import <WebKit/_WKRemoteObjectInterface.h>
#import <WebKit/_WKRemoteObjectRegistry.h>
#import <WebKit/_WKTextExtraction.h>
#import <pal/spi/cocoa/NSKeyedUnarchiverSPI.h>
#import <wtf/SoftLinking.h>
#import <wtf/WorkQueue.h>
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/text/MakeString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(SafariSafeBrowsing);
SOFT_LINK_CLASS(SafariSafeBrowsing, SSBLookupContext);

@interface WKWebView (TextExtractionTests)
- (NSString *)synchronouslyGetDebugText:(_WKTextExtractionConfiguration *)configuration;
- (_WKTextExtractionResult *)synchronouslyExtractDebugTextResult:(_WKTextExtractionConfiguration *)configuration;
- (_WKTextExtractionInteractionResult *)synchronouslyPerformInteraction:(_WKTextExtractionInteraction *)interaction;
- (NSData *)synchronouslyGetSelectorPathDataForNode:(_WKJSHandle *)node;
- (_WKJSHandle *)synchronouslyGetNodeForSelectorPathData:(NSData *)data;
@end

@interface _WKTextExtractionInteraction (TextExtractionTests)
- (NSString *)debugDescriptionInWebView:(WKWebView *)webView error:(NSError **)outError;
@end

@interface _WKTextExtractionResult (TextExtractionTests)
- (_WKJSHandle *)jsHandleForNodeIdentifier:(NSString *)nodeIdentifier searchText:(NSString *)searchText;
@end

@implementation WKWebView (TextExtractionTests)

- (NSString *)synchronouslyGetDebugText:(_WKTextExtractionConfiguration *)configuration
{
    return [[self synchronouslyExtractDebugTextResult:configuration] textContent];
}

- (_WKTextExtractionResult *)synchronouslyExtractDebugTextResult:(_WKTextExtractionConfiguration *)configuration
{
    RetainPtr configurationToUse = configuration;
    if (!configurationToUse)
        configurationToUse = adoptNS([_WKTextExtractionConfiguration new]);

    __block bool done = false;
    __block RetainPtr<_WKTextExtractionResult> result;
    [self _extractDebugTextWithConfiguration:configurationToUse.get() completionHandler:^(_WKTextExtractionResult *extractionResult) {
        result = extractionResult;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (_WKTextExtractionInteractionResult *)synchronouslyPerformInteraction:(_WKTextExtractionInteraction *)interaction
{
    __block bool done = false;
    __block RetainPtr<_WKTextExtractionInteractionResult> result;
    [self _performInteraction:interaction completionHandler:^(_WKTextExtractionInteractionResult *theResult) {
        result = theResult;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (NSData *)synchronouslyGetSelectorPathDataForNode:(_WKJSHandle *)node
{
    __block bool done = false;
    __block RetainPtr<NSData> result;
    [self _getSelectorPathDataForNode:node completionHandler:^(NSData *data) {
        result = data;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

- (_WKJSHandle *)synchronouslyGetNodeForSelectorPathData:(NSData *)data
{
    __block bool done = false;
    __block RetainPtr<_WKJSHandle> result;
    [self _getNodeForSelectorPathData:data completionHandler:^(_WKJSHandle *handle) {
        result = handle;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

@implementation _WKTextExtractionInteraction (TextExtractionTests)

- (NSString *)debugDescriptionInWebView:(WKWebView *)webView error:(NSError **)outError
{
    __block bool done = false;
    __block RetainPtr<NSString> result;
    __block RetainPtr<NSError> error;
    [self debugDescriptionInWebView:webView completionHandler:^(NSString *description, NSError *theError) {
        result = description;
        error = theError;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    if (outError)
        *outError = error.autorelease();
    return result.autorelease();
}

@end

@implementation _WKTextExtractionResult (TextExtractionTests)

- (_WKJSHandle *)jsHandleForNodeIdentifier:(NSString *)nodeIdentifier searchText:(NSString *)searchText
{
    __block bool done = false;
    __block RetainPtr<_WKJSHandle> result;
    [self requestJSHandleForNodeIdentifier:nodeIdentifier searchText:searchText completionHandler:^(_WKJSHandle *handle) {
        result = handle;
        done = true;
    }];
    TestWebKitAPI::Util::run(&done);
    return result.autorelease();
}

@end

@interface JSHandleReceiver : NSObject<JSHandlePlugInProtocol>
@property (nonatomic, copy) void (^dictionaryReceiver)(NSDictionary *);
@end
@implementation JSHandleReceiver
- (void)receiveDictionaryFromWebProcess:(NSDictionary *)dictionary
{
    _dictionaryReceiver(dictionary);
}
@end

namespace TestWebKitAPI {

static NSString *extractNodeIdentifier(NSString *debugText, NSString *searchText)
{
    for (NSString *line in [debugText componentsSeparatedByString:@"\n"]) {
        if (![line containsString:searchText])
            continue;

        RetainPtr regex = [NSRegularExpression regularExpressionWithPattern:@"uid=(((?:\\d+_)+)?\\d+)" options:0 error:nil];
        RetainPtr match = [regex firstMatchInString:line options:0 range:NSMakeRange(0, line.length)];
        if (!match)
            continue;

        NSRange identifierRange = [match rangeAtIndex:1];
        return [line substringWithRange:identifierRange];
    }

    return nil;
}

#if PLATFORM(MAC)

TEST(TextExtractionTests, SelectPopupMenu)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setTextExtractionEnabled:YES];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr debugText = [webView synchronouslyGetDebugText:nil];
    RetainPtr click = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);
    [click setNodeIdentifier:extractNodeIdentifier(debugText.get(), @"menu")];

    __block bool doneSelectingOption = false;
    __block RetainPtr<NSString> debugTextAfterClickingSelect;
    [webView _performInteraction:click.get() completionHandler:^(_WKTextExtractionInteractionResult *clickResult) {
        EXPECT_FALSE(clickResult.error);
        EXPECT_TRUE([[webView synchronouslyGetDebugText:nil] containsString:@"nativePopupMenu"]);

        RetainPtr selectOption = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionSelectMenuItem]);
        [selectOption setText:@"Three"];
        [webView _performInteraction:selectOption.get() completionHandler:^(_WKTextExtractionInteractionResult *selectOptionResult) {
            EXPECT_FALSE(selectOptionResult.error);
            doneSelectingOption = true;
        }];
    }];

    Util::run(&doneSelectingOption);
    EXPECT_WK_STREQ("Three", [webView stringByEvaluatingJavaScript:@"select.value"]);
}

#endif // PLATFORM(MAC)

TEST(TextExtractionTests, InteractionDebugDescription)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setTextExtractionEnabled:YES];

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr debugText = [webView synchronouslyGetDebugText:nil];
    RetainPtr testButtonID = extractNodeIdentifier(debugText.get(), @"Test");
    RetainPtr emailID = extractNodeIdentifier(debugText.get(), @"email");
    RetainPtr composeID = extractNodeIdentifier(debugText.get(), @"Compose");
    RetainPtr selectID = extractNodeIdentifier(debugText.get(), @"select");

#if ENABLE(TEXT_EXTRACTION_FILTER)
    EXPECT_FALSE([debugText containsString:@"crazy ones"]);
#endif

    NSError *error = nil;
    NSString *description = nil;
    {
        RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);

        [interaction setNodeIdentifier:testButtonID.get()];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ("Click on button labeled “Click Me”, with rendered text “Test”", description);
        EXPECT_NULL(error);

        [interaction setNodeIdentifier:emailID.get()];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ("Click on input of type email with placeholder “Recipient address”", description);
        EXPECT_NULL(error);

        [interaction setNodeIdentifier:composeID.get()];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ("Click on editable div labeled “Compose a new message”, with rendered text “Subject  'The quick brown fox jumped over the lazy dog'”, containing child labeled “Heading”", description);
        EXPECT_NULL(error);
    }
    {
        RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionTextInput]);

        [interaction setNodeIdentifier:emailID.get()];
        [interaction setText:@"squirrelfish@webkit.org"];
        [interaction setReplaceAll:YES];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ("Enter text “squirrelfish@webkit.org” into input of type email with placeholder “Recipient address”, replacing any existing content", description);
        EXPECT_NULL(error);

        [interaction setNodeIdentifier:composeID.get()];
        [interaction setText:@"«Testing»"];
        [interaction setReplaceAll:NO];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ("Enter text “'Testing'” into editable div labeled “Compose a new message”, with rendered text “Subject  'The quick brown fox jumped over the lazy dog'”, containing child labeled “Heading”", description);
        EXPECT_NULL(error);
    }
    {
        RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionSelectMenuItem]);
        [interaction setNodeIdentifier:selectID.get()];
        [interaction setText:@"Three"];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ("Select menu item “Three” in select with role “menu”", description);
        EXPECT_NULL(error);
    }
    {
        RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);
        auto clickLocation = [webView elementMidpointFromSelector:@"#test-button"];
        [interaction setLocation:clickLocation];
        description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        RetainPtr expectedString = [NSString stringWithFormat:@"Click at coordinates (%.0f, %.0f) on child node of button labeled “Click Me”, with rendered text “Test”", clickLocation.x, clickLocation.y];
        EXPECT_WK_STREQ(expectedString.get(), description);
        EXPECT_NULL(error);
    }
}

TEST(TextExtractionTests, TargetNodeAndClientAttributes)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);
    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];
    [webView evaluateJavaScript:@"getSelection().selectAllChildren(document.querySelector('h3[aria-label=\"Heading\"]'))" completionHandler:nil];

    RetainPtr world = [WKContentWorld _worldWithConfiguration:^{
        RetainPtr configuration = adoptNS([_WKContentWorldConfiguration new]);
        [configuration setAllowJSHandleCreation:YES];
        return configuration.autorelease();
    }()];

    RetainPtr editorHandle = [webView querySelector:@"div[contenteditable]" frame:nil world:world.get()];
    RetainPtr headingHandle = [webView querySelector:@"h3[aria-label='Heading']" frame:nil world:world.get()];
    RetainPtr debugText = [webView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setTargetNode:editorHandle.get()];
        [configuration addClientAttribute:@"extra-data-1" value:@"abc" forNode:editorHandle.get()];
        [configuration addClientAttribute:@"extra-data-1" value:@"123" forNode:headingHandle.get()];
        [configuration addClientAttribute:@"extra-data-2" value:@"xyz" forNode:headingHandle.get()];
        return configuration.autorelease();
    }()];

    EXPECT_TRUE([debugText containsString:@"Compose a new message"]);
    EXPECT_TRUE([debugText containsString:@"aria-label='Compose a new message',extra-data-1='abc'"]);
    EXPECT_TRUE([debugText containsString:@"aria-label='Heading',extra-data-1='123',extra-data-2='xyz'"]);
    EXPECT_TRUE([debugText containsString:@"Subject"]);
    EXPECT_TRUE([debugText containsString:@"The quick brown fox jumped over the lazy dog"]);
    EXPECT_FALSE([debugText containsString:@"select,"]);
    EXPECT_FALSE([debugText containsString:@"Click Me"]);
    EXPECT_FALSE([debugText containsString:@"Recipient address"]);
}

TEST(TextExtractionTests, ReplacementStrings)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);
    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr debugTextWithoutReplacements = [webView synchronouslyGetDebugText:nil];
    EXPECT_TRUE([debugTextWithoutReplacements containsString:@"The quick brown fox jumped over the lazy dog"]);

    RetainPtr debugTextWithReplacements = [webView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setReplacementStrings:@{
            @"fox": @"cat",
            @"dog": @"mouse",
            @"lazy": @""
        }];
        return configuration.autorelease();
    }()];

    EXPECT_FALSE([debugTextWithReplacements containsString:@"fox"]);
    EXPECT_FALSE([debugTextWithReplacements containsString:@"dog"]);
    EXPECT_FALSE([debugTextWithReplacements containsString:@"lazy"]);
    EXPECT_TRUE([debugTextWithReplacements containsString:@"The quick brown cat jumped over the  mouse"]);
}

TEST(TextExtractionTests, VisibleTextOnly)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);
    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr debugText = [webView synchronouslyGetDebugText:[_WKTextExtractionConfiguration configurationForVisibleTextOnly]];

    EXPECT_TRUE([debugText containsString:@"Test"]);
    EXPECT_TRUE([debugText containsString:@"foo"]);
    EXPECT_TRUE([debugText containsString:@"Subject"]);
    EXPECT_TRUE([debugText containsString:@"“The quick brown fox jumped over the lazy dog”"]);
    EXPECT_TRUE([debugText containsString:@"0"]);
#if ENABLE(TEXT_EXTRACTION_FILTER)
    EXPECT_FALSE([debugText containsString:@"Here’s to the crazy ones"]);
    EXPECT_FALSE([debugText containsString:@"The round pegs in the square holes"]);
    EXPECT_FALSE([debugText containsString:@"The ones who see things differently"]);
    EXPECT_FALSE([debugText containsString:@"And they have no respect for the status quo"]);
    EXPECT_FALSE([debugText containsString:@"They push the human race forward"]);
    EXPECT_FALSE([debugText containsString:@"Because the people who are crazy"]);
#endif // ENABLE(TEXT_EXTRACTION_FILTER)
}

TEST(TextExtractionTests, FilterOptions)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);
    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    auto extractTextWithFilterOptions = [webView](_WKTextExtractionFilterOptions options) {
        return [webView synchronouslyExtractDebugTextResult:^{
            RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
            [configuration setFilterOptions:options];
            return configuration.autorelease();
        }()];
    };

    {
        RetainPtr result = extractTextWithFilterOptions(_WKTextExtractionFilterNone);
        EXPECT_TRUE([[result textContent] containsString:@"“The quick brown fox jumped over the lazy dog”"]);
        EXPECT_TRUE([[result textContent] containsString:@"Here’s to the crazy ones"]);
        EXPECT_FALSE([result filteredOutAnyText]);
    }
    {
        RetainPtr result = extractTextWithFilterOptions(_WKTextExtractionFilterTextRecognition);
        EXPECT_TRUE([[result textContent] containsString:@"“The quick brown fox jumped over the lazy dog”"]);
#if ENABLE(TEXT_EXTRACTION_FILTER)
        EXPECT_FALSE([[result textContent] containsString:@"Here’s to the crazy ones"]);
        EXPECT_TRUE([result filteredOutAnyText]);
#endif
    }
    {
        RetainPtr result = extractTextWithFilterOptions(_WKTextExtractionFilterClassifier);
        EXPECT_TRUE([[result textContent] containsString:@"“The quick brown fox jumped over the lazy dog”"]);
        EXPECT_TRUE([[result textContent] containsString:@"Here’s to the crazy ones"]);
        EXPECT_FALSE([result filteredOutAnyText]);
    }
}

TEST(TextExtractionTests, FilterRedundantTextInLinks)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);

    [webView synchronouslyLoadHTMLString:@"<body>"
        "<a class='first' href='http://apple.com'>apple</a>"
        "<a class='second' href='http://webkit.org'>webkit</a>"
        "</body>"];

    RetainPtr world = [WKContentWorld _worldWithConfiguration:^{
        RetainPtr configuration = adoptNS([_WKContentWorldConfiguration new]);
        [configuration setAllowJSHandleCreation:YES];
        return configuration.autorelease();
    }()];

    RetainPtr debugText = [webView synchronouslyGetDebugText:^{
        RetainPtr firstLink = [webView querySelector:@".first" frame:nil world:world.get()];
        RetainPtr secondLink = [webView querySelector:@".second" frame:nil world:world.get()];
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setIncludeURLs:NO];
        [configuration setIncludeRects:NO];
        [configuration setNodeIdentifierInclusion:_WKTextExtractionNodeIdentifierInclusionNone];
        [configuration addClientAttribute:@"href" value:@"url1.com" forNode:firstLink.get()];
        [configuration addClientAttribute:@"href" value:@"webkit.org" forNode:secondLink.get()];
        return configuration.autorelease();
    }()];

    EXPECT_TRUE([debugText containsString:@"link,href='url1.com','apple'"]);
    EXPECT_TRUE([debugText containsString:@"link,href='webkit.org'"]);
}

TEST(TextExtractionTests, NodesToSkip)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);

    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr world = [WKContentWorld _worldWithConfiguration:^{
        RetainPtr configuration = adoptNS([_WKContentWorldConfiguration new]);
        [configuration setAllowJSHandleCreation:YES];
        return configuration.autorelease();
    }()];

    RetainPtr selectHandle = [webView querySelector:@"select" frame:nil world:world.get()];
    RetainPtr inputHandle = [webView querySelector:@"input[type=email]" frame:nil world:world.get()];
    RetainPtr editorHandle = [webView querySelector:@"div[contenteditable]" frame:nil world:world.get()];
    RetainPtr hiddenTextHandle = [webView querySelector:@"h4" frame:nil world:world.get()];
    RetainPtr debugText = [webView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setNodesToSkip:@[editorHandle.get(), selectHandle.get(), inputHandle.get(), hiddenTextHandle.get()]];
        [configuration setOutputFormat:_WKTextExtractionOutputFormatMarkdown];
        [configuration setNodeIdentifierInclusion:_WKTextExtractionNodeIdentifierInclusionNone];
        [configuration setIncludeRects:NO];
        return configuration.autorelease();
    }()];

    NSArray<NSString *> *lines = [debugText componentsSeparatedByString:@"\n"];
    EXPECT_EQ([lines count], 2u);
    EXPECT_WK_STREQ("Test", lines[0]);
    EXPECT_WK_STREQ("0", lines[1]);
}

TEST(TextExtractionTests, RequestJSHandleForNodeIdentifier)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);

    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr extractionResult = [webView synchronouslyExtractDebugTextResult:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setIncludeRects:NO];
        [configuration setIncludeURLs:NO];
        return configuration.autorelease();
    }()];

    RetainPtr debugTextForSubject = [webView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setIncludeRects:NO];
        [configuration setIncludeURLs:NO];
        [configuration setNodeIdentifierInclusion:_WKTextExtractionNodeIdentifierInclusionNone];

        RetainPtr nodeID = extractNodeIdentifier([extractionResult textContent], @"Compose a new message");
        [configuration setTargetNode:[extractionResult jsHandleForNodeIdentifier:nodeID.get() searchText:@"Subject"]];
        return configuration.autorelease();
    }()];

    EXPECT_WK_STREQ(debugTextForSubject.get(), @"root\n\taria-label='Heading','Subject'");

    RetainPtr debugTextForBody = [webView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setIncludeRects:NO];
        [configuration setIncludeURLs:NO];
        [configuration setNodeIdentifierInclusion:_WKTextExtractionNodeIdentifierInclusionNone];
        [configuration setTargetNode:[extractionResult jsHandleForNodeIdentifier:nil searchText:@"The quick brown fox"]];
        return configuration.autorelease();
    }()];

    EXPECT_WK_STREQ(debugTextForBody.get(), @"root,'“The quick brown fox jumped over the lazy dog”'");
}

TEST(TextExtractionTests, ResolveTargetNodeFromSelectorData)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    [[configuration preferences] _setTextExtractionEnabled:YES];

    RetainPtr selectorData = [&] {
        RetainPtr originalWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
        [originalWebView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

        RetainPtr world = [WKContentWorld _worldWithConfiguration:^{
            RetainPtr configuration = adoptNS([_WKContentWorldConfiguration new]);
            [configuration setAllowJSHandleCreation:YES];
            return configuration.autorelease();
        }()];

        RetainPtr subjectHandle = [originalWebView querySelector:@"h3" frame:nil world:world.get()];
        return [originalWebView synchronouslyGetSelectorPathDataForNode:subjectHandle.get()];
    }();

    EXPECT_NOT_NULL(selectorData.get());

    RetainPtr newWebView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);
    [newWebView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr resolvedHandle = [newWebView synchronouslyGetNodeForSelectorPathData:selectorData.get()];
    EXPECT_NOT_NULL(resolvedHandle.get());

    RetainPtr debugText = [newWebView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setIncludeRects:NO];
        [configuration setIncludeURLs:NO];
        [configuration setNodeIdentifierInclusion:_WKTextExtractionNodeIdentifierInclusionNone];
        [configuration setTargetNode:resolvedHandle.get()];
        return configuration.autorelease();
    }()];

    EXPECT_WK_STREQ(debugText.get(), @"root\n\taria-label='Heading','Subject'");
}

#if HAVE(SAFARI_SAFE_BROWSING_NAMESPACED_LISTS)

typedef void(^WKSafeBrowsingNamespacedListBlock)(NSDictionary<NSString *, NSArray<NSString *> *> *, NSError *);
static void overrideGetListsForNamespace(SSBLookupContext *instance, SEL, NSString *, NSString *, WKSafeBrowsingNamespacedListBlock completion)
{
    static NeverDestroyed workQueue = WorkQueue::create("Queue for simulating SSB API"_s);
    workQueue.get()->dispatch([completion = makeBlockPtr(completion)] {
        RetainPtr hardCodedResult = @{
            @"test1/domains": @[ @".*" ],
            @"test1/filter": @[ @"return input.length >= 1000 ? '<too long>' : null" ],
            @"test2/domains": @[ @".*" ],
            @"test2/filter": @[ @"return input.replaceAll('o', '•').replaceAll('u', 'v')" ],
        };
        completion(hardCodedResult.get(), nil);
    });
}

TEST(TextExtractionTests, FilteringRules)
{
    InstanceMethodSwizzler safeBrowsingSwizzler {
        getSSBLookupContextClassSingleton(),
        @selector(_getListsForNamespace:collectionId:completionHandler:),
        reinterpret_cast<IMP>(overrideGetListsForNamespace)
    };

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);

    [webView synchronouslyLoadTestPageNamed:@"debug-text-extraction"];

    RetainPtr debugText = [webView synchronouslyGetDebugText:^{
        RetainPtr configuration = adoptNS([_WKTextExtractionConfiguration new]);
        [configuration setOutputFormat:_WKTextExtractionOutputFormatHTML];
        [configuration setNodeIdentifierInclusion:_WKTextExtractionNodeIdentifierInclusionNone];
        [configuration setIncludeRects:NO];
        return configuration.autorelease();
    }()];

    EXPECT_TRUE([debugText containsString:@"<input type='email' placeholder='Recipient address'>f••</input>"]);
    EXPECT_TRUE([debugText containsString:@"<h3 aria-label='Heading'>Svbject</h3>"]);
    EXPECT_TRUE([debugText containsString:@"The qvick br•wn f•x jvmped •ver the lazy d•g"]);
}

#endif // HAVE(SAFARI_SAFE_BROWSING_NAMESPACED_LISTS)

static String mainFrameMarkup(uint16_t port)
{
    return makeString(R"(<!DOCTYPE html>
        <html>
            <head>
                <meta name='viewport' content='width=device-width, initial-scale=1'>
                <style>
                    iframe {
                        width: 300px;
                        height: 300px;
                    }
                </style>
            </head>
            <body>
                <iframe class='same'></iframe>
                <iframe class='cross'></iframe>
                <a href='https://webkit.org'>Link to WebKit home page</a>
                <script>
                    subframeLoadedCount = 0;
                    sameOriginFrame = document.querySelector('iframe.same');
                    sameOriginFrame.addEventListener('load', () => subframeLoadedCount++, { once: true });
                    sameOriginFrame.src = 'subframe-same.html';

                    crossOriginFrame = document.querySelector('iframe.cross');
                    crossOriginFrame.addEventListener('load', () => subframeLoadedCount++, { once: true });
                    crossOriginFrame.src = 'http://localhost:)"_s, port, R"(/subframe-cross.html';
                </script>
            </body>
        </html>)"_s);
}

static String subFrameMarkup(ASCIILiteral buttonText)
{
    return makeString(R"(<!DOCTYPE html>
        <html>
            <body>
                <h1>Click count: <span id='click-count'>0</span></h1>
                <article aria-label='Button container'>
                    <button>)"_s, buttonText, R"(</button>
                </article>
                <script>
                    const clickCount = document.getElementById('click-count');
                    const button = document.querySelector('button');
                    button.addEventListener('click', () => {
                        clickCount.textContent = 1 + parseInt(clickCount.textContent);
                    });
                </script>
            </body>
        </html>)"_s);
}

TEST(TextExtractionTests, SubframeInteractions)
{
    HTTPServer server { {
        { "/subframe-cross.html"_s, { subFrameMarkup("Cross origin: click here"_s) } },
        { "/subframe-same.html"_s, { subFrameMarkup("Same origin: click here"_s) } },
    }, HTTPServer::Protocol::Http };

    server.addResponse("/"_s, { mainFrameMarkup(server.port()) });

    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:^{
        RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);

    __block RetainPtr subframes = adoptNS([NSMutableArray new]);
    RetainPtr navigationDelegate = adoptNS([TestNavigationDelegate new]);
    [navigationDelegate setDidCommitLoadWithRequestInFrame:^(WKWebView *, NSURLRequest *, WKFrameInfo *frame) {
        if (!frame.mainFrame && ![frame.request.URL.scheme isEqualToString:@"about"])
            [subframes addObject:frame];
    }];
    [webView setNavigationDelegate:navigationDelegate.get()];
    [webView loadRequest:server.request()];
    [navigationDelegate waitForDidFinishNavigation];

    Util::waitForConditionWithLogging([webView] {
        return [[webView objectByEvaluatingJavaScript:@"subframeLoadedCount"] intValue] == 2;
    }, 2, @"Expected subframes to finish loading.");

    RetainPtr extractionConfiguration = adoptNS([_WKTextExtractionConfiguration new]);
    [extractionConfiguration setIncludeRects:NO];
    [extractionConfiguration setIncludeURLs:NO];
    [extractionConfiguration setAdditionalFrames:subframes.get()];

    RetainPtr world = [WKContentWorld _worldWithConfiguration:^{
        RetainPtr configuration = adoptNS([_WKContentWorldConfiguration new]);
        [configuration setAllowJSHandleCreation:YES];
        return configuration.autorelease();
    }()];

    for (WKFrameInfo *subframe in subframes.get()) {
        RetainPtr button = [webView querySelector:@"button" frame:subframe world:world.get()];
        EXPECT_NOT_NULL(button);
        [extractionConfiguration addClientAttribute:@"foo" value:@"bar" forNode:button.get()];
    }

    auto numberOfMatches = [](NSString *text, NSString *patternString) {
        RetainPtr pattern = [NSRegularExpression regularExpressionWithPattern:patternString options:0 error:nil];
        return [pattern numberOfMatchesInString:text options:0 range:NSMakeRange(0, text.length)];
    };

    RetainPtr debugText = [webView synchronouslyGetDebugText:extractionConfiguration.get()];
    EXPECT_EQ(numberOfMatches(debugText.get(), @"foo='bar'"), 2u);

    {
        RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);
        [interaction setNodeIdentifier:extractNodeIdentifier(debugText.get(), @"Same origin: click here")];

        NSError *error = nil;
        RetainPtr description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ(description.get(), @"Click on button under article labeled “Button container”, with rendered text “Same origin: click here”");

        RetainPtr result = [webView synchronouslyPerformInteraction:interaction.get()];
        EXPECT_NULL([result error]);
    }
    {
        RetainPtr interaction = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);
        [interaction setNodeIdentifier:extractNodeIdentifier(debugText.get(), @"Cross origin: click here")];

        NSError *error = nil;
        RetainPtr description = [interaction debugDescriptionInWebView:webView.get() error:&error];
        EXPECT_WK_STREQ(description.get(), @"Click on button under article labeled “Button container”, with rendered text “Cross origin: click here”");

        RetainPtr result = [webView synchronouslyPerformInteraction:interaction.get()];
        EXPECT_NULL([result error]);
    }

    RetainPtr debugTextAfterClicks = [webView synchronouslyGetDebugText:extractionConfiguration.get()];
    EXPECT_EQ(numberOfMatches(debugTextAfterClicks.get(), @"Click count: 1"), 2u);
}

TEST(TextExtractionTests, InjectedBundle)
{
    WKWebViewConfiguration *configuration = [WKWebViewConfiguration _test_configurationWithTestPlugInClassName:@"JSHandlePlugIn"];
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration]);
    RetainPtr receiver = adoptNS([JSHandleReceiver new]);
    __block RetainPtr<_WKJSHandle> handle;
    __block RetainPtr<_WKJSHandle> decodedHandle;
    receiver.get().dictionaryReceiver = ^(NSDictionary *dictionary) {
        handle = dynamic_objc_cast<_WKJSHandle>(dictionary[@"testkey"]);
        decodedHandle = [NSKeyedUnarchiver _strictlyUnarchivedObjectOfClasses:[NSSet setWithObject:_WKJSHandle.class] fromData:dynamic_objc_cast<NSData>(dictionary[@"testdatakey"]) error:nil];
    };

    _WKRemoteObjectInterface *interface = [_WKRemoteObjectInterface remoteObjectInterfaceWithProtocol:@protocol(JSHandlePlugInProtocol)];
    [interface setClasses:[NSSet setWithObjects:NSDictionary.class, NSString.class, _WKJSHandle.class, nil] forSelector:@selector(receiveDictionaryFromWebProcess:) argumentIndex:0 ofReply:NO];
    [[webView _remoteObjectRegistry] registerExportedObject:receiver.get() interface:interface];

    [webView loadHTMLString:@"text outside <div id='testelement'> text inside </div>" baseURL:nil];
    while (!handle)
        Util::spinRunLoop();
    EXPECT_NOT_NULL(handle.get().frame._documentIdentifier);
    EXPECT_TRUE([handle.get().frame._documentIdentifier isEqual:decodedHandle.get().frame._documentIdentifier]);
    EXPECT_TRUE([handle.get().frame._documentIdentifier isEqual:[webView mainFrame].info._documentIdentifier]);
    EXPECT_TRUE([handle.get().frame _isSameFrame:[webView mainFrame].info]);
}

TEST(TextExtractionTests, ClickInteractionWhileInBackground)
{
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 400, 400) configuration:^{
        RetainPtr configuration = adoptNS([WKWebViewConfiguration new]);
        [configuration _setBackgroundTextExtractionEnabled:YES];
        [[configuration preferences] _setTextExtractionEnabled:YES];
        return configuration.autorelease();
    }()]);

    [webView synchronouslyLoadHTMLString:@R"HTML(
        <!DOCTYPE html>
        <html>
        <head>
            <meta name='viewport' content='width=device-width, initial-scale=1'>
        </head>
        <body>
            <button>Click Me</button>
            <div id='result'>pending</div>
            <script>
                document.querySelector('button').addEventListener('click', async () => {
                    for (let i = 0; i < 3; ++i) {
                        await new Promise(resolve => setTimeout(resolve, 50));
                        await new Promise(requestAnimationFrame);
                    }
                    document.getElementById('result').textContent = 'completed';
                });
            </script>
        </body>
        </html>
    )HTML"];

#if PLATFORM(IOS_FAMILY)
    [NSNotificationCenter.defaultCenter postNotificationName:UIApplicationDidEnterBackgroundNotification object:UIApplication.sharedApplication userInfo:@{@"isSuspendedUnderLock": @NO }];
    [NSNotificationCenter.defaultCenter postNotificationName:UISceneDidEnterBackgroundNotification object:[[webView window] windowScene] userInfo:nil];
#else
    [[webView window] orderOut:nil];
#endif

    RetainPtr debugText = [webView synchronouslyGetDebugText:nil];
    RetainPtr buttonID = extractNodeIdentifier(debugText.get(), @"Click Me");
    EXPECT_NOT_NULL(buttonID.get());

    RetainPtr click = adoptNS([[_WKTextExtractionInteraction alloc] initWithAction:_WKTextExtractionActionClick]);
    [click setNodeIdentifier:buttonID.get()];

    RetainPtr result = [webView synchronouslyPerformInteraction:click.get()];
    EXPECT_NULL([result error]);

    Util::waitForConditionWithLogging([webView] {
        return [[webView stringByEvaluatingJavaScript:@"document.getElementById('result').textContent"] isEqualToString:@"completed"];
    }, 5, @"Expected result text to become 'completed'.");
}

} // namespace TestWebKitAPI
