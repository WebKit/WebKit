/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#import "NSURLUtilities.h"

#if HAVE(NSURL_TITLE)

#import <pal/cocoa/LinkPresentationSoftLink.h>

#if !HAVE(LINK_PRESENTATION_CHANGE_FOR_RADAR_115801517)
// FIXME: Remove this once HAVE(LINK_PRESENTATION_CHANGE_FOR_RADAR_115801517) is true
// for all platforms where HAVE(NSURL_TITLE) is set.
@interface NSURL ()
@property (nonatomic, copy, setter=_setTitle:) NSString *_title;
@end
#endif

@implementation NSURL (WebKitUtilities)

- (void)_web_setTitle:(NSString *)title
{
#if HAVE(LINK_PRESENTATION_CHANGE_FOR_RADAR_115801517)
    // -[LPLinkMetadata setTitle:] additionally sets the `_title` SPI attribute on
    // the NSURL URL in OS versions where this codepath is compiled.
    auto metadata = adoptNS([PAL::allocLPLinkMetadataInstance() init]);
    [metadata setURL:self];
    [metadata setTitle:title];
#else
    self._title = title;
#endif
}

- (NSString *)_web_title
{
#if HAVE(LINK_PRESENTATION_CHANGE_FOR_RADAR_115801517)
    // -[LPLinkMetadata title] falls back to the `_title` SPI attribute on the NSURL
    // in OS versions where this codepath is compiled.
    auto metadata = adoptNS([PAL::allocLPLinkMetadataInstance() init]);
    [metadata setURL:self];
    return [metadata title];
#else
    return self._title;
#endif
}

@end

#endif // HAVE(NSURL_TITLE)
