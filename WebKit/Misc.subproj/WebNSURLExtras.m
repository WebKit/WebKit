/*
    WebNSURLExtras.h
    Private (SPI) header
    Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebNSURLExtras.h>

@implementation NSURL (WebNSURLExtras)

+ (NSURL *)_web_URLWithData:(NSData *)data
{
    NSString *string = [[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding] autorelease]; 
    return [NSURL _web_URLWithString:string];  
#if 0
    // URL API FIXME: Use new URL API when available
    return (NSURL *)CFURLCreateAbsoluteURLWithBytes(NULL, [data bytes], [data length], NSISOLatin1StringEncoding, NULL, YES);
#endif 
}      

+ (NSURL *)_web_URLWithData:(NSData *)data relativeToURL:(NSURL *)baseURL
{
    NSString *string = [[[NSString alloc] initWithData:data encoding:NSISOLatin1StringEncoding] autorelease];
    return [NSURL _web_URLWithString:string relativeToURL:baseURL];
#if 0
    // URL API FIXME: Use new URL API when available
    return (NSURL *)CFURLCreateAbsoluteURLWithBytes(NULL, [data bytes], [data length], NSISOLatin1StringEncoding, (CFURLRef)baseURL, YES);
#endif
}

- (NSData *)_web_URLAsData
{
    return [[self absoluteString] dataUsingEncoding:NSISOLatin1StringEncoding allowLossyConversion:YES];
#if 0
    // URL API FIXME: Convert to new URL API when available
    NSData *result = nil;

    static int URLAsDataBufferSize = 2048;
    UInt8 static_buffer[URLAsDataBufferSize];
    CFIndex bytesFilled = CFURLGetBytes((CFURLRef)self, static_buffer, URLAsDataBufferSize);
    if (bytesFilled != -1) {
        result = [NSData dataWithBytes:static_buffer length:bytesFilled];
    }
    else {
        CFIndex bytesToAllocate = CFURLGetBytes((CFURLRef)self, NULL, 0);
        UInt8 *buffer = malloc(bytesToAllocate);
        bytesFilled = CFURLGetBytes((CFURLRef)self, buffer, bytesToAllocate);
        NSURL_ASSERT(bytesFilled == bytesToAllocate);
        result = [NSData dataWithBytesNoCopy:buffer length:bytesFilled];
    }

    return result;
#endif
}

- (NSString *)_web_absoluteString
{
    return [self absoluteString];
#if 0
    // URL API FIXME: Convert to new URL API when available
    return [[[NSString alloc] initWithData:[self _web_URLAsData] encoding:NSISOLatin1StringEncoding] autorelease];
#endif
}

- (int)_web_URLStringLength
{
    int length = 0;
    if (!CFURLGetBaseURL((CFURLRef)self)) {
        length = CFURLGetBytes((CFURLRef)self, NULL, 0);
    }
    else {
        length = [[self absoluteString] length];
    }
    return length;
}

@end
