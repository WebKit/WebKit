/*
     File: MovieControllerLayer.m
 
 Disclaimer: IMPORTANT:  This Apple software is supplied to you by Apple
 Inc. ("Apple") in consideration of your agreement to the following
 terms, and your use, installation, modification or redistribution of
 this Apple software constitutes acceptance of these terms.  If you do
 not agree with these terms, please do not use, install, modify or
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and
 subject to these terms, Apple grants you a personal, non-exclusive
 license, under Apple's copyrights in this original Apple software (the
 "Apple Software"), to use, reproduce, modify and redistribute the Apple
 Software, with or without modifications, in source and/or binary forms;
 provided that if you redistribute the Apple Software in its entirety and
 without modifications, you must retain this notice and the following
 text and disclaimers in all such redistributions of the Apple Software.
 Neither the name, trademarks, service marks or logos of Apple Inc. may
 be used to endorse or promote products derived from the Apple Software
 without specific prior written permission from Apple.  Except as
 expressly stated in this notice, no other rights or licenses, express or
 implied, are granted by Apple herein, including but not limited to any
 patent rights that may be infringed by your derivative works or by other
 works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE
 MAKES NO WARRANTIES, EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION
 THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY AND FITNESS
 FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS USE AND
 OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL
 OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, REPRODUCTION,
 MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED
 AND WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE),
 STRICT LIABILITY OR OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE
 POSSIBILITY OF SUCH DAMAGE.
 
 Copyright (C) 2009 Apple Inc. All Rights Reserved.
 
 */

#import <WebKit/npapi.h>
#import <WebKit/npfunctions.h>
#import <WebKit/npruntime.h>

#import <QuartzCore/QuartzCore.h>
#import <QTKit/QTKit.h>

#import "MovieControllerLayer.h"

// Browser function table
static NPNetscapeFuncs* browser;

// Structure for per-instance storage
typedef struct PluginObject
{
    NPP npp;
    
    NPWindow window;
    
    CALayer *rootLayer;
    MovieControllerLayer *controllerLayer;
    QTMovieLayer *movieLayer;
    
    CALayer *mouseDownLayer;
    
    NSURL *movieURL;
    QTMovie *movie;
} PluginObject;

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPP_Destroy(NPP instance, NPSavedData** save);
NPError NPP_SetWindow(NPP instance, NPWindow* window);
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32 NPP_WriteReady(NPP instance, NPStream* stream);
int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void NPP_Print(NPP instance, NPPrint* platformPrint);
int16 NPP_HandleEvent(NPP instance, void* event);
void NPP_URLNotify(NPP instance, const char* URL, NPReason reason, void* notifyData);
NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value);
NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value);

#pragma export on
// Mach-o entry points
NPError NP_Initialize(NPNetscapeFuncs *browserFuncs);
NPError NP_GetEntryPoints(NPPluginFuncs *pluginFuncs);
void NP_Shutdown(void);
#pragma export off

NPError NP_Initialize(NPNetscapeFuncs* browserFuncs)
{
    browser = browserFuncs;
    return NPERR_NO_ERROR;
}

NPError NP_GetEntryPoints(NPPluginFuncs* pluginFuncs)
{
    pluginFuncs->version = 11;
    pluginFuncs->size = sizeof(pluginFuncs);
    pluginFuncs->newp = NPP_New;
    pluginFuncs->destroy = NPP_Destroy;
    pluginFuncs->setwindow = NPP_SetWindow;
    pluginFuncs->newstream = NPP_NewStream;
    pluginFuncs->destroystream = NPP_DestroyStream;
    pluginFuncs->asfile = NPP_StreamAsFile;
    pluginFuncs->writeready = NPP_WriteReady;
    pluginFuncs->write = (NPP_WriteProcPtr)NPP_Write;
    pluginFuncs->print = NPP_Print;
    pluginFuncs->event = NPP_HandleEvent;
    pluginFuncs->urlnotify = NPP_URLNotify;
    pluginFuncs->getvalue = NPP_GetValue;
    pluginFuncs->setvalue = NPP_SetValue;
    
    return NPERR_NO_ERROR;
}

void NP_Shutdown(void)
{

}

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved)
{
    // Create per-instance storage
    PluginObject *obj = (PluginObject *)malloc(sizeof(PluginObject));
    bzero(obj, sizeof(PluginObject));
    
    obj->npp = instance;
    instance->pdata = obj;
    
    // Ask the browser if it supports the Core Animation drawing model
    NPBool supportsCoreAnimation;
    if (browser->getvalue(instance, NPNVsupportsCoreAnimationBool, &supportsCoreAnimation) != NPERR_NO_ERROR)
        supportsCoreAnimation = FALSE;
    
    if (!supportsCoreAnimation)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
    
    // If the browser supports the Core Animation drawing model, enable it.
    browser->setvalue(instance, NPPVpluginDrawingModel, (void *)NPDrawingModelCoreAnimation);

    // If the browser supports the Cocoa event model, enable it.
    NPBool supportsCocoa;
    if (browser->getvalue(instance, NPNVsupportsCocoaBool, &supportsCocoa) != NPERR_NO_ERROR)
        supportsCocoa = FALSE;
    
    if (!supportsCocoa)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
    
    browser->setvalue(instance, NPPVpluginEventModel, (void *)NPEventModelCocoa);
    
    for (int16 i = 0; i < argc; i++) {
        if (strcasecmp(argn[i], "movieurl") == 0) {
            NSString *urlString = [NSString stringWithUTF8String:argv[i]];
            if (urlString) 
                obj->movieURL = [[NSURL URLWithString:urlString] retain];
            break;
        }
        
    }
    
    return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save)
{
    // Free per-instance storage
    PluginObject *obj = instance->pdata;

    [obj->movie stop];
    [obj->rootLayer release];
    
    free(obj);
    
    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window)
{
    PluginObject *obj = instance->pdata;
    obj->window = *window;

    return NPERR_NO_ERROR;
}
 

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype)
{
    *stype = NP_ASFILEONLY;
    return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    return NPERR_NO_ERROR;
}

