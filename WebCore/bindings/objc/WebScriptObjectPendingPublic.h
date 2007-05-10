#include <JavaScriptCore/JSBase.h>

@class WebFrame;

@interface WebScriptObject (WebPendingPublic)
/*!
    @method scriptObjectForJSObject:frame:
    @param jsObject The JSObjectRef to wrap in a WebScriptObject.
    @param frame The WebFrame with which to associate the WebScriptObject.
    @result The equivalent WebScriptObject for jsObject. If such an object
    has already been created, the existing one is returned. Otherwise, this 
    method creates a new object.
    @discussion Use this method to bridge between the WebScriptObject and 
    JavaScriptCore APIs. The returned WebScriptObject's usable lifetime is tied 
    to its associated frame. If the frame navigates to a new location or is removed
    from its WebView, the WebScriptObject becomes "inert" and returns nil or 
    @"Undefined" to all messages.
*/
+ (id)scriptObjectForJSObject:(JSObjectRef)jsObject frame:(WebFrame *)frame;

/*!
    @method JSObject
    @result The equivalent JSObjectRef for this WebScriptObject.
    @discussion Use this method to bridge between the WebScriptObject and 
    JavaScriptCore APIs.
*/
- (JSObjectRef)JSObject;
@end
