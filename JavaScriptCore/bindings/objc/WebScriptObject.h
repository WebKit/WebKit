/*
    Copyright (C) 2004 Apple Computer, Inc. All rights reserved.    
    
    Public header file.
 */

#import <Foundation/Foundation.h>

// NSObject (WebScripting) ----------------------------------------------------- 

/*
    The methods in WebScripting are optionally implemented by classes whose
    interfaces are exported (wrapped) to a web scripting environment.  The 
    scripting environment currently supported by WebKit uses the JavaScript
    language.
    
    Instances automatically reflect their interfaces in the scripting environment.  This
    automatic reflection can be overriden using the class methods defined in the WebScripting
    informal protocol.
    
    Access to the attributes of an instance is done using KVC. Specifically the following
    KVC methods:
	
        - (void)setValue:(id)value forKey:(NSString *)key
        - (id)valueForKey:(NSString *)key
	
    Instances may also intercept property set/get operations and method invocations that are
    made by the scripting environment, but not reflected.  This is done using the KVC
    methods:

        - (void)setValue:(id)value forUndefinedKey:(NSString *)key
        - (id)valueForUndefinedKey:(NSString *)key
    
    If clients need to raise an exception in the script environment
    they can call [WebScriptObject throwException:].  Note that throwing an
    exception using this method will only succeed if the method that throws the exception
    is being called within the scope of a script invocation.
    
    By default all attributes, as defined by KVC, will be exposed.  However, a
    class may further exclude properties that they do not want to expose
    to web script.
	
    Not all methods are exposed.  Only those methods whose parameters and return
    type meets the export criteria will exposed.  Valid types are ObjectiveC instances
    and scalars.  Other types are not allowed.  Classes may further exclude method
    that they do not want to expose.
    
    Types will be converted to appropriate types in the scripting environment.
    After any KVC coercion occurs the ObjectiveC types will converted to a type
    appropriate for the script environment.  For JavaScript NSNumber will be
    converted to numbers.  NSString will be converted to strings.  NSArray will
    be mapped to a special read-only array.  NSNull will be converted to null.  
    WebUndefined will be converted to undefined.  WebScriptObjects will be unwrapped.
    Instances of other classes will be wrapped when passed to the script environment
    and unwrapped when returned to ObjectiveC.  Similar conversion happens in the
    other direction.
*/
@interface NSObject (WebScripting)

/*!
    @method webScriptNameForSelector:
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
+ (NSString *)webScriptNameForSelector:(SEL)aSelector;

/*!
    @method isSelectorExcludedFromWebScript:
    @param aSelect  The selector the will be exposed to the script environment.
    @discussion	Return YES to prevent the selector appearing in the script
    environment.  Return NO to expose the selector in the script environment.
    If this method is not implemented on the class all the selector that match
    the export criteria will be exposed.
    @result Returns YES to hide the selector, NO to expose the selector.
*/
+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector;

/*!
    @method webScriptNameForKey:
    @param name The name of the instance variable that will be exposed to the
    script enviroment.  Only that properties that meet the export criteria will
    be exposed.
    @discussion Provide an alternate name for a property.
    @result Returns the name to be used to represent the specificed property in the
    scripting environment.
*/
+ (NSString *)webScriptNameForKey:(const char *)name;

/*!
    @method isKeyExcludedFromWebScript:
    @param name The name of the instance variable that will be exposed to the
    scrip environment.
    @discussion Return YES to exclude the property from visibility in the script environement.
    Return NO to expose the instance varible to the script environment.
    @result Returns YES to hide the property, NO to expose the property.
*/
+ (BOOL)isKeyExcludedFromWebScript:(const char *)name;

/*!
    @method invokeUndefinedMethodFromWebScript:withArguments:
    @param name The name of the method to invoke.
    @param args The args to pass the method.
    @discussion If a script attempt to invoke a method that is not an exposed
    method, scriptInvocation:withArgs: will be called.
    @result The return value of the invocation.  The value will be converted as appropriate
    for the script environment.
*/
- (id)invokeUndefinedMethodFromWebScript:(NSString *)name withArguments:(NSArray *)args;

