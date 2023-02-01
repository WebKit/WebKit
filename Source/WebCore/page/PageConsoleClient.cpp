/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
#include "PageConsoleClient.h"

#include "CachedImage.h"
#include "CanvasRenderingContext2D.h"
#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "ElementChildIterator.h"
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
#include "InspectorController.h"
#include "InspectorInstrumentation.h"
#include "IntRect.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSExecState.h"
#include "JSHTMLCanvasElement.h"
#include "JSImageBitmap.h"
#include "JSImageBitmapRenderingContext.h"
#include "JSImageData.h"
#include "JSNode.h"
#include "Node.h"
#include "Page.h"
#include "ScriptableDocumentParser.h"
#include "Settings.h"
#include <JavaScriptCore/ConsoleMessage.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/ScriptArguments.h>
#include <JavaScriptCore/ScriptCallStack.h>
#include <JavaScriptCore/ScriptCallStackFactory.h>
#include <JavaScriptCore/StrongInlines.h>
#include <wtf/Stopwatch.h>
#include <wtf/text/WTFString.h>

#if ENABLE(OFFSCREEN_CANVAS)
#include "JSOffscreenCanvas.h"
#include "OffscreenCanvas.h"
#endif

#if ENABLE(WEBGL)
#include "JSWebGLRenderingContext.h"
#include "JSWebGL2RenderingContext.h"
#include "WebGL2RenderingContext.h"
#include "WebGLRenderingContext.h"
#endif

namespace WebCore {
using namespace Inspector;

PageConsoleClient::PageConsoleClient(Page& page)
    : m_page(page)
{
}

PageConsoleClient::~PageConsoleClient() = default;

static int muteCount = 0;
static bool printExceptions = false;

bool PageConsoleClient::shouldPrintExceptions()
{
    return printExceptions;
}

void PageConsoleClient::setShouldPrintExceptions(bool print)
{
    printExceptions = print;
}

void PageConsoleClient::mute()
{
    muteCount++;
}

void PageConsoleClient::unmute()
{
    ASSERT(muteCount > 0);
    muteCount--;
}

void PageConsoleClient::addMessage(std::unique_ptr<Inspector::ConsoleMessage>&& consoleMessage)
{
    if (!m_page.usesEphemeralSession()) {
        String message;
        Span<const String> additionalArguments;
        Vector<String> messageArgumentsVector;
        if (consoleMessage->type() == MessageType::Image) {
            ASSERT(consoleMessage->arguments());
            messageArgumentsVector = consoleMessage->arguments()->getArgumentsAsStrings();
            if (!messageArgumentsVector.isEmpty()) {
                message = messageArgumentsVector.first();
                additionalArguments = messageArgumentsVector.span().subspan(1);
            }
        } else
            message = consoleMessage->message();

        m_page.chrome().client().addMessageToConsole(consoleMessage->source(), consoleMessage->level(), message, consoleMessage->line(), consoleMessage->column(), consoleMessage->url());
        m_page.chrome().client().addMessageWithArgumentsToConsole(consoleMessage->source(), consoleMessage->level(), message, additionalArguments, consoleMessage->line(), consoleMessage->column(), consoleMessage->url());

        if (UNLIKELY(m_page.settings().logsPageMessagesToSystemConsoleEnabled() || shouldPrintExceptions())) {
            if (consoleMessage->type() == MessageType::Image) {
                ASSERT(consoleMessage->arguments());
                ConsoleClient::printConsoleMessageWithArguments(consoleMessage->source(), consoleMessage->type(), consoleMessage->level(), consoleMessage->arguments()->globalObject(), *consoleMessage->arguments());
            } else
                ConsoleClient::printConsoleMessage(consoleMessage->source(), consoleMessage->type(), consoleMessage->level(), consoleMessage->toString(), consoleMessage->url(), consoleMessage->line(), consoleMessage->column());
        }
    }

    InspectorInstrumentation::addMessageToConsole(m_page, WTFMove(consoleMessage));
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, unsigned long requestIdentifier, Document* document)
{
    String url;
    unsigned line = 0;
    unsigned column = 0;
    if (document)
        document->getParserLocation(url, line, column);

    addMessage(source, level, message, url, line, column, nullptr, JSExecState::currentState(), requestIdentifier);
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& message, Ref<ScriptCallStack>&& callStack)
{
    addMessage(source, level, message, String(), 0, 0, WTFMove(callStack), 0);
}

void PageConsoleClient::addMessage(MessageSource source, MessageLevel level, const String& messageText, const String& suggestedURL, unsigned suggestedLineNumber, unsigned suggestedColumnNumber, RefPtr<ScriptCallStack>&& callStack, JSC::JSGlobalObject* lexicalGlobalObject, unsigned long requestIdentifier)
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

void PageConsoleClient::messageWithTypeAndLevel(MessageType type, MessageLevel level, JSC::JSGlobalObject* lexicalGlobalObject, Ref<Inspector::ScriptArguments>&& arguments)
{
    String messageText;
    Span<const String> additionalArguments;
    Vector<String> messageArgumentsVector = arguments->getArgumentsAsStrings();
    if (!messageArgumentsVector.isEmpty()) {
        messageText = messageArgumentsVector.first();
        additionalArguments = messageArgumentsVector.span().subspan(1);
    }

    auto message = makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, type, level, messageText, arguments.copyRef(), lexicalGlobalObject);

