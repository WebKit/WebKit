/*
 * Copyright (C) 2016 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitWebViewSessionState.h"

#include "WebKitWebViewSessionStatePrivate.h"
#include <WebCore/BackForwardItemIdentifier.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

using namespace WebKit;

struct _WebKitWebViewSessionState {
    _WebKitWebViewSessionState(SessionState&& state)
        : sessionState(WTFMove(state))
        , referenceCount(1)
    {
    }

    SessionState sessionState;
    int referenceCount;
};

G_DEFINE_BOXED_TYPE(WebKitWebViewSessionState, webkit_web_view_session_state, webkit_web_view_session_state_ref, webkit_web_view_session_state_unref)

// Version information:
//  - Version 2: removed backforward list item identifier since it's always regenerated.
//  - Version 1: initial version.
static const guint16 g_sessionStateVersion = 2;
#define HTTP_BODY_ELEMENT_TYPE_STRING_V1 "(uaysxmxmds)"
#define HTTP_BODY_ELEMENT_FORMAT_STRING_V1 "(uay&sxmxmd&s)"
#define HTTP_BODY_TYPE_STRING_V1 "m(sa" HTTP_BODY_ELEMENT_TYPE_STRING_V1 ")"
#define HTTP_BODY_FORMAT_STRING_V1 "m(&sa" HTTP_BODY_ELEMENT_TYPE_STRING_V1 ")"
#define FRAME_STATE_TYPE_STRING_V1  "(ssssasmayxx(ii)d" HTTP_BODY_TYPE_STRING_V1 "av)"
#define FRAME_STATE_FORMAT_STRING_V1  "(&s&s&s&sasmayxx(ii)d@" HTTP_BODY_TYPE_STRING_V1 "av)"
#define BACK_FORWARD_LIST_ITEM_TYPE_STRING_V1  "(ts" FRAME_STATE_TYPE_STRING_V1 "u)"
#define BACK_FORWARD_LIST_ITEM_TYPE_STRING_V2  "(s" FRAME_STATE_TYPE_STRING_V1 "u)"
#define BACK_FORWARD_LIST_ITEM_FORMAT_STRING_V1  "(t&s@" FRAME_STATE_TYPE_STRING_V1 "u)"
#define BACK_FORWARD_LIST_ITEM_FORMAT_STRING_V2  "(&s@" FRAME_STATE_TYPE_STRING_V1 "u)"
#define SESSION_STATE_TYPE_STRING_V1 "(qa" BACK_FORWARD_LIST_ITEM_TYPE_STRING_V1 "mu)"
#define SESSION_STATE_TYPE_STRING_V2 "(qa" BACK_FORWARD_LIST_ITEM_TYPE_STRING_V2 "mu)"

// Use our own enum types to ensure the serialized format even if the core enums change.
enum ExternalURLsPolicy {
    Allow,
    AllowExternalSchemes,
    NotAllow
};

static inline unsigned toExternalURLsPolicy(WebCore::ShouldOpenExternalURLsPolicy policy)
{
    switch (policy) {
    case WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow:
        return ExternalURLsPolicy::Allow;
    case WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes:
        return ExternalURLsPolicy::AllowExternalSchemes;
    case WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow:
        return ExternalURLsPolicy::NotAllow;
    }

    return ExternalURLsPolicy::NotAllow;
}

static inline WebCore::ShouldOpenExternalURLsPolicy toWebCoreExternalURLsPolicy(unsigned policy)
{
    switch (policy) {
    case ExternalURLsPolicy::Allow:
        return WebCore::ShouldOpenExternalURLsPolicy::ShouldAllow;
    case ExternalURLsPolicy::AllowExternalSchemes:
        return WebCore::ShouldOpenExternalURLsPolicy::ShouldAllowExternalSchemes;
    case ExternalURLsPolicy::NotAllow:
        return WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow;
    }

    return WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow;
}

enum HTMLBodyElementType {
    Data,
    File,
    Blob
};

static inline unsigned toHTMLBodyElementType(HTTPBody::Element::Type type)
{
    switch (type) {
    case HTTPBody::Element::Type::Data:
        return HTMLBodyElementType::Data;
    case HTTPBody::Element::Type::File:
        return HTMLBodyElementType::File;
    case HTTPBody::Element::Type::Blob:
        return HTMLBodyElementType::Blob;
    }

    return HTMLBodyElementType::Data;
}

static inline HTTPBody::Element::Type toHTTPBodyElementType(unsigned type)
{
    switch (type) {
    case HTMLBodyElementType::Data:
        return HTTPBody::Element::Type::Data;
    case HTMLBodyElementType::File:
        return HTTPBody::Element::Type::File;
    case HTMLBodyElementType::Blob:
        return HTTPBody::Element::Type::Blob;
    }

    return HTTPBody::Element::Type::Data;
}

static inline void encodeHTTPBody(GVariantBuilder* sessionBuilder, const HTTPBody& httpBody)
{
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("(sa" HTTP_BODY_ELEMENT_TYPE_STRING_V1 ")"));
    g_variant_builder_add(sessionBuilder, "s", httpBody.contentType.utf8().data());
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("a" HTTP_BODY_ELEMENT_TYPE_STRING_V1));
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE(HTTP_BODY_ELEMENT_TYPE_STRING_V1));
    for (const auto& element : httpBody.elements) {
        g_variant_builder_add(sessionBuilder, "u", toHTMLBodyElementType(element.type));
        g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("ay"));
        for (auto item : element.data)
            g_variant_builder_add(sessionBuilder, "y", item);
        g_variant_builder_close(sessionBuilder);
        g_variant_builder_add(sessionBuilder, "s", element.filePath.utf8().data());
        g_variant_builder_add(sessionBuilder, "x", element.fileStart);
        if (element.fileLength)
            g_variant_builder_add(sessionBuilder, "mx", TRUE, element.fileLength.value());
        else
            g_variant_builder_add(sessionBuilder, "mx", FALSE);
        if (element.expectedFileModificationTime)
            g_variant_builder_add(sessionBuilder, "md", TRUE, element.expectedFileModificationTime.value());
        else
            g_variant_builder_add(sessionBuilder, "md", FALSE);
        g_variant_builder_add(sessionBuilder, "s", element.blobURLString.utf8().data());
    }
    g_variant_builder_close(sessionBuilder);
    g_variant_builder_close(sessionBuilder);
    g_variant_builder_close(sessionBuilder);
}

static inline void encodeFrameState(GVariantBuilder* sessionBuilder, const FrameState& frameState)
{
    g_variant_builder_add(sessionBuilder, "s", frameState.urlString.utf8().data());
    g_variant_builder_add(sessionBuilder, "s", frameState.originalURLString.utf8().data());
    g_variant_builder_add(sessionBuilder, "s", frameState.referrer.utf8().data());
    g_variant_builder_add(sessionBuilder, "s", frameState.target.utf8().data());
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("as"));
    for (const auto& state : frameState.documentState)
        g_variant_builder_add(sessionBuilder, "s", state.utf8().data());
    g_variant_builder_close(sessionBuilder);
    if (!frameState.stateObjectData)
        g_variant_builder_add(sessionBuilder, "may", FALSE);
    else {
        g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("may"));
        g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("ay"));
        for (auto item : frameState.stateObjectData.value())
            g_variant_builder_add(sessionBuilder, "y", item);
        g_variant_builder_close(sessionBuilder);
        g_variant_builder_close(sessionBuilder);
    }
    g_variant_builder_add(sessionBuilder, "x", frameState.documentSequenceNumber);
    g_variant_builder_add(sessionBuilder, "x", frameState.itemSequenceNumber);
    g_variant_builder_add(sessionBuilder, "(ii)", frameState.scrollPosition.x(), frameState.scrollPosition.y());
    g_variant_builder_add(sessionBuilder, "d", static_cast<gdouble>(frameState.pageScaleFactor));
    if (!frameState.httpBody)
        g_variant_builder_add(sessionBuilder, HTTP_BODY_TYPE_STRING_V1, FALSE);
    else {
        g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE(HTTP_BODY_TYPE_STRING_V1));
        encodeHTTPBody(sessionBuilder, frameState.httpBody.value());
        g_variant_builder_close(sessionBuilder);
    }
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("av"));
    for (const auto& child : frameState.children) {
        GVariantBuilder frameStateBuilder;
        g_variant_builder_init(&frameStateBuilder, G_VARIANT_TYPE(FRAME_STATE_TYPE_STRING_V1));
        encodeFrameState(&frameStateBuilder, child);
        g_variant_builder_add(sessionBuilder, "v", g_variant_builder_end(&frameStateBuilder));
    }
    g_variant_builder_close(sessionBuilder);
}

static inline void encodePageState(GVariantBuilder* sessionBuilder, const PageState& pageState)
{
    g_variant_builder_add(sessionBuilder, "s", pageState.title.utf8().data());
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE(FRAME_STATE_TYPE_STRING_V1));
    encodeFrameState(sessionBuilder, pageState.mainFrameState);
    g_variant_builder_close(sessionBuilder);
    g_variant_builder_add(sessionBuilder, "u", toExternalURLsPolicy(pageState.shouldOpenExternalURLsPolicy));
}

static inline void encodeBackForwardListItemState(GVariantBuilder* sessionBuilder, const BackForwardListItemState& item)
{
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE(BACK_FORWARD_LIST_ITEM_TYPE_STRING_V2));
    encodePageState(sessionBuilder, item.pageState);
    g_variant_builder_close(sessionBuilder);
}

static inline void encodeBackForwardListState(GVariantBuilder* sessionBuilder, const BackForwardListState& backForwardListState)
{
    g_variant_builder_open(sessionBuilder, G_VARIANT_TYPE("a" BACK_FORWARD_LIST_ITEM_TYPE_STRING_V2));
    for (const auto& item : backForwardListState.items)
        encodeBackForwardListItemState(sessionBuilder, item);
    g_variant_builder_close(sessionBuilder);

    if (backForwardListState.currentIndex)
        g_variant_builder_add(sessionBuilder, "mu", TRUE, backForwardListState.currentIndex.value());
    else
        g_variant_builder_add(sessionBuilder, "mu", FALSE);
}

static GBytes* encodeSessionState(const SessionState& sessionState)
{
    GVariantBuilder sessionBuilder;
    g_variant_builder_init(&sessionBuilder, G_VARIANT_TYPE(SESSION_STATE_TYPE_STRING_V2));
    g_variant_builder_add(&sessionBuilder, "q", g_sessionStateVersion);
    encodeBackForwardListState(&sessionBuilder, sessionState.backForwardListState);
    GRefPtr<GVariant> variant = g_variant_builder_end(&sessionBuilder);
    return g_variant_get_data_as_bytes(variant.get());
}

static inline bool decodeHTTPBody(GVariant* httpBodyVariant, HTTPBody& httpBody)
{
    gboolean hasHTTPBody;
    const char* contentType;
    GUniqueOutPtr<GVariantIter> elementsIter;
    g_variant_get(httpBodyVariant, HTTP_BODY_FORMAT_STRING_V1, &hasHTTPBody, &contentType, &elementsIter.outPtr());
    if (!hasHTTPBody)
        return false;
    httpBody.contentType = String::fromUTF8(contentType);
    gsize elementsLength = g_variant_iter_n_children(elementsIter.get());
    if (!elementsLength)
        return true;
    httpBody.elements.reserveInitialCapacity(elementsLength);
    unsigned type;
    GVariantIter* dataIter;
    const char* filePath;
    gint64 fileStart;
    gboolean hasFileLength;
    gint64 fileLength;
    gboolean hasFileModificationTime;
    gdouble fileModificationTime;
    const char* blobURLString;
    while (g_variant_iter_loop(elementsIter.get(), HTTP_BODY_ELEMENT_FORMAT_STRING_V1, &type, &dataIter, &filePath, &fileStart, &hasFileLength, &fileLength, &hasFileModificationTime, &fileModificationTime, &blobURLString)) {
        HTTPBody::Element element;
        element.type = toHTTPBodyElementType(type);
        if (gsize dataLength = g_variant_iter_n_children(dataIter)) {
            element.data.reserveInitialCapacity(dataLength);
            guchar dataValue;
            while (g_variant_iter_next(dataIter, "y", &dataValue))
                element.data.uncheckedAppend(dataValue);
        }
        element.filePath = String::fromUTF8(filePath);
        element.fileStart = fileStart;
        if (hasFileLength)
            element.fileLength = fileLength;
        if (hasFileModificationTime)
            element.expectedFileModificationTime = WallTime::fromRawSeconds(fileModificationTime);
        element.blobURLString = String::fromUTF8(blobURLString);

        httpBody.elements.uncheckedAppend(WTFMove(element));
    }

    return true;
}

static inline void decodeFrameState(GVariant* frameStateVariant, FrameState& frameState)
{
    const char* urlString;
    const char* originalURLString;
    const char* referrer;
    const char* target;
    GUniqueOutPtr<GVariantIter> documentStateIter;
    GUniqueOutPtr<GVariantIter> stateObjectDataIter;
    gint64 documentSequenceNumber;
    gint64 itemSequenceNumber;
    gint32 scrollPositionX, scrollPositionY;
    gdouble pageScaleFactor;
    GVariant* httpBodyVariant;
    GUniqueOutPtr<GVariantIter> childrenIter;
    g_variant_get(frameStateVariant, FRAME_STATE_FORMAT_STRING_V1, &urlString, &originalURLString, &referrer, &target,
        &documentStateIter.outPtr(), &stateObjectDataIter.outPtr(), &documentSequenceNumber, &itemSequenceNumber,
        &scrollPositionX, &scrollPositionY, &pageScaleFactor, &httpBodyVariant, &childrenIter.outPtr());
    frameState.urlString = String::fromUTF8(urlString);
    frameState.originalURLString = String::fromUTF8(originalURLString);
    // frameState.referrer must not be an empty string since we never want to
    // send an empty Referer header. Bug #159606.
    if (strlen(referrer))
        frameState.referrer = String::fromUTF8(referrer);
    frameState.target = String::fromUTF8(target);
    if (gsize documentStateLength = g_variant_iter_n_children(documentStateIter.get())) {
        frameState.documentState.reserveInitialCapacity(documentStateLength);
        const char* documentStateString;
        while (g_variant_iter_next(documentStateIter.get(), "&s", &documentStateString))
            frameState.documentState.uncheckedAppend(String::fromUTF8(documentStateString));
    }
    if (stateObjectDataIter) {
        Vector<uint8_t> stateObjectVector;
        if (gsize stateObjectDataLength = g_variant_iter_n_children(stateObjectDataIter.get())) {
            stateObjectVector.reserveInitialCapacity(stateObjectDataLength);
            guchar stateObjectDataValue;
            while (g_variant_iter_next(stateObjectDataIter.get(), "y", &stateObjectDataValue))
                stateObjectVector.uncheckedAppend(stateObjectDataValue);
        }
        frameState.stateObjectData = WTFMove(stateObjectVector);
    }
    frameState.documentSequenceNumber = documentSequenceNumber;
    frameState.itemSequenceNumber = itemSequenceNumber;
    frameState.scrollPosition.setX(scrollPositionX);
    frameState.scrollPosition.setY(scrollPositionY);
    frameState.pageScaleFactor = pageScaleFactor;
    HTTPBody httpBody;
    if (decodeHTTPBody(httpBodyVariant, httpBody))
        frameState.httpBody = WTFMove(httpBody);
    g_variant_unref(httpBodyVariant);
    while (GRefPtr<GVariant> child = adoptGRef(g_variant_iter_next_value(childrenIter.get()))) {
        FrameState childFrameState;
        GRefPtr<GVariant> childVariant = adoptGRef(g_variant_get_variant(child.get()));
        decodeFrameState(childVariant.get(), childFrameState);
        frameState.children.append(WTFMove(childFrameState));
    }
}

static inline void decodeBackForwardListItemStateV1(GVariantIter* backForwardListStateIter, BackForwardListState& backForwardListState)
{
    guint64 identifier;
    const char* title;
    GVariant* frameStateVariant;
    unsigned shouldOpenExternalURLsPolicy;
    while (g_variant_iter_loop(backForwardListStateIter, BACK_FORWARD_LIST_ITEM_FORMAT_STRING_V1, &identifier, &title, &frameStateVariant, &shouldOpenExternalURLsPolicy)) {
        BackForwardListItemState state;
        state.pageState.title = String::fromUTF8(title);
        decodeFrameState(frameStateVariant, state.pageState.mainFrameState);
        state.pageState.shouldOpenExternalURLsPolicy = toWebCoreExternalURLsPolicy(shouldOpenExternalURLsPolicy);
        backForwardListState.items.uncheckedAppend(WTFMove(state));
    }
}

static inline void decodeBackForwardListItemState(GVariantIter* backForwardListStateIter, BackForwardListState& backForwardListState, guint16 version)
{
    gsize backForwardListStateLength = g_variant_iter_n_children(backForwardListStateIter);
    if (!backForwardListStateLength)
        return;

    backForwardListState.items.reserveInitialCapacity(backForwardListStateLength);
    if (version == 1) {
        decodeBackForwardListItemStateV1(backForwardListStateIter, backForwardListState);
        return;
    }

    const char* title;
    GVariant* frameStateVariant;
    unsigned shouldOpenExternalURLsPolicy;
    while (g_variant_iter_loop(backForwardListStateIter, BACK_FORWARD_LIST_ITEM_FORMAT_STRING_V2, &title, &frameStateVariant, &shouldOpenExternalURLsPolicy)) {
        BackForwardListItemState state;
        state.pageState.title = String::fromUTF8(title);
        decodeFrameState(frameStateVariant, state.pageState.mainFrameState);
        state.pageState.shouldOpenExternalURLsPolicy = toWebCoreExternalURLsPolicy(shouldOpenExternalURLsPolicy);
        backForwardListState.items.uncheckedAppend(WTFMove(state));
    }
}

static bool decodeSessionState(GBytes* data, SessionState& sessionState)
{
    static const char* sessionStateTypeStringVersions[] = {
        SESSION_STATE_TYPE_STRING_V2,
        SESSION_STATE_TYPE_STRING_V1,
        nullptr
    };
    const char* sessionStateTypeString = nullptr;
    GRefPtr<GVariant> variant;
    for (unsigned i = 0; sessionStateTypeStringVersions[i]; ++i) {
        sessionStateTypeString = sessionStateTypeStringVersions[i];
        variant = g_variant_new_from_bytes(G_VARIANT_TYPE(sessionStateTypeString), data, FALSE);
        if (g_variant_is_normal_form(variant.get()))
            break;
        variant = nullptr;
    }
    if (!variant)
        return false;

    guint16 version;
    GUniqueOutPtr<GVariantIter> backForwardListStateIter;
    gboolean hasCurrentIndex;
    guint32 currentIndex;
    g_variant_get(variant.get(), sessionStateTypeString, &version, &backForwardListStateIter.outPtr(), &hasCurrentIndex, &currentIndex);
    if (!version || version > g_sessionStateVersion)
        return false;

    decodeBackForwardListItemState(backForwardListStateIter.get(), sessionState.backForwardListState, version);

    if (hasCurrentIndex)
        sessionState.backForwardListState.currentIndex = std::min<uint32_t>(currentIndex, sessionState.backForwardListState.items.size() - 1);
    return true;
}

WebKitWebViewSessionState* webkitWebViewSessionStateCreate(SessionState&& sessionState)
{
    WebKitWebViewSessionState* state = static_cast<WebKitWebViewSessionState*>(fastMalloc(sizeof(WebKitWebViewSessionState)));
    new (state) WebKitWebViewSessionState(WTFMove(sessionState));
    return state;
}

const SessionState& webkitWebViewSessionStateGetSessionState(WebKitWebViewSessionState* state)
{
    return state->sessionState;
}

/**
 * webkit_web_view_session_state_new:
 * @data: a #GBytes
 *
 * Creates a new #WebKitWebViewSessionState from serialized data.
 *
 * Returns: (transfer full): a new #WebKitWebViewSessionState, or %NULL if @data doesn't contain a
 *     valid serialized #WebKitWebViewSessionState.
 *
 * Since: 2.12
 */
