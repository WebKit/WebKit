/*	
    WebPluginNullEventSender.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import "WebPluginNullEventSender.h"
#import <Carbon/Carbon.h>
#import <WebFoundation/WebAssertions.h>
#import <WebKit/WebKitDebug.h>
#import <WebKit/WebPluginView.h>

@implementation WebNetscapePluginNullEventSender

-(id)initWithPluginView:(WebNetscapePluginView *)pluginView
{
    [super init];
    view = [pluginView retain];
    return self;
}

-(void)dealloc
{
    [view release];
    [super dealloc];
}

-(void)sendNullEvents
{
    EventRecord event;
    
    [WebNetscapePluginView getCarbonEvent:&event];
    
    // plug-in should not react to cursor position when not active.
    // FIXME: How does passing a v and h of 0 prevent it from reacting to the cursor position?
    if (![[view window] isKeyWindow]) {
        event.where.v = 0;
        event.where.h = 0;
    }
    
    [view sendEvent:&event];
    
    //WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "NPP_HandleEvent(nullEvent): %d  when: %u %d", acceptedEvent, (unsigned)event.when, shouldStop);
    
    // FIXME: Why .01? Why not 0? Why not a larger number?
    [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:.01];
}

-(void)stop
{
    WEBKITDEBUGLEVEL(WEBKIT_LOG_PLUGINS, "Stopping null events");
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil];
}

@end
