/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#import "KWQSlot.h"

#import <kxmlcore/Assertions.h>

#import "dom_docimpl.h"
#import "khtml_part.h"
#import "kjs_window.h"
#import "render_form.h"
#import "render_layer.h"
#import "xmlhttprequest.h"
#ifdef SVG_SUPPORT
#define id ID_HACK
#import "ksvg2/misc/KSVGTimeScheduler.h"
#undef id
#endif

using DOM::DocumentImpl;
using khtml::CachedObject;
using khtml::DocLoader;
using khtml::Loader;
using khtml::RenderFileButton;
using khtml::RenderFormElement;
using khtml::RenderLineEdit;
using khtml::RenderSelect;
using khtml::RenderSlider;
using khtml::RenderTextArea;
using khtml::RenderWidget;
using khtml::RenderScrollMediator;
using KIO::Job;
using KJS::WindowQObject;
using KJS::XMLHttpRequestQObject;
#ifdef SVG_SUPPORT
using KSVG::TimeScheduler;
#endif

enum FunctionNumber {
    signalFinishedParsing,
    slotChildCompleted,
    slotChildCompletedWithBool,
    slotChildStarted,
    slotClicked,
    slotEndLifeSupport,
    slotFinishedParsing,
    slotLoaderRequestDone,
    slotLoaderRequestStarted,
    slotParentCompleted,
    slotParentDestroyed,
    slotPerformSearch,
    slotRedirect,
    slotReturnPressed,
    slotSelected,
    slotSelectionChanged,
    slotSliderValueChanged,
    slotStateChanged,
    slotSubmitFormAgain,
    slotTextChanged,
    slotTextChangedWithString,
    slotValueChanged,
    slotWidgetDestructed,
    slotData_Loader,
    slotData_XMLHttpRequest,
    slotRedirection_KHTMLPart,
    slotRedirection_XMLHttpRequest,
    slotFinished_KHTMLPart,
    slotFinished_Loader,
    slotFinished_XMLHttpRequest,
    slotReceivedResponse,
#ifdef SVG_SUPPORT
	slotTimerNotify,
#endif
};

KWQSlot::KWQSlot(QObject *object, const char *member)
{
    #define CASE(function, parameters, type) \
        if (KWQNamesMatch(member, "SLOT:" #function #parameters)) { \
            m_function = function; \
        } else
    
    CASE(slotClicked, (), RenderFormElement)
    CASE(slotChildCompleted, (), KHTMLPart)
    CASE(slotChildStarted, (KIO::Job *), KHTMLPart)
    CASE(slotEndLifeSupport, (), KHTMLPart)
    CASE(slotFinishedParsing, (), KHTMLPart)
    CASE(slotLoaderRequestDone, (khtml::DocLoader *, khtml::CachedObject *), KHTMLPart)
    CASE(slotLoaderRequestStarted, (khtml::DocLoader *, khtml::CachedObject *), KHTMLPart)
    CASE(slotParentCompleted, (), KHTMLPart)
    CASE(slotPerformSearch, (), RenderLineEdit)
    CASE(slotRedirect, (), KHTMLPart)
    CASE(slotReturnPressed, (), RenderLineEdit)
    CASE(slotSelected, (int), RenderSelect)
    CASE(slotSelectionChanged, (), RenderFormElement)
    CASE(slotSliderValueChanged, (), RenderSlider)
    CASE(slotTextChanged, (), RenderTextArea)
    CASE(slotValueChanged, (int), RenderScrollMediator)
    CASE(slotWidgetDestructed, (), RenderWidget)
#ifdef SVG_SUPPORT
	CASE(slotTimerNotify, (), TimeScheduler)
#endif
       
    #undef CASE

    if (KWQNamesMatch(member, SIGNAL(finishedParsing()))) {
        m_function = signalFinishedParsing;
    } else if (KWQNamesMatch(member, SLOT(slotChildCompleted(bool)))) {
        m_function = slotChildCompletedWithBool;
    } else if (KWQNamesMatch(member, SLOT(parentDestroyed()))) {
        m_function = slotParentDestroyed;
    } else if (KWQNamesMatch(member, SLOT(submitFormAgain()))) {
        m_function = slotSubmitFormAgain;
    } else if (KWQNamesMatch(member, SLOT(slotTextChanged(const QString &)))) {
        m_function = slotTextChangedWithString;
    } else if (KWQNamesMatch(member, SLOT(slotData(KIO::Job *, const char *, int)))) {
	if (object->isKHTMLLoader()) {
	    m_function = slotData_Loader;
	} else {
	    m_function = slotData_XMLHttpRequest;
	}
    } else if (KWQNamesMatch(member, SLOT(slotRedirection(KIO::Job *, const KURL&)))) {
	if (object->isKHTMLPart()) {
	    m_function = slotRedirection_KHTMLPart;
	} else {
	    m_function = slotRedirection_XMLHttpRequest;
	}
    } else if (KWQNamesMatch(member, SLOT(slotFinished(KIO::Job *, NSData *)))) {
	m_function = slotFinished_Loader;        
    } else if (KWQNamesMatch(member, SLOT(slotFinished(KIO::Job *)))) {
	if (object->isKHTMLPart()) {
	    m_function = slotFinished_KHTMLPart;
	} else {
	    m_function = slotFinished_XMLHttpRequest;
	}
    } else if (KWQNamesMatch(member, SLOT(slotReceivedResponse(KIO::Job *, NSURLResponse *)))) {
	m_function = slotReceivedResponse;
    } else {
        ERROR("trying to create a slot for unknown member %s", member);
        return;
    }
    
    m_object = object;
}
    
void KWQSlot::call() const
{
    if (m_object.isNull()) {
        return;
    }
    
    #define CASE(member, type, function) \
        case member: \
            static_cast<type *>(m_object.pointer())->function(); \
            return;
    
    switch (m_function) {
        CASE(signalFinishedParsing, DocumentImpl, m_finishedParsing.call)
        CASE(slotChildCompleted, KHTMLPart, slotChildCompleted)
        CASE(slotClicked, RenderFormElement, slotClicked)
        CASE(slotEndLifeSupport, KHTMLPart, slotEndLifeSupport)
        CASE(slotFinishedParsing, KHTMLPart, slotFinishedParsing)
        CASE(slotParentCompleted, KHTMLPart, slotParentCompleted)
        CASE(slotParentDestroyed, WindowQObject, parentDestroyed)
        CASE(slotPerformSearch, RenderLineEdit, slotPerformSearch)
        CASE(slotRedirect, KHTMLPart, slotRedirect)
        CASE(slotReturnPressed, RenderLineEdit, slotReturnPressed)
        CASE(slotSelectionChanged, RenderFormElement, slotSelectionChanged)
        CASE(slotSliderValueChanged, RenderSlider, slotSliderValueChanged)
        CASE(slotSubmitFormAgain, KHTMLPart, submitFormAgain)
        CASE(slotTextChanged, RenderTextArea, slotTextChanged)
        CASE(slotWidgetDestructed, RenderWidget, slotWidgetDestructed)
#ifdef SVG_SUPPORT
		CASE(slotTimerNotify, TimeScheduler, slotTimerNotify)
#endif
    }
    
    #undef CASE
}

void KWQSlot::call(bool b) const
{
    if (m_object.isNull()) {
        return;
    }
    
    #define CASE(member, type, function) \
        case member: \
            static_cast<type *>(m_object.pointer())->function(b); \
            return;
    
    switch (m_function) {
        CASE(slotChildCompletedWithBool, KHTMLPart, slotChildCompleted)
    }
    
    #undef CASE
    
    call();
}

void KWQSlot::call(int i) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotSelected:
            static_cast<RenderSelect *>(m_object.pointer())->slotSelected(i);
            return;
        case slotValueChanged:
            static_cast<RenderScrollMediator *>(m_object.pointer())->slotValueChanged(i);
            return;
    }
    
    call();
}