    String url = message->url();
    unsigned lineNumber = message->line();
    unsigned columnNumber = message->column();

    InspectorInstrumentation::addMessageToConsole(m_page, WTFMove(message));

    if (m_page.usesEphemeralSession())
        return;

    if (!messageArgumentsVector.isEmpty()) {
        m_page.chrome().client().addMessageToConsole(MessageSource::ConsoleAPI, level, messageText, lineNumber, columnNumber, url);
        m_page.chrome().client().addMessageWithArgumentsToConsole(MessageSource::ConsoleAPI, level, messageText, additionalArguments, lineNumber, columnNumber, url);
    }

    if (m_page.settings().logsPageMessagesToSystemConsoleEnabled() || PageConsoleClient::shouldPrintExceptions())
        ConsoleClient::printConsoleMessageWithArguments(MessageSource::ConsoleAPI, type, level, lexicalGlobalObject, WTFMove(arguments));
}

void PageConsoleClient::count(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    InspectorInstrumentation::consoleCount(m_page, lexicalGlobalObject, label);
}

void PageConsoleClient::countReset(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    InspectorInstrumentation::consoleCountReset(m_page, lexicalGlobalObject, label);
}

void PageConsoleClient::profile(JSC::JSGlobalObject* lexicalGlobalObject, const String& title)
{
    // FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
    InspectorInstrumentation::startProfiling(m_page, lexicalGlobalObject, title);
}

void PageConsoleClient::profileEnd(JSC::JSGlobalObject* lexicalGlobalObject, const String& title)
{
    // FIXME: <https://webkit.org/b/153499> Web Inspector: console.profile should use the new Sampling Profiler
    InspectorInstrumentation::stopProfiling(m_page, lexicalGlobalObject, title);
}

void PageConsoleClient::takeHeapSnapshot(JSC::JSGlobalObject*, const String& title)
{
    InspectorInstrumentation::takeHeapSnapshot(m_page.mainFrame(), title);
}

void PageConsoleClient::time(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    InspectorInstrumentation::startConsoleTiming(m_page.mainFrame(), lexicalGlobalObject, label);
}

void PageConsoleClient::timeLog(JSC::JSGlobalObject* lexicalGlobalObject, const String& label, Ref<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::logConsoleTiming(m_page.mainFrame(), lexicalGlobalObject, label, WTFMove(arguments));
}

void PageConsoleClient::timeEnd(JSC::JSGlobalObject* lexicalGlobalObject, const String& label)
{
    InspectorInstrumentation::stopConsoleTiming(m_page.mainFrame(), lexicalGlobalObject, label);
}

void PageConsoleClient::timeStamp(JSC::JSGlobalObject*, Ref<ScriptArguments>&& arguments)
{
    InspectorInstrumentation::consoleTimeStamp(m_page.mainFrame(), WTFMove(arguments));
}

