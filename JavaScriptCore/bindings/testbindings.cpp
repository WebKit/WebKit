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

#include "NP_runtime.h"

#include "runtime.h"
#include "runtime_object.h"


#define LOG(formatAndArgs...) { \
    fprintf (stderr, "%s:  ", __PRETTY_FUNCTION__); \
    fprintf(stderr, formatAndArgs); \
}


// ------------------ NP Interface definition --------------------
typedef struct
{
	NP_Object object;
	double doubleValue;
	int intValue;
	const char *stringValue;
	bool boolValue;
} MyInterfaceObject;


static bool identifiersInitialized = false;

#define ID_DOUBLE_VALUE				0
#define ID_INT_VALUE				1
#define ID_STRING_VALUE				2
#define ID_BOOLEAN_VALUE			3
#define ID_NULL_VALUE				4
#define ID_UNDEFINED_VALUE			5
#define	NUM_PROPERTY_IDENTIFIERS	6

static NP_Identifier myPropertyIdentifiers[NUM_PROPERTY_IDENTIFIERS];
static const NP_UTF8 *myPropertyIdentifierNames[NUM_PROPERTY_IDENTIFIERS] = {
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

static NP_Identifier myMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NP_UTF8 *myMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
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
	NP_GetIdentifiers (myPropertyIdentifierNames, NUM_PROPERTY_IDENTIFIERS, myPropertyIdentifiers);
	NP_GetIdentifiers (myMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, myMethodIdentifiers);
};

bool myInterfaceHasProperty (NP_Class *theClass, NP_Identifier name)
{	
	int i;
	for (i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++) {
		if (name == myPropertyIdentifiers[i]){
			return true;
		}
	}
	return false;
}

bool myInterfaceHasMethod (NP_Class *theClass, NP_Identifier name)
{
	int i;
	for (i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
		if (name == myMethodIdentifiers[i]){
			return true;
		}
	}
	return false;
}

NP_Object *myInterfaceGetProperty (MyInterfaceObject *obj, NP_Identifier name)
{
	if (name == myPropertyIdentifiers[ID_DOUBLE_VALUE]){
		return NP_CreateNumberWithDouble (obj->doubleValue); 
	}
	else if (name == myPropertyIdentifiers[ID_INT_VALUE]){
		return NP_CreateNumberWithInt (obj->intValue); 
	}
	else if (name == myPropertyIdentifiers[ID_STRING_VALUE]){
		return NP_CreateStringWithUTF8 (obj->stringValue);
	}
	else if (name == myPropertyIdentifiers[ID_BOOLEAN_VALUE]){
		return NP_CreateBoolean (obj->boolValue);
	}
	else if (name == myPropertyIdentifiers[ID_NULL_VALUE]){
		return NP_GetNull ();
	}
	else if (name == myPropertyIdentifiers[ID_UNDEFINED_VALUE]){
		return NP_GetUndefined (); 
	}
	
	return NP_GetUndefined();
}

void myInterfaceSetProperty (MyInterfaceObject *obj, NP_Identifier name, NP_Object *value)
{
	if (name == myPropertyIdentifiers[ID_DOUBLE_VALUE]) {
		if (NP_IsKindOfClass (value, NP_NumberClass)) {
			obj->doubleValue = NP_DoubleFromNumber (value); 
		}
		else {
			NP_SetExceptionWithUTF8 ((NP_Object *)obj, "Attempt to set a double value with a non-number type.");
		}
	}
	else if (name == myPropertyIdentifiers[ID_INT_VALUE]) {
		if (NP_IsKindOfClass (value, NP_NumberClass)) {
			obj->intValue = NP_IntFromNumber (value); 
		}
		else {
			NP_SetExceptionWithUTF8 ((NP_Object *)obj, "Attempt to set a int value with a non-number type.");
		}
	}
	else if (name == myPropertyIdentifiers[ID_STRING_VALUE]) {
		if (NP_IsKindOfClass (value, NP_StringClass)) {
			if (obj->stringValue)
				free((void *)obj->stringValue);
			obj->stringValue = NP_UTF8FromString (value);
		}
		else {
			NP_SetExceptionWithUTF8 ((NP_Object *)obj, "Attempt to set a string value with a non-string type.");
		}
	}
	else if (name == myPropertyIdentifiers[ID_BOOLEAN_VALUE]) {
		if (NP_IsKindOfClass (value, NP_StringClass)) {
			obj->boolValue = NP_BoolFromBoolean (value);
		}
		else {
			NP_SetExceptionWithUTF8 ((NP_Object *)obj, "Attempt to set a bool value with a non-boolean type.");
		}
	}
	else if (name == myPropertyIdentifiers[ID_NULL_VALUE]) {
		// Do nothing!
	}
	else if (name == myPropertyIdentifiers[ID_UNDEFINED_VALUE]) {
		// Do nothing!
	}
}

