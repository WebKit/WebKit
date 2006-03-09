/*
 IMPORTANT:  This Apple software is supplied to you by Apple Computer, Inc. ("Apple") in
 consideration of your agreement to the following terms, and your use, installation, 
 modification or redistribution of this Apple software constitutes acceptance of these 
 terms.  If you do not agree with these terms, please do not use, install, modify or 
 redistribute this Apple software.
 
 In consideration of your agreement to abide by the following terms, and subject to these 
 terms, Apple grants you a personal, non-exclusive license, under AppleÕs copyrights in 
 this original Apple software (the "Apple Software"), to use, reproduce, modify and 
 redistribute the Apple Software, with or without modifications, in source and/or binary 
 forms; provided that if you redistribute the Apple Software in its entirety and without 
 modifications, you must retain this notice and the following text and disclaimers in all 
 such redistributions of the Apple Software.  Neither the name, trademarks, service marks 
 or logos of Apple Computer, Inc. may be used to endorse or promote products derived from 
 the Apple Software without specific prior written permission from Apple. Except as expressly
 stated in this notice, no other rights or licenses, express or implied, are granted by Apple
 herein, including but not limited to any patent rights that may be infringed by your 
 derivative works or by other works in which the Apple Software may be incorporated.
 
 The Apple Software is provided by Apple on an "AS IS" basis.  APPLE MAKES NO WARRANTIES, 
 EXPRESS OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTIES OF NON-INFRINGEMENT, 
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE, REGARDING THE APPLE SOFTWARE OR ITS 
 USE AND OPERATION ALONE OR IN COMBINATION WITH YOUR PRODUCTS.
 
 IN NO EVENT SHALL APPLE BE LIABLE FOR ANY SPECIAL, INDIRECT, INCIDENTAL OR CONSEQUENTIAL 
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS 
 OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) ARISING IN ANY WAY OUT OF THE USE, 
 REPRODUCTION, MODIFICATION AND/OR DISTRIBUTION OF THE APPLE SOFTWARE, HOWEVER CAUSED AND 
 WHETHER UNDER THEORY OF CONTRACT, TORT (INCLUDING NEGLIGENCE), STRICT LIABILITY OR 
 OTHERWISE, EVEN IF APPLE HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#import "PluginObject.h"

void pluginInvalidate ();
bool pluginHasProperty (NPClass *theClass, NPIdentifier name);
bool pluginHasMethod (NPClass *theClass, NPIdentifier name);
void pluginGetProperty (PluginObject *obj, NPIdentifier name, NPVariant *variant);
void pluginSetProperty (PluginObject *obj, NPIdentifier name, const NPVariant *variant);
void pluginInvoke (PluginObject *obj, NPIdentifier name, NPVariant *args, uint32_t argCount, NPVariant *result);
void pluginInvokeDefault (PluginObject *obj, NPVariant *args, uint32_t argCount, NPVariant *result);
NPObject *pluginAllocate (NPP npp, NPClass *theClass);
void pluginDeallocate (PluginObject *obj);

static NPClass _pluginFunctionPtrs = { 
    NP_CLASS_STRUCT_VERSION,
    (NPAllocateFunctionPtr) pluginAllocate, 
    (NPDeallocateFunctionPtr) pluginDeallocate, 
    (NPInvalidateFunctionPtr) pluginInvalidate,
    (NPHasMethodFunctionPtr) pluginHasMethod,
    (NPInvokeFunctionPtr) pluginInvoke,
    (NPInvokeDefaultFunctionPtr) pluginInvokeDefault,
    (NPHasPropertyFunctionPtr) pluginHasProperty,
    (NPGetPropertyFunctionPtr) pluginGetProperty,
    (NPSetPropertyFunctionPtr) pluginSetProperty,
};
 
NPClass *getPluginClass(void)
{
    return &_pluginFunctionPtrs;
}

static bool identifiersInitialized = false;

#define ID_PROPERTY_PROPERTY        0
#define NUM_PROPERTY_IDENTIFIERS    1

static NPIdentifier pluginPropertyIdentifiers[NUM_PROPERTY_IDENTIFIERS];
static const NPUTF8 *pluginPropertyIdentifierNames[NUM_PROPERTY_IDENTIFIERS] = {
    "property"
};

#define ID_TEST_CALLBACK_METHOD     0
#define ID_TEST_GETURL              1
#define NUM_METHOD_IDENTIFIERS      2

static NPIdentifier pluginMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *pluginMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
    "testCallback",
    "getURL"
};

static void initializeIdentifiers()
{
    browser->getstringidentifiers (pluginPropertyIdentifierNames, NUM_PROPERTY_IDENTIFIERS, pluginPropertyIdentifiers);
    browser->getstringidentifiers (pluginMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, pluginMethodIdentifiers);
};

bool pluginHasProperty (NPClass *theClass, NPIdentifier name)
{
    for (int i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++) {
        if (name == pluginPropertyIdentifiers[i])
            return true;
    }
    return false;
}

bool pluginHasMethod (NPClass *theClass, NPIdentifier name)
{
    for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++) {
        if (name == pluginMethodIdentifiers[i])
            return true;
    }
    return false;
}

void pluginGetProperty (PluginObject *obj, NPIdentifier name, NPVariant *variant)
{
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_PROPERTY]) {
        // just return "property"
        NPString propertyNameString;
        propertyNameString.UTF8Characters = "property";
        propertyNameString.UTF8Length = sizeof("property") - 1;
        variant->type = NPVariantType_String;
        variant->value.stringValue = propertyNameString;
    } else
        variant->type = NPVariantType_Void;
}

void pluginSetProperty (PluginObject *obj, NPIdentifier name, const NPVariant *variant)
{
}

void pluginInvoke (PluginObject *obj, NPIdentifier name, NPVariant *args, unsigned argCount, NPVariant *result)
{
    if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_METHOD]) {
        // call whatever method name we're given
        if (argCount > 0 && args->type == NPVariantType_String) {
            NPVariant browserResult;
            
            NPObject *windowScriptObject;
            browser->getvalue(obj->npp, NPPVpluginScriptableNPObject, &windowScriptObject);

            NPString argString = args[0].value.stringValue;
            int size = argString.UTF8Length + 1;
            NPUTF8 callbackString[size];
            strncpy(callbackString, argString.UTF8Characters, argString.UTF8Length);
            callbackString[size - 1] = '\0';

            NPIdentifier callbackMethodID = browser->getstringidentifier(callbackString);
            browser->invoke(obj->npp, windowScriptObject, callbackMethodID, 0, 0, &browserResult);
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_GETURL]) {
        if (argCount == 2 && args[0].type == NPVariantType_String && args[1].type == NPVariantType_String) {
            NPString argString = args[0].value.stringValue;
            int size = argString.UTF8Length + 1;
            NPUTF8 urlString[size];
            strncpy(urlString, argString.UTF8Characters, argString.UTF8Length);
            urlString[size - 1] = '\0';

            argString = args[1].value.stringValue;
            size = argString.UTF8Length + 1;
            NPUTF8 targetString[size];
            strncpy(targetString, argString.UTF8Characters, argString.UTF8Length);
            targetString[size - 1] = '\0';
            
            browser->geturl(obj->npp, urlString, targetString);
        }
    }


    result->type = NPVariantType_Void;
}

void pluginInvokeDefault (PluginObject *obj, NPVariant *args, unsigned argCount, NPVariant *result)
{
    result->type = NPVariantType_Void;
}

void pluginInvalidate ()
{
    // Make sure we've released any remainging references to JavaScript
    // objects.
}

NPObject *pluginAllocate (NPP npp, NPClass *theClass)
{
    PluginObject *newInstance = (PluginObject *)malloc (sizeof(PluginObject));
    
    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    newInstance->npp = npp;
    
    return (NPObject *)newInstance;
}

void pluginDeallocate (PluginObject *obj) 
{
    free ((void *)obj);
}