static JSC::JSObject* objectArgumentAt(ScriptArguments& arguments, unsigned index)
{
    return arguments.argumentCount() > index ? arguments.argumentAt(index).getObject() : nullptr;
}

static CanvasRenderingContext* canvasRenderingContext(JSC::VM& vm, JSC::JSValue target)
{
    if (auto* canvas = JSHTMLCanvasElement::toWrapped(vm, target))
        return canvas->renderingContext();
#if ENABLE(OFFSCREEN_CANVAS)
    if (auto* canvas = JSOffscreenCanvas::toWrapped(vm, target))
        return canvas->renderingContext();
#endif
    if (auto* context = JSCanvasRenderingContext2D::toWrapped(vm, target))
        return context;
    if (auto* context = JSImageBitmapRenderingContext::toWrapped(vm, target))
        return context;
#if ENABLE(WEBGL)
    if (auto* context = JSWebGLRenderingContext::toWrapped(vm, target))
        return context;
    if (auto* context = JSWebGL2RenderingContext::toWrapped(vm, target))
        return context;
#endif
    return nullptr;
}

void PageConsoleClient::record(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    if (LIKELY(!InspectorInstrumentation::hasFrontends()))
        return;

    if (auto* target = objectArgumentAt(arguments, 0)) {
        if (auto* context = canvasRenderingContext(lexicalGlobalObject->vm(), target))
            InspectorInstrumentation::consoleStartRecordingCanvas(*context, *lexicalGlobalObject, objectArgumentAt(arguments, 1));
    }
}

void PageConsoleClient::recordEnd(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    if (LIKELY(!InspectorInstrumentation::hasFrontends()))
        return;

    if (auto* target = objectArgumentAt(arguments, 0)) {
        if (auto* context = canvasRenderingContext(lexicalGlobalObject->vm(), target))
            InspectorInstrumentation::consoleStopRecordingCanvas(*context);
    }
}

static std::optional<String> snapshotCanvas(HTMLCanvasElement& canvasElement, CanvasRenderingContext& canvasRenderingContext)
{
#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(canvasRenderingContext))
        downcast<WebGLRenderingContextBase>(canvasRenderingContext).setPreventBufferClearForInspector(true);
#else
    UNUSED_PARAM(canvasRenderingContext);
#endif

    auto result = canvasElement.toDataURL("image/png"_s);

#if ENABLE(WEBGL)
    if (is<WebGLRenderingContextBase>(canvasRenderingContext))
        downcast<WebGLRenderingContextBase>(canvasRenderingContext).setPreventBufferClearForInspector(false);
#endif

    if (!result.hasException())
        return result.releaseReturnValue().string;

    return std::nullopt;
}

