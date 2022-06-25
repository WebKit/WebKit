/*
 * Copyright (C) 2004 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"

#import "BridgeJSC.h"
#import "JSCJSValue.h"
#import "JSObject.h"
#import "interpreter.h"
#import "runtime_object.h"
#import "types.h"
#import <Foundation/Foundation.h>
#import <WebKit/WebScriptObject.h>
#import <stdio.h>
#import <string.h>

#define LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:  ", __PRETTY_FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}

@interface MySecondInterface : NSObject
{
    double doubleValue;
}

- init;

@end

@implementation MySecondInterface

- init
{
    LOG ("\n");
    doubleValue = 666.666;
    return self;
}

@end

@interface MyFirstInterface : NSObject
{
    int myInt;
    RetainPtr<MySecondInterface> mySecondInterface;
    RetainPtr<id> jsobject;
    NSString *string;
}

- (int)getInt;
- (void)setInt: (int)anInt;
- (MySecondInterface *)getMySecondInterface;
- (void)logMessage:(NSString *)message;
- (void)setJSObject:(id)jsobject;
@end

@implementation MyFirstInterface

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(logMessage:))
        return @"logMessage";
    if (aSelector == @selector(logMessages:))
        return @"logMessages";
    if (aSelector == @selector(logMessage:prefix:))
        return @"logMessageWithPrefix";
    return nil;
}

+ (BOOL)isSelectorExcludedFromWebScript:(SEL)aSelector
{
    return NO;
}

+ (BOOL)isKeyExcludedFromWebScript:(const char *)name
{
    return NO;
}

/*
- (id)invokeUndefinedMethodFromWebScript:(NSString *)name withArguments:(NSArray *)args
{
    NSLog (@"Call to undefined method %@", name);
    NSLog (@"%d args\n", [args count]);
    int i;
    for (i = 0; i < [args count]; i++) {
            NSLog (@"%d: %@\n", i, [args objectAtIndex:i]);
    }
    return @"success";
}
*/

/*
- (id)valueForUndefinedKey:(NSString *)key
{
    NSLog (@"%s:  key = %@", __PRETTY_FUNCTION__, key);
    return @"aValue";
}
*/

- (void)setValue:(id)value forUndefinedKey:(NSString *)key
{
    NSLog (@"%s:  key = %@", __PRETTY_FUNCTION__, key);
}

- init
{
    LOG ("\n");
    mySecondInterface = adoptNS([[MySecondInterface alloc] init]);
    return self;
}

- (int)getInt 
{
    LOG ("myInt = %d\n", myInt);
    return myInt;
}

- (void)setInt: (int)anInt 
{
    LOG ("anInt = %d\n", anInt);
    myInt = anInt;
}

- (NSString *)getString
{
    return string;
}

- (MySecondInterface *)getMySecondInterface 
{
    LOG ("\n");
    return mySecondInterface.get();
}

- (void)logMessage:(NSString *)message
{
    printf ("%s\n", [message lossyCString]);
}

- (void)logMessages:(id)messages
{
    int i, count = [[messages valueForKey:@"length"] intValue];
    for (i = 0; i < count; i++)
        printf ("%s\n", [[messages webScriptValueAtIndex:i] lossyCString]);
}

- (void)logMessage:(NSString *)message prefix:(NSString *)prefix
{
    printf ("%s:%s\n", [prefix lossyCString], [message lossyCString]);
}

- (void)setJSObject:(id)jso
{
    jsobject = jso;
}

- (void)callJSObject:(int)arg1 :(int)arg2
{
    id foo1 = [jsobject callWebScriptMethod:@"call" withArguments:[NSArray arrayWithObjects:jsobject.get(), [NSNumber numberWithInt:arg1], [NSNumber numberWithInt:arg2], nil]];
    printf ("foo (via call) = %s\n", [[foo1 description] lossyCString] );
    id foo2 = [jsobject callWebScriptMethod:@"apply" withArguments:[NSArray arrayWithObjects:jsobject.get(), [NSArray arrayWithObjects:[NSNumber numberWithInt:arg1], [NSNumber numberWithInt:arg2], nil], nil]];
    printf ("foo (via apply) = %s\n", [[foo2 description] lossyCString] );
}

@end


using namespace JSC;
using namespace JSC::Bindings;

class GlobalImp : public ObjectImp {
public:
  virtual String className() const { return "global"; }
};

#define BufferSize 200000
static char code[BufferSize];

const char *readJavaScriptFromFile (const char *file)
{
    FILE *f = fopen(file, "r");
    if (!f) {
        fprintf(stderr, "Error opening %s.\n", file);
        return 0;
    }
    
    int num = fread(code, 1, BufferSize, f);
    code[num] = '\0';
    if(num >= BufferSize)
        fprintf(stderr, "Warning: File may have been too long.\n");

    fclose(f);
    
    return code;
}

int main(int argc, char **argv)
{
    // expecting a filename
    if (argc < 2) {
        fprintf(stderr, "You have to specify at least one filename\n");
        return -1;
    }
    
    bool ret = true;
    {
        @autoreleasepool {
            JSLock lock;

            // create interpreter w/ global object
            Object global(new GlobalImp());
            Interpreter interp;
            interp.setGlobalObject(global);
            JSGlobalObject* lexicalGlobalObject = interp.globalObject();

            auto myInterface = adoptNS([[MyFirstInterface alloc] init]);

            global.put(lexicalGlobalObject, Identifier::fromString(lexicalGlobalObject, "myInterface"_s), Instance::createRuntimeObject(Instance::ObjectiveCLanguage, (void *)myInterface.get()));

            for (int i = 1; i < argc; i++) {
                const char *code = readJavaScriptFromFile(argv[i]);

                if (code) {
                    // run
                    Completion comp(interp.evaluate(code));

                    if (comp.complType() == Throw) {
                        Value exVal = comp.value();
                        char *msg = exVal.toString(lexicalGlobalObject).ascii();
                        int lineno = -1;
                        if (exVal.type() == ObjectType) {
                            Value lineVal = Object::dynamicCast(exVal).get(lexicalGlobalObject, Identifier::fromString(lexicalGlobalObject, "line"_s));
                            if (lineVal.type() == NumberType)
                                lineno = int(lineVal.toNumber(lexicalGlobalObject));
                        }
                        if (lineno != -1)
                            fprintf(stderr, "Exception, line %d: %s\n", lineno, msg);
                        else
                            fprintf(stderr, "Exception: %s\n", msg);
                        ret = false;
                    } else if (comp.complType() == ReturnValue) {
                        char *msg = comp.value().toString(interp.globalObject()).ascii();
                        fprintf(stderr, "Return value: %s\n", msg);
                    }
                }
            }

            myInterface = nil;
        }
    } // end block, so that Interpreter and global get deleted
    
    return ret ? 0 : 3;
}
