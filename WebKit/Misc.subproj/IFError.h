/*	
        IFError.h
	Copyright 2001, Apple, Inc. All rights reserved.
        
        Public header file.
*/
#import <Foundation/Foundation.h>

@interface IFError : NSObject
{
    int errorCode;
}

- initWithErrorCode: (int)c;
- (int)errorCode;
- (NSString *)errorDescription;

@end
