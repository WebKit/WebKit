/*
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
 */
#ifndef _WEB_SCRIPT_OBJECT_H_
#define _WEB_SCRIPT_OBJECT_H_


// NSObject (WebScriptMethods) ----------------------------------------------------- 

/*
    The methods in WebScriptMethods are optionally implemented by classes whose
    interfaces are exported (wrapped) to a scripting environment in the context of WebKit.
    The scripting environment currently supported by WebKit uses the JavaScript
    language.
    
    Instances automatically reflect their interfaces in the scripting environment.  This
    automatic reflection can be overriden using the class methods defined in the WebScriptMethods
    informal protocol.
    
    Instances may also intercept property set/get operations and method invocations that are
    made by the scripting environment, but not reflected.
    
    Not all methods are exposed.  Only those methods whose parameters and return
    type meets the export criteria will exposed.  Valid types are ObjectiveC instances
    and scalars.  Other types are not allowed.
    
    Types will be converted to appropriate types in the scripting environment.
    Scalars and NSNumber will be converted to numbers.  NSString will be converted
    to strings.  NSNull will be converted to null.  WebUndefined will be converted
    to undefined.  WebScriptObjects will be unwrapped.  Instances of other classes
    will be wrapped when passed to the script environment and unwrapped when
    returned to ObjectiveC.  Similar conversion happens in the other direction.
    
    If an instance variable of an object is set directly from a script, and it is
    an object, the previous value will be released and the new value will be retained.
*/
@interface NSObject (WebScriptMethods)

/*!
	@method scriptNameForSelector:
	@param aSelector The selector that will be exposed to the script environment.
    @discussion Use the returned string as the exported name for the selector
    in the script environment.  It is the responsibility of the class to ensure
    uniqueness of the returned name.  If nil is returned or this
    method is not implemented the default name for the selector will
    be used.  The default name concatenates the components of the
    ObjectiveC selector name and replaces ':' with '_'.  '_' characters
    are escaped with an additional '$', i.e. '_' becomes "$_".  '$' are
    also escaped, i.e.
        ObjectiveC name         Default script name
        moveTo::                move__
        moveTo_                 moveTo$_
        moveTo$_                moveTo$$$_
    @result Returns the name to be used to represent the specificed selector in the
    scripting environment.
*/
+ (NSString *)scriptNameForSelector:(SEL)aSelector;

/*!
    @method isSelectorExcludedFromScript:
    @param aSelect  The selector the will be exposed to the script environment.
    @discussion	Return NO to prevent the selector appearing in the script
    environment.  Return YES to expose the selector in the script environment.
    If this method is not implemented on the class all the selector that match
    the export criteria will be exposed.
    @result Returns NO to hide the selector, YES to expose the selector.
*/
+ (BOOL)isSelectorExcludedFromScript:(SEL)aSelector;

/*!
    @method scriptNameForProperty:
	@param name The name of the instance variable that will be exposed to the
    script enviroment.  Only that properties that meet the export criteria will
    be exposed.
    @discussion Provide an alternate name for a property.
    @result Returns the name to be used to represent the specificed property in the
    scripting environment.
*/
+ (NSString *)scriptNameForProperty:(const char *)name;

/*!
    @method isPropertyExcludedFromScript:
    @param name The name of the instance variable that will be exposed to the
    scrip environment.
    @discussion Return NO to exclude the property from visibility in the script environement.
    Return YES to expose the instance varible to the script environment.
    @result Returns NO to hide the property, YES to expose the property.
*/
+ (BOOL)isPropertyExcludedFromScript:(const char *)name;

/*!
    @method setObject:forScriptProperty:exceptionMessage:
    @param value The vale to be set for the property name.
    @param name The name of the property being set.
    @param exceptionMessage Set to the a message that will be used to contruct a script exception.
    @discussion If a script attempts to set a property that is not an exposed
    instance variable, setObject:forScriptProperty: will be called.  Setting the
    *exceptionMessage to a non-nil value will cause an exception to be raised in the script
    environment.  
*/
- (void)setObject:(id)value forScriptProperty:(NSString *)name exceptionMessage:(NSString **)exceptionMessage;

/*!
    @method objectForScriptProperty:exceptionMessage:
    @param name
    @param exceptionMessage Set to the a message that will be used to contruct a script exception.
    @discussion If a script attempts to get a property that is not an exposed
    instance variable, objectForScriptProperty: will be be called.
    @result The value of the property.  Setting the *exceptionMessage to a non-nil value will
    cause an exception to be raised in the script environment.  The return value will be
    ignored if an exception message is set.
*/
- (id)objectForScriptProperty:(NSString *)name exceptionMessage:(NSString **)exceptionMessage;

/*!
    @method scriptInvocation:withArgs:exceptionMessage:
    @param name The name of the method to invoke.
    @param args The args to pass the method.
    @param exceptionMessage Set to the a message that will be used to contruct a script exception.
    @discussion If a script attempt to invoke a method that is not an exposed
    method, scriptInvocation:withArgs: will be called.
    @result The return value of the invocation.  The value will be converted as appropriate
    for the script environment.  Setting the *exceptionMessage to a non-nil value will
    cause an exception to be raised in the script environment.  The return value will be
    ignored if an exception message is set.
*/
- (id)scriptInvocation:(NSString *)name withArgs:(NSArray *)args exceptionMessage:(NSString **)exceptionMessage;