int32 NPP_WriteReady(NPP instance, NPStream* stream)
{
    return 0;
}

int32 NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer)
{
    return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
}

void NPP_Print(NPP instance, NPPrint* platformPrint)
{

}

static void handleMouseDown(PluginObject *obj, NPCocoaEvent *event)
{
    CGPoint point = CGPointMake(event->data.mouse.pluginX, 
                                // Flip the y coordinate
                                obj->window.height - event->data.mouse.pluginY);
    
    obj->mouseDownLayer = [obj->rootLayer hitTest:point];
    if (obj->mouseDownLayer == obj->controllerLayer) {
        [obj->controllerLayer handleMouseDown:[obj->rootLayer convertPoint:point toLayer:obj->controllerLayer]];
        return;
    }    
}

static void togglePlayPause(PluginObject *obj)
{
    if (!obj->movie)
        return;
    
    if ([obj->movie rate] == 0)
        [obj->movie play];
    else
        [obj->movie stop];

}

static void handleMouseUp(PluginObject *obj, NPCocoaEvent *event)
{
    CGPoint point = CGPointMake(event->data.mouse.pluginX, 
                                // Flip the y coordinate
                                obj->window.height - event->data.mouse.pluginY);

    CALayer *mouseDownLayer = obj->mouseDownLayer;
    obj->mouseDownLayer = nil;
    if (mouseDownLayer == obj->controllerLayer) {
        [obj->controllerLayer handleMouseUp:[obj->rootLayer convertPoint:point toLayer:obj->controllerLayer]];
        return;
    }
}

static void handleMouseDragged(PluginObject *obj, NPCocoaEvent *event)
{
    CGPoint point = CGPointMake(event->data.mouse.pluginX, 
                                // Flip the y coordinate
                                obj->window.height - event->data.mouse.pluginY);
    
    if (obj->mouseDownLayer == obj->controllerLayer) {
        [obj->controllerLayer handleMouseDragged:[obj->rootLayer convertPoint:point toLayer:obj->controllerLayer]];
        return;
    }    
}

static void handleMouseEntered(PluginObject *obj)
{
    // Show the controller layer.
    obj->controllerLayer.opacity = 1.0;
}

static void handleMouseExited(PluginObject *obj)
{
    // Hide the controller layer if the movie is playing.
    if ([obj->movie rate])
        obj->controllerLayer.opacity = 0.0;
}

static int handleKeyDown(PluginObject *obj, NPCocoaEvent *event)
{
    NSString *characters = (NSString *)event->data.key.characters;
    
    if ([characters length] == 1 && [characters characterAtIndex:0] == ' ') {
        togglePlayPause(obj);
        return 1;
    }
    
    return 0;
}


static int handleScrollEvent(PluginObject *obj, NPCocoaEvent *event)
{
    double delta = event->data.mouse.deltaY;
    if (delta < 0)
        [obj->movie stepForward];
    else
        [obj->movie stepBackward];
    return 0;
}



int16 NPP_HandleEvent(NPP instance, void* event)
{
    PluginObject *obj = instance->pdata;

    NPCocoaEvent *cocoaEvent = event;
    
    switch(cocoaEvent->type) {
        case NPCocoaEventMouseDown:
            handleMouseDown(obj, cocoaEvent);
            return 1;
        case NPCocoaEventMouseUp:
            handleMouseUp(obj, cocoaEvent);
            return 1;
        case NPCocoaEventMouseDragged:
            handleMouseDragged(obj, cocoaEvent);
            return 1;
        case NPCocoaEventMouseEntered:
            handleMouseEntered(obj);
            return 1;
        case NPCocoaEventMouseExited:
            handleMouseExited(obj);
            return 1;
        case NPCocoaEventKeyDown:
            return handleKeyDown(obj, cocoaEvent);
        case NPCocoaEventScrollWheel:
            return handleScrollEvent(obj, cocoaEvent);
    }
    
    return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{

}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    PluginObject *obj = instance->pdata;

    switch (variable) {
        case NPPVpluginCoreAnimationLayer:
            if (!obj->rootLayer) {
                // Setup layer hierarchy.
                obj->rootLayer = [[CALayer layer] retain];
                
                obj->movieLayer = [QTMovieLayer layer];
                obj->movieLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
                [obj->rootLayer addSublayer:obj->movieLayer];

                obj->controllerLayer = [MovieControllerLayer layer];
                [obj->rootLayer addSublayer:obj->controllerLayer];
                
                if (obj->movieURL) {
                    NSDictionary *attributes = [NSDictionary dictionaryWithObjectsAndKeys:obj->movieURL, QTMovieURLAttribute,
                                                [NSNumber numberWithBool:YES], QTMovieOpenForPlaybackAttribute, 
                                                [NSNumber numberWithBool:YES], QTMovieLoopsAttribute, 
                                                nil];
                    obj->movie = [QTMovie movieWithAttributes:attributes error:nil];

                    if (obj->movie) {
                        obj->movieLayer.movie = obj->movie;
                        [obj->controllerLayer setMovie:obj->movie];
                    }
                }
                    
            }
            
            // Make sure to return a retained layer
            *((CALayer **)value) = [obj->rootLayer retain];
            
            return NPERR_NO_ERROR;
            
        default:
            return NPERR_GENERIC_ERROR;
    }
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}
