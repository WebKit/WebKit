/*	
    IFPluginNullEventSender.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "IFPluginNullEventSender.h"
#import <Carbon/Carbon.h>
#import <WebKitDebug.h>
#import <WebKit/IFPluginView.h>

@implementation IFPluginNullEventSender

-(id)initializeWithNPP:(NPP)pluginInstance functionPointer:(NPP_HandleEventProcPtr)handleEventFunction window:(NSWindow *)theWindow
{
    instance = pluginInstance;
    NPP_HandleEvent = handleEventFunction;
    shouldStop = FALSE;
    window = [theWindow retain];
    return self;
}

-(void) dealloc
{
    [window release];
}

-(void)sendNullEvents
{
    if (!shouldStop) {
        EventRecord event;
        bool acceptedEvent;
        
        [IFPluginView getCarbonEvent:&event];
        
        // plug-in should not react to cursor position when not active.
        if(![window isKeyWindow]){
            event.where.v = 0;
            event.where.h = 0;
        }
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

