// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */
#include <Foundation/Foundation.h>

#import <WebKit/WebScriptObject.h>

#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

#include "runtime.h"
#include "runtime_object.h"

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
	MySecondInterface *mySecondInterface;
	id jsobject;
}

- (int)getInt;
- (void)setInt: (int)anInt;
- (MySecondInterface *)getMySecondInterface;
- (void)logMessage:(NSString *)message;
- (void)setJSObject:(id)jsobject;
@end

@implementation MyFirstInterface

- init
{
    LOG ("\n");
    mySecondInterface = [[MySecondInterface alloc] init];
    return self;
}

- (void)dealloc
{
    LOG ("\n");
    [mySecondInterface release];
    [super dealloc];
}

+ (NSString *)webScriptNameForSelector:(SEL)aSelector
{
    if (aSelector == @selector(logMessage:))
        return @"logMessage";
    return nil;
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
	return @"This is a string from ObjC";
}

- (MySecondInterface *)getMySecondInterface 
{
    LOG ("\n");
    return mySecondInterface;
}

- (void)logMessage:(NSString *)message
{
    printf ("%s\n", [message lossyCString]);
}

- (void)setJSObject:(id)jso
{
    [jsobject autorelease];
    jsobject = [jso retain];
}

- (void)callJSObject:(int)arg1 :(int)arg2
{
    id foo = [jsobject callWebScriptMethod:@"call" withArguments:[NSArray arrayWithObjects:jsobject, [NSNumber numberWithInt:arg1], [NSNumber numberWithInt:arg2], nil]];
    printf ("foo = %s\n", [[foo description] lossyCString] );
}

@end


using namespace KJS;
using namespace KJS::Bindings;

class GlobalImp : public ObjectImp {
public:
  virtual UString className() const { return "global"; }
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
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];
        
        Interpreter::lock();
        
        // create interpreter w/ global object
        Object global(new GlobalImp());
        Interpreter interp(global);
        ExecState *exec = interp.globalExec();
        
        MyFirstInterface *myInterface = [[MyFirstInterface alloc] init];
        
        global.put(exec, Identifier("myInterface"), Instance::createRuntimeObject(Instance::ObjectiveCLanguage, (void *)myInterface));
        
        for (int i = 1; i < argc; i++) {
            const char *code = readJavaScriptFromFile(argv[i]);
            
            if (code) {
                // run
                Completion comp(interp.evaluate(code));
                
                if (comp.complType() == Throw) {
                    Value exVal = comp.value();
                    char *msg = exVal.toString(exec).ascii();
                    int lineno = -1;
                    if (exVal.type() == ObjectType) {
                        Value lineVal = Object::dynamicCast(exVal).get(exec,Identifier("line"));
                        if (lineVal.type() == NumberType)
                            lineno = int(lineVal.toNumber(exec));
                    }
                    if (lineno != -1)
                        fprintf(stderr,"Exception, line %d: %s\n",lineno,msg);
                    else
                        fprintf(stderr,"Exception: %s\n",msg);
                    ret = false;
                }
                else if (comp.complType() == ReturnValue) {
                    char *msg = comp.value().toString(interp.globalExec()).ascii();
                    fprintf(stderr,"Return value: %s\n",msg);
                }
            }
        }
        
        [myInterface release];
        
        Interpreter::unlock();
        
        [pool release];
    } // end block, so that Interpreter and global get deleted
    
    return ret ? 0 : 3;
}