void KWQSlot::call(const QString &string) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotTextChangedWithString:
            static_cast<RenderFormElement *>(m_object.pointer())->slotTextChanged(string);
	    return;
    }
    
    call();
}

void KWQSlot::call(Job *job) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotChildStarted:
            static_cast<KHTMLPart *>(m_object.pointer())->slotChildStarted(job);
            return;
        case slotFinished_KHTMLPart:
            static_cast<KHTMLPart *>(m_object.pointer())->slotFinished(job);
            return;
        case slotFinished_XMLHttpRequest:
            static_cast<XMLHttpRequestQObject *>(m_object.pointer())->slotFinished(job);
            return;
    }
    
    call();
}

void KWQSlot::call(Job *job, const char *data, int size) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotData_Loader:
	    static_cast<Loader *>(m_object.pointer())->slotData(job, data, size);
	    return;
        case slotData_XMLHttpRequest:
	    static_cast<XMLHttpRequestQObject *>(m_object.pointer())->slotData(job, data, size);
	    return;
    }

    call();
}

void KWQSlot::call(Job *job, const KURL &url) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotRedirection_KHTMLPart:
	    static_cast<KHTMLPart *>(m_object.pointer())->slotRedirection(job, url);
	    return;
        case slotRedirection_XMLHttpRequest:
	    static_cast<XMLHttpRequestQObject *>(m_object.pointer())->slotRedirection(job, url);
	    return;
    }

    call();
}

void KWQSlot::callWithData(KIO::Job *job, NSData *allData) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotFinished_Loader:
            static_cast<Loader *>(m_object.pointer())->slotFinished(job, allData);
            return;
    }
    
    call(job);
}

void KWQSlot::callWithResponse(KIO::Job *job, NSURLResponse *response) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotReceivedResponse:
	    static_cast<Loader *>(m_object.pointer())->slotReceivedResponse(job, response);
	    return;
    }
    
    call();
}

void KWQSlot::call(DocLoader *loader, CachedObject *cachedObject) const
{
    if (m_object.isNull()) {
        return;
    }
    
    switch (m_function) {
        case slotLoaderRequestDone:
            static_cast<KHTMLPart *>(m_object.pointer())->slotLoaderRequestDone(loader, cachedObject);
            return;
        case slotLoaderRequestStarted:
            static_cast<KHTMLPart *>(m_object.pointer())->slotLoaderRequestStarted(loader, cachedObject);
            return;
    }
    
    call();
}

bool operator==(const KWQSlot &a, const KWQSlot &b)
{
    return a.m_object == b.m_object && (a.m_object == 0 || a.m_function == b.m_function);
}

bool KWQNamesMatch(const char *a, const char *b)
{
    char ca = *a;
    char cb = *b;
    
    for (;;) {
        while (ca == ' ') {
            ca = *++a;
        }
        while (cb == ' ') {
            cb = *++b;
        }
        
        if (ca != cb) {
            return false;
        }
        if (ca == 0) {
            return true;
        }
        
        ca = *++a;
        cb = *++b;
    }
}
