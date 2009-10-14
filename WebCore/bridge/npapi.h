/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
 
 
 /*
  *  Netscape client plug-in API spec
  */
 

#ifndef _NPAPI_H_
#define _NPAPI_H_

#ifdef INCLUDE_JAVA
#include "jri.h"                /* Java Runtime Interface */
#else
#define jref    void *
#define JRIEnv  void
#endif

#ifdef _WIN32
#    ifndef XP_WIN
#        define XP_WIN 1
#    endif /* XP_WIN */
#endif /* _WIN32 */

#ifdef __MWERKS__
#    define _declspec __declspec
#    ifdef macintosh
#        ifndef XP_MAC
#            define XP_MAC 1
#        endif /* XP_MAC */
#    endif /* macintosh */
#    ifdef __INTEL__
#        undef NULL
#        ifndef XP_WIN
#            define XP_WIN 1
#        endif /* __INTEL__ */
#    endif /* XP_PC */
#endif /* __MWERKS__ */

#ifdef __SYMBIAN32__
#   ifndef XP_SYMBIAN
#       define XP_SYMBIAN 1
#       undef XP_WIN
#   endif
#endif  /* __SYMBIAN32__ */

#if defined(__APPLE_CC__) && !defined(__MACOS_CLASSIC__) && !defined(XP_UNIX)
#   define XP_MACOSX
#endif

#ifdef XP_MAC
    #include <Quickdraw.h>
    #include <Events.h>
#endif

#if defined(XP_MACOSX) && defined(__LP64__)
#define NP_NO_QUICKDRAW
#define NP_NO_CARBON
#endif

#ifdef XP_MACOSX
    #include <ApplicationServices/ApplicationServices.h>
    #include <OpenGL/OpenGL.h>
#ifndef NP_NO_CARBON
    #include <Carbon/Carbon.h>
#endif
#endif

#ifdef XP_UNIX
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>
    #include <stdio.h>
#endif

#if defined(XP_SYMBIAN)
    #include <QEvent>
    #include <QRegion>
#endif

#ifdef XP_WIN
    #include <windows.h>
#endif

/*----------------------------------------------------------------------*/
/*             Plugin Version Constants                                 */
/*----------------------------------------------------------------------*/

#define NP_VERSION_MAJOR 0
#define NP_VERSION_MINOR 24

/*----------------------------------------------------------------------*/
/*             Definition of Basic Types                                */
/*----------------------------------------------------------------------*/

/* QNX sets the _INT16 and friends defines, but does not typedef the types */
#ifdef __QNXNTO__
#undef _UINT16
#undef _INT16
#undef _UINT32
#undef _INT32
#endif

#ifndef _UINT16
#define _UINT16
typedef unsigned short uint16;
#endif

#ifndef _UINT32
#define _UINT32
#ifdef __LP64__
typedef unsigned int uint32;
#else /* __LP64__ */
typedef unsigned long uint32;
#endif /* __LP64__ */
#endif

#ifndef _INT16
#define _INT16
typedef short int16;
#endif

#ifndef _INT32
#define _INT32
#ifdef __LP64__
typedef int int32;
#else /* __LP64__ */
typedef long int32;
#endif /* __LP64__ */
#endif

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef NULL
#define NULL (0L)
#endif

typedef unsigned char    NPBool;
typedef int16            NPError;
typedef int16            NPReason;
typedef char*            NPMIMEType;



/*----------------------------------------------------------------------*/
/*             Structures and definitions             */
/*----------------------------------------------------------------------*/

#if !defined(__LP64__)
#if defined(XP_MAC) || defined(XP_MACOSX)
#pragma options align=mac68k
#endif
#endif /* __LP64__ */

/*
 *  NPP is a plug-in's opaque instance handle
 */
typedef struct _NPP
{
    void*    pdata;            /* plug-in private data */
    void*    ndata;            /* netscape private data */
} NPP_t;

typedef NPP_t*    NPP;


