/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
 
#ifndef _NPAPI_H_
#define _NPAPI_H_

#include <CoreServices/CoreServices.h>
#include <Foundation/Foundation.h>
/*
 * Values for mode passed to NPP_New:
 */
#define NP_EMBED		1
#define NP_FULL 		2

/*
 * Values for stream type passed to NPP_NewStream:
 */
#define NP_NORMAL		1
#define NP_SEEK 		2
#define NP_ASFILE		3
#define NP_ASFILEONLY		4

/*----------------------------------------------------------------------*/
/*		     Definition of Basic Types				*/
/*----------------------------------------------------------------------*/

#ifndef _uint16
typedef unsigned short uint16;
#endif
#ifndef _uint32
#if defined(__alpha)
typedef unsigned int uint32;
#else /* __alpha */
typedef unsigned long uint32;
#endif /* __alpha */
#endif
#ifndef _INT16
typedef short int16;
#endif
#ifndef _INT32
#if defined(__alpha)
typedef int int32;
#else /* __alpha */
typedef long int32;
#endif /* __alpha */
#endif

typedef int16 NPError;
typedef unsigned char NPBool;
typedef int16 NPReason;
typedef char* NPMIMEType;


/*
 *  NPP is a plug-in's opaque instance handle
 */
typedef struct _NPP
{
    void*	pdata;			/* plug-in private data */
    void*	ndata;			/* netscape private data */
} NPP_t;

typedef NPP_t*	NPP;

typedef struct _NPStream
{
    void*		pdata;		/* plug-in private data */
    void*		ndata;		/* netscape private data */
    const char* 	url;
    uint32		end;
    uint32		lastmodified;
    void*		notifyData;
} NPStream;


typedef struct _NPByteRange
{
    int32	offset; 		/* negative offset means from the end */
    uint32	length;
    struct _NPByteRange* next;
} NPByteRange;


typedef struct _NPSavedData
{
    int32	len;
    void*	buf;
} NPSavedData;


typedef struct _NPRect
{
    uint16	top;
    uint16	left;
    uint16	bottom;
    uint16	right;
} NPRect;

typedef RgnHandle NPRegion;

typedef struct NP_Port
{
    CGrafPtr	port;		/* Grafport */
    int32		portx;		/* position inside the topmost window */
    int32		porty;
} NP_Port;


/*
 *  Non-standard event types that can be passed to HandleEvent
 */
#define getFocusEvent	    (osEvt + 16)
#define loseFocusEvent	    (osEvt + 17)
#define adjustCursorEvent   (osEvt + 18)


/*
 * List of variable names for which NPP_GetValue shall be implemented
 */
typedef enum {
	NPPVpluginNameString = 1,
	NPPVpluginDescriptionString,
	NPPVpluginWindowBool,
	NPPVpluginTransparentBool
} NPPVariable;

/*
 * List of variable names for which NPN_GetValue is implemented by Mozilla
 */
typedef enum {
	NPNVxDisplay = 1,
	NPNVxtAppContext,
    NPNVnetscapeWindow,
	NPNVjavascriptEnabledBool,
	NPNVasdEnabledBool,
	NPNVisOfflineBool
} NPNVariable;

typedef enum {
    NPWindowTypeWindow = 1,
    NPWindowTypeDrawable
} NPWindowType;

typedef struct _NPWindow
{
    void*	window; 	/* Platform specific window handle */
    int32	x;			/* Position of top left corner relative */
    int32	y;			/*	to a netscape page.					*/
    uint32	width;		/* Maximum window size */
    uint32	height;
    NPRect	clipRect;	/* Clipping rectangle in port coordinates */
						/* Used by MAC only.			  */
    NPWindowType type;	/* Is this a window or a drawable? */
} NPWindow;

typedef struct _NPFullPrint
{
    NPBool	pluginPrinted;	/* Set TRUE if plugin handled fullscreen */
							/*	printing							 */
    NPBool	printOne;		/* TRUE if plugin should print one copy  */
							/*	to default printer					 */
    void*	platformPrint;	/* Platform-specific printing info */
} NPFullPrint;

