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

static char hexDigit(int i) {
    if (i < 0 || i > 16) {
        ERROR("illegal hex digit");
        return '0';
    }
    int h = i;
    if (h >= 10) {
        h = h - 10 + 'a'; 
    }
    else {
        h += '0';
    }
    return h;
}

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

@implementation NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithUserTypedString:(NSString *)string
{
    if (string == nil) {
        return nil;
    }
    string = [string _web_stringByTrimmingWhitespace];
    NSData *userTypedData = [string dataUsingEncoding:NSUTF8StringEncoding];
    ASSERT(userTypedData);
        
    const UInt8 *inBytes = [userTypedData bytes];
    int inLength = [userTypedData length];
    if (inLength == 0) {
        return [NSURL URLWithString:@""];
    }
    
    char *outBytes = malloc(inLength * 3); // large enough to %-escape every character
    char *p = outBytes;
    int outLength = 0;
    int i;
    for (i = 0; i < inLength; i++) {
        UInt8 c = inBytes[i];
        if (c <= 0x20 || c >= 0x7f) {
            *p++ = '%';
            *p++ = hexDigit(c >> 4);
            *p++ = hexDigit(c & 0xf);
            outLength += 3;
        }
        else {
            *p++ = c;
            outLength++;
        }
    }
 
    NSData *data = [NSData dataWithBytesNoCopy:outBytes length:outLength]; // adopts outBytes
    return [self _web_URLWithData:data relativeToURL:nil];
}

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
        const UInt8 *bytes = [data bytes];
        // NOTE: We use UTF-8 here since this encoding is used when computing strings when returning URL components
        // (e.g calls to NSURL -path). However, this function is not tolerant of illegal UTF-8 sequences, which
        // could either be a malformed string or bytes in a different encoding, like shift-jis, so we fall back
        // onto using ISO Latin 1 in those cases.
        result = (NSURL *)CFURLCreateAbsoluteURLWithBytes(NULL, bytes, length, kCFStringEncodingUTF8, (CFURLRef)baseURL, YES);
        if (!result) {
            result = (NSURL *)CFURLCreateAbsoluteURLWithBytes(NULL, bytes, length, kCFStringEncodingISOLatin1, (CFURLRef)baseURL, YES);
        }
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

- (NSString *)_web_originalDataAsString
{
    return [[[NSString alloc] initWithData:[self _web_originalData] encoding:NSISOLatin1StringEncoding] autorelease];
}

- (NSString *)_web_userVisibleString
{
    NSData *data = [self _web_originalData];
    const unsigned char *before = [data bytes];
    int length = [data length];

    const unsigned char *p = before;
    int bufferLength = (length * 3) + 1;
    char *after = malloc(bufferLength); // large enough to %-escape every character
    char *q = after;
    int i;
    for (i = 0; i < length; i++) {
        unsigned char c = p[i];
        // escape control characters, space, and delete
        if (c <= 0x20 || c == 0x7f) {
            *q++ = '%';
            *q++ = hexDigit(c >> 4);
            *q++ = hexDigit(c & 0xf);
        }
        // unescape escape sequences that indicate bytes greater than 0x7f
        else if (c == '%' && (i + 1 < length && isHexDigit(p[i + 1])) && i + 2 < length && isHexDigit(p[i + 2])) {
            unsigned char u = (hexDigitValue(p[i + 1]) << 4) | hexDigitValue(p[i + 2]);
            if (u > 0x7f) {
                // unescape
                *q++ = u;
            }
            else {
                // do not unescape
                *q++ = p[i];
                *q++ = p[i + 1];
                *q++ = p[i + 2];
            }
            i += 2;
        } 
        else {
            *q++ = c;
        }
    }
    *q = '\0';
  
    // Check string to see if it can be converted to display using UTF-8  
    NSString *result = [NSString stringWithUTF8String:after];
    if (!result) {
        // Could not convert to UTF-8.
        // Convert characters greater than 0x7f to escape sequences.
        // Shift current string to the end of the buffer
        // then we will copy back bytes to the start of the buffer 
        // as we convert.
        int afterlength = q - after;
        char *p = after + bufferLength - afterlength - 1;
        memmove(p, after, afterlength + 1); // copies trailing '\0'
        char *q = after;
        while (*p) {
            unsigned char c = *p;
            if (c > 0x7f) {
                *q++ = '%';
                *q++ = hexDigit(c >> 4);
                *q++ = hexDigit(c & 0xf);
            }
            else {
                *q++ = *p;
            }
            p++;
        }
        *q = '\0';
        result = [NSString stringWithUTF8String:after];
    }
    free(after);
    
    return result;
}

- (BOOL)_web_isEmpty
{
    int length = 0;
    if (!CFURLGetBaseURL((CFURLRef)self)) {
        length = CFURLGetBytes((CFURLRef)self, NULL, 0);
    }
    else {
        length = [[self _web_userVisibleString] length];
    }
    return length == 0;
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
    return [[self _web_originalDataAsString] _webkit_isJavaScriptURL];
}

- (NSString *)_webkit_scriptIfJavaScriptURL
{
    return [[self _web_originalDataAsString] _webkit_scriptIfJavaScriptURL];
}

- (BOOL)_webkit_isFTPDirectoryURL
{
    return [[self _web_originalDataAsString] _webkit_isFTPDirectoryURL];
}

- (BOOL)_webkit_shouldLoadAsEmptyDocument
{
    return [[self _web_originalDataAsString] _web_hasCaseInsensitivePrefix:@"about:"] || [self _web_isEmpty];
}

@end


@implementation NSString (WebNSURLExtras)

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
