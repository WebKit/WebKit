#include <JavaScriptCore/JSBase.h>

@class WebFrame;

@interface WebScriptObject (WebPendingPublic)
/*!
    @method JSObject
    @result The equivalent JSObjectRef for this WebScriptObject.
    @discussion Use this method to bridge between the WebScriptObject and 
    JavaScriptCore APIs.
*/
- (JSObjectRef)JSObject;
@end