typedef struct _NPStream
{
    void*        pdata;        /* plug-in private data */
    void*        ndata;        /* netscape private data */
    const char*  url;
    uint32       end;
    uint32       lastmodified;
    void*        notifyData;
    const char*  headers;      /* Response headers from host.
                                * Exists only for >= NPVERS_HAS_RESPONSE_HEADERS.
                                * Used for HTTP only; NULL for non-HTTP.
                                * Available from NPP_NewStream onwards.
                                * Plugin should copy this data before storing it.
                                * Includes HTTP status line and all headers,
                                * preferably verbatim as received from server,
                                * headers formatted as in HTTP ("Header: Value"),
                                * and newlines (\n, NOT \r\n) separating lines.
                                * Terminated by \n\0 (NOT \n\n\0). */
} NPStream;


typedef struct _NPByteRange
{
    int32      offset;         /* negative offset means from the end */
    uint32     length;
    struct _NPByteRange* next;
} NPByteRange;


typedef struct _NPSavedData
{
    int32    len;
    void*    buf;
} NPSavedData;


typedef struct _NPRect
{
    uint16    top;
    uint16    left;
    uint16    bottom;
    uint16    right;
} NPRect;


#ifdef XP_UNIX
/*
 * Unix specific structures and definitions
 */

/*
 * Callback Structures.
 *
 * These are used to pass additional platform specific information.
 */
enum {
    NP_SETWINDOW = 1,
    NP_PRINT
};

typedef struct
{
    int32        type;
} NPAnyCallbackStruct;

typedef struct
{
    int32           type;
    Display*        display;
    Visual*         visual;
    Colormap        colormap;
    unsigned int    depth;
} NPSetWindowCallbackStruct;

typedef struct
{
    int32            type;
    FILE*            fp;
} NPPrintCallbackStruct;

#endif /* XP_UNIX */

/*
 *   The following masks are applied on certain platforms to NPNV and 
 *   NPPV selectors that pass around pointers to COM interfaces. Newer 
 *   compilers on some platforms may generate vtables that are not 
 *   compatible with older compilers. To prevent older plugins from 
 *   not understanding a new browser's ABI, these masks change the 
 *   values of those selectors on those platforms. To remain backwards
 *   compatible with differenet versions of the browser, plugins can 
 *   use these masks to dynamically determine and use the correct C++
 *   ABI that the browser is expecting. This does not apply to Windows 
 *   as Microsoft's COM ABI will likely not change.
 */

#define NP_ABI_GCC3_MASK  0x10000000
/*
 *   gcc 3.x generated vtables on UNIX and OSX are incompatible with 
 *   previous compilers.
 */
#if (defined (XP_UNIX) && defined(__GNUC__) && (__GNUC__ >= 3))
#define _NP_ABI_MIXIN_FOR_GCC3 NP_ABI_GCC3_MASK
#else
#define _NP_ABI_MIXIN_FOR_GCC3 0
#endif

#define NP_ABI_MACHO_MASK 0x01000000
/*
 *   On OSX, the Mach-O executable format is significantly
 *   different than CFM. In addition to having a different
 *   C++ ABI, it also has has different C calling convention.
 *   You must use glue code when calling between CFM and
 *   Mach-O C functions. 
 */
#if (defined(TARGET_RT_MAC_MACHO))
#define _NP_ABI_MIXIN_FOR_MACHO NP_ABI_MACHO_MASK
#else
#define _NP_ABI_MIXIN_FOR_MACHO 0
#endif


#define NP_ABI_MASK (_NP_ABI_MIXIN_FOR_GCC3 | _NP_ABI_MIXIN_FOR_MACHO)

/*
 * List of variable names for which NPP_GetValue shall be implemented
 */
