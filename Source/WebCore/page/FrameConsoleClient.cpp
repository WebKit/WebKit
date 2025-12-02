/*
 * Copyright (C) 2013-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FrameConsoleClient.h"

#include "CachedImage.h"
#include "CanvasRenderingContext2D.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ElementChildIteratorInlines.h"
#include "Frame.h"
#include "FrameSnapshotting.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "HTMLPictureElement.h"
#include "HTMLVideoElement.h"
#include "Image.h"
#include "ImageBitmap.h"
#include "ImageBitmapRenderingContext.h"
#include "ImageBuffer.h"
#include "ImageData.h"
#include "InspectorCanvas.h"
#include "InspectorInstrumentation.h"
#include "IntRect.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSDOMRectReadOnly.h"
#include "JSExecState.h"
#include "JSHTMLCanvasElement.h"
#include "JSImageBitmap.h"
#include "JSImageBitmapRenderingContext.h"
#include "JSImageData.h"
#include "JSNode.h"
#include "LocalFrame.h"
#include "Node.h"
#include "PageInspectorController.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include "StringCallback.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/Stopwatch.h>
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

#if ENABLE(WEBDRIVER_BIDI)
#include "AutomationInstrumentation.h"
#endif

namespace WebCore {
using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameConsoleClient);

FrameConsoleClient::FrameConsoleClient(LocalFrame& frame)
    : m_frame(frame)
{
}

FrameConsoleClient::~FrameConsoleClient() = default;

static int muteCount = 0;
static bool printExceptions = false;

bool FrameConsoleClient::shouldPrintExceptions()
{
    return printExceptions;
}

void FrameConsoleClient::setShouldPrintExceptions(bool print)
{
    printExceptions = print;
}

void FrameConsoleClient::mute()
{
    muteCount++;
}

void FrameConsoleClient::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

void FrameConsoleClient::logMessageToSystemConsole(const Inspector::ConsoleMessage& consoleMessage)
{
    if (consoleMessage.type() == MessageType::Image) {
        RefPtr arguments = consoleMessage.arguments();
        ConsoleClient::printConsoleMessageWithArguments(consoleMessage.source(), consoleMessage.type(), consoleMessage.level(), arguments->globalObject(), *arguments);
        return;
    }
    ConsoleClient::printConsoleMessage(consoleMessage.source(), consoleMessage.type(), consoleMessage.level(), consoleMessage.toString(), consoleMessage.url(), consoleMessage.line(), consoleMessage.column());
}

void FrameConsoleClient::addMessage(std::unique_ptr<Inspector::ConsoleMessage>&& consoleMessage)
{
    Ref frame = m_frame.get();
    RefPtr page = frame->page();
    if (!page)
        return;

    if (!page->usesEphemeralSession()) {
        String message;
        std::span<const String> additionalArguments;
        Vector<String> messageArgumentsVector;
        if (consoleMessage->type() == MessageType::Image) {
            messageArgumentsVector = RefPtr { consoleMessage->arguments() }->getArgumentsAsStrings();
            if (!messageArgumentsVector.isEmpty()) {
                message = messageArgumentsVector.first();
                additionalArguments = messageArgumentsVector.subspan(1);
            }
        } else
            message = consoleMessage->message();

        page->chrome().client().addMessageToConsole(consoleMessage->source(), consoleMessage->level(), message, consoleMessage->line(), consoleMessage->column(), consoleMessage->url());

        if (RefPtr consoleMessageListener = page->consoleMessageListenerForTesting())
            consoleMessageListener->invoke(message);

        if (page->settings().logsPageMessagesToSystemConsoleEnabled() || shouldPrintExceptions()) [[unlikely]]
            logMessageToSystemConsole(*consoleMessage);
    }

#if ENABLE(WEBDRIVER_BIDI)
    AutomationInstrumentation::addMessageToConsole(consoleMessage);
#endif
    InspectorInstrumentation::addMessageToConsole(frame.get(), WTFMove(consoleMessage));
}

void FrameConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
{
    String url;
    unsigned line = 0;
    unsigned column = 0;
    if (document)
        document->getParserLocation(url, line, column);

    addMessage(source, level, message, url, line, column, nullptr, JSExecState::currentState(), requestIdentifier);
}

void FrameConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, Ref<ScriptCallStack>&& callStack)
{
    addMessage(source, level, message, String(), 0, 0, WTFMove(callStack), 0);
}

void FrameConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& suggestedURL, unsigned suggestedLineNumber, unsigned suggestedColumnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::JSGlobalObject* lexicalGlobalObject, unsigned long requestIdentifier)
{
    if (muteCount && source != MessageSource::ConsoleAPI)
        return;

    std::unique_ptr<Inspector::ConsoleMessage> message;

    if (callStack)
        message = makeUnique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, callStack.releaseNonNull(), requestIdentifier);
    else
        message = makeUnique<Inspector::ConsoleMessage>(source, MessageType::Log, level, messageText, suggestedURL, suggestedLineNumber, suggestedColumnNumber, lexicalGlobalObject, requestIdentifier);

    addMessage(WTFMove(message));
}

void FrameConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* lexicalGlobalObject, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    std::span<const String> additionalArguments;
    Vector<String> messageArgumentsVector = arguments->getArgumentsAsStrings();
    if (!messageArgumentsVector.isEmpty()) {
        messageText = messageArgumentsVector.first();
        additionalArguments = messageArgumentsVector.subspan(1);
    }

    auto message = makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, arguments.copyRef(), lexicalGlobalObject);

    String url = message->url();
    unsigned lineNumber = message->line();
    unsigned columnNumber = message->column();

#if ENABLE(WEBDRIVER_BIDI)
    AutomationInstrumentation::addMessageToConsole(message);
#endif
    Ref frame = m_frame.get();
    InspectorInstrumentation::addMessageToConsole(frame.get(), WTFMove(message));

    RefPtr page = frame->page();
    if (!page)
        return;

    if (page->usesEphemeralSession())
        return;

    if (!messageArgumentsVector.isEmpty()) {
        page->chrome().client().addMessageToConsole(MessageSource::ConsoleAPI, level, messageText, lineNumber, columnNumber, url);

        if (RefPtr consoleMessageListener = page->consoleMessageListenerForTesting())
            consoleMessageListener->invoke(messageText);
    }

    if (page->settings().logsPageMessagesToSystemConsoleEnabled() || FrameConsoleClient::shouldPrintExceptions())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, lexicalGlobalObject, WTFMove(arguments));
}

void FrameConsoleClient::count(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::consoleCount(frame.get(), lexicalGlobalObject, label);
}

void FrameConsoleClient::countReset(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::consoleCountReset(frame.get(), lexicalGlobalObject, label);
}

void FrameConsoleClient::profile(JSC::JSGlobalObject*, const String& title)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::startProfiling(frame.get(), title);
}

void FrameConsoleClient::profileEnd(JSC::JSGlobalObject*, const String& title)
{
    Ref frame = m_frame.get();
    // FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
    InspectorInstrumentation::stopProfiling(frame.get(), title);
}

void FrameConsoleClient::takeHeapSnapshot(JSC::JSGlobalObject*, const String& title)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::takeHeapSnapshot(frame.get(), title);
}

void FrameConsoleClient::time(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::startConsoleTiming(frame.get(), lexicalGlobalObject, label);
}

void FrameConsoleClient::timeLog(JSC::JSGlobalObject* lexicalGlobalObject, const String& label, Ref<ScriptArguments>&& arguments)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::logConsoleTiming(frame.get(), lexicalGlobalObject, label, WTFMove(arguments));
}

void FrameConsoleClient::timeEnd(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::stopConsoleTiming(frame.get(), lexicalGlobalObject, label);
}

void FrameConsoleClient::timeStamp(JSC::JSGlobalObject*, Ref<ScriptArguments>&& arguments)
{
    Ref frame = m_frame.get();
    InspectorInstrumentation::consoleTimeStamp(frame.get(), WTFMove(arguments));
}

static JSC::JSObject* objectArgumentAt(ScriptArguments& arguments, unsigned index)
{
    return arguments.argumentCount() > index ? arguments.argumentAt(index).getObject() : nullptr;
}

static RefPtr<CanvasRenderingContext> canvasRenderingContext(JSC::VM& vm, JSC::JSValue target)
{
    if (RefPtr canvas = JSHTMLCanvasElement::toWrapped(vm, target))
        return canvas->renderingContext();
#if ENABLE(OFFSCREEN_CANVAS)
    if (RefPtr canvas = JSOffscreenCanvas::toWrapped(vm, target))
        return canvas->renderingContext();
    if (RefPtr context = JSOffscreenCanvasRenderingContext2D::toWrapped(vm, target))
        return context;
#endif
    if (RefPtr context = JSCanvasRenderingContext2D::toWrapped(vm, target))
        return context;
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

void FrameConsoleClient::record(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    if (!InspectorInstrumentation::hasFrontends()) [[likely]]
        return;

    if (auto* target = objectArgumentAt(arguments, 0)) {
        if (RefPtr context = canvasRenderingContext(lexicalGlobalObject->vm(), target))
            InspectorInstrumentation::consoleStartRecordingCanvas(*context, *lexicalGlobalObject, objectArgumentAt(arguments, 1));
    }
}

void FrameConsoleClient::recordEnd(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    if (!InspectorInstrumentation::hasFrontends()) [[likely]]
        return;

    if (auto* target = objectArgumentAt(arguments, 0)) {
        if (RefPtr context = canvasRenderingContext(lexicalGlobalObject->vm(), target))
            InspectorInstrumentation::consoleStopRecordingCanvas(*context);
    }
}

void FrameConsoleClient::screenshot(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    String dataURL;
    JSC::JSValue target;

    auto timestamp = WallTime::now();

    if (arguments->argumentCount()) {
        auto possibleTarget = arguments->argumentAt(0);

        if (RefPtr node = JSNode::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
                RefPtr<ImageBuffer> snapshot;

                // Only try to do something special for subclasses of Node if they're detached from the DOM tree.
                if (!node->document().contains(*node)) {
                    auto snapshotImageElement = [&snapshot] (HTMLImageElement& imageElement) {
                        if (auto* cachedImage = imageElement.cachedImage()) {
                            if (RefPtr image = cachedImage->image(); image && image != &Image::nullImage()) {
                                snapshot = ImageBuffer::create(image->size(), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, /* scale */ 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
                                snapshot->context().drawImage(*image, FloatPoint(0, 0));
                            }
                        }
                    };

                    if (RefPtr imgElement = dynamicDowncast<HTMLImageElement>(node))
                        snapshotImageElement(*imgElement);
                    else if (RefPtr pictureElement = dynamicDowncast<HTMLPictureElement>(node)) {
                        if (RefPtr firstImage = childrenOfType<HTMLImageElement>(*pictureElement).first())
                            snapshotImageElement(*firstImage);
                    }
