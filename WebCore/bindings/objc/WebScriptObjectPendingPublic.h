#include <JavaScriptCore/JSBase.h>

@interface WebScriptObject (WebPendingPublic)
/*!
    @method scriptObjectForJSObject:
    @param jsObject The JSObjectRef to wrap in a WebScriptObject.
    @result The equivalent WebScriptObject for jsObject. If such an object
    has not yet been created, this method creates one. The WebScriptObject returned 
    is autoreleased.
    @discussion Use this method to bridge between the WebScriptObject and 
    JavaScriptCore APIs.
*/
+ (id)scriptObjectForJSObject:(JSObjectRef)jsObject;

/*!
    @method JSObject
    @result The equivalent JSObjectRef for this WebScriptObject.
    @discussion Use this method to bridge between the WebScriptObject and 
    JavaScriptCore APIs.
*/
- (JSObjectRef)JSObject;
@end