typedef enum {
    NPPVpluginNameString = 1,
    NPPVpluginDescriptionString,
    NPPVpluginWindowBool,
    NPPVpluginTransparentBool,

    NPPVjavaClass,                /* Not implemented in WebKit */
    NPPVpluginWindowSize,         /* Not implemented in WebKit */
    NPPVpluginTimerInterval,      /* Not implemented in WebKit */

    NPPVpluginScriptableInstance = (10 | NP_ABI_MASK), /* Not implemented in WebKit */
    NPPVpluginScriptableIID = 11, /* Not implemented in WebKit */

    /* 12 and over are available on Mozilla builds starting with 0.9.9 */
    NPPVjavascriptPushCallerBool = 12,  /* Not implemented in WebKit */
    NPPVpluginKeepLibraryInMemory = 13, /* Not implemented in WebKit */
    NPPVpluginNeedsXEmbed         = 14, /* Not implemented in WebKit */

    /* Get the NPObject for scripting the plugin. */
    NPPVpluginScriptableNPObject  = 15,

    /* Get the plugin value (as \0-terminated UTF-8 string data) for
     * form submission if the plugin is part of a form. Use
     * NPN_MemAlloc() to allocate memory for the string data.
     */
    NPPVformValue = 16,    /* Not implemented in WebKit */

    NPPVpluginUrlRequestsDisplayedBool = 17, /* Not implemented in WebKit */

    /* Checks if the plugin is interested in receiving the http body of
     * failed http requests (http status != 200).
     */
    NPPVpluginWantsAllNetworkStreams = 18,

    /* Checks to see if the plug-in would like the browser to load the "src" attribute. */
    NPPVpluginCancelSrcStream = 20,

#ifdef XP_MACOSX
    /* Used for negotiating drawing models */
    NPPVpluginDrawingModel = 1000,
    /* Used for negotiating event models */
    NPPVpluginEventModel = 1001,
    /* In the NPDrawingModelCoreAnimation drawing model, the browser asks the plug-in for a Core Animation layer. */
    NPPVpluginCoreAnimationLayer = 1003
#endif
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
    NPNVisOfflineBool,

    /* 10 and over are available on Mozilla builds starting with 0.9.4 */
    NPNVserviceManager = (10 | NP_ABI_MASK),  /* Not implemented in WebKit */
    NPNVDOMElement     = (11 | NP_ABI_MASK),  /* Not implemented in WebKit */
    NPNVDOMWindow      = (12 | NP_ABI_MASK),  /* Not implemented in WebKit */
    NPNVToolkit        = (13 | NP_ABI_MASK),  /* Not implemented in WebKit */
    NPNVSupportsXEmbedBool = 14,              /* Not implemented in WebKit */

    /* Get the NPObject wrapper for the browser window. */
    NPNVWindowNPObject = 15,

    /* Get the NPObject wrapper for the plugins DOM element. */
    NPNVPluginElementNPObject = 16,

    NPNVSupportsWindowless = 17,
    
    NPNVprivateModeBool = 18

#ifdef XP_MACOSX
    , NPNVpluginDrawingModel = 1000 /* The NPDrawingModel specified by the plugin */

#ifndef NP_NO_QUICKDRAW
    , NPNVsupportsQuickDrawBool = 2000 /* TRUE if the browser supports the QuickDraw drawing model */
#endif
    , NPNVsupportsCoreGraphicsBool = 2001 /* TRUE if the browser supports the CoreGraphics drawing model */
    , NPNVsupportsOpenGLBool = 2002 /* TRUE if the browser supports the OpenGL drawing model (CGL on Mac) */
    , NPNVsupportsCoreAnimationBool = 2003 /* TRUE if the browser supports the CoreAnimation drawing model */

#ifndef NP_NO_CARBON
    , NPNVsupportsCarbonBool = 3000 /* TRUE if the browser supports the Carbon event model */
#endif
    , NPNVsupportsCocoaBool = 3001 /* TRUE if the browser supports the Cocoa event model */
    
#endif /* XP_MACOSX */
} NPNVariable;

typedef enum {
   NPNURLVCookie = 501,
   NPNURLVProxy
} NPNURLVariable;

/*
 * The type of a NPWindow - it specifies the type of the data structure
 * returned in the window field.
 */
typedef enum {
    NPWindowTypeWindow = 1,
    NPWindowTypeDrawable
} NPWindowType;

#ifdef XP_MACOSX

/*
 * The drawing model for a Mac OS X plugin.  These are the possible values for the NPNVpluginDrawingModel variable.
 */
 
typedef enum {
#ifndef NP_NO_QUICKDRAW
    NPDrawingModelQuickDraw = 0,
#endif
    NPDrawingModelCoreGraphics = 1,
    NPDrawingModelOpenGL = 2,
    NPDrawingModelCoreAnimation = 3
} NPDrawingModel;

