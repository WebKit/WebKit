#include "npapi.h"

NPError NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData){
    printf("NPN_GetURLNotify\n");
    return NPERR_GENERIC_ERROR;

}

NPError NPN_GetURL(NPP instance, const char* url, const char* target){
    printf("NPN_GetURL\n");
    return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURLNotify(NPP instance, const char* url, const char* target, UInt32 len, const char* buf, NPBool file, void* notifyData){
    printf("NPN_PostURLNotify\n");
    return NPERR_GENERIC_ERROR;
}

NPError NPN_PostURL(NPP instance, const char* url, const char* target, UInt32 len, const char* buf, NPBool file){
    printf("NPN_PostURL\n");
    return NPERR_GENERIC_ERROR;
}

NPError NPN_RequestRead(NPStream* stream, NPByteRange* rangeList){
    printf("NPN_RequestRead\n");
    return NPERR_GENERIC_ERROR;
}

NPError NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream){
    printf("NPN_NewStream\n");
    return NPERR_GENERIC_ERROR;
}

SInt32	NPN_Write(NPP instance, NPStream* stream, SInt32 len, void* buffer){
    printf("NPN_Write\n");
    return NPERR_GENERIC_ERROR;
}

NPError NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason){
    printf("NPN_DestroyStream\n");
    return NPERR_GENERIC_ERROR;
}

void NPN_Status(NPP instance, const char* message){
    printf("NPN_Status\n");
}

const char* NPN_UserAgent(NPP instance){
    printf("NPN_UserAgent\n");
    return "IE";
}

void* NPN_MemAlloc(UInt32 size){
    printf("NPN_MemAlloc\n");
    return malloc(size);

}

void NPN_MemFree(void* ptr){
    printf("NPN_MemFree\n");
    free(ptr);

}

UInt32 NPN_MemFlush(UInt32 size){
    printf("NPN_MemFlush\n");
    return 0;
}

void NPN_ReloadPlugins(NPBool reloadPages){
    printf("NPN_ReloadPlugins\n");

}

NPError NPN_GetValue(NPP instance, NPNVariable variable, void *value){
    printf("NPN_GetValue\n");
    return NPERR_GENERIC_ERROR;
}

NPError NPN_SetValue(NPP instance, NPPVariable variable, void *value){
    printf("NPN_SetValue\n");
    return NPERR_GENERIC_ERROR;
}	

void NPN_InvalidateRect(NPP instance, NPRect *invalidRect){
    printf("NPN_InvalidateRect\n");

}

void NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion){
    printf("NPN_InvalidateRegion\n");

}

void NPN_ForceRedraw(NPP instance){
    printf("NPN_ForceRedraw\n");

}


void *functionPointerForTVector(void *tvp) {
    UInt32 template[6] = {0x3D800000, 0x618C0000, 0x800C0000, 0x804C0004, 0x7C0903A6, 0x4E800420};
    UInt32 *newGlue = NULL;
    
    if (tvp != NULL) {
        newGlue = malloc(sizeof(template));
        if (newGlue != NULL) {
            unsigned i;
            for (i = 0; i < 6; i++) newGlue[i] = template[i];
            newGlue[0] |= ((UInt32)tvp >> 16);
            newGlue[1] |= ((UInt32)tvp & 0xFFFF);
            MakeDataExecutable(newGlue, sizeof(template));
        }
    }
    return newGlue;
}

void *tVectorForFunctionPointer(void *fp) {
    void **newGlue = NULL;
    if (fp != NULL) {
        newGlue = malloc(2 * sizeof(void *));
        if (newGlue != NULL) {
            newGlue[0] = fp;
            newGlue[1] = NULL;
        }
    }
    return newGlue;
}