//
//  IFNullPluginView.h
//  WebKit
//
//  Created by Chris Blumenberg on Fri Apr 05 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <AppKit/AppKit.h>


@interface IFNullPluginView : NSImageView {

    BOOL errorSent;
}

- initWithFrame:(NSRect)frame mimeType:(NSString *)mime arguments:(NSDictionary *)arguments;

@end