/*
 * The event model for a Mac OS X plugin. These are the possible values for the NPNVpluginEventModel variable.
 */

typedef enum {
#ifndef NP_NO_CARBON
    NPEventModelCarbon = 0,
#endif
    NPEventModelCocoa = 1,
} NPEventModel;

typedef enum {
    NPCocoaEventDrawRect = 1,
    NPCocoaEventMouseDown,
    NPCocoaEventMouseUp,
    NPCocoaEventMouseMoved,
    NPCocoaEventMouseEntered,
    NPCocoaEventMouseExited,
    NPCocoaEventMouseDragged,
    NPCocoaEventKeyDown,
    NPCocoaEventKeyUp,
    NPCocoaEventFlagsChanged,
    NPCocoaEventFocusChanged,
    NPCocoaEventWindowFocusChanged,
    NPCocoaEventScrollWheel,
    NPCocoaEventTextInput
} NPCocoaEventType;

typedef struct _NPNSString NPNSString;
typedef struct _NPNSWindow NPNSWindow;
typedef struct _NPNSMenu NPNSMenu;

typedef struct _NPCocoaEvent {
    NPCocoaEventType type;
    uint32 version;
    
    union {
        struct {
            uint32 modifierFlags;
            double pluginX;
            double pluginY;            
            int32 buttonNumber;
            int32 clickCount;
            double deltaX;
            double deltaY;
            double deltaZ;
        } mouse;
        struct {
            uint32 modifierFlags;
            NPNSString *characters;
            NPNSString *charactersIgnoringModifiers;
            NPBool isARepeat;
            uint16 keyCode;
        } key;
        struct {
            CGContextRef context;

            double x;
            double y;
            double width;
            double height;
        } draw;
        struct {
            NPBool hasFocus;
        } focus;
        struct {
            NPNSString *text;
        } text;
    } data;
} NPCocoaEvent;

#endif

typedef struct _NPWindow
{
    void*    window;     /* Platform specific window handle */
    int32    x;            /* Position of top left corner relative */
    int32    y;            /*    to a netscape page.                    */
    uint32    width;        /* Maximum window size */
    uint32    height;
    NPRect    clipRect;    /* Clipping rectangle in port coordinates */
                        /* Used by MAC only.              */
#if defined(XP_UNIX) || defined(XP_SYMBIAN)
    void *    ws_info;    /* Platform-dependent additonal data */
#endif /* XP_UNIX || XP_SYMBIAN */
    NPWindowType type;    /* Is this a window or a drawable? */
} NPWindow;


typedef struct _NPFullPrint
{
    NPBool    pluginPrinted;    /* Set TRUE if plugin handled fullscreen */
                            /*    printing                             */
    NPBool    printOne;        /* TRUE if plugin should print one copy  */
                            /*    to default printer                     */
    void*    platformPrint;    /* Platform-specific printing info */
} NPFullPrint;

typedef struct _NPEmbedPrint
{
    NPWindow    window;
    void*    platformPrint;    /* Platform-specific printing info */
} NPEmbedPrint;

typedef struct _NPPrint
{
    uint16    mode;                        /* NP_FULL or NP_EMBED */
    union
    {
        NPFullPrint     fullPrint;        /* if mode is NP_FULL */
        NPEmbedPrint    embedPrint;        /* if mode is NP_EMBED */
    } print;
} NPPrint;

#ifdef XP_MACOSX
typedef NPNSMenu NPMenu;
#else
typedef void * NPMenu;
#endif

typedef enum {
    NPCoordinateSpacePlugin = 1,
    NPCoordinateSpaceWindow,
    NPCoordinateSpaceFlippedWindow,
    NPCoordinateSpaceScreen,
    NPCoordinateSpaceFlippedScreen
} NPCoordinateSpace;

#if defined(XP_MAC) || defined(XP_MACOSX)

#ifndef NP_NO_CARBON
typedef EventRecord    NPEvent;
#endif

#elif defined(XP_SYMBIAN)
typedef QEvent NPEvent;
#elif defined(XP_WIN)
typedef struct _NPEvent
{
    uint16   event;
    uint32   wParam;
    uint32   lParam;
} NPEvent;
#elif defined (XP_UNIX)
typedef XEvent NPEvent;
#else
typedef void*            NPEvent;
#endif /* XP_MAC */

