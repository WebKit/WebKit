/*
    WebNSURLExtras.m
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNSURLExtras.h>

#import <WebKit/WebAssertions.h>

#import <Foundation/NSString_NSURLExtras.h>
#import <Foundation/NSURLProtocolPrivate.h>
#import <Foundation/NSURLRequest.h>
#import <Foundation/NSURL_NSURLExtras.h>

static int URLBytesBufferLength = 2048;

static inline void ReleaseIfNotNULL(CFTypeRef object)
{
    if (object) {
        CFRelease(object);
    }
}

@implementation NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string
{
    if (string == nil) {
        return nil;
    }
    return [self _web_URLWithDataAsString:string relativeToURL:nil];
}

+ (NSURL *)_web_URLWithDataAsString:(NSString *)string relativeToURL:(NSURL *)baseURL
{
    if (string == nil) {
        return nil;
    }
    string = [string _web_stringByTrimmingWhitespace];
    NSData *data = [string dataUsingEncoding:NSISOLatin1StringEncoding];
    return [self _web_URLWithData:data relativeToURL:baseURL];
}

+ (NSURL *)_web_URLWithData:(NSData *)data
{
    if (data == nil) {
        return nil;
    }
    return [self _web_URLWithData:data relativeToURL:nil];
}      

+ (NSURL *)_web_URLWithData:(NSData *)data relativeToURL:(NSURL *)baseURL
{
    if (data == nil) {
        return nil;
    }

    NSURL *result = nil;
    int length = [data length];
    if (length > 0) {
        result = (NSURL *)CFURLCreateAbsoluteURLWithBytes(NULL, [data bytes], [data length], kCFStringEncodingISOLatin1, (CFURLRef)baseURL, YES);
        [result autorelease];
    }
    else {
        result = [NSURL URLWithString:@""];
    }
    return result;
}

- (NSData *)_web_originalData
{
    NSData *data = nil;

    UInt8 static_buffer[URLBytesBufferLength];
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)self, static_buffer, URLBytesBufferLength);
    if (bytesFilled != -1) {
        data = [NSData dataWithBytes:static_buffer length:bytesFilled];
    }
    else {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)self, NULL, 0);
        UInt8 *buffer = malloc(bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)self, buffer, bytesToAllocate);
        ASSERT(bytesFilled == bytesToAllocate);
        // buffer is adopted by the NSData
        data = [NSData dataWithBytesNoCopy:buffer length:bytesFilled];
    }

    NSURL *baseURL = (NSURL *)CFURLGetBaseURL((CFURLRef)self);
    if (baseURL) {
        NSURL *URL = [NSURL _web_URLWithData:data relativeToURL:baseURL];
        return [URL _web_originalData];
    }
    else {
        return data;
    }
}

- (NSString *)_web_displayableString
{
    return [[[NSString alloc] initWithData:[self _web_originalData] encoding:NSISOLatin1StringEncoding] autorelease];
}

- (int)_web_URLStringLength
{
    int length = 0;
    if (!CFURLGetBaseURL((CFURLRef)self)) {
        length = CFURLGetBytes((CFURLRef)self, NULL, 0);
    }
    else {
        length = [[self _web_displayableString] length];
    }
    return length;
}

- (const char *)_web_URLCString
{
    NSMutableData *data = [NSMutableData data];
    [data appendData:[self _web_originalData]];
    [data appendBytes:"\0" length:1];
    return (const char *)[data bytes];
 }

- (NSURL *)_webkit_canonicalize
{
    NSURLRequest *request = [[NSURLRequest alloc] initWithURL:self];
    Class concreteClass = [NSURLProtocol _protocolClassForRequest:request];
    if (!concreteClass) {
        [request release];
        return self;
    }

    NSURL *result = nil;
    NSURLRequest *newRequest = [concreteClass canonicalRequestForRequest:request];
    NSURL *newURL = [newRequest URL];
    result = [[newURL retain] autorelease];
    [request release];

    return result;
}

- (NSURL *)_webkit_URLByRemovingFragment
{
    // Check to see if a fragment exists before decomposing the URL.
    CFStringRef frag = CFURLCopyFragment((CFURLRef)self, NULL);
    if (!frag) {
        return self;
    }
    
    ReleaseIfNotNULL(frag);
    
    WebURLComponents components = [self _web_URLComponents];
    components.fragment = nil;
    NSURL *result = [NSURL _web_URLWithComponents:components];
    return result ? result : self;
}

- (BOOL)_webkit_isJavaScriptURL
{
    return [[self absoluteString] _webkit_isJavaScriptURL];
}

- (NSString *)_webkit_scriptIfJavaScriptURL
{
    return [[self absoluteString] _webkit_scriptIfJavaScriptURL];
}

- (BOOL)_webkit_isFTPDirectoryURL
{
    return [[self absoluteString] _webkit_isFTPDirectoryURL];
}

- (BOOL)_webkit_shouldLoadAsEmptyDocument
{
    return [[self absoluteString] _web_hasCaseInsensitivePrefix:@"about:"] || [[self absoluteString] length] == 0;
}

@end


@implementation NSString (WebNSURLExtras)

static BOOL isHexDigit(char c)
{
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

static int hexDigitValue(char c)
{
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    ERROR("illegal hex digit");
    return 0;
}

- (BOOL)_webkit_isJavaScriptURL
{
    return [self _web_hasCaseInsensitivePrefix:@"javascript:"];
}

- (NSString *)_webkit_stringByReplacingValidPercentEscapes
{
    const char *before = [self UTF8String];
    char *after = malloc(strlen(before) + 1);
    
    const char *p = before;
    char *q = after;
    
    while (*p) {
        if (*p == '%' && isHexDigit(p[1]) && isHexDigit(p[2])) {
            *q++ = (hexDigitValue(p[1]) << 4) | hexDigitValue(p[2]);
            p += 3;
        } else {
            *q++ = *p++;
        }
    }
    *q = '\0';
    
    NSString *result = [NSString stringWithUTF8String:after];
    free(after);
    
    // FIXME: This returns the original string with all the % escapes intact
    // if there are any illegal UTF-8 sequences. Instead, we should detect illegal
    // UTF-8 sequences while decoding above and either leave them as % escapes
    // (but decode all the others) or turn them in to ? characters.
    return result ? result : self;
}

- (NSString *)_webkit_scriptIfJavaScriptURL
{
    if (![self _webkit_isJavaScriptURL]) {
        return nil;
    }
    return [[self substringFromIndex:11] _webkit_stringByReplacingValidPercentEscapes];
}

- (BOOL)_webkit_isFTPDirectoryURL
{
    int length = [self length];
    if (length < 5) {  // 5 is length of "ftp:/"
	return NO;
    }
    unichar lastChar = [self characterAtIndex:length - 1];
    return lastChar == '/' && [self _web_hasCaseInsensitivePrefix:@"ftp:"];
}

@end