/*!
    @method finalizeForScript
    @discussion finalizeForScript is called on objects exposed to the script
    environment just before the script environment is reset.  After calls to
    finalizeForScript the object will no longer be referenced by the script environment.
    Further any references to WebScriptObjects made by the exposed object will
    be invalid and have undefined consequences.
*/
- (void)finalizeForScript;

@end



// WebView (WebScriptMethods) --------------------------------------- 

@interface WebView (WebScriptMethods)

/*!
    @method windowScriptObject
    @discussion windowScriptObject return a WebScriptObject that represents the
    window object from the script environment.
    @result Returns the window object from the script environment.
*/
- (WebScriptObject *)windowScriptObject;
@end



// WebScriptObject -------------------------------------------------- 

@class WebScriptObjectPrivate;

/*!
    @class WebScriptObject
    @discussion WebScriptObjects are used to wrap script objects passed from
    script environments to ObjectiveC.  WebScriptObjects cannot be created
    directly.  In normal uses of WebKit, you gain access to the script
    environment using the "windowScriptObject" method on WebView.
*/
@interface WebScriptObject : NSObject
{
    WebScriptObjectPrivate *_private;
}

/*!
    @method callScriptMethod:withArguments:
    @param name The name of the method to call in the script environment.
    @param args The arguments to pass to the script environment.
    @discussion Calls the specified method in the script environment using the
    specified arguments.
    @result Returns the result of calling the script method.
*/
- (id)callScriptMethod:(NSString *)name withArguments:(NSArray *)args;

/*!
    @method evaluateScript:
    @param script The script to execute in the target script environment.
    @discussion The script will be executed in the target script environment.  The format
    of the script is dependent of the target script environment.
    @result Returns the result of evaluating the script in the script environment.
*/
- (id)evaluateScript:(NSString *)script;

/*!
    @method objectForScriptProperty:
    @param name The name of the property to return.
    @discussion Returns the property of the object from the script environment.
    @result Returns the property of the object from the script environment.
*/
- (id)objectForScriptProperty:(NSString *)name;

/*!
    @method setObject:forScriptProperty:
    @param name The name of the property to set.
    @param value The value of the property.
    @discussion Set the property of the object in the script environment.
*/
- (void)setObject:(id)value forScriptProperty:(NSString *)name;

/*!
    @method removeScriptProperty:
    @param name The name of the property to remove.
    @discussion Removes the property from the object in the script environment.
*/
- (void)removeScriptProperty:(NSString *)name;

/*!
    @method toString
    @discussion Converts the target object to a string representation.  The coercion
    of non string objects type is dependent on the script environment.
    @result Returns the string representation of the object.
*/
- (NSString *)toString;

/*!
    @method propertyAtIndex:
    @param index The index of the property to return.  Index based access is dependent 
    @discussion Gets the value of the property at the specified index.
    @result The value of the property.
*/
- (id)scriptPropertyAtIndex:(unsigned int)index;

/*!
    @method setPropertyAtIndex:value:
    @param index The index of the property to set.
    @param value The value of the property to set.
    @discussion Sets the property value at the specified index.
*/
- (void)setScriptPropertyAtIndex:(unsigned int)index value:(id)value;

/*!
    @method setException:
    @param description The description of the exception.
    @discussion Raises an exception in the script environment.
*/
- (void)setException: (NSString *)description;

@end



// WebUndefined --------------------------------------------------------------

/*!
    @class WebUndefined
*/
@interface WebUndefined : NSObject <NSCoding, NSCopying>

/*!
    @method undefined
    @result The WebUndefined shared instance.
*/
+ (WebUndefined *)undefined;

@end


@interface NSObject (WebFrameLoadDelegate) ...
/*!
    @method webView:windowScriptObjectAvailable:
    @abstract Notifies the delegate that the scripting object for a page is available.  This is called
    before the page is loaded.  It may be useful to allow delegates to bind native objects to the window.
    @param webView The webView sending the message.
    @param windowScriptObject The WebScriptObject for the window in the scripting environment.
*/
- (void)webView:(WebView *)webView windowScriptObjectAvailable:(WebScriptObject *)windowScriptObject;
@end


// NSObject (WebScriptablePlugin) -------------------------------------------- 

@interface NSObject (WebPlugin) ...
/*!
    @method objectForScript
    @discussion objectForScript is used to expose a plugin's API.  The methods of the
    object are exposed to the script environment.  See the WebScriptMethod informal
    protocol for more details.
    @result Returns the object that exposes the plugin's interface.  The class of this
    object can implement methods from the WebScriptMethods informal protocol.
*/
- (id)objectForScript;
@end



// NSObject (WebPluginContainer) --------------------------------------------- 

@interface NSObject (WebPluginContainer) ...
/*!
    @method webFrame
    @discussion The webFrame method allows the plugin to access the WebFrame that
    contains the plugin.  This method is optionally implemented on classes
    that implement the WebPluginContainer protocol.
    @result Return the WebFrame that contains the plugin.
*/
- (WebFrame *)webFrame;
@end



// DOMObject ------------------------------------------------------------------
// All DOM objects may be manipulated using the formal DOM API, or indirectly via
// their scripting interface.  This requires a change in DOMObject's inheritance.

/*!
	@class DOMObject
	@discussion	All DOM objects may be manipulated using the formal DOM API, or indirectly via
	their scripting interface.  This requires a change in DOMObject's inheritance.
	...
*/
@interface DOMObject : WebScriptObject <NSCopying>
{
    DOMObjectInternal *_internal;
}
...
@end