#if defined(XP_MAC)
typedef RgnHandle NPRegion;
#elif defined(XP_MACOSX)
/* 
 * NPRegion's type depends on the drawing model specified by the plugin (see NPNVpluginDrawingModel).
 * NPQDRegion represents a QuickDraw RgnHandle and is used with the QuickDraw drawing model.
 * NPCGRegion repesents a graphical region when using any other drawing model.
 */
typedef void *NPRegion;
#ifndef NP_NO_QUICKDRAW
typedef RgnHandle NPQDRegion;
#endif
typedef CGPathRef NPCGRegion;
#elif defined(XP_WIN)
typedef HRGN NPRegion;
#elif defined(XP_UNIX)
typedef Region NPRegion;
#elif defined(XP_SYMBIAN)
typedef QRegion* NPRegion;
#else
typedef void *NPRegion;
#endif /* XP_MAC */

#ifdef XP_MACOSX

/* 
 * NP_CGContext is the type of the NPWindow's 'window' when the plugin specifies NPDrawingModelCoreGraphics
 * as its drawing model.
 */

typedef struct NP_CGContext
{
    CGContextRef context;
#ifdef NP_NO_CARBON
    NPNSWindow *window;
#else
    void *window; // Can be either an NSWindow or a WindowRef depending on the event model
#endif
} NP_CGContext;

/* 
 * NP_GLContext is the type of the NPWindow's 'window' when the plugin specifies NPDrawingModelOpenGL as its
 * drawing model.
 */

typedef struct NP_GLContext
{
    CGLContextObj context;
#ifdef NP_NO_CARBON
    NPNSWindow *window;
#else
    void *window; // Can be either an NSWindow or a WindowRef depending on the event model
#endif
} NP_GLContext;

#endif /* XP_MACOSX */

#if defined(XP_MAC) || defined(XP_MACOSX)

/*
 *  Mac-specific structures and definitions.
 */

#ifndef NP_NO_QUICKDRAW

/* 
 * NP_Port is the type of the NPWindow's 'window' when the plugin specifies NPDrawingModelQuickDraw as its
 * drawing model, or the plugin does not specify a drawing model.
 *
 * It is not recommended that new plugins use NPDrawingModelQuickDraw or NP_Port, as QuickDraw has been
 * deprecated in Mac OS X 10.5.  CoreGraphics is the preferred drawing API.
 *
 * NP_Port is not available in 64-bit.
 */
 
typedef struct NP_Port
{
    CGrafPtr     port;        /* Grafport */
    int32        portx;        /* position inside the topmost window */
    int32        porty;
} NP_Port;

#endif /* NP_NO_QUICKDRAW */

/*
 *  Non-standard event types that can be passed to HandleEvent
 */
#define getFocusEvent        (osEvt + 16)
#define loseFocusEvent        (osEvt + 17)
#define adjustCursorEvent   (osEvt + 18)

#endif /* XP_MAC */


/*
 * Values for mode passed to NPP_New:
 */
#define NP_EMBED        1
#define NP_FULL         2

/*
 * Values for stream type passed to NPP_NewStream:
 */
#define NP_NORMAL        1
#define NP_SEEK         2
#define NP_ASFILE        3
#define NP_ASFILEONLY        4

#define NP_MAXREADY    (((unsigned)(~0)<<1)>>1)

#if !defined(__LP64__)
#if defined(XP_MAC) || defined(XP_MACOSX)
#pragma options align=reset
#endif
#endif /* __LP64__ */


/*----------------------------------------------------------------------*/
/*             Error and Reason Code definitions            */
/*----------------------------------------------------------------------*/

/*
 *    Values of type NPError:
 */
