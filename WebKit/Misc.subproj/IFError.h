/*	
        IFError.h
	Copyright 2001, 2002, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Foundation/Foundation.h>

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
