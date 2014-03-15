/*
 IMPORTANT:  This Apple software is supplied to you by Apple Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation, 
 modification or redistribution of this Apple software constitutes acceptance of these 
 terms.  If you do not agree with these terms, please do not use, install, modify or 
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and subject to these 
 terms, Apple grants you a personal, non-exclusive license, under Apple’s copyrights in 
 this original Apple software (the "Apple Software"), to use, reproduce, modify and 
 redistribute the Apple Software, with or without modifications, in source and/or binary 
 forms; provided that if you redistribute the Apple Software in its entirety and without 
 modifications, you must retain this notice and the following text and disclaimers in all 
 such redistributions of the Apple Software.  Neither the name, trademarks, service marks 
 or logos of Apple Inc. may be used to endorse or promote products derived from 
 the Apple Software without specific prior written permission from Apple. Except as expressly
 stated in this notice, no other rights or licenses, express or implied, are granted by Apple
 herein, including but not limited to any patent rights that may be infringed by your 
 derivative works or by other works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, 
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS 
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKit/npapi.h>
#import <WebKit/npfunctions.h>
#import <WebKit/npruntime.h>

#import <Cocoa/Cocoa.h>

#import "MenuHandler.h"

// Browser function table
static NPNetscapeFuncs* browser;

// Structure for per-instance storage
typedef struct PluginObject
{
    NPP npp;
    
    NPWindow window;
    
    NSString *string;
    bool hasFocus;
    bool mouseIsInsidePlugin;
    
    MenuHandler *menuHandler;
} PluginObject;

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved);
NPError NPP_Destroy(NPP instance, NPSavedData** save);
NPError NPP_SetWindow(NPP instance, NPWindow* window);
NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype);
NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32_t NPP_WriteReady(NPP instance, NPStream* stream);
int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer);
void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void NPP_Print(NPP instance, NPPrint* platformPrint);
int16_t NPP_HandleEvent(NPP instance, void* event);
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

NPError NPP_New(NPMIMEType pluginType, NPP instance, uint16_t mode, int16_t argc, char* argn[], char* argv[], NPSavedData* saved)
{
    // Create per-instance storage
    PluginObject *obj = (PluginObject *)malloc(sizeof(PluginObject));
    bzero(obj, sizeof(PluginObject));
    
    obj->npp = instance;
    instance->pdata = obj;
    
    // Ask the browser if it supports the CoreGraphics drawing model
    NPBool supportsCoreGraphics;
    if (browser->getvalue(instance, NPNVsupportsCoreGraphicsBool, &supportsCoreGraphics) != NPERR_NO_ERROR)
        supportsCoreGraphics = FALSE;
    
    if (!supportsCoreGraphics)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
    
    // If the browser supports the CoreGraphics drawing model, enable it.
    browser->setvalue(instance, NPPVpluginDrawingModel, (void *)NPDrawingModelCoreGraphics);

    // If the browser supports the Cocoa event model, enable it.
    NPBool supportsCocoa;
    if (browser->getvalue(instance, NPNVsupportsCocoaBool, &supportsCocoa) != NPERR_NO_ERROR)
        supportsCocoa = FALSE;
    
    if (!supportsCocoa)
        return NPERR_INCOMPATIBLE_VERSION_ERROR;
    
    browser->setvalue(instance, NPPVpluginEventModel, (void *)NPEventModelCocoa);
    
    return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save)
{
    // Free per-instance storage
    PluginObject *obj = instance->pdata;
    
    [obj->string release];
    [obj->menuHandler release];
    
    free(obj);
    
    return NPERR_NO_ERROR;
}

NPError NPP_SetWindow(NPP instance, NPWindow* window)
{
    PluginObject *obj = instance->pdata;
    obj->window = *window;

    return NPERR_NO_ERROR;
}
 

NPError NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16_t* stype)
{
    *stype = NP_ASFILEONLY;
    return NPERR_NO_ERROR;
}

NPError NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason)
{
    return NPERR_NO_ERROR;
}

int32_t NPP_WriteReady(NPP instance, NPStream* stream)
{
    return 0;
}

int32_t NPP_Write(NPP instance, NPStream* stream, int32_t offset, int32_t len, void* buffer)
{
    return 0;
}

void NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname)
{
}

void NPP_Print(NPP instance, NPPrint* platformPrint)
{

}

static void handleDraw(PluginObject *obj, NPCocoaEvent *event)
{
    NSGraphicsContext *oldContext = [[NSGraphicsContext currentContext] retain];
    
    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithGraphicsPort:event->data.draw.context
                                                                            flipped:YES];

    [NSGraphicsContext setCurrentContext:context];
    
    NSRect rect = NSMakeRect(0, 0, obj->window.width, obj->window.height);
    
    [[NSColor lightGrayColor] set];
    [NSBezierPath fillRect:rect];

    // If the plugin has focus, draw a focus indicator
    if (obj->hasFocus) {
        [[NSColor blackColor] set];
        NSBezierPath *path = [NSBezierPath bezierPathWithRect:rect];
        [path setLineWidth:5];
        [path stroke];
    }
    
    [obj->string drawAtPoint:NSMakePoint(10, 10) withAttributes:nil];
    
    [NSGraphicsContext setCurrentContext:oldContext];
}

static NSString *eventType(NPCocoaEventType type)
{
    switch (type) {
        case NPCocoaEventScrollWheel:
            return @"NPCocoaEventScrollWheel";
        case NPCocoaEventMouseDown:
            return @"NPCocoaEventMouseDown";
        case NPCocoaEventMouseUp:
            return @"NPCocoaEventMouseUp";            
        case NPCocoaEventMouseMoved:
            return @"NPCocoaEventMouseMoved";            
        case NPCocoaEventMouseDragged:
            return @"NPCocoaEventMouseDragged";            
        case NPCocoaEventMouseEntered:
            return @"NPCocoaEventMouseEntered";            
        case NPCocoaEventMouseExited:
            return @"NPCocoaEventMouseExited";
        case NPCocoaEventKeyDown:
            return @"NPCocoaEventKeyDown";
        case NPCocoaEventKeyUp:
            return @"NPCocoaEventKeyUp";
        case NPCocoaEventFlagsChanged:
            return @"NPCocoaEventFlagsChanged";
        default:
            return @"unknown";
    }    
}

static void invalidatePlugin(PluginObject *obj)
{
    NPRect rect;
    rect.left = 0;
    rect.top = 0;
    rect.right = obj->window.width;
    rect.bottom = obj->window.height;
    
    browser->invalidaterect(obj->npp, &rect);    
}


static void handleMouseEvent(PluginObject *obj, NPCocoaEvent *event)
{
    NSString *string = [NSString stringWithFormat:@"Type: %@\n"
                                                   "Modifier flags: 0x%x\n"
                                                   "Coordinates: (%g, %g)\n"
                                                   "Button number: %d\n"
                                                   "Click count: %d\n"
                                                   "Delta: (%g, %g, %g)",
                                                   eventType(event->type), 
                                                   event->data.mouse.modifierFlags,
                                                   event->data.mouse.pluginX,
                                                   event->data.mouse.pluginY,
                                                   event->data.mouse.buttonNumber,
                                                   event->data.mouse.clickCount,
                                                   event->data.mouse.deltaX, event->data.mouse.deltaY, event->data.mouse.deltaZ];
    
    
    [obj->string release];
    obj->string = [string retain];
 
    invalidatePlugin(obj);
    
    if (event->data.mouse.buttonNumber == 1) {
        if (!obj->menuHandler)
            obj->menuHandler = [[MenuHandler alloc] initWithBrowserFuncs:browser instance:obj->npp];
        
        browser->popupcontextmenu(obj->npp, (NPNSMenu *)[obj->menuHandler menu]);
    }
}

static void handleKeyboardEvent(PluginObject *obj, NPCocoaEvent *event)
{
    NSString *string = [NSString stringWithFormat:@"Type: %@\n"
                        "Modifier flags: 0x%x\n"
                        "Characters: %@\n"
                        "Characters ignoring modifiers: %@\n"
                        "Is a repeat: %@\n"
                        "Key code: %d",
                        eventType(event->type), 
                        event->data.key.modifierFlags,
                        event->data.key.characters,
                        event->data.key.charactersIgnoringModifiers,
                        event->data.key.isARepeat ? @"YES" : @"NO",
                        event->data.key.keyCode];
    
    
    [obj->string release];
    obj->string = [string retain];
    
    invalidatePlugin(obj);
}

int16_t NPP_HandleEvent(NPP instance, void* event)
{
    PluginObject *obj = instance->pdata;

    NPCocoaEvent *cocoaEvent = event;
    
    switch(cocoaEvent->type) {
        case NPCocoaEventFocusChanged:
            obj->hasFocus = cocoaEvent->data.focus.hasFocus;
            invalidatePlugin(obj);
            return 1;
            
        case NPCocoaEventDrawRect:
            handleDraw(obj, cocoaEvent);
            return 1;
        
        case NPCocoaEventKeyDown:
        case NPCocoaEventKeyUp:
        case NPCocoaEventFlagsChanged:
            handleKeyboardEvent(obj, cocoaEvent);
            return 1;
            
        case NPCocoaEventMouseDown:
        case NPCocoaEventMouseUp:
            
        // FIXME: NPCocoaEventMouseMoved is currently disabled in order to see other events more clearly
        // without "drowning" in mouse moved events.
//        case NPCocoaEventMouseMoved:
        case NPCocoaEventMouseEntered:
        case NPCocoaEventMouseExited:
        case NPCocoaEventMouseDragged:
        case NPCocoaEventScrollWheel:
            handleMouseEvent(obj, cocoaEvent);
            return 1;
    }
    
    return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{

}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}
