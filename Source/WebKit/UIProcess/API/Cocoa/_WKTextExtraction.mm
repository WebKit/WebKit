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
#import "_WKTextExtractionInternal.h"

#import "Logging.h"
#import "WKWebViewInternal.h"
#import "_WKJSHandleInternal.h"
#import <WebKit/WKError.h>
#import <wtf/RetainPtr.h>

@implementation _WKTextExtractionConfiguration {
    RetainPtr<_WKJSHandle> _targetNode;
    HashMap<RetainPtr<NSString>, HashMap<RetainPtr<_WKJSHandle>, RetainPtr<NSString>>> _clientNodeAttributes;
    RetainPtr<NSDictionary<NSString *, NSString *>> _replacementStrings;
    RetainPtr<NSArray<_WKJSHandle *>> _nodesToSkip;
    RetainPtr<NSArray<WKFrameInfo *>> _additionalFrames;
}

- (instancetype)init
{
    return [self _initForOnlyVisibleText:NO];
}

+ (instancetype)configurationForVisibleTextOnly
{
    return adoptNS([[self alloc] _initForOnlyVisibleText:YES]).autorelease();
}

- (instancetype)_initForOnlyVisibleText:(BOOL)onlyVisibleText
{
    if (!(self = [super init]))
        return nil;

    _dataDetectorTypes = _WKTextExtractionDataDetectorNone;
    _filterOptions = _WKTextExtractionFilterAll;
    _includeURLs = !onlyVisibleText;
    _includeRects = !onlyVisibleText;
    _nodeIdentifierInclusion = onlyVisibleText ? _WKTextExtractionNodeIdentifierInclusionNone : _WKTextExtractionNodeIdentifierInclusionInteractive;
    _includeEventListeners = !onlyVisibleText;
    _includeAccessibilityAttributes = !onlyVisibleText;
    _includeTextInAutoFilledControls = !onlyVisibleText;
    _onlyIncludeVisibleText = onlyVisibleText;
    _targetRect = CGRectNull;
    _maxWordsPerParagraph = NSUIntegerMax;
    _maxWordsPerParagraphPolicy = _WKTextExtractionWordLimitPolicyAlways;
    return self;
}

- (_WKJSHandle *)targetNode
{
    return _targetNode.get();
}

- (void)setTargetNode:(_WKJSHandle *)targetNode
{
    _targetNode = adoptNS([targetNode copy]);
}

- (NSArray<_WKJSHandle *> *)nodesToSkip
{
    return _nodesToSkip.get();
}

- (void)setNodesToSkip:(NSArray<_WKJSHandle *> *)nodesToSkip
{
    _nodesToSkip = adoptNS([nodesToSkip copy]);
}

- (NSArray<WKFrameInfo *> *)additionalFrames
{
    return _additionalFrames.get() ?: @[ ];
}

- (void)setAdditionalFrames:(NSArray<WKFrameInfo *> *)frames
{
    _additionalFrames = adoptNS([frames copy]);
}

- (void)addClientAttribute:(NSString *)attributeName value:(NSString *)attributeValue forNode:(_WKJSHandle *)node
{
    _clientNodeAttributes.ensure(RetainPtr { attributeName }, [] {
        return HashMap<RetainPtr<_WKJSHandle>, RetainPtr<NSString>> { };
    }).iterator->value.set(RetainPtr { node }, RetainPtr { attributeValue });
}

- (void)forEachClientNodeAttribute:(void(^)(NSString *attribute, NSString *value, _WKJSHandle *))block
{
    for (auto [attribute, values] : _clientNodeAttributes) {
        for (auto [handle, value] : values)
            block(attribute.get(), value.get(), handle.get());
    }
}

- (NSDictionary<NSString *, NSString *> *)replacementStrings
{
    return _replacementStrings.get();
}

- (void)setReplacementStrings:(NSDictionary<NSString *, NSString *> *)replacementStrings
{
    _replacementStrings = adoptNS([replacementStrings copy]);
}

#define ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value) do { \
    if (_onlyIncludeVisibleText && value) { \
        RELEASE_LOG_ERROR(TextExtraction, "%{public}s ignored for text-only %{public}@", __PRETTY_FUNCTION__, [self class]); \
        return; \
    } \
} while (0)

