/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#ifndef _NP_OBJC_H_
#define _NP_OBJC_H_

/*
    These methods in WebJavaScriptMethods are optionally implemented by classes whose
    interfaces are exported to JavaScript.
*/
@interface NSObject (WebJavaScriptMethods)

/*
    Use the return string as the exported name for the selector
    in JavaScript.  It is the responsibility of the class to ensure
    uniqueness of the returned name.  If nil is returned or this
    method is not implemented the default name for the selector will
    be used.  The default name concatenates the components of the
    ObjectiveC selector name and replaces ':' with '_'.  '_' characters
    are escaped with an additional '$', i.e. '_' becomes "$_".  '$' are
    also escaped, i.e.  moveTo:: becomes move__, moveTo_ becomes moveTo$_,
    and moveTo$_ becomes moveTo$$$_.  It is strongly recommended that
    exported interfaces are carefully chosen to avoid convoluted names,
    and that for each exported interface an appropriate unique name is
    return by JavaScriptNameForProperty.
*/

+ (NSString *)JavaScriptNameForSelector:(SEL)aSelector;

// Exclude the selector from visibility in JavaScript.
+ (BOOL)isSelectorExcludedFromJavaScript:(SEL)aSelector;

// Provide an alternate name for a property.
+ (NSString *)JavaScriptNameForProperty:(const char *)name;

// Exclude the property from visibility in JavaScript.
+ (BOOL)isPropertyExcludedFromJavaScript:(const char *)name;

@end


@interface WebFrame
- (WebJavaScriptObject *)getJavaScriptObjectForWindow;
- (WebJavaScriptObject *)getJavaScriptObjectForSelf;
@end

@class WebJavaScriptObjectPrivate;
@interface WebJavaScriptObject : NSObject
{
    WebJavaScriptObjectPrivate *_private;
}

- (id)callMethodNamed:(NSString *)name withArguments:(NSArray *)args;
- (id)evaluateScript:(NSString *)script;
- (id)getPropertyNamed:(NSString *)name;
- (void)setPropertyNamed:(NSString *)name value:(id)value;
- (void)removePropertyNamed:(NSString *)name;
- (NSString *)toString;
- (id)getPropertyAtIndex:(unsigned int)index;
- (void)setPropertyAtIndex:(unsigned int)index value:(id)value;
@end


/*
    Extensions to WebKit

    /*
        WebNativeObjectForInstance can be used to export an instance via the 
        Netscape plugin extension API, NPP_GetNativeObjectForJavaScript().
    */
    NP_Object *WebNativeObjectForInstance (id instance);
    id WebInstanceFromNativeObject (NP_Object *obj);

    @protocol WebPluginViewFactory <NSObject>
    ..
    + (id)getObjectForJavaScript;
    ..
    @end

    @protocol WebPluginContainer <NSObject>
    ..
    - (WebFrame *)getWebFrame;
    ..
    @end
*/

/*
    Example usage:
    
    
    Any ObjectiveC instance may be exported to JavaScript via setMember
    or setSlot.  All the methods on the exported instance will be
    automatically exposed.  A typical cocoa plugin would set it's interface via
    getObjectForJavaScript in it's WebPluginViewFactory.  A non-Cocoa plugin
    could expose it's interface using WebJavaScriptBindingObjectForInstance() and
    the Netscape extension NPP_GetNativeObjectForJavaScript().

    Alternatively, could publish it's interface by calling setProperty:value:
    on a WebJavaScriptObject.

    WebJavaScriptObject *window;
    window = [[container getWebFrame] getJavaScriptObjectForWindow]
    [window setProperty:@"Interactual" value:myInterface];

    Assuming a class for myInterface is something like:

    @interface InteractualInterface
    ...
    - (void)stop;
    - (void)start;
    - (void)setChapter:(int)chapterNumber;
    ...
    @end

    from JavaScript the plugin's interface can then be accessed like
    this:

    Interactual.stop();
    Interactual.setChapter_(10);
    Interactual.start();



*/
