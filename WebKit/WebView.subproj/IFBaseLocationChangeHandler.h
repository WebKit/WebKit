/*	
        IFBaseLocationChangeHandler.h
	Copyright 2002, Apple, Inc. All rights reserved.

        Public header file.
*/

#import <Foundation/Foundation.h>

#import <WebKit/IFLocationChangeHandler.h>

@protocol IFLocationChangeHandler;

@interface IFBaseLocationChangeHandler : NSObject <IFLocationChangeHandler>
{
    NSURL *url;
    NSString *MIMEType;
    IFContentPolicy contentPolicy;
}

// Maintain a persistent database of type-to-policy.
+ (void)setGlobalContentPolicy: (IFContentPolicy)policy forMIMEType: (NSString *)type;
+ (IFContentPolicy)globaContentPolicyForMIMEType: (NSString *)typen;
+ (NSDictionary *)globalContentPolicies;

+ (BOOL)canViewMIMEType: (NSString *)MIMEType;
+ (IFContentPolicy)builtinPolicyForMIMEType: (NSString *)MIMEType;

+ (NSString *)suggestedFileanemForURL: (NSURL *) andMIMEType: (NSString *)type;
+ (NSString *)suggestedDirectoryForURL: (NSURL *) andMIMEType: (NSString *)type;

// Returns the extension from the URL.  May be used in conjunction with 
// the MIME type to determine how a location should be handled.
+ (NSString *)extensionForURL: (NSURL *)url;

- (NSURL *)URL;

- (NSString *)MIMEType;

@end