void PageConsoleClient::screenshot(JSC::JSGlobalObject* lexicalGlobalObject, Ref<ScriptArguments>&& arguments)
{
    JSC::VM& vm = lexicalGlobalObject->vm();
    String dataURL;
    JSC::JSValue target;

    auto timestamp = m_page.inspectorController().executionStopwatch().elapsedTime();

    if (arguments->argumentCount()) {
        auto possibleTarget = arguments->argumentAt(0);

        if (auto* node = JSNode::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (UNLIKELY(InspectorInstrumentation::hasFrontends())) {
                RefPtr<ImageBuffer> snapshot;

                // Only try to do something special for subclasses of Node if they're detached from the DOM tree.
                if (!node->document().contains(node)) {
                    auto snapshotImageElement = [&snapshot] (HTMLImageElement& imageElement) {
                        if (auto* cachedImage = imageElement.cachedImage()) {
                            auto* image = cachedImage->image();
                            if (image && image != &Image::nullImage()) {
                                snapshot = ImageBuffer::create(image->size(), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
                                snapshot->context().drawImage(*image, FloatPoint(0, 0));
                            }
                        }
                    };

                    if (is<HTMLImageElement>(node))
                        snapshotImageElement(downcast<HTMLImageElement>(*node));
                    else if (is<HTMLPictureElement>(node)) {
                        if (auto* firstImage = childrenOfType<HTMLImageElement>(downcast<HTMLPictureElement>(*node)).first())
                            snapshotImageElement(*firstImage);
                    }
#if ENABLE(VIDEO)
                    else if (is<HTMLVideoElement>(node)) {
                        auto& videoElement = downcast<HTMLVideoElement>(*node);
                        unsigned videoWidth = videoElement.videoWidth();
                        unsigned videoHeight = videoElement.videoHeight();
                        snapshot = ImageBuffer::create(FloatSize(videoWidth, videoHeight), RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8);
                        videoElement.paintCurrentFrameInContext(snapshot->context(), FloatRect(0, 0, videoWidth, videoHeight));
                    }
#endif
                    else if (is<HTMLCanvasElement>(node)) {
                        auto& canvasElement = downcast<HTMLCanvasElement>(*node);
                        if (auto* canvasRenderingContext = canvasElement.renderingContext()) {
                            if (auto result = snapshotCanvas(canvasElement, *canvasRenderingContext))
                                dataURL = result.value();
                        }
                    }
                }

                if (dataURL.isEmpty()) {
                    if (!snapshot)
                        snapshot = WebCore::snapshotNode(m_page.mainFrame(), *node, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() });

                    if (snapshot)
                        dataURL = snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
                }
            }
        } else if (auto* imageData = JSImageData::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (UNLIKELY(InspectorInstrumentation::hasFrontends())) {
                auto sourceSize = imageData->size();
                if (auto imageBuffer = ImageBuffer::create(sourceSize, RenderingPurpose::Unspecified, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8)) {
                    IntRect sourceRect(IntPoint(), sourceSize);
                    imageBuffer->putPixelBuffer(imageData->pixelBuffer(), sourceRect);
                    dataURL = imageBuffer->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
                }
            }
        } else if (auto* imageBitmap = JSImageBitmap::toWrapped(vm, possibleTarget)) {
            target = possibleTarget;
            if (UNLIKELY(InspectorInstrumentation::hasFrontends())) {
                if (auto* imageBuffer = imageBitmap->buffer())
                    dataURL = imageBuffer->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
            }
        } else if (auto* context = canvasRenderingContext(vm, possibleTarget)) {
            auto& canvas = context->canvasBase();
            if (is<HTMLCanvasElement>(canvas)) {
                target = possibleTarget;
                if (UNLIKELY(InspectorInstrumentation::hasFrontends())) {
                    if (auto result = snapshotCanvas(downcast<HTMLCanvasElement>(canvas), *context))
                        dataURL = result.value();
                }
            }

            // FIXME: <https://webkit.org/b/180833> Web Inspector: support OffscreenCanvas for Canvas related operations
        } else {
            String base64;
            if (possibleTarget.getString(lexicalGlobalObject, base64) && startsWithLettersIgnoringASCIICase(base64, "data:"_s) && base64.length() > 5) {
                target = possibleTarget;
                dataURL = base64;
            }
        }
    }

    if (UNLIKELY(InspectorInstrumentation::hasFrontends())) {
        if (!target) {
            // If no target is provided, capture an image of the viewport.
            auto viewportRect = m_page.mainFrame().view()->unobscuredContentRect();
            if (auto snapshot = WebCore::snapshotFrameRect(m_page.mainFrame(), viewportRect, { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() }))
                dataURL = snapshot->toDataURL("image/png"_s, std::nullopt, PreserveResolution::Yes);
        }

        if (dataURL.isEmpty()) {
            addMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Image, MessageLevel::Error, "Could not capture screenshot"_s, WTFMove(arguments)));
            return;
        }
    }

    Vector<JSC::Strong<JSC::Unknown>> adjustedArguments;
    adjustedArguments.append({ vm, target ? target : JSC::jsNontrivialString(vm, "Viewport"_s) });
    for (size_t i = (!target ? 0 : 1); i < arguments->argumentCount(); ++i)
        adjustedArguments.append({ vm, arguments->argumentAt(i) });
    arguments = ScriptArguments::create(lexicalGlobalObject, WTFMove(adjustedArguments));
    addMessage(makeUnique<Inspector::ConsoleMessage>(MessageSource::ConsoleAPI, MessageType::Image, MessageLevel::Log, dataURL, WTFMove(arguments), lexicalGlobalObject, 0, timestamp));
}

} // namespace WebCore
