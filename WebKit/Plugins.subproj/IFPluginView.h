/*	
    IFPluginView.h
	Copyright 2002, Apple, Inc. All rights reserved.
*/

#import <Cocoa/Cocoa.h>
#import <npapi.h>

@class IFPluginNullEventSender;
@class IFWebDataSource;
@class WCPlugin;
@protocol IFWebController;

@interface IFPluginView : NSView
{
    WCPlugin *plugin;
    IFPluginNullEventSender *eventSender;
    unsigned argsCount;
    char **cAttributes, **cValues;
    
    id <IFWebController> webController;
    IFWebDataSource *webDataSource;
    
    NPP instance;
    NPWindow window;
    NP_Port nPort;
    NPP_t instanceStruct;

    BOOL canRestart, isHidden, isStarted, fullMode;
            
    NSString *URL, *mime;
    NSURL *baseURL;
    NSTrackingRectTag trackingTag;
    NSMutableArray *filesToErase, *activeURLHandles;
    
    NPP_NewProcPtr NPP_New;
    NPP_DestroyProcPtr NPP_Destroy;
    NPP_SetWindowProcPtr NPP_SetWindow;
    NPP_NewStreamProcPtr NPP_NewStream;
    NPP_DestroyStreamProcPtr NPP_DestroyStream;
    NPP_StreamAsFileProcPtr NPP_StreamAsFile;
    NPP_WriteReadyProcPtr NPP_WriteReady;
    NPP_WriteProcPtr NPP_Write;
    NPP_PrintProcPtr NPP_Print;
    NPP_HandleEventProcPtr NPP_HandleEvent;
    NPP_URLNotifyProcPtr NPP_URLNotify;
    NPP_GetValueProcPtr NPP_GetValue;
    NPP_SetValueProcPtr NPP_SetValue;
}

- initWithFrame:(NSRect)r plugin:(WCPlugin *)plug url:(NSString *)location mime:(NSString *)mime arguments:(NSDictionary *)arguments mode:(uint16)mode;
-(void)drawRect:(NSRect)rect;
-(void)setWindow;
-(void)viewHasMoved:(NSNotification *)notification;
-(void) windowWillClose:(NSNotification *)notification;
-(void)newStream:(NSURL *)streamURL mimeType:(NSString *)mimeType notifyData:(void *)notifyData;
-(NSView *) findSuperview:(NSString *)viewName;
-(BOOL)acceptsFirstResponder;
-(BOOL)becomeFirstResponder;
-(BOOL)resignFirstResponder;
-(void)sendActivateEvent:(BOOL)isActive;
-(void)sendUpdateEvent;
-(void)mouseDown:(NSEvent *)theEvent;
-(void)mouseUp:(NSEvent *)theEvent;
-(void)mouseEntered:(NSEvent *)theEvent;
-(void)mouseExited:(NSEvent *)theEvent;
-(void)keyDown:(NSEvent *)theEvent;
-(void)keyUp:(NSEvent *)theEvent;
-(void)stop;
-(void)start;

// plug-in to browser calls
-(NPError)getURLNotify:(const char *)url target:(const char *)target notifyData:(void *)notifyData;
-(NPError)getURL:(const char *)url target:(const char *)target;
-(NPError)postURLNotify:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file notifyData:(void *)notifyData;
-(NPError)postURL:(const char *)url target:(const char *)target len:(UInt32)len buf:(const char *)buf file:(NPBool)file;
-(NPError)newStream:(NPMIMEType)type target:(const char *)target stream:(NPStream**)stream;
-(NPError)write:(NPStream*)stream len:(SInt32)len buffer:(void *)buffer;
-(NPError)destroyStream:(NPStream*)stream reason:(NPReason)reason;
-(void)status:(const char *)message;
-(NPError)getValue:(NPNVariable)variable value:(void *)value;
-(NPError)setValue:(NPPVariable)variable value:(void *)value;
-(void)invalidateRect:(NPRect *)invalidRect;
-(void)invalidateRegion:(NPRegion)invalidateRegion;
-(void)forceRedraw;

@end

NSString* startupVolumeName(void);
