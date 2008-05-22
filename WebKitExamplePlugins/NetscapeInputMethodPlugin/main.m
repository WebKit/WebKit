/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
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
 or logos of Apple Computer, Inc. may be used to endorse or promote products derived from 
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
#import <WebKit/nptextinput.h>

#import <Cocoa/Cocoa.h>

// Browser function table
static NPNetscapeFuncs* browser;

// Structure for per-instance storage
typedef struct PluginObject
{
    NPP npp;
    
    NPWindow window;
    
    bool pluginHasFocus;
    
    bool textFieldHasFocus;
    NSRect textFieldRect;
    
    NSRange markedRange;
    NSRange selectedRange;
    NSTextStorage *textStorage;
    NSLayoutManager *layoutManager;
    NSTextContainer *textContainer;
    
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

    obj->textFieldRect = NSMakeRect(10, 10, 200, 100);

    obj->textStorage = [[NSTextStorage alloc] initWithString:@""];
    obj->layoutManager = [[NSLayoutManager alloc] init];
    [obj->textStorage addLayoutManager:obj->layoutManager];
    
    obj->textContainer = [[NSTextContainer alloc] initWithContainerSize:obj->textFieldRect.size];
    [obj->layoutManager addTextContainer:obj->textContainer];

    obj->selectedRange.location = [obj->textStorage length];
    
    obj->markedRange = NSMakeRange(NSNotFound, 0);

    return NPERR_NO_ERROR;
}

