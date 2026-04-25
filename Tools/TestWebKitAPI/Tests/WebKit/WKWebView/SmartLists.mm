/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if ENABLE_SWIFTUI

#import "Helpers/cocoa/DecomposedAttributedText.h"
#import "Helpers/PlatformUtilities.h"
#import "Helpers/cocoa/SmartListsSupport.h"
#import "Helpers/Test.h"
#import "Helpers/cocoa/TestUIDelegate.h"
#import "Helpers/cocoa/TestWKWebView.h"
#import <WebKit/WKMenuItemIdentifiersPrivate.h>
#import <WebKit/WKPreferencesPrivate.h>
#import <WebKit/WKWebViewPrivate.h>
#import <WebKit/_WKFeature.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/TextStream.h>
#import <wtf/unicode/CharacterNames.h>

#if PLATFORM(MAC)

// MARK: Utilities

static NSString* const WebSmartListsEnabled = @"WebSmartListsEnabled";

static void setSmartListsPreference(WKWebViewConfiguration *configuration, BOOL value)
{
    auto preferences = [configuration preferences];
    for (_WKFeature *feature in [WKPreferences _features]) {
        if ([feature.key isEqualToString:@"SmartListsAvailable"]) {
            [preferences _setEnabled:value forFeature:feature];
            break;
        }
    }
}

static NSNumber *userDefaultsValue()
{
    return [[NSUserDefaults standardUserDefaults] objectForKey:WebSmartListsEnabled];
}

static void resetUserDefaults()
{
    [[NSUserDefaults standardUserDefaults] removeObjectForKey:WebSmartListsEnabled];
}

static void setUserDefaultsValue(BOOL value)
{
    [[NSUserDefaults standardUserDefaults] setBool:value forKey:WebSmartListsEnabled];
}

static RetainPtr<NSMenu> invokeContextMenu(TestWKWebView *webView)
{
    RetainPtr delegate = adoptNS([[TestUIDelegate alloc] init]);

    __block RetainPtr<NSMenu> proposedMenu;
    __block bool gotProposedMenu = false;
    [delegate setGetContextMenuFromProposedMenu:^(NSMenu *menu, _WKContextMenuElementInfo *, id<NSSecureCoding>, void (^completion)(NSMenu *)) {
        proposedMenu = menu;
        completion(nil);
        gotProposedMenu = true;
    }];

    [webView setUIDelegate:delegate.get()];

    [webView waitForNextPresentationUpdate];
    [webView rightClickAtPoint:NSMakePoint(10, [webView frame].size.height - 10)];
    TestWebKitAPI::Util::run(&gotProposedMenu);

    return proposedMenu;
}

#endif // PLATFORM(MAC)

static void runTest(NSString *input, NSString *expectedHTML, NSString *expectedSelectionPath, NSInteger selectionOffset, NSString *stylesheet = nil, bool isRTL = false)
{
    RetainPtr expectedSelection = [SmartListsTestSelectionConfiguration caretSelectionWithPath:expectedSelectionPath offset:selectionOffset];
    RetainPtr configuration = adoptNS([[SmartListsTestConfiguration alloc] initWithExpectedHTML:expectedHTML expectedSelection:expectedSelection.get() input:input stylesheet:stylesheet isRTL:isRTL]);

    __block bool finished = false;
    __block RetainPtr<SmartListsTestResult> result;
    [SmartListsSupport processConfiguration:configuration.get() completionHandler:^(SmartListsTestResult *testResult, NSError *error) {
        if (error) {
            TextStream errorMessage;
            errorMessage << error;
            EXPECT_NULL(error) << errorMessage.release().utf8().data();
        }
        result = testResult;
        finished = true;
    }];

    TestWebKitAPI::Util::run(&finished);

    TextStream stream;
    stream << "expected " << [result actualHTML] << " to equal " << [result expectedHTML];
    EXPECT_WK_STREQ([result expectedRenderTree], [result actualRenderTree]) << stream.release().utf8().data();
}

// MARK: Tests

#if PLATFORM(MAC)

TEST(SmartLists, EnablementIsLogicallyConsistentWhenInterfacedThroughResponder)
{
    resetUserDefaults();

    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectZero configuration:configuration.get()]);
    [webView synchronouslyLoadHTMLString:@"<div>hi</div>"];
    [webView waitForNextPresentationUpdate];

    // Case 1: user default => nil, preference => false

    setSmartListsPreference(configuration.get(), NO);

    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_NULL(userDefaultsValue());

    [webView _setSmartListsEnabled:YES];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_NULL(userDefaultsValue());

    // Case 2: user default => nil, preference => true

    setSmartListsPreference(configuration.get(), YES);

    EXPECT_TRUE([webView _isSmartListsEnabled]);
    EXPECT_NULL(userDefaultsValue());

    [webView _setSmartListsEnabled:NO];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_FALSE([userDefaultsValue() boolValue]);

    [webView _toggleSmartLists:nil];
    EXPECT_TRUE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    // Case 3: user default => true, preference => false

    setSmartListsPreference(configuration.get(), NO);
    setUserDefaultsValue(YES);

    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    [webView _setSmartListsEnabled:YES];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    // Case 4: user default => true, preference => true

    setSmartListsPreference(configuration.get(), YES);
    setUserDefaultsValue(YES);

    EXPECT_TRUE([webView _isSmartListsEnabled]);
    EXPECT_TRUE([userDefaultsValue() boolValue]);

    [webView _setSmartListsEnabled:NO];
    EXPECT_FALSE([webView _isSmartListsEnabled]);
    EXPECT_FALSE([userDefaultsValue() boolValue]);
}

TEST(SmartLists, ContextMenuItemStateIsConsistentWithAvailability)
{
    RetainPtr configuration = adoptNS([[WKWebViewConfiguration alloc] init]);
    RetainPtr webView = adoptNS([[TestWKWebView alloc] initWithFrame:CGRectMake(0, 0, 800, 600) configuration:configuration.get()]);

    [webView synchronouslyLoadHTMLString:@"<body contenteditable>hi</body>"];
    [webView waitForNextPresentationUpdate];

    // Case 1: Available
    {
        setSmartListsPreference(configuration.get(), YES);

        NSString *script = @"document.body.focus()";
        [webView stringByEvaluatingJavaScript:script];

        RetainPtr menu = invokeContextMenu(webView.get());
        RetainPtr substitutionMenu = [menu itemWithTitle:@"Substitutions"];
        EXPECT_NOT_NULL(substitutionMenu.get());

        RetainPtr smartListsItem = [[substitutionMenu submenu] itemWithTitle:@"Smart Lists"];
        EXPECT_TRUE([smartListsItem isEnabled]);
    }

    // Case 2: Unavailable
    {
        setSmartListsPreference(configuration.get(), NO);

        NSString *script = @"document.body.focus()";
        [webView stringByEvaluatingJavaScript:script];

        RetainPtr menu = invokeContextMenu(webView.get());
        RetainPtr substitutionMenu = [menu itemWithTitle:@"Substitutions"];
        EXPECT_NOT_NULL(substitutionMenu.get());

        RetainPtr smartListsItem = [[substitutionMenu submenu] itemWithTitle:@"Smart Lists"];
        EXPECT_FALSE([smartListsItem isEnabled]);
    }
}

#endif // PLATFORM(MAC)

TEST(SmartLists, InsertingSpaceAndTextAfterBulletPointGeneratesListWithText)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ul>
            <li>Hello</li>
            <li>World</li>
        </ul>
    </body>
    )"""_s;

    runTest(@"* Hello\n* World", expectedHTML.createNSString().get(), @"//body/ul/li[2]/text()", @"World".length);

    RetainPtr inputWithBullet = makeString(WTF::Unicode::bullet, " Hello\n"_s, WTF::Unicode::bullet, " World"_s).createNSString();
    runTest(inputWithBullet.get(), expectedHTML.createNSString().get(), @"//body/ul/li[2]/text()", @"World".length);
}

TEST(SmartLists, InsertingSpaceAndTextAfterHyphenGeneratesDashedList)
{
    auto marker = WTF::makeString(WTF::Unicode::enDash, WTF::Unicode::noBreakSpace, WTF::Unicode::noBreakSpace);

    static constexpr auto expectedHTMLTemplate = R"""(
    <body contenteditable="">
        <ul style="list-style-type: '<MARKER>';">
            <li>Hello</li>
            <li>World</li>
        </ul>
    </body>
    )"""_s;

    RetainPtr expectedHTML = WTF::makeStringByReplacingAll(expectedHTMLTemplate, "<MARKER>"_s, marker).createNSString();

    runTest(@"- Hello\n- World", expectedHTML.get(), @"//body/ul/li[2]/text()", @"World".length);
}

TEST(SmartLists, InsertingSpaceAfterBulletPointGeneratesEmptyList)
{
    static constexpr auto expectedHTML = R"""(
    <body>
        <ul style="list-style-type: disc;">
            <li>A</li>
            <li><br></li>
        </ul>
    </body>
    )"""_s;

    runTest(@"* A\n* ", expectedHTML.createNSString().get(), @"//body/ul/li[2]/br", 0);
}

TEST(SmartLists, InsertingSpaceAfterBulletPointInMiddleOfSentenceDoesNotGenerateList)
{
    static constexpr auto expectedHTML = R"""(
    <body>
        ABC * DEF
    </body>
    )"""_s;

    runTest(@"ABC * DEF", expectedHTML.createNSString().get(), @"//body/text()", @"ABC * DEF".length);
}

TEST(SmartLists, InsertingSpaceAfterPeriodAtStartOfSentenceDoesNotGenerateList)
{
    static constexpr auto expectedHTML = R"""(
    <body>
        . Hi
    </body>
    )"""_s;

    runTest(@". Hi", expectedHTML.createNSString().get(), @"//body/text()", @". Hi".length);
}

TEST(SmartLists, InsertingSpaceAfterNumberGeneratesOrderedList)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ol>
            <li>Hello</li>
            <li>World</li>
        </ol>
    </body>
    )"""_s;

    runTest(@"1. Hello\n2. World", expectedHTML.createNSString().get(), @"//body/ol/li[2]/text()", @"World".length);

    runTest(@"1) Hello\n2) World", expectedHTML.createNSString().get(), @"//body/ol/li[2]/text()", @"World".length);
}

