/*
    Copyright (C) 2011 ProFUSION embedded systems
    Copyright (C) 2011 Samsung Electronics

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef DumpRenderTreeSupportEfl_h
#define DumpRenderTreeSupportEfl_h

#include <Eina.h>
#include <FindOptions.h>
#include <IntRect.h>
#include <JavaScriptCore/APICast.h>
#include <JavaScriptCore/JSStringRef.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

typedef struct _Evas_Object Evas_Object;
typedef struct _Ewk_History_Item Ewk_History_Item;

typedef Vector<Ewk_History_Item*> HistoryItemChildrenVector;

namespace WebCore {
class Frame;
}

class EAPI DumpRenderTreeSupportEfl {
public:
    DumpRenderTreeSupportEfl() { }

    ~DumpRenderTreeSupportEfl() { }

    static unsigned activeAnimationsCount(const Evas_Object* ewkFrame);
    static bool callShouldCloseOnWebView(Evas_Object* ewkFrame);
    static void clearFrameName(Evas_Object* ewkFrame);
    static void clearOpener(Evas_Object* ewkFrame);
    static String counterValueByElementId(const Evas_Object* ewkFrame, const char* elementId);
    static bool elementDoesAutoCompleteForElementWithId(const Evas_Object* ewkFrame, const String& elementId);
    static Eina_List* frameChildren(const Evas_Object* ewkFrame);
    static WebCore::Frame* frameParent(const Evas_Object* ewkFrame);
    static void layoutFrame(Evas_Object* ewkFrame);
    static int numberOfPages(const Evas_Object* ewkFrame, float pageWidth, float pageHeight);
    static int numberOfPagesForElementId(const Evas_Object* ewkFrame, const char* elementId, float pageWidth, float pageHeight);
    static String pageProperty(const Evas_Object* ewkFrame, const char* propertyName, int pageNumber);
    static String pageSizeAndMarginsInPixels(const Evas_Object* ewkFrame, int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft);
    static bool pauseAnimation(Evas_Object* ewkFrame, const char* name, const char* elementId, double time);
    static bool pauseTransition(Evas_Object* ewkFrame, const char* name, const char* elementId, double time);
    static unsigned pendingUnloadEventCount(const Evas_Object* ewkFrame);
    static String renderTreeDump(Evas_Object* ewkFrame);
    static String responseMimeType(const Evas_Object* ewkFrame);
    static void resumeAnimations(Evas_Object* ewkFrame);
    static WebCore::IntRect selectionRectangle(const Evas_Object* ewkFrame);
    static String suitableDRTFrameName(const Evas_Object* ewkFrame);
    static void suspendAnimations(Evas_Object* ewkFrame);
    static void setValueForUser(JSContextRef, JSValueRef nodeObject, JSStringRef value);
    static void setAutofilled(JSContextRef, JSValueRef nodeObject, bool autofilled);
    static void setDefersLoading(Evas_Object* ewkView, bool defers);
    static void setLoadsSiteIconsIgnoringImageLoadingSetting(Evas_Object* ewkView, bool loadsSiteIconsIgnoringImageLoadingPreferences);

    static void addUserStyleSheet(const Evas_Object* ewkView, const char* sourceCode, bool allFrames);
    static void executeCoreCommandByName(const Evas_Object* ewkView, const char* name, const char* value);
    static bool findString(const Evas_Object* ewkView, const char* text, WebCore::FindOptions);
    static bool isCommandEnabled(const Evas_Object* ewkView, const char* name);
    static void setJavaScriptProfilingEnabled(const Evas_Object* ewkView, bool enabled);
    static void setSmartInsertDeleteEnabled(Evas_Object* ewkView, bool enabled);
    static void setSelectTrailingWhitespaceEnabled(Evas_Object* ewkView, bool enabled);

    static void garbageCollectorCollect();
    static void garbageCollectorCollectOnAlternateThread(bool waitUntilDone);
    static size_t javaScriptObjectsCount();
    static unsigned workerThreadCount();

    static HistoryItemChildrenVector childHistoryItems(const Ewk_History_Item*);
    static String historyItemTarget(const Ewk_History_Item*);
    static bool isTargetItem(const Ewk_History_Item*);

    static void setMockScrollbarsEnabled(bool);

    static void dumpConfigurationForViewport(Evas_Object* ewkView, int deviceDPI, const WebCore::IntSize& deviceSize, const WebCore::IntSize& availableSize);

    static void deliverAllMutationsIfNecessary();
    static void setEditingBehavior(Evas_Object* ewkView, const char* editingBehavior);
    static String markerTextForListItem(JSContextRef, JSValueRef nodeObject);
    static void setInteractiveFormValidationEnabled(Evas_Object* ewkView, bool enabled);
    static JSValueRef computedStyleIncludingVisitedInfo(JSContextRef, JSValueRef);
    static void setAuthorAndUserStylesEnabled(Evas_Object* ewkView, bool);
};

#endif // DumpRenderTreeSupportEfl_h
