/*	
    WebPluginNullEventSender.m
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <WebKit/WebBaseNetscapePluginView.h>
#import <WebKit/WebKitLogging.h>
#import <WebKit/WebPluginNullEventSender.h>

#import <WebFoundation/WebAssertions.h>

#import <Carbon/Carbon.h>

@implementation WebNetscapePluginNullEventSender

-(id)initWithPluginView:(WebBaseNetscapePluginView *)pluginView
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
    
    [WebBaseNetscapePluginView getCarbonEvent:&event];
    
    // plug-in should not react to cursor position when not active.
    // FIXME: How does passing a v and h of 0 prevent it from reacting to the cursor position?
    if (![[view window] isKeyWindow]) {
        event.where.v = 0;
        event.where.h = 0;
    }
    
    [view sendEvent:&event];
    
    //LOG(Plugins, "NPP_HandleEvent(nullEvent): %d  when: %u %d", acceptedEvent, (unsigned)event.when, shouldStop);
    
    // FIXME: Why .01? Why not 0? Why not a larger number?
    [self performSelector:@selector(sendNullEvents) withObject:nil afterDelay:.01];
}

-(void)stop
{
    LOG(Plugins, "Stopping null events");
    [NSObject cancelPreviousPerformRequestsWithTarget:self selector:@selector(sendNullEvents) object:nil];
}

@end