WebKitWebViewSessionState* webkit_web_view_session_state_new(GBytes* data)
{
    g_return_val_if_fail(data, nullptr);

    SessionState sessionState;
    if (!decodeSessionState(data, sessionState))
        return nullptr;
    return webkitWebViewSessionStateCreate(WTFMove(sessionState));
}

/**
 * webkit_web_view_session_state_ref:
 * @state: a #WebKitWebViewSessionState
 *
 * Atomically increments the reference count of @state by one. This
 * function is MT-safe and may be called from any thread.
 *
 * Returns: The passed in #WebKitWebViewSessionState
 *
 * Since: 2.12
 */
WebKitWebViewSessionState* webkit_web_view_session_state_ref(WebKitWebViewSessionState* state)
{
    g_return_val_if_fail(state, nullptr);
    g_atomic_int_inc(&state->referenceCount);
    return state;
}

/**
 * webkit_web_view_session_state_unref:
 * @state: a #WebKitWebViewSessionState
 *
 * Atomically decrements the reference count of @state by one. If the
 * reference count drops to 0, all memory allocated by the #WebKitWebViewSessionState is
 * released. This function is MT-safe and may be called from any thread.
 *
 * Since: 2.12
 */
void webkit_web_view_session_state_unref(WebKitWebViewSessionState* state)
{
    g_return_if_fail(state);
    if (g_atomic_int_dec_and_test(&state->referenceCount)) {
        state->~WebKitWebViewSessionState();
        fastFree(state);
    }
}

/**
 * webkit_web_view_session_state_serialize:
 * @state: a #WebKitWebViewSessionState
 *
 * Serializes a #WebKitWebViewSessionState.
 *
 * Returns: (transfer full): a #GBytes containing the @state serialized.
 *
 * Since: 2.12
 */
GBytes* webkit_web_view_session_state_serialize(WebKitWebViewSessionState* state)
{
    g_return_val_if_fail(state, nullptr);

    return encodeSessionState(state->sessionState);
}
