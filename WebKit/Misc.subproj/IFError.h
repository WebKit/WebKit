/*	
        IFError.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Foundation/Foundation.h>


// WebFoundation error codes < 10000
// WebKit error codes >= 10000

typedef enum {
    IFNonHTMLContentNotSupportedError = 10000
} IFErrorCode;

@interface IFError : NSObject
{
    int errorCode;
    NSURL *_failingURL;
}

- initWithErrorCode: (int)c;
- initWithErrorCode: (int)c failingURL: (NSURL *)url;
- (int)errorCode;
- (NSString *)errorDescription;
- (NSURL *)failingURL;

@end