/*!
    @method invokeDefaultMethodWithArguments:
    @param args The args to pass the method.
    @discussion If a script attempts to invoke a method on an exposed object
    directly this method will be called.
*/
- (id)invokeDefaultMethodWithArguments:(NSArray *)args;

/*!
    @method finalizeForWebScript
    @discussion finalizeForScript is called on objects exposed to the script
    environment just before the script environment releases the object.  After calls to
    finalizeForWebScript the object will no longer be referenced by the script environment.
    Further, any references to WebScriptObjects made by the exposed object will
    be invalid and have undefined consequences.
*/
- (void)finalizeForWebScript;

@end


// WebScriptObject -------------------------------------------------- 

@class WebScriptObjectPrivate;

/*!
    @class WebScriptObject
    @discussion WebScriptObjects are used to wrap script objects passed from
    script environments to ObjectiveC.  WebScriptObjects cannot be created
    directly.  In normal uses of WebKit, you gain access to the script
    environment using the "windowScriptObject" method on WebView.
    
    The following KVC methods are commonly used to access properties of the
    WebScriptObject:
    
	- (void)setValue:(id)value forKey:(NSString *)key
	- (id)valueForKey:(NSString *)key
	
	As it possible to remove attributes from web script objects the following
	additional method augments the basic KVC methods:
	
	- (void)removeWebScriptKey:(NSString *)name;
	
	Also the sparse array access allowed in web script objects doesn't map well to NSArray, so
	the following methods can be used to access index based properties:
	
	- (id)webScriptValueAtIndex:(unsigned int)index;
	- (void)setWebScriptValueAtIndex:(unsigned int)index value:(id)value;
*/
@interface WebScriptObject : NSObject
{
    WebScriptObjectPrivate *_private;
}

/*!
    @method throwException:
    @discussion Throws an exception in the current script execution context.
    @result Either NO if an exception could not be raised, YES otherwise.
*/
+ (BOOL)throwException:(NSString *)exceptionMessage;

/*!
    @method callWebScriptMethod:withArguments:
    @param name The name of the method to call in the script environment.
    @param args The arguments to pass to the script environment.
    @discussion Calls the specified method in the script environment using the
    specified arguments.
    @result Returns the result of calling the script method.
*/
- (id)callWebScriptMethod:(NSString *)name withArguments:(NSArray *)args;

/*!
    @method evaluateWebScript:
    @param script The script to execute in the target script environment.
    @discussion The script will be executed in the target script environment.  The format
    of the script is dependent of the target script environment.
    @result Returns the result of evaluating the script in the script environment.
*/
- (id)evaluateWebScript:(NSString *)script;

/*!
    @method removeWebScriptKey:
    @param name The name of the property to remove.
    @discussion Removes the property from the object in the script environment.
*/
- (void)removeWebScriptKey:(NSString *)name;

/*!
    @method toString
    @discussion Converts the target object to a string representation.  The coercion
    of non string objects type is dependent on the script environment.
    @result Returns the string representation of the object.
*/
- (NSString *)stringRepresentation;

/*!
    @method propertyAtIndex:
    @param index The index of the property to return.  Index based access is dependent 
    @discussion Gets the value of the property at the specified index.
    @result The value of the property.
*/
- (id)webScriptValueAtIndex:(unsigned int)index;

/*!
    @method setPropertyAtIndex:value:
    @param index The index of the property to set.
    @param value The value of the property to set.
    @discussion Sets the property value at the specified index.
*/
- (void)setWebScriptValueAtIndex:(unsigned int)index value:(id)value;

/*!
    @method setException:
    @param description The description of the exception.
    @discussion Raises an exception in the script environment in the context of the
    current object.
*/
- (void)setException: (NSString *)description;

@end


// WebUndefined --------------------------------------------------------------

/*!
    @class WebUndefined
*/
@interface WebUndefined : NSObject <NSCoding, NSCopying>
{
}

/*!
    @method undefined
    @result The WebUndefined shared instance.
*/
+ (WebUndefined *)undefined;

@end