#define NPERR_BASE                            0
#define NPERR_NO_ERROR                        (NPERR_BASE + 0)
#define NPERR_GENERIC_ERROR                    (NPERR_BASE + 1)
#define NPERR_INVALID_INSTANCE_ERROR        (NPERR_BASE + 2)
#define NPERR_INVALID_FUNCTABLE_ERROR        (NPERR_BASE + 3)
#define NPERR_MODULE_LOAD_FAILED_ERROR        (NPERR_BASE + 4)
#define NPERR_OUT_OF_MEMORY_ERROR            (NPERR_BASE + 5)
#define NPERR_INVALID_PLUGIN_ERROR            (NPERR_BASE + 6)
#define NPERR_INVALID_PLUGIN_DIR_ERROR        (NPERR_BASE + 7)
#define NPERR_INCOMPATIBLE_VERSION_ERROR    (NPERR_BASE + 8)
#define NPERR_INVALID_PARAM                (NPERR_BASE + 9)
#define NPERR_INVALID_URL                    (NPERR_BASE + 10)
#define NPERR_FILE_NOT_FOUND                (NPERR_BASE + 11)
#define NPERR_NO_DATA                        (NPERR_BASE + 12)
#define NPERR_STREAM_NOT_SEEKABLE            (NPERR_BASE + 13)

/*
 *    Values of type NPReason:
 */
#define NPRES_BASE                0
#define NPRES_DONE                    (NPRES_BASE + 0)
#define NPRES_NETWORK_ERR            (NPRES_BASE + 1)
#define NPRES_USER_BREAK            (NPRES_BASE + 2)

/*
 *      Don't use these obsolete error codes any more.
 */
#define NP_NOERR  NP_NOERR_is_obsolete_use_NPERR_NO_ERROR
#define NP_EINVAL NP_EINVAL_is_obsolete_use_NPERR_GENERIC_ERROR
#define NP_EABORT NP_EABORT_is_obsolete_use_NPRES_USER_BREAK

/*
 * Version feature information
 */
#define NPVERS_HAS_STREAMOUTPUT     8
#define NPVERS_HAS_NOTIFICATION     9
#define NPVERS_HAS_LIVECONNECT        9
#define NPVERS_WIN16_HAS_LIVECONNECT    9
#define NPVERS_68K_HAS_LIVECONNECT    11
#define NPVERS_HAS_WINDOWLESS       11
#define NPVERS_HAS_XPCONNECT_SCRIPTING    13  /* Not implemented in WebKit */
#define NPVERS_HAS_NPRUNTIME_SCRIPTING    14
#define NPVERS_HAS_FORM_VALUES            15  /* Not implemented in WebKit; see bug 13061 */
#define NPVERS_HAS_POPUPS_ENABLED_STATE   16  /* Not implemented in WebKit */
#define NPVERS_HAS_RESPONSE_HEADERS       17
#define NPVERS_HAS_NPOBJECT_ENUM          18
#define NPVERS_HAS_PLUGIN_THREAD_ASYNC_CALL 19
#define NPVERS_HAS_ALL_NETWORK_STREAMS    20
#define NPVERS_HAS_URL_AND_AUTH_INFO      21
#define NPVERS_HAS_PRIVATE_MODE           22
#define NPVERS_MACOSX_HAS_EVENT_MODELS    23
#define NPVERS_HAS_CANCEL_SRC_STREAM      24

/*----------------------------------------------------------------------*/
/*             Function Prototypes                */
/*----------------------------------------------------------------------*/

#if defined(_WINDOWS) && !defined(WIN32)
#define NP_LOADDS  _loadds
#else
#define NP_LOADDS
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NPP_* functions are provided by the plugin and called by the navigator.
 */

#ifdef XP_UNIX
char*                    NPP_GetMIMEDescription(void);
#endif /* XP_UNIX */

NPError     NPP_Initialize(void);
void        NPP_Shutdown(void);
NPError     NP_LOADDS    NPP_New(NPMIMEType pluginType, NPP instance,
                                uint16 mode, int16 argc, char* argn[],
                                char* argv[], NPSavedData* saved);
NPError     NP_LOADDS    NPP_Destroy(NPP instance, NPSavedData** save);
NPError     NP_LOADDS    NPP_SetWindow(NPP instance, NPWindow* window);
NPError     NP_LOADDS    NPP_NewStream(NPP instance, NPMIMEType type,
                                      NPStream* stream, NPBool seekable,
                                      uint16* stype);
NPError     NP_LOADDS    NPP_DestroyStream(NPP instance, NPStream* stream,
                                          NPReason reason);