#if ENABLE(VIDEO)
                    else if (RefPtr videoElement = dynamicDowncast<HTMLVideoElement>(node)) {
                        unsigned videoWidth = videoElement->videoWidth();
                        unsigned videoHeight = videoElement->videoHeight();
                        snapshot = ImageBuffer::create(FloatSize(videoWidth, videoHeight), RenderingMode::Unaccelerated, RenderingPurpose::Unspecified, /* scale */ 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
                        videoElement->paintCurrentFrameInContext(snapshot->context(), FloatRect(0, 0, videoWidth, videoHeight));
                    }
#endif
                    else if (RefPtr canvasElement = dynamicDowncast<HTMLCanvasElement>(node)) {
                        if (RefPtr canvasRenderingContext = canvasElement->renderingContext()) {
                            if (auto result = InspectorCanvas::getContentAsDataURL(*canvasRenderingContext))
                                dataURL = result.value();
                        }
                    }
                }

                if (dataURL.isEmpty()) {
                    if (!snapshot) {
                        Ref frame = m_frame.get();
                        if (RefPtr localMainFrame = frame->localMainFrame())
                            snapshot = WebCore::snapshotNode(*localMainFrame, *node, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });
                    }

                    if (snapshot)
                        dataURL = snapshot->toDataURL("image/png"_s, /* quality */ std::nullopt, PreserveResolution::Yes);
                }
            }
        } else if (RefPtr imageData = JSImageData::toWrapped(vm, possibleTarget)) {
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
        } else if (RefPtr rect = JSDOMRectReadOnly::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (InspectorInstrumentation::hasFrontends()) [[unlikely]] {
                Ref frame = m_frame.get();
                if (RefPtr localMainFrame = frame->localMainFrame()) {
                    if (RefPtr snapshot = WebCore::snapshotFrameRect(*localMainFrame, enclosingIntRect(rect->toFloatRect()), { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }))
                        dataURL = snapshot->toDataURL("image/png"_s, /* quality */ std::nullopt, PreserveResolution::Yes);
                }
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
        if (!target) {
            Ref frame = m_frame.get();
            if (RefPtr localMainFrame = frame->localMainFrame()) {
                // If no target is provided, capture an image of the viewport.
                auto viewportRect = localMainFrame->protectedView()->unobscuredContentRect();
                if (RefPtr snapshot = WebCore::snapshotFrameRect(*localMainFrame, viewportRect, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }))
                    dataURL = snapshot->toDataURL("image/png"_s, /* quality */ std::nullopt, PreserveResolution::Yes);
            }
        }

        if (dataURL.isEmpty()) {
            addMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Image, MessageLevel::Error, "Could not capture screenshot"_s, WTFMove(arguments)));
            return;
        }
    }

    Vector<JSC::Strong<JSC::Unknown>> adjustedArguments;
    adjustedArguments.reserveInitialCapacity(arguments->argumentCount() + !target);
    adjustedArguments.append({ vm, target ? target : JSC::jsNontrivialString(vm, "Viewport"_s) });
    for (size_t i = (!target ? 0 : 1); i < arguments->argumentCount(); ++i)
        adjustedArguments.append({ vm, arguments->argumentAt(i) });
    addMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Image, MessageLevel::Log, dataURL, ScriptArguments::create(lexicalGlobalObject, WTFMove(adjustedArguments)), lexicalGlobalObject, /* requestIdentifier */ 0, timestamp));
}

} // namespace WebCore
