/*	
    WebPluginNullEventSender.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebPluginNullEventSender.h"
#import <Carbon/Carbon.h>
#import <WebKitDebug.h>
#import <WebKit/WebPluginView.h>

@implementation WebPluginNullEventSender

-(id)initWithPluginView:(WebPluginView *)pluginView
{
    [super init];
    
    instance = [pluginView pluginInstance];
    NPP_HandleEvent = [pluginView NPP_HandleEvent];
    window = [[pluginView window] retain];
    
    return self;
}

-(void) dealloc
{
    [window release];
    
    [super dealloc];
}

-(void)sendNullEvents
{
    if (!shouldStop) {
        EventRecord event;
        BOOL acceptedEvent;
        
        [WebPluginView getCarbonEvent:&event];
        
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
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stopping null events\n");
    shouldStop = TRUE;
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil];
}

@end

