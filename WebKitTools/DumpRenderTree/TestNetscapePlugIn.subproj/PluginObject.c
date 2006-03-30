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

static void pluginInvalidate(NPObject *obj);
static bool pluginHasProperty(NPObject *obj, NPIdentifier name);
static bool pluginHasMethod(NPObject *obj, NPIdentifier name);
static bool pluginGetProperty(NPObject *obj, NPIdentifier name, NPVariant *variant);
static bool pluginSetProperty(NPObject *obj, NPIdentifier name, const NPVariant *variant);
static bool pluginInvoke(NPObject *obj, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result);
static bool pluginInvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result);
static NPObject *pluginAllocate(NPP npp, NPClass *theClass);
static void pluginDeallocate(NPObject *obj);

NPNetscapeFuncs *browser;

static NPClass pluginClass = { 
    NP_CLASS_STRUCT_VERSION,
    pluginAllocate, 
    pluginDeallocate, 
    pluginInvalidate,
    pluginHasMethod,
    pluginInvoke,
    pluginInvokeDefault,
    pluginHasProperty,
    pluginGetProperty,
    pluginSetProperty,
};
 
NPClass *getPluginClass(void)
{
    return &pluginClass;
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
#define ID_REMOVE_DEFAULT_METHOD    2

#define NUM_METHOD_IDENTIFIERS      3

static NPIdentifier pluginMethodIdentifiers[NUM_METHOD_IDENTIFIERS];
static const NPUTF8 *pluginMethodIdentifierNames[NUM_METHOD_IDENTIFIERS] = {
    "testCallback",
    "getURL",
    "removeDefaultMethod",
};

static NPUTF8* createCStringFromNPVariant(const NPVariant *variant)
{
    size_t length = NPVARIANT_TO_STRING(*variant).UTF8Length;
    NPUTF8* result = malloc(length + 1);
    memcpy(result, NPVARIANT_TO_STRING(*variant).UTF8Characters, length);
    result[length] = '\0';
    return result;
}

static void initializeIdentifiers(void)
{
    browser->getstringidentifiers(pluginPropertyIdentifierNames, NUM_PROPERTY_IDENTIFIERS, pluginPropertyIdentifiers);
    browser->getstringidentifiers(pluginMethodIdentifierNames, NUM_METHOD_IDENTIFIERS, pluginMethodIdentifiers);
}

static bool pluginHasProperty(NPObject *obj, NPIdentifier name)
{
    for (int i = 0; i < NUM_PROPERTY_IDENTIFIERS; i++)
        if (name == pluginPropertyIdentifiers[i])
            return true;
    return false;
}

static bool pluginHasMethod(NPObject *obj, NPIdentifier name)
{
    for (int i = 0; i < NUM_METHOD_IDENTIFIERS; i++)
        if (name == pluginMethodIdentifiers[i])
            return true;
    return false;
}

static bool pluginGetProperty(NPObject *obj, NPIdentifier name, NPVariant *variant)
{
    if (name == pluginPropertyIdentifiers[ID_PROPERTY_PROPERTY]) {
        STRINGZ_TO_NPVARIANT("property", *variant);
        return true;
    }
    return false;
}

static bool pluginSetProperty(NPObject *obj, NPIdentifier name, const NPVariant *variant)
{
    return false;
}

static bool pluginInvoke(NPObject *header, NPIdentifier name, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    PluginObject *obj = (PluginObject *)header;
    if (name == pluginMethodIdentifiers[ID_TEST_CALLBACK_METHOD]) {
        // call whatever method name we're given
        if (argCount > 0 && NPVARIANT_IS_STRING(args[0])) {
            NPObject *windowScriptObject;
            browser->getvalue(obj->npp, NPPVpluginScriptableNPObject, &windowScriptObject);

            NPUTF8* callbackString = createCStringFromNPVariant(&args[0]);
            NPIdentifier callbackIdentifier = browser->getstringidentifier(callbackString);
            free(callbackString);

            NPVariant browserResult;
            browser->invoke(obj->npp, windowScriptObject, callbackIdentifier, 0, 0, &browserResult);
            browser->releasevariantvalue(&browserResult);

            VOID_TO_NPVARIANT(*result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_TEST_GETURL]) {
        if (argCount == 2 && NPVARIANT_IS_STRING(args[0]) && NPVARIANT_IS_STRING(args[1])) {
            NPUTF8* urlString = createCStringFromNPVariant(&args[0]);
            NPUTF8* targetString = createCStringFromNPVariant(&args[1]);
            browser->geturl(obj->npp, urlString, targetString);
            free(urlString);
            free(targetString);

            VOID_TO_NPVARIANT(*result);
            return true;
        }
    } else if (name == pluginMethodIdentifiers[ID_REMOVE_DEFAULT_METHOD]) {
        pluginClass.invokeDefault = 0;
        VOID_TO_NPVARIANT(*result);
        return true;
    }

    return false;
}

static bool pluginInvokeDefault(NPObject *obj, const NPVariant *args, uint32_t argCount, NPVariant *result)
{
    INT32_TO_NPVARIANT(1, *result);
    return true;
}

static void pluginInvalidate(NPObject *obj)
{
}

static NPObject *pluginAllocate(NPP npp, NPClass *theClass)
{
    PluginObject *newInstance = malloc(sizeof(PluginObject));
    
    if (!identifiersInitialized) {
        identifiersInitialized = true;
        initializeIdentifiers();
    }

    newInstance->npp = npp;
    
    return (NPObject *)newInstance;
}

static void pluginDeallocate(NPObject *obj) 
{
    free(obj);
}
