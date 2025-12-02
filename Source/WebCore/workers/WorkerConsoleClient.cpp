/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WorkerConsoleClient.h"

#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorCanvas.h"
#include "InspectorInstrumentation.h"
#include "IntRect.h"
#include "JSImageBitmap.h"
#include "JSImageBitmapRenderingContext.h"
#include "JSImageData.h"
#include "WorkerGlobalScope.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "JSOffscreenCanvas.h"
#include "JSOffscreenCanvasRenderingContext2D.h"
#include "OffscreenCanvas.h"
#include "OffscreenCanvasRenderingContext2D.h"
#endif

#if ENABLE(WEBGL)
#include "JSWebGL2RenderingContext.h"
#include "JSWebGLRenderingContext.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContext.h"
#endif

namespace WebCore {
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WorkerConsoleClient);

WorkerConsoleClient::WorkerConsoleClient(WorkerOrWorkletGlobalScope& globalScope)
    : m_globalScope(globalScope)
{
}

WorkerConsoleClient::~WorkerConsoleClient() = default;

void WorkerConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* exec, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    arguments->getFirstArgumentAsString(messageText);
    auto message = makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, WTFMove(arguments), exec);
    Ref { m_globalScope.get() }->addConsoleMessage(WTFMove(message));
}

void WorkerConsoleClient::count(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::consoleCount(*worker, exec, label);
}

void WorkerConsoleClient::countReset(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::consoleCountReset(*worker, exec, label);
}

void WorkerConsoleClient::time(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::startConsoleTiming(*worker, exec, label);
}

void WorkerConsoleClient::timeLog(JSC::JSGlobalObject* exec, const String& label, Ref<ScriptArguments>&& arguments)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::logConsoleTiming(*worker, exec, label, WTFMove(arguments));
}

void WorkerConsoleClient::timeEnd(JSC::JSGlobalObject* exec, const String& label)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::stopConsoleTiming(*worker, exec, label);
}

void WorkerConsoleClient::profile(JSC::JSGlobalObject*, const String& title)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::startProfiling(*worker, title);
}

void WorkerConsoleClient::profileEnd(JSC::JSGlobalObject*, const String& title)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::stopProfiling(*worker, title);
}

void WorkerConsoleClient::takeHeapSnapshot(JSC::JSGlobalObject*, const String& title)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::takeHeapSnapshot(*worker, title);
}

void WorkerConsoleClient::timeStamp(JSC::JSGlobalObject*, Ref<ScriptArguments>&& arguments)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (RefPtr worker = dynamicDowncast<WorkerGlobalScope>(m_globalScope.get()))
        InspectorInstrumentation::consoleTimeStamp(*worker, WTFMove(arguments));
}

static JSC::JSObject* objectArgumentAt(ScriptArguments& arguments, unsigned index)
{
    return arguments.argumentCount() > index ? arguments.argumentAt(index).getObject() : nullptr;
}

static RefPtr<CanvasRenderingContext> canvasRenderingContext(JSC::VM& vm, JSC::JSValue target)
{
#if ENABLE(OFFSCREEN_CANVAS)
    if (RefPtr canvas = JSOffscreenCanvas::toWrapped(vm, target))
        return canvas->renderingContext();
    if (RefPtr context = JSOffscreenCanvasRenderingContext2D::toWrapped(vm, target))
        return context;
#endif
    if (RefPtr context = JSImageBitmapRenderingContext::toWrapped(vm, target))
        return context;
#if ENABLE(WEBGL)
    if (RefPtr context = JSWebGLRenderingContext::toWrapped(vm, target))
        return context;
    if (RefPtr context = JSWebGL2RenderingContext::toWrapped(vm, target))
        return context;
#endif
    return nullptr;
}

void WorkerConsoleClient::record(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    if (!InspectorInstrumentation::hasFrontends()) [[likely]]
        return;

    if (auto* target = objectArgumentAt(arguments, 0)) {
        if (RefPtr context = canvasRenderingContext(lexicalGlobalObject->vm(), target))
            InspectorInstrumentation::consoleStartRecordingCanvas(*context, *lexicalGlobalObject, objectArgumentAt(arguments, 1));
    }
}

void WorkerConsoleClient::recordEnd(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    if (!InspectorInstrumentation::hasFrontends()) [[likely]]
        return;

    if (auto* target = objectArgumentAt(arguments, 0)) {
        if (RefPtr context = canvasRenderingContext(lexicalGlobalObject->vm(), target))
            InspectorInstrumentation::consoleStopRecordingCanvas(*context);
    }
}

void WorkerConsoleClient::screenshot(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    // FIXME: <https://webkit.org/b/217724> Add support for WorkletGlobalScope.
    if (!is<WorkerGlobalScope>(m_globalScope))
        return;

    JSC::VM& vm = lexicalGlobalObject->vm();
    String dataURL;
    JSC::JSValue target;

    auto timestamp = WallTime::now();

    if (arguments->argumentCount()) {
        auto possibleTarget = arguments->argumentAt(0);

        if (RefPtr imageData = JSImageData::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
                if (RefPtr imageBuffer = ImageBuffer::create(imageData->size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, /* scale */ 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8)) {
                    imageBuffer->putPixelBuffer(imageData->byteArrayPixelBuffer().get(), IntRect(IntPoint(), imageData->size()));
                    dataURL = imageBuffer->toDataURL("image/png"_s, /* quality */ std::nullopt, PreserveResolution::Yes);
                }
            }
        } else if (RefPtr imageBitmap = JSImageBitmap::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
                if (RefPtr imageBuffer = imageBitmap->buffer())
                    dataURL = imageBuffer->toDataURL("image/png"_s, /* quality */ std::nullopt, PreserveResolution::Yes);
            }
        } else if (RefPtr context = canvasRenderingContext(vm, possibleTarget)) {
            target = possibleTarget;
            if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
                if (auto result = InspectorCanvas::getContentAsDataURL(*context))
                    dataURL = result.value();
            }
        } else {
            String base64;
            if (possibleTarget.getString(lexicalGlobalObject, base64) && base64.length() > 5 && startsWithLettersIgnoringASCIICase(base64, "data:"_s)) {
                target = possibleTarget;
                dataURL = base64;
            }
        }
    }

    if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
        if (dataURL.isEmpty()) {
            InspectorInstrumentation::addMessageToConsole(protectedGlobalScope(), makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Image, MessageLevel::Error, "Could not capture screenshot"_s, WTFMove(arguments)));
            return;
        }
    }

    Vector<JSC::Strong<JSC::Unknown>> adjustedArguments;
    adjustedArguments.reserveInitialCapacity(arguments->argumentCount() + !target);
    adjustedArguments.append({ vm, target ? target : JSC::jsNontrivialString(vm, "Viewport"_s) });
    for (size_t i = (!target ? 0 : 1); i < arguments->argumentCount(); ++i)
        adjustedArguments.append({ vm, arguments->argumentAt(i) });
    InspectorInstrumentation::addMessageToConsole(protectedGlobalScope(), makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Image, MessageLevel::Log, dataURL, ScriptArguments::create(lexicalGlobalObject, WTFMove(adjustedArguments)), lexicalGlobalObject, /* requestIdentifier */ 0, timestamp));
}

Ref<WorkerOrWorkletGlobalScope> WorkerConsoleClient::protectedGlobalScope()
{
    return m_globalScope.get();
}

} // namespace WebCore