int32        NP_LOADDS    NPP_WriteReady(NPP instance, NPStream* stream);
int32        NP_LOADDS    NPP_Write(NPP instance, NPStream* stream, int32 offset,
                                  int32 len, void* buffer);
void        NP_LOADDS    NPP_StreamAsFile(NPP instance, NPStream* stream,
                                         const char* fname);
void        NP_LOADDS    NPP_Print(NPP instance, NPPrint* platformPrint);
int16            NPP_HandleEvent(NPP instance, void* event);
void        NP_LOADDS    NPP_URLNotify(NPP instance, const char* url,
                                      NPReason reason, void* notifyData);
jref        NP_LOADDS            NPP_GetJavaClass(void);
NPError     NPP_GetValue(NPP instance, NPPVariable variable,
                                     void *value);
NPError     NPP_SetValue(NPP instance, NPNVariable variable,
                                     void *value);

/*
 * NPN_* functions are provided by the navigator and called by the plugin.
 */

void        NPN_Version(int* plugin_major, int* plugin_minor,
                            int* netscape_major, int* netscape_minor);
NPError     NPN_GetURLNotify(NPP instance, const char* url,
                                 const char* target, void* notifyData);
NPError     NPN_GetURL(NPP instance, const char* url,
                           const char* target);
NPError     NPN_PostURLNotify(NPP instance, const char* url,
                                  const char* target, uint32 len,
                                  const char* buf, NPBool file,
                                  void* notifyData);
NPError     NPN_PostURL(NPP instance, const char* url,
                            const char* target, uint32 len,
                            const char* buf, NPBool file);
NPError     NPN_RequestRead(NPStream* stream, NPByteRange* rangeList);
NPError     NPN_NewStream(NPP instance, NPMIMEType type,
                              const char* target, NPStream** stream);
int32        NPN_Write(NPP instance, NPStream* stream, int32 len,
                          void* buffer);
NPError     NPN_DestroyStream(NPP instance, NPStream* stream,
                                  NPReason reason);
void        NPN_Status(NPP instance, const char* message);
const char*    NPN_UserAgent(NPP instance);
void*        NPN_MemAlloc(uint32 size);
void        NPN_MemFree(void* ptr);
uint32        NPN_MemFlush(uint32 size);
void        NPN_ReloadPlugins(NPBool reloadPages);
JRIEnv*     NPN_GetJavaEnv(void);
jref        NPN_GetJavaPeer(NPP instance);
NPError     NPN_GetValue(NPP instance, NPNVariable variable,
                             void *value);
NPError     NPN_SetValue(NPP instance, NPPVariable variable,
                             void *value);
void        NPN_InvalidateRect(NPP instance, NPRect *invalidRect);
void        NPN_InvalidateRegion(NPP instance, NPRegion invalidRegion);
void        NPN_ForceRedraw(NPP instance);
void        NPN_PushPopupsEnabledState(NPP instance, NPBool enabled);
void        NPN_PopPopupsEnabledState(NPP instance);
void        NPN_PluginThreadAsyncCall(NPP instance, void (*func) (void *), void *userData);
NPError     NPN_GetValueForURL(NPP instance, NPNURLVariable variable, const char* url, char** value, uint32* len);
NPError     NPN_SetValueForURL(NPP instance, NPNURLVariable variable, const char* url, const char* value, uint32 len);
NPError     NPN_GetAuthenticationInfo(NPP instance, const char* protocol, const char* host, int32 port, const char* scheme, const char *realm, char** username, uint32* ulen, char** password, uint32* plen);
uint32      NPN_ScheduleTimer(NPP instance, uint32 interval, NPBool repeat, void (*timerFunc)(NPP npp, uint32 timerID));
void        NPN_UnscheduleTimer(NPP instance, uint32 timerID);
NPError     NPN_PopUpContextMenu(NPP instance, NPMenu* menu);
NPBool      NPN_ConvertPoint(NPP instance, double sourceX, double sourceY, NPCoordinateSpace sourceSpace, double *destX, double *destY, NPCoordinateSpace destSpace);
    
#ifdef __cplusplus
}  /* end extern "C" */
#endif

#endif /* _NPAPI_H_ */