typedef struct _NPEmbedPrint
{
    NPWindow	window;
    void*	platformPrint;	/* Platform-specific printing info */
} NPEmbedPrint;

typedef struct _NPPrint
{
    uint16	mode;						/* NP_FULL or NP_EMBED */
    union
    {
		NPFullPrint		fullPrint;		/* if mode is NP_FULL */
		NPEmbedPrint	embedPrint;		/* if mode is NP_EMBED */
    } print;
} NPPrint;


typedef NPError	(*NPN_GetURLNotifyProcPtr)(NPP instance, const char* url, const char* window, void* notifyData);
typedef NPError (*NPN_PostURLNotifyProcPtr)(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file, void* notifyData);
typedef NPError	(*NPN_RequestReadProcPtr)(NPStream* stream, NPByteRange* rangeList);
typedef NPError	(*NPN_NewStreamProcPtr)(NPP instance, NPMIMEType type, const char* window, NPStream** stream);
typedef int32 (*NPN_WriteProcPtr)(NPP instance, NPStream* stream, int32 len, void* buffer);
typedef NPError (*NPN_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef void (*NPN_StatusProcPtr)(NPP instance, const char* message);
typedef const char*(*NPN_UserAgentProcPtr)(NPP instance);
typedef void* (*NPN_MemAllocProcPtr)(uint32 size);
typedef void (*NPN_MemFreeProcPtr)(void* ptr);
typedef uint32 (*NPN_MemFlushProcPtr)(uint32 size);
typedef void (*NPN_ReloadPluginsProcPtr)(NPBool reloadPages);
typedef NPError	(*NPN_GetValueProcPtr)(NPP instance, NPNVariable variable, void *ret_alue);
typedef NPError	(*NPN_SetValueProcPtr)(NPP instance, NPPVariable variable, void *ret_alue);
typedef void (*NPN_InvalidateRectProcPtr)(NPP instance, NPRect *rect);
typedef void (*NPN_InvalidateRegionProcPtr)(NPP instance, NPRegion region);
typedef void (*NPN_ForceRedrawProcPtr)(NPP instance);
typedef NPError	(*NPN_GetURLProcPtr)(NPP instance, const char* url, const char* window);
typedef NPError (*NPN_PostURLProcPtr)(NPP instance, const char* url, const char* window, uint32 len, const char* buf, NPBool file);
typedef void* (*NPN_GetJavaEnvProcPtr)(void);
typedef void* (*NPN_GetJavaPeerProcPtr)(NPP instance);

typedef NPError	(*NPP_NewProcPtr)(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
typedef NPError	(*NPP_DestroyProcPtr)(NPP instance, NPSavedData** save);
typedef NPError	(*NPP_SetWindowProcPtr)(NPP instance, NPWindow* window);
typedef NPError	(*NPP_NewStreamProcPtr)(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
typedef NPError	(*NPP_DestroyStreamProcPtr)(NPP instance, NPStream* stream, NPReason reason);
typedef void 	(*NPP_StreamAsFileProcPtr)(NPP instance, NPStream* stream, const char* fname);
typedef int16 (*NPP_WriteReadyProcPtr)(NPP instance, NPStream* stream);
typedef int16 (*NPP_WriteProcPtr)(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
typedef void (*NPP_PrintProcPtr)(NPP instance, NPPrint* platformPrint);
typedef int16 (*NPP_HandleEventProcPtr)(NPP instance, void* event);
typedef void (*NPP_URLNotifyProcPtr)(NPP instance, const char* url, NPReason reason, void* notifyData);
typedef NPError	(*NPP_GetValueProcPtr)(NPP instance, NPPVariable variable, void *ret_alue);
typedef NPError	(*NPP_SetValueProcPtr)(NPP instance, NPNVariable variable, void *ret_alue);
typedef void (*NPP_ShutdownProcPtr)(void);
typedef void*	JRIGlobalRef; //not using this right now

typedef struct _NPNetscapeFuncs {
    uint16 size;
    uint16 version;
    NPN_GetURLProcPtr geturl;
    NPN_PostURLProcPtr posturl;
    NPN_RequestReadProcPtr requestread;
    NPN_NewStreamProcPtr newstream;
    NPN_WriteProcPtr write;
    NPN_DestroyStreamProcPtr destroystream;
    NPN_StatusProcPtr status;
    NPN_UserAgentProcPtr uagent;
    NPN_MemAllocProcPtr memalloc;
    NPN_MemFreeProcPtr memfree;
    NPN_MemFlushProcPtr memflush;
    NPN_ReloadPluginsProcPtr reloadplugins;
    NPN_GetJavaEnvProcPtr getJavaEnv;
    NPN_GetJavaPeerProcPtr getJavaPeer;
    NPN_GetURLNotifyProcPtr geturlnotify;
    NPN_PostURLNotifyProcPtr posturlnotify;
    NPN_GetValueProcPtr getvalue;
    NPN_SetValueProcPtr setvalue;
    NPN_InvalidateRectProcPtr invalidaterect;
    NPN_InvalidateRegionProcPtr invalidateregion;
    NPN_ForceRedrawProcPtr forceredraw;
} NPNetscapeFuncs;

typedef struct _NPPluginFuncs {
    uint16 size;
    uint16 version;
    NPP_NewProcPtr newp;
    NPP_DestroyProcPtr destroy;
    NPP_SetWindowProcPtr setwindow;
    NPP_NewStreamProcPtr newstream;
    NPP_DestroyStreamProcPtr destroystream;
    NPP_StreamAsFileProcPtr asfile;
    NPP_WriteReadyProcPtr writeready;
    NPP_WriteProcPtr write;
    NPP_PrintProcPtr print;
    NPP_HandleEventProcPtr event;
    NPP_URLNotifyProcPtr urlnotify;
    JRIGlobalRef javaClass;
    NPP_GetValueProcPtr getvalue;
    NPP_SetValueProcPtr setvalue;
} NPPluginFuncs;

/*----------------------------------------------------------------------*/
/*		     Error and Reason Code definitions			*/
/*----------------------------------------------------------------------*/

/*
 *	Values of type NPError:
 */
#define NPERR_BASE							0
#define NPERR_NO_ERROR						(NPERR_BASE + 0)
#define NPERR_GENERIC_ERROR					(NPERR_BASE + 1)
#define NPERR_INVALID_INSTANCE_ERROR		(NPERR_BASE + 2)
#define NPERR_INVALID_FUNCTABLE_ERROR		(NPERR_BASE + 3)
#define NPERR_MODULE_LOAD_FAILED_ERROR		(NPERR_BASE + 4)
#define NPERR_OUT_OF_MEMORY_ERROR			(NPERR_BASE + 5)
#define NPERR_INVALID_PLUGIN_ERROR			(NPERR_BASE + 6)
#define NPERR_INVALID_PLUGIN_DIR_ERROR		(NPERR_BASE + 7)
#define NPERR_INCOMPATIBLE_VERSION_ERROR	(NPERR_BASE + 8)
#define NPERR_INVALID_PARAM				(NPERR_BASE + 9)
#define NPERR_INVALID_URL					(NPERR_BASE + 10)
#define NPERR_FILE_NOT_FOUND				(NPERR_BASE + 11)
#define NPERR_NO_DATA						(NPERR_BASE + 12)
#define NPERR_STREAM_NOT_SEEKABLE			(NPERR_BASE + 13)

/*
 *	Values of type NPReason:
 */
#define NPRES_BASE				0
#define NPRES_DONE					(NPRES_BASE + 0)
#define NPRES_NETWORK_ERR			(NPRES_BASE + 1)
#define NPRES_USER_BREAK			(NPRES_BASE + 2)

/*
 *      Don't use these obsolete error codes any more.
 */
#define NP_NOERR  NP_NOERR_is_obsolete_use_NPERR_NO_ERROR
#define NP_EINVAL NP_EINVAL_is_obsolete_use_NPERR_GENERIC_ERROR
#define NP_EABORT NP_EABORT_is_obsolete_use_NPRES_USER_BREAK

/*
 * Version feature information
 */
#define NPVERS_HAS_STREAMOUTPUT 	8
#define NPVERS_HAS_NOTIFICATION 	9
#define NPVERS_HAS_LIVECONNECT		9
#define NPVERS_WIN16_HAS_LIVECONNECT	9
#define NPVERS_68K_HAS_LIVECONNECT	11
#define NPVERS_HAS_WINDOWLESS       11

#if defined(__cplusplus)
extern "C" {
#endif

typedef NPError (* mainFuncPtr)(NPNetscapeFuncs*, NPPluginFuncs*, NPP_ShutdownProcPtr*);

typedef NPError (* initializeFuncPtr)(NPNetscapeFuncs*);
typedef NPError (* getEntryPointsFuncPtr)(NPPluginFuncs*);

void *functionPointerForTVector(void *);
void *tVectorForFunctionPointer(void *);

/*
 * NPN_* functions are provided by the navigator and called by the plugin.
 */

void		NPN_Version(int* plugin_major, int* plugin_minor, int* netscape_major, int* netscape_minor);
NPError 	NPN_GetURLNotify(NPP instance, const char* url, const char* target, void* notifyData);
NPError 	NPN_GetURL(NPP instance, const char* url, const char* target);
NPError 	NPN_PostURLNotify(NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file, void* notifyData);
NPError 	NPN_PostURL(NPP instance, const char* url, const char* target, uint32 len, const char* buf, NPBool file);
NPError 	NPN_RequestRead(NPStream* stream, NPByteRange* rangeList);
NPError 	NPN_NewStream(NPP instance, NPMIMEType type, const char* target, NPStream** stream);
int32		NPN_Write(NPP instance, NPStream* stream, int32 len, void* buffer);
NPError 	NPN_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
void		NPN_Status(NPP instance, const char* message);
const char*	NPN_UserAgent(NPP instance);
void*		NPN_MemAlloc(uint32 size);
void		NPN_MemFree(void* ptr);
uint32		NPN_MemFlush(uint32 size);
void		NPN_ReloadPlugins(NPBool reloadPages);
void* 		NPN_GetJavaEnv(void);
void* 		NPN_GetJavaPeer(NPP instance);
NPError 	NPN_GetValue(NPP instance, NPNVariable variable, void *value);
NPError 	NPN_SetValue(NPP instance, NPPVariable variable, void *value);
void		NPN_InvalidateRect(NPP instance, NPRect *invalidRect);
void		NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion);
void		NPN_ForceRedraw(NPP instance);

#if defined(__cplusplus)
} // extern "C"
#endif

/*
 * NPP_* functions are provided by the plugin and called by the navigator.
 */
/*
void		NPP_Shutdown(void);
NPError		NPP_New(NPMIMEType pluginType, NPP instance, uint16 mode, int16 argc, char* argn[], char* argv[], NPSavedData* saved);
NPError		NPP_Destroy(NPP instance, NPSavedData** save);
NPError		NPP_SetWindow(NPP instance, NPWindow* window);
NPError		NPP_NewStream(NPP instance, NPMIMEType type, NPStream* stream, NPBool seekable, uint16* stype);
NPError		NPP_DestroyStream(NPP instance, NPStream* stream, NPReason reason);
int32		NPP_WriteReady(NPP instance, NPStream* stream);
int32		NPP_Write(NPP instance, NPStream* stream, int32 offset, int32 len, void* buffer);
void		NPP_StreamAsFile(NPP instance, NPStream* stream, const char* fname);
void		NPP_Print(NPP instance, NPPrint* platformPrint);
int16		NPP_HandleEvent(NPP instance, void* event);
void		NPP_URLNotify(NPP instance, const char* url, NPReason reason, void* notifyData);
NPError		NPP_GetValue(void *instance, NPPVariable variable, void *value);
NPError		NPP_SetValue(void *instance, NPNVariable variable, void *value);
*/
#endif /* _NPAPI_H_ */