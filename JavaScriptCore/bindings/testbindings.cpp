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
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

#include "npruntime.h"

#include "runtime.h"
#include "runtime_object.h"


#define LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:  ", __PRETTY_FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}


// ------------------ NP Interface definition --------------------
typedef struct
{
	NPObject object;
	double doubleValue;
	int intValue;
	const char *stringValue;
	bool boolValue;
} MyObject;


static bool identifiersInitialized = false;

#define ID_DOUBLE_VALUE				0
#define ID_INT_VALUE				1
#define ID_STRING_VALUE				2
#define ID_BOOLEAN_VALUE			3
#define ID_NULL_VALUE				4
#define ID_UNDEFINED_VALUE			5
#define	NUM_PROPERTY_IDENTIFIERS	6

static NPIdentifier myPropertyIdentifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8 *myPropertyIdentifierNames[NUM_PROPERTY_IDENTIFIERS] = {
	"doubleValue",
	"intValue",
	"stringValue",
	"booleanValue",
	"nullValue",
	"undefinedValue"
};

#define ID_LOG_MESSAGE				0
#define ID_SET_DOUBLE_VALUE			1
#define ID_SET_INT_VALUE			2
#define ID_SET_STRING_VALUE			3
#define ID_SET_BOOLEAN_VALUE		4
#define ID_GET_DOUBLE_VALUE			5
#define ID_GET_INT_VALUE			6
#define ID_GET_STRING_VALUE			7
#define ID_GET_BOOLEAN_VALUE		8
#define NUM_METHOD_IDENTIFIERS		9

static NPIdentifier myMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *myMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
	"logMessage",
	"setDoubleValue",
	"setIntValue",
	"setStringValue",
	"setBooleanValue",
	"getDoubleValue",
	"getIntValue",
	"getStringValue",
	"getBooleanValue"
};

static void initializeIdentifiers()
{
	NPN_GetIdentifiers (myPropertyIdentifierNames, NUM_PROPERTY_IDENTIFIERS, myPropertyIdentifiers);
	NPN_GetIdentifiers (myMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, myMethodIdentifiers);
};

bool myHasProperty (NPClass *theClass, NPIdentifier name)
{	
	int i;
	for (i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++) {
		if (name == myPropertyIdentifiers[i]){
			return true;
		}
	}
	return false;
}

bool myHasMethod (NPClass *theClass, NPIdentifier name)
{
	int i;
	for (i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
		if (name == myMethodIdentifiers[i]){
			return true;
		}
	}
	return false;
}

NPObject *myGetProperty (MyObject *obj, NPIdentifier name)
{
	if (name == myPropertyIdentifiers[ID_DOUBLE_VALUE]){
		return NPN_CreateNumberWithDouble (obj->doubleValue); 
	}
	else if (name == myPropertyIdentifiers[ID_INT_VALUE]){
		return NPN_CreateNumberWithInt (obj->intValue); 
	}
	else if (name == myPropertyIdentifiers[ID_STRING_VALUE]){
		return NPN_CreateStringWithUTF8 (obj->stringValue, -1);
	}
	else if (name == myPropertyIdentifiers[ID_BOOLEAN_VALUE]){
		return NPN_CreateBoolean (obj->boolValue);
	}
	else if (name == myPropertyIdentifiers[ID_NULL_VALUE]){
		return NPN_GetNull ();
	}
	else if (name == myPropertyIdentifiers[ID_UNDEFINED_VALUE]){
		return NPN_GetUndefined (); 
	}
	
	return NPN_GetUndefined();
}

void mySetProperty (MyObject *obj, NPIdentifier name, NPObject *value)
{
	if (name == myPropertyIdentifiers[ID_DOUBLE_VALUE]) {
		if (NPN_IsKindOfClass (value, NPNumberClass)) {
			obj->doubleValue = NPN_DoubleFromNumber (value); 
		}
		else {
			NPN_SetExceptionWithUTF8 ((NPObject *)obj, "Attempt to set a double value with a non-number type.", -1);
		}
	}
	else if (name == myPropertyIdentifiers[ID_INT_VALUE]) {
		if (NPN_IsKindOfClass (value, NPNumberClass)) {
			obj->intValue = NPN_IntFromNumber (value); 
		}
		else {
			NPN_SetExceptionWithUTF8 ((NPObject *)obj, "Attempt to set a int value with a non-number type.", -1);
		}
	}
	else if (name == myPropertyIdentifiers[ID_STRING_VALUE]) {
		if (NPN_IsKindOfClass (value, NPStringClass)) {
			if (obj->stringValue)
				free((void *)obj->stringValue);
			obj->stringValue = NPN_UTF8FromString (value);
		}
		else {
			NPN_SetExceptionWithUTF8 ((NPObject *)obj, "Attempt to set a string value with a non-string type.", -1);
		}
	}
	else if (name == myPropertyIdentifiers[ID_BOOLEAN_VALUE]) {
		if (NPN_IsKindOfClass (value, NPStringClass)) {
			obj->boolValue = NPN_BoolFromBoolean (value);
		}
		else {
			NPN_SetExceptionWithUTF8 ((NPObject *)obj, "Attempt to set a bool value with a non-boolean type.", -1);
		}
	}
	else if (name == myPropertyIdentifiers[ID_NULL_VALUE]) {
		// Do nothing!
	}
	else if (name == myPropertyIdentifiers[ID_UNDEFINED_VALUE]) {
		// Do nothing!
	}
}

void logMessage (NPString *message)
{
	printf ("%s\n", NPN_UTF8FromString (message));
}

void setDoubleValue (MyObject *obj, NPNumber *number)
{
	obj->doubleValue = NPN_DoubleFromNumber (number);
}

void setIntValue (MyObject *obj, NPNumber *number)
{
	obj->intValue = NPN_IntFromNumber (number);
}

void setStringValue (MyObject *obj, NPString *string)
{
	NPN_DeallocateUTF8 ((NPUTF8 *)obj->stringValue);
	obj->stringValue = NPN_UTF8FromString (string);
}

void setBooleanValue (MyObject *obj, NPBoolean *boolean)
{
	obj->boolValue = NPN_BoolFromBoolean (boolean);
}

NPNumber *getDoubleValue (MyObject *obj)
{
	return NPN_CreateNumberWithDouble (obj->doubleValue);
}

NPNumber *getIntValue (MyObject *obj)
{
	return NPN_CreateNumberWithInt (obj->intValue);
}

NPString *getStringValue (MyObject *obj)
{
	return NPN_CreateStringWithUTF8 (obj->stringValue, -1);
}

NPBoolean *getBooleanValue (MyObject *obj)
{
	return NPN_CreateBoolean (obj->boolValue);
}

NPObject *myInvoke (MyObject *obj, NPIdentifier name, NPObject **args, unsigned argCount)
{
	if (name == myMethodIdentifiers[ID_LOG_MESSAGE]) {
		if (argCount == 1 && NPN_IsKindOfClass (args[0], NPStringClass))
			logMessage ((NPString *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_DOUBLE_VALUE]) {
		if (argCount == 1 && NPN_IsKindOfClass (args[0], NPNumberClass))
			setDoubleValue (obj, (NPNumber *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_INT_VALUE]) {
		if (argCount == 1 && NPN_IsKindOfClass (args[0], NPNumberClass))
			setIntValue (obj, (NPNumber *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_STRING_VALUE]) {
		if (argCount == 1 && NPN_IsKindOfClass (args[0], NPStringClass))
			setStringValue (obj, (NPString *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_BOOLEAN_VALUE]) {
		if (argCount == 1 && NPN_IsKindOfClass (args[0], NPBooleanClass))
			setBooleanValue (obj, (NPBoolean *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_GET_DOUBLE_VALUE]) {
		return getDoubleValue (obj);
	}
	else if (name == myMethodIdentifiers[ID_GET_INT_VALUE]) {
		return getIntValue (obj);
	}
	else if (name == myMethodIdentifiers[ID_GET_STRING_VALUE]) {
		return getStringValue (obj);
	}
	else if (name == myMethodIdentifiers[ID_GET_BOOLEAN_VALUE]) {
		return getBooleanValue (obj);
	}
	return NPN_GetUndefined();
}

NPObject *myAllocate ()
{
	MyObject *newInstance = (MyObject *)malloc (sizeof(MyObject));
	
	if (!identifiersInitialized) {
		identifiersInitialized = true;
		initializeIdentifiers();
	}
	
	
	newInstance->doubleValue = 666.666;
	newInstance->intValue = 1234;
	newInstance->boolValue = true;
	newInstance->stringValue = strdup("Hello world");
	
	return (NPObject *)newInstance;
}

void myInvalidate ()
{
	// Make sure we've released any remainging references to JavaScript
	// objects.
}

void myDeallocate (MyObject *obj) 
{
	free ((void *)obj);
}

static NPClass _myFunctionPtrs = { 
	kNPClassStructVersionCurrent,
	(NPAllocateFunctionPtr) myAllocate, 
	(NPDeallocateFunctionPtr) myDeallocate, 
	(NPInvalidateFunctionPtr) myInvalidate,
	(NPHasMethodFunctionPtr) myHasMethod,
	(NPInvokeFunctionPtr) myInvoke,
	(NPHasPropertyFunctionPtr) myHasProperty,
	(NPGetPropertyFunctionPtr) myGetProperty,
	(NPSetPropertyFunctionPtr) mySetProperty,
};
static NPClass *myFunctionPtrs = &_myFunctionPtrs;

// --------------------------------------------------------

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
        Interpreter::lock();
        
        // create interpreter w/ global object
        Object global(new GlobalImp());
        Interpreter interp(global);
        ExecState *exec = interp.globalExec();
        
        MyObject *myObject = (MyObject *)NPN_CreateObject (myFunctionPtrs);
        
        global.put(exec, Identifier("myInterface"), Instance::createRuntimeObject(Instance::CLanguage, (void *)myObject));
        
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
                
        NPN_ReleaseObject ((NPObject *)myObject);
        
        Interpreter::unlock();
        
    } // end block, so that Interpreter and global get deleted
    
    return ret ? 0 : 3;
}
