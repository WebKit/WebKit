/*	
    IFPluginNullEventSender.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFPluginNullEventSender.h"
#import <Carbon/Carbon.h>
#import <WebKitDebug.h>

@implementation IFPluginNullEventSender

-(id)initializeWithNPP:(NPP)pluginInstance functionPointer:(NPP_HandleEventProcPtr)HandleEventFunction;
{
    instance = pluginInstance;
    NPP_HandleEvent = HandleEventFunction;
    shouldStop = FALSE;
    return self;
}

-(void)sendNullEvents
{
    EventRecord event;
    bool acceptedEvent;
    Point point;
    
    GetGlobalMouse(&point);
    
    if(!shouldStop){
        event.what = nullEvent;
        event.message = 0;
        event.when = TickCount();
        event.where = point;
        event.modifiers = GetCurrentKeyModifiers();
        acceptedEvent = NPP_HandleEvent(instance, &event);
        //WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(nullEvent): %d  when: %u %d\n", acceptedEvent, (unsigned)event.when, shouldStop);
        [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:.01];
    }
}

-(void) stop
{
    WEBKITDEBUG("Stopping null events\n");
    shouldStop = TRUE;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil];
}

@end