TEST(SmartLists, InsertingSpaceAfterMultipleDigitNumberGeneratesOrderedList)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ol start="123" style="list-style-type: decimal;">
            <li>Hello</li>
            <li>World</li>
        </ol>
    </body>
    )"""_s;

    runTest(@"123. Hello\n125. World", expectedHTML.createNSString().get(), @"//body/ol/li[2]/text()", @"World".length);
}

TEST(SmartLists, InsertingSpaceAfterInvalidNumberDoesNotGenerateOrderedList)
{
    static constexpr auto expectedZeroHTML = R"""(
    <body>0. Hello</body>
    )"""_s;

    runTest(@"0. Hello", expectedZeroHTML.createNSString().get(), @"//body/text()", @"0. Hello".length);

    static constexpr auto expectedNegativeNumberHTML = R"""(
    <body>-42. Hello</body>
    )"""_s;

    runTest(@"-42. Hello", expectedNegativeNumberHTML.createNSString().get(), @"//body/text()", @"-42. Hello".length);

    static constexpr auto expectedPlusPrefixedHTML = R"""(
    <body>+42. Hello</body>
    )"""_s;

    runTest(@"+42. Hello", expectedPlusPrefixedHTML.createNSString().get(), @"//body/text()", @"+42. Hello".length);
}

TEST(SmartLists, InsertingSpaceAfterLargeNumberDoesNotGenerateOrderedList)
{
    static constexpr auto expectedHTML = R"""(
    <body>1000. hi</body>
    )"""_s;

    NSString *input = @"1000. hi";

    runTest(input, expectedHTML.createNSString().get(), @"//body/text()", input.length);
}

TEST(SmartLists, InsertingDifferentListStylesDoesNotMergeLists)
{
    auto dashMarker = WTF::makeString(WTF::Unicode::enDash, WTF::Unicode::noBreakSpace, WTF::Unicode::noBreakSpace);

    static constexpr auto expectedHTMLTemplate = R"""(
    <body contenteditable="">
        <ul style="list-style-type: disc;" class="Apple-disc-list">
            <li>A</li>
            <li>B</li>
            <li>C</li>
            <li>D</li>
            <li>E</li>
        </ul>
        <div>
            <ul class="Apple-dash-list" style="list-style-type: '<DASH_MARKER>';">
                <li>A</li>
                <li>B</li>
            </ul>
        </div>
    </body>)"""_s;

    RetainPtr expectedHTML = WTF::makeStringByReplacingAll(expectedHTMLTemplate, "<DASH_MARKER>"_s, dashMarker).createNSString();

    runTest(@"* A\n* B\nC\n\n* D\n* E\n\n- A\n- B", expectedHTML.get(), @"//body/div/ul/li[2]/text()", @"B".length);
}

TEST(SmartLists, InsertingListMergesWithPreviousListIfPossible)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ol start="1" style="list-style-type: decimal;" class="Apple-decimal-list">
            <li>A</li>
            <li>B</li>
            <li>C</li>
            <li>D</li>
            <li>E</li>
        </ol>
    </body>)"""_s;

    RetainPtr input = @""
    "1. A\n"
    "2. B\n"
    "C\n"
    "\n"
    "5. D\n"
    "6. E"
    "";

    runTest(input.get(), expectedHTML.createNSString().get(), @"//body/ol/li[5]/text()", 1);
}

TEST(SmartLists, InsertingSpaceInsideListElementDoesNotActivateSmartLists)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ul style="list-style-type: disc;" class="Apple-disc-list">
            <li>A</li>
            <li>B</li>
            <li>1. Hi</li>
        </ul>
    </body>)"""_s;

    runTest(@"* A\n* B\n1. Hi", expectedHTML.createNSString().get(), @"//body/ul/li[3]/text()", @"1. Hi".length);
}

TEST(SmartLists, NewlineOnEmptyListElementShouldRemovePlainTextMarkers)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ul class="Apple-disc-list" style="list-style-type: disc;">
            <li>A</li>
            <li>B</li>
        </ul>
        <div>C</div>
    </body>)"""_s;

    runTest(@"* A\n* B\n\nC", expectedHTML.createNSString().get(), @"//body/div/text()", 1);
}

TEST(SmartLists, OrderedSmartListWithRTL)
{
    RetainPtr expectedHTML = @"<body dir=\"rtl\" contenteditable=\"\">"
    "<ol start=\"1\" style=\"list-style-type: decimal;\" class=\"Apple-decimal-list\">"
        "<li>تفاحة</li>"
        "<li>برتقال</li>"
    "</ol>"
    "</body>";

    runTest(@"1. تفاحة\n2. برتقال", expectedHTML.get(), @"//body/ol/li[2]/text()", @"برتقال".length, nullptr, true);
}

TEST(SmartLists, SingleLineMarkerDoesNotTriggerConversion)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        * Hello
    </body>
    )"""_s;

    runTest(@"* Hello", expectedHTML.createNSString().get(), @"//body/text()", @"* Hello".length);
}

TEST(SmartLists, MismatchedMarkersDoNotTriggerConversion)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        * Hello
        <div>- World</div>
    </body>
    )"""_s;

    runTest(@"* Hello\n- World", expectedHTML.createNSString().get(), @"//body/div/text()", @"- World".length);
}

TEST(SmartLists, MarkersSeparatedByBlankLineDoNotTriggerConversion)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        * A
        <div><br></div>
        <div>* B</div>
    </body>
    )"""_s;

    runTest(@"* A\n\n* B", expectedHTML.createNSString().get(), @"//body/div[2]/text()", @"* B".length);
}

TEST(SmartLists, OrderedAndUnorderedMismatchDoesNotTriggerConversion)
{
    {
        static constexpr auto expectedHTML = R"""(
        <body contenteditable="">
            1. Hello
            <div>* World</div>
        </body>
        )"""_s;

        runTest(@"1. Hello\n* World", expectedHTML.createNSString().get(), @"//body/div/text()", @"* World".length);
    }

    {
        static constexpr auto expectedHTML = R"""(
        <body contenteditable="">
            * Hello
            <div>1. World</div>
        </body>
        )"""_s;

        runTest(@"* Hello\n1. World", expectedHTML.createNSString().get(), @"//body/div/text()", @"1. World".length);
    }
}

TEST(SmartLists, MatchingOrderedMarkersWithDifferentDelimitersGenerateList)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ol>
            <li>A</li>
            <li>B</li>
        </ol>
    </body>
    )"""_s;

    runTest(@"1. A\n2) B", expectedHTML.createNSString().get(), @"//body/ol/li[2]/text()", @"B".length);
}

TEST(SmartLists, OrderedListStartingFromNonOneGeneratesCorrectStartAttribute)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        <ol start="5" style="list-style-type: decimal;">
            <li>A</li>
            <li>B</li>
        </ol>
    </body>
    )"""_s;

    runTest(@"5. A\n6. B", expectedHTML.createNSString().get(), @"//body/ol/li[2]/text()", @"B".length);
}

TEST(SmartLists, OrderedMarkersSeparatedByBlankLineDoNotTriggerConversion)
{
    static constexpr auto expectedHTML = R"""(
    <body contenteditable="">
        1. A
        <div><br></div>
        <div>2. B</div>
    </body>
    )"""_s;

    runTest(@"1. A\n\n2. B", expectedHTML.createNSString().get(), @"//body/div[2]/text()", @"2. B".length);
}

#endif // ENABLE_SWIFTUI
