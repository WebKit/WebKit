//
//  IFPluginNullEventSender.h
//  WebKit
//
//  Created by Chris Blumenberg on Mon Apr 08 2002.
//  Copyright (c) 2002 __MyCompanyName__. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <npapi.h>

@interface IFPluginNullEventSender : NSObject{
    NPP instance;
    NPP_HandleEventProcPtr NPP_HandleEvent;
    bool shouldStop;
}

-(id)initializeWithNPP:(NPP)pluginInstance functionPointer:(NPP_HandleEventProcPtr)HandleEventFunction;
-(void)sendNullEvents;
-(void)stop;
@end