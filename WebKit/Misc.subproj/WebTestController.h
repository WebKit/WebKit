//
//  WebTestController.h
//  WebKit
//
//  Created by Darin Adler on Thu Aug 08 2002.
//  Copyright (c) 2002 Apple Computer, Inc. All rights reserved.
//

#import <Foundation/Foundation.h>

@protocol WebDrawingObserver <NSObject>
@end

@interface WebTestController : NSObject
{
}

+ (void)setDrawingObserver:(id <WebDrawingObserver>)observer;

@end
