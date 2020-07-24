/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#import "config.h"

#import <CoreMedia/CMBufferQueue.h>
#import <CoreMedia/CMFormatDescription.h>
#import <CoreMedia/CMSampleBuffer.h>
#import <Foundation/Foundation.h>
#import <WebCore/MediaSample.h>
#import <WebCore/RuntimeEnabledFeatures.h>
#import <WebCore/SharedBuffer.h>
#import <WebCore/SourceBufferParserWebM.h>
#import <WebCore/VP9UtilitiesCocoa.h>
#import <WebCore/WebCoreDecompressionSession.h>
#import <getopt.h>
#import <webrtc/sdk/WebKit/WebKitVP9Decoder.h>
#import <wtf/CPUTime.h>
#import <wtf/MonotonicTime.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/WTFSemaphore.h>
#import <wtf/cf/TypeCastsCF.h>

using namespace WebCore;

static Function<void()>& updateOutputFunction()
{
    static NeverDestroyed<Function<void()>> function;
    return function;
}

int main(int argc, char* argv[])
{
    int enableHardwareDecoder = true;
    static struct option longopts[] = {
        { "hardware",    no_argument, &enableHardwareDecoder, 1 },
        { "no-hardware", no_argument, &enableHardwareDecoder, 0 },
        { NULL,          0,           NULL, 0 }
    };

    auto printUsage = [&] {
        fprintf(stderr, "Usage: %s [options] <file>\n", argv[0]);
        fprintf(stderr, "\tOptions:\n"
            "\t\t--[no-]hardware     # Enable or disable the hardware decoder\n"
        );
    };


    char c;
    while ((c = getopt_long(argc, argv, ":hn", longopts, NULL)) != -1) {
        switch (c) {
        case '0': break;
        case '?':
            fprintf(stderr, "Invalid argument -%c.\n", optopt);
            printUsage();
            return -1;
        }
    };

    if (optind == argc || optind < argc - 1) {
        printUsage();
        return -1;
    }

    String filename = argv[optind];

    @autoreleasepool {
        WTF::initializeMainThread();
        webrtc::registerWebKitVP9Decoder();
        registerSupplementalVP9Decoder();
        RuntimeEnabledFeatures::sharedFeatures().setWebMParserEnabled(true);

        auto buffer = SharedBuffer::createWithContentsOfFile(filename.utf8().data());
        if (!buffer) {
            fprintf(stderr, "Could not read file at \"%s\"\n", filename.utf8().data());
            return -1;
        }

        fprintf(stdout, "Parsing \"%s\"...\n", filename.utf8().data());

        SourceBufferParserWebM parser;
        parser.setDidEncounterErrorDuringParsingCallback([&] (uint64_t errorCode) {
            fprintf(stderr, "Parser encountered error %llu, exiting", errorCode);
            exit(-1);
        });

        CMBufferQueueRef bufferQueue = nullptr;
        if (noErr != CMBufferQueueCreate(kCFAllocatorDefault, 0, CMBufferQueueGetCallbacksForUnsortedSampleBuffers(), &bufferQueue)) {
            fprintf(stderr, "Could not create buffer queue, exting");
            return -1;
        }

        bool firstSample = true;
        Optional<CPUTime> startCPUTime;
        MonotonicTime startTime;
        uint64_t decodedSamples { 0 };

        updateOutputFunction() = [&] {
            auto endTime = MonotonicTime::now();
            auto endCPUTime = CPUTime::get();
            auto duration = endTime - startTime;
            float fps = decodedSamples / duration.value();

            fprintf(stdout, "Decoded %llu samples in %g seconds (%g fps)\n", decodedSamples, duration.value(), fps);
            if (startCPUTime && endCPUTime)
                fprintf(stdout, "CPU Usage: %g %%\n", endCPUTime->percentageCPUUsageSince(*startCPUTime));
        };

        struct sigaction action { };
        action.sa_flags = SA_SIGINFO;
        action.sa_sigaction = [] (int, siginfo_t*, void*) {
            if (updateOutputFunction())
                updateOutputFunction()();
        };
        sigaction(SIGINFO, &action, nullptr);

        WTF::Semaphore sampleSemaphore { 0 };
        parser.setDidProvideMediaDataCallback([&] (Ref<MediaSample>&& sample, uint64_t, const String&) {
            auto platformSample = sample->platformSample();
            if (platformSample.type != PlatformSample::CMSampleBufferType)
                return;

            auto cmSampleBuffer = platformSample.sample.cmSampleBuffer;

            auto formatDescription = CMSampleBufferGetFormatDescription(cmSampleBuffer);
            if (CMFormatDescriptionGetMediaType(formatDescription) != kCMMediaType_Video)
                return;

            if (firstSample) {
                auto size = sample->presentationSize();
                startTime = MonotonicTime::now();
                startCPUTime = CPUTime::get();
                fprintf(stdout, "Size: %g x %g\n", size.width(), size.height());
                firstSample = false;
            }

            CMBufferQueueEnqueue(bufferQueue, cmSampleBuffer);
            sampleSemaphore.signal();
        });


        Semaphore finishedParsingSemaphore { 0 };
        auto parserQueue = dispatch_queue_create("data parser queue", DISPATCH_QUEUE_CONCURRENT);
        dispatch_async(parserQueue, [&, buffer = WTFMove(buffer)] {
            Vector<unsigned char> bytes;
            bytes.resize(buffer->size());
            memcpy(bytes.data(), buffer->data(), buffer->size());
            parser.appendData(WTFMove(bytes));
            dispatch_async(dispatch_get_main_queue(), [&] {
                finishedParsingSemaphore.signal();
            });
        });

        Semaphore finishedDecodingSemaphore { 0 };
        auto decoderQueue = dispatch_queue_create("decoder queue", DISPATCH_QUEUE_CONCURRENT);
        dispatch_async(decoderQueue, [&] {
            auto decompressionSession = WebCoreDecompressionSession::createOpenGL();
            decompressionSession->setHardwareDecoderEnabled(enableHardwareDecoder);

            do {
                sampleSemaphore.wait();
                if (CMBufferQueueIsEmpty(bufferQueue)) {
                    finishedDecodingSemaphore.signal();
                    return;
                }

                auto sample = adoptCF((CMSampleBufferRef)(const_cast<void*>(CMBufferQueueDequeueAndRetain(bufferQueue))));
                decompressionSession->decodeSampleSync(sample.get());
                ++decodedSamples;
            } while (true);
        });

        bool shouldKeepRunning = true;
        auto monitorQueue = dispatch_queue_create("monitor queue", DISPATCH_QUEUE_CONCURRENT);
        dispatch_async(monitorQueue, [&] {
            finishedParsingSemaphore.wait();
            sampleSemaphore.signal();
            finishedDecodingSemaphore.wait();

            updateOutputFunction()();

            shouldKeepRunning = false;
        });

        NSRunLoop *runLoop = [NSRunLoop currentRunLoop];
        while (shouldKeepRunning && [runLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:1]]) { }
    }
    return 0;
}