NPError NPP_Destroy(NPP instance, NPSavedData** save)
{
    // Free per-instance storage
    PluginObject *obj = instance->pdata;
    
    [obj->textStorage release];
    [obj->layoutManager release];
    
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

static void handleDraw(PluginObject *obj)
{
    NSGraphicsContext *oldContext = [[NSGraphicsContext currentContext] retain];
    
    NSGraphicsContext *context = [NSGraphicsContext graphicsContextWithGraphicsPort:((NP_CGContext *)obj->window.window)->context
                                                                            flipped:YES];


    [NSGraphicsContext setCurrentContext:context];
    
    NSRect rect = NSMakeRect(0, 0, obj->window.width, obj->window.height);
    
    [[NSColor lightGrayColor] set];
    [NSBezierPath fillRect:rect];

    if (obj->pluginHasFocus) {
        [[NSColor blackColor] set];
        [NSBezierPath strokeRect:rect];
    }
    
    [[NSColor whiteColor] set];
    [NSBezierPath fillRect:obj->textFieldRect];

    // Draw the text
    NSRange glyphRange = [obj->layoutManager glyphRangeForTextContainer:obj->textContainer];
    if (glyphRange.length > 0) {
        [obj->layoutManager drawBackgroundForGlyphRange:glyphRange atPoint:obj->textFieldRect.origin];
        [obj->layoutManager drawGlyphsForGlyphRange:glyphRange atPoint:obj->textFieldRect.origin];
    }
    
    NSBezierPath *textInputBorder = [NSBezierPath bezierPathWithRect:obj->textFieldRect];
    [[NSColor blackColor] set];
    
    if (obj->pluginHasFocus && obj->textFieldHasFocus)
        [textInputBorder setLineWidth:2];
    else
        [textInputBorder setLineWidth:1];
    
    [textInputBorder stroke];
    
    if (obj->pluginHasFocus && obj->textFieldHasFocus) {
        NSUInteger rectCount;
        NSRect *rectArray = [obj->layoutManager rectArrayForCharacterRange:obj->selectedRange
                                            withinSelectedCharacterRange:obj->selectedRange
                                                        inTextContainer:obj->textContainer
                                                                rectCount:&rectCount];
        
        [[NSColor blackColor] set];
        for (unsigned i = 0; i < rectCount; i++) {
            NSRect rect = rectArray[i];
            rect.origin.x += obj->textFieldRect.origin.x;
            rect.origin.y += obj->textFieldRect.origin.y;
            
            [NSBezierPath strokeRect:rect];
        }        
    }
    
    [NSGraphicsContext setCurrentContext:oldContext];
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

static void handleFocusChanged(NPCocoaEvent *cocoaEvent, PluginObject *obj)
{
    obj->pluginHasFocus = cocoaEvent->event.focus.hasFocus;
    
    invalidatePlugin(obj);
}

static void handleMouseMoved(NPCocoaEvent *cocoaEvent, PluginObject *obj)
{
    NSPoint point = NSMakePoint(cocoaEvent->event.mouse.pluginX, cocoaEvent->event.mouse.pluginY);
    
    if (NSPointInRect(point, obj->textFieldRect))
        [[NSCursor IBeamCursor] set];
    else
        [[NSCursor arrowCursor] set];
}

static void handleMouseDown(NPCocoaEvent *cocoaEvent, PluginObject *obj) 
{
    NSPoint point = NSMakePoint(cocoaEvent->event.mouse.pluginX, cocoaEvent->event.mouse.pluginY);
    
    obj->textFieldHasFocus = NSPointInRect(point, obj->textFieldRect);
    
    invalidatePlugin(obj);
}

int16 NPP_HandleEvent(NPP instance, void* event)
{
    PluginObject *obj = instance->pdata;

    NPCocoaEvent *cocoaEvent = event;
    
    switch (cocoaEvent->type) {
        case NPCocoaEventDrawRect:
            handleDraw(obj);
            return 1;
        case NPCocoaEventFocusChanged:
            handleFocusChanged(cocoaEvent, obj);
            return 1;
        case NPCocoaEventMouseMoved:
            handleMouseMoved(cocoaEvent, obj);
            return 1;
        case NPCocoaEventMouseDown:
            handleMouseDown(cocoaEvent, obj);
            return 1;
        case NPCocoaEventKeyDown:
            // If the text field has focus we ignore the event, causing it
            // to be sent to the input manager.
            if (obj->textFieldHasFocus)
                return 0;
            else
                return 1;
                
    }
    
    return 0;
}

void NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData)
{

}

static NSRange selectionRange(PluginObject *obj)
{
    if (obj->markedRange.location != NSNotFound)
        return obj->markedRange;
    else
        return obj->selectedRange;
}

/* Text Input */

void NPP_InsertText(NPP npp, id aString)
{
    PluginObject *obj = npp->pdata;
    
    NSRange range = selectionRange(obj);
    
    // Get rid of the marked text
    if (NPP_HasMarkedText(npp)) {
        [obj->textStorage deleteCharactersInRange:obj->markedRange];
        range.length = 0;
    }
    
    [obj->textStorage replaceCharactersInRange:range withString:aString];
    
    obj->selectedRange.location = range.location + [aString length];
    obj->selectedRange.length = 0;
    
    obj->markedRange = NSMakeRange(NSNotFound, 0);

    invalidatePlugin(obj);
}

void NPP_DoCommandBySelector(NPP npp, SEL aSelector)
{
    PluginObject *obj = npp->pdata;

    if (aSelector == @selector(moveRight:)) {
        if (obj->selectedRange.location == [obj->textStorage length])
            return;
        
        obj->selectedRange.location++;  
        invalidatePlugin(obj);
    } else if (aSelector == @selector(moveLeft:)) {
        if (obj->selectedRange.location == 0)
            return;
        
        obj->selectedRange.location--;
        invalidatePlugin(obj);
    }
}

static NSDictionary *markedTextAttributes()
{
    static NSDictionary *markedTextAttributes = nil;
    if (!markedTextAttributes) {
        NSTextView *tv = [[NSTextView alloc] initWithFrame:NSMakeRect(0, 0, 100, 100)];
        markedTextAttributes = [[tv markedTextAttributes] retain];
        [tv release];
    }
    
    return markedTextAttributes;
}

void NPP_SetMarkedText(NPP npp, id aString, NSRange selRange)
{
    PluginObject *obj = npp->pdata;

    BOOL isAttributedString = [aString isKindOfClass:[NSAttributedString class]];
    
    NSRange range = selectionRange(obj);
    
    if (!isAttributedString)        
        aString = [[[NSAttributedString alloc] initWithString:aString attributes:markedTextAttributes()] autorelease];
    
    [obj->textStorage replaceCharactersInRange:range withAttributedString:aString];
    
    obj->selectedRange.location = range.location + selRange.location;
    obj->selectedRange.length = selRange.length;
    
    obj->markedRange = NSMakeRange(range.location, [aString length]);
    
    invalidatePlugin(obj);
}

void NPP_UnmarkText(NPP npp)
{
}

BOOL NPP_HasMarkedText(NPP npp)
{
    PluginObject *obj = npp->pdata;

    return obj->markedRange.location != NSNotFound;
}

NSAttributedString *NPP_AttributedSubstringFromRange(NPP npp, NSRange theRange)
{
    return nil;
}

NSRange NPP_MarkedRange(NPP npp)
{
    PluginObject *obj = npp->pdata;

    return obj->markedRange;
}

NSRange NPP_SelectedRange(NPP npp)
{
    PluginObject *obj = npp->pdata;
    
    return obj->selectedRange;
}

NSRect NPP_FirstRectForCharacterRange(NPP npp, NSRange theRange)
{
    PluginObject *obj = npp->pdata;

    NSUInteger rectCount;
    NSRect *rectArray = [obj->layoutManager rectArrayForCharacterRange:theRange
                                     withinSelectedCharacterRange:theRange
                                                  inTextContainer:obj->textContainer
                                                        rectCount:&rectCount];
    
    return rectArray[0];
}

static NPPluginTextInputFuncs* pluginTextInputFuncs()
{
    static NPPluginTextInputFuncs textInputFuncs;
    static bool initialized = false;
    
    if (!initialized) {
        textInputFuncs.version = 0;
        textInputFuncs.size = sizeof(textInputFuncs);

        textInputFuncs.insertText = NPP_InsertText;
        textInputFuncs.doCommandBySelector = NPP_DoCommandBySelector;
        textInputFuncs.setMarkedText = NPP_SetMarkedText;
        textInputFuncs.unmarkText = NPP_UnmarkText;
        textInputFuncs.hasMarkedText = NPP_HasMarkedText;
        textInputFuncs.attributedSubstringFromRange = NPP_AttributedSubstringFromRange;
        textInputFuncs.markedRange = NPP_MarkedRange;
        textInputFuncs.selectedRange = NPP_SelectedRange;
        textInputFuncs.firstRectForCharacterRange = NPP_FirstRectForCharacterRange;

        initialized = true;
    }
    
    return &textInputFuncs;
}

NPError NPP_GetValue(NPP instance, NPPVariable variable, void *value)
{
    switch (variable) {
        case NPPVpluginTextInputFuncs:
            *(NPPluginTextInputFuncs**)value = pluginTextInputFuncs();            
            return NPERR_NO_ERROR;
    }
    
    return NPERR_GENERIC_ERROR;
}

NPError NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    return NPERR_GENERIC_ERROR;
}