void logMessage (NP_String *message)
{
	printf ("%s\n", NP_UTF8FromString (message));
}

void setDoubleValue (MyInterfaceObject *obj, NP_Number *number)
{
	obj->doubleValue = NP_DoubleFromNumber (number);
}

void setIntValue (MyInterfaceObject *obj, NP_Number *number)
{
	obj->intValue = NP_IntFromNumber (number);
}

void setStringValue (MyInterfaceObject *obj, NP_String *string)
{
	NP_DeallocateUTF8 ((NP_UTF8 *)obj->stringValue);
	obj->stringValue = NP_UTF8FromString (string);
}

void setBooleanValue (MyInterfaceObject *obj, NP_Boolean *boolean)
{
	obj->boolValue = NP_BoolFromBoolean (boolean);
}

NP_Number *getDoubleValue (MyInterfaceObject *obj)
{
	return NP_CreateNumberWithDouble (obj->doubleValue);
}

NP_Number *getIntValue (MyInterfaceObject *obj)
{
	return NP_CreateNumberWithInt (obj->intValue);
}

NP_String *getStringValue (MyInterfaceObject *obj)
{
	return NP_CreateStringWithUTF8 (obj->stringValue);
}

NP_Boolean *getBooleanValue (MyInterfaceObject *obj)
{
	return NP_CreateBoolean (obj->boolValue);
}

NP_Object *myInterfaceInvoke (MyInterfaceObject *obj, NP_Identifier name, NP_Object **args, unsigned argCount)
{
	if (name == myMethodIdentifiers[ID_LOG_MESSAGE]) {
		if (argCount == 1 && NP_IsKindOfClass (args[0], NP_StringClass))
			logMessage ((NP_String *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_DOUBLE_VALUE]) {
		if (argCount == 1 && NP_IsKindOfClass (args[0], NP_NumberClass))
			setDoubleValue (obj, (NP_Number *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_INT_VALUE]) {
		if (argCount == 1 && NP_IsKindOfClass (args[0], NP_NumberClass))
			setIntValue (obj, (NP_Number *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_STRING_VALUE]) {
		if (argCount == 1 && NP_IsKindOfClass (args[0], NP_StringClass))
			setStringValue (obj, (NP_String *)args[0]);
		return 0;
	}
	else if (name == myMethodIdentifiers[ID_SET_BOOLEAN_VALUE]) {
		if (argCount == 1 && NP_IsKindOfClass (args[0], NP_BooleanClass))
			setBooleanValue (obj, (NP_Boolean *)args[0]);
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
	return NP_GetUndefined();
}

NP_Object *myInterfaceAllocate ()
{
	MyInterfaceObject *newInstance = (MyInterfaceObject *)malloc (sizeof(MyInterfaceObject));
	
	if (!identifiersInitialized) {
		identifiersInitialized = true;
		initializeIdentifiers();
	}
	
	
	newInstance->doubleValue = 666.666;
	newInstance->intValue = 1234;
	newInstance->boolValue = true;
	newInstance->stringValue = strdup("Hello world");
	
	return (NP_Object *)newInstance;
}

void myInterfaceInvalidate ()
{
	// Make sure we've released any remainging references to JavaScript
	// objects.
}

void myInterfaceDeallocate (MyInterfaceObject *obj) 
{
	free ((void *)obj);
}

static NP_Class _myInterface = { 
	kNP_ClassStructVersionCurrent,
	(NP_AllocateInterface) myInterfaceAllocate, 
	(NP_DeallocateInterface) myInterfaceDeallocate, 
	(NP_InvalidateInterface) myInterfaceInvalidate,
	(NP_HasMethodInterface) myInterfaceHasMethod,
	(NP_InvokeInterface) myInterfaceInvoke,
	(NP_HasPropertyInterface) myInterfaceHasProperty,
	(NP_GetPropertyInterface) myInterfaceGetProperty,
	(NP_SetPropertyInterface) myInterfaceSetProperty,
};
static NP_Class *myInterface = &_myInterface;

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
        
        MyInterfaceObject *myInterfaceObject = (MyInterfaceObject *)NP_CreateObject (myInterface);
        
        global.put(exec, Identifier("myInterface"), Instance::createRuntimeObject(Instance::CLanguage, (void *)myInterfaceObject));
        
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
                
        NP_ReleaseObject ((NP_Object *)myInterfaceObject);
        
        Interpreter::unlock();
        
    } // end block, so that Interpreter and global get deleted
    
    return ret ? 0 : 3;
}