- (void)setIncludeURLs:(BOOL)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _includeURLs = value;
}

- (void)setIncludeRects:(BOOL)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _includeRects = value;
}

- (void)setNodeIdentifierInclusion:(_WKTextExtractionNodeIdentifierInclusion)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _nodeIdentifierInclusion = value;
}

- (void)setIncludeEventListeners:(BOOL)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _includeEventListeners = value;
}

- (void)setIncludeAccessibilityAttributes:(BOOL)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _includeAccessibilityAttributes = value;
}

- (void)setIncludeTextInAutoFilledControls:(BOOL)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _includeTextInAutoFilledControls = value;
}

- (void)setOutputFormat:(_WKTextExtractionOutputFormat)outputFormat
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(outputFormat != _WKTextExtractionOutputFormatTextTree);

    _outputFormat = outputFormat;
}

- (void)setShortenURLs:(BOOL)value
{
    ENSURE_VALID_TEXT_ONLY_CONFIGURATION(value);

    _shortenURLs = value;
}

#undef ENSURE_VALID_TEXT_ONLY_CONFIGURATION

@end

@implementation _WKTextExtractionResult {
    RetainPtr<NSString> _textContent;
    RetainPtr<NSDictionary<NSString *, NSURL *>> _shortenedURLs;
    __weak WKWebView *_webView;
}

- (instancetype)initWithWebView:(WKWebView *)webView textContent:(NSString *)textContent filteredOutAnyText:(BOOL)filteredOutAnyText shortenedURLs:(NSDictionary<NSString *, NSURL *> *)shortenedURLs
{
    if (self = [super init]) {
        _textContent = textContent;
        _filteredOutAnyText = filteredOutAnyText;
        _shortenedURLs = shortenedURLs;
        _webView = webView;
    }
    return self;
}

- (NSString *)textContent
{
    return _textContent.get();
}

- (NSDictionary<NSString *, NSURL *> *)shortenedURLs
{
    return _shortenedURLs.get();
}

- (void)requestJSHandleForNodeIdentifier:(NSString *)nodeIdentifier searchText:(NSString *)searchText completionHandler:(void (^)(_WKJSHandle *))completionHandler
{
    RetainPtr webView = _webView;
    if (!webView)
        return completionHandler(nil);

    [webView _requestJSHandleForNodeIdentifier:nodeIdentifier searchText:searchText completionHandler:completionHandler];
}

@end

@implementation _WKTextExtractionInteraction {
    RetainPtr<NSString> _nodeIdentifier;
    RetainPtr<NSString> _text;
}

@synthesize action = _action;
@synthesize replaceAll = _replaceAll;
@synthesize location = _location;
@synthesize hasSetLocation = _hasSetLocation;
@synthesize scrollToVisible = _scrollToVisible;
@synthesize scrollDelta = _scrollDelta;

- (instancetype)initWithAction:(_WKTextExtractionAction)action
{
    if (!(self = [super init]))
        return nil;

    _location = CGPointZero;
    _action = action;
    return self;
}

- (NSString *)nodeIdentifier
{
    return _nodeIdentifier.get();
}

- (void)setNodeIdentifier:(NSString *)nodeIdentifier
{
    _nodeIdentifier = adoptNS(nodeIdentifier.copy);
}

- (NSString *)text
{
    return _text.get();
}

- (void)setText:(NSString *)text
{
    _text = adoptNS(text.copy);
}

- (void)setLocation:(CGPoint)location
{
    _hasSetLocation = YES;
    _location = location;
}

- (void)debugDescriptionInWebView:(WKWebView *)webView completionHandler:(void (^)(NSString *, NSError *))completionHandler
{
    [webView _describeInteraction:self completionHandler:completionHandler];
}

@end

@implementation _WKTextExtractionInteractionResult {
    RetainPtr<NSError> _error;
}

- (instancetype)initWithErrorDescription:(NSString *)errorDescription
{
    if (!(self = [super init]))
        return nil;

    if (errorDescription)
        _error = [NSError errorWithDomain:WKErrorDomain code:WKErrorUnknown userInfo:@{ NSDebugDescriptionErrorKey: errorDescription }];

    return self;
}

- (NSError *)error
{
    return _error.get();
}

@end
