// Copyright (C) 2024 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

#if HAVE_CORE_ANIMATION_SEPARATED_LAYERS && compiler(>=6.2)

import os
@_spi(Private) internal import Vision
internal import WebKit_Internal

extension WKSeparatedImageView {
    func pickViewMode() async {
        pickViewModeTask?.cancel()
        let task = Task { [weak self] () -> ViewMode in
            guard let self else { return .unknown }
            return try await self.analyze()
        }
        pickViewModeTask = task

        do {
            viewMode = try await task.value
            pickViewModeTask = nil

            if let imageHash {
                cachedViewModeInfo = (imageHash, viewMode)
            }
            scheduleUpdate()
        } catch {
            // The pickViewModeTask was cancelled.
            pickViewModeTask = nil
            Logger.separatedImage.log("\(self.logPrefix) - Cancelled Image Analysis.")
        }
    }

    @concurrent
    func analyze() async throws -> ViewMode {
        try await Task.sleep(for: SeparatedImageViewConstants.cancellationDelay)

        let logPrefix = await self.logPrefix

        guard let image = await self.cgImage else {
            try Task.checkCancellation()
            Logger.separatedImage.error("\(logPrefix) - ImageAnalysis result: bad CGImage.")
            return .unknown
        }

        guard let imageData = await self.imageData else {
            try Task.checkCancellation()
            Logger.separatedImage.error("\(logPrefix) - ImageAnalysis result: bad imageData.")
            return .failed
        }

        guard image.hasCompatibleDimensions else {
            try Task.checkCancellation()
            Logger.separatedImage.log("\(logPrefix) - ImageAnalysis result: incompatible dimension.")
            return .small
        }

        guard let scores = await AnalysisActor.shared.scoreImage(imageData) else {
            try Task.checkCancellation()
            Logger.separatedImage.log("\(logPrefix) - ImageAnalysis result: unable to scoreImage.")
            return .failed
        }

        let confidence = scores.confidence
        try Task.checkCancellation()
        guard confidence >= SeparatedImageViewConstants.confidenceThreshold else {
            Logger.separatedImage.log(
                "\(logPrefix) - ImageAnalysis result: too low confidence [\(confidence) < \(SeparatedImageViewConstants.confidenceThreshold)]."
            )
            return .inaesthetic
        }

        #if USE_APPLE_INTERNAL_SDK
        let screenShotScore = scores.screenShotScore
        guard screenShotScore < SeparatedImageViewConstants.screenShotScoreThreshold else {
            try Task.checkCancellation()
            Logger.separatedImage.log(
                "\(logPrefix) - ImageAnalysis result: screenshot detected [\(screenShotScore) > \(SeparatedImageViewConstants.screenShotScoreThreshold)]."
            )
            return .inaesthetic
        }

        let documentScore = scores.textDocumentScore
        guard documentScore < SeparatedImageViewConstants.documentScoreThreshold else {
            try Task.checkCancellation()
            Logger.separatedImage.log(
                "\(logPrefix) - ImageAnalysis result: document detected [\(documentScore) > \(SeparatedImageViewConstants.documentScoreThreshold)]."
            )
            return .inaesthetic
        }
        #endif

        let overallScore = scores.overallScore
        guard overallScore >= SeparatedImageViewConstants.overallScoreThreshold else {
            try Task.checkCancellation()
            Logger.separatedImage.log(
                "\(logPrefix) - ImageAnalysis result: too low overallScore [\(overallScore) < \(SeparatedImageViewConstants.overallScoreThreshold)]."
            )
            return .inaesthetic
        }

        try Task.checkCancellation()
        Logger.separatedImage.log("\(logPrefix) - ImageAnalysis result: success.")
        return .portal
    }
}

// Protects access to the taskQueue, requests run sequentially outside of the MainActor.
actor AnalysisActor {
    typealias Result = CalculateImageAestheticsScoresRequest.Result?

    static let shared = AnalysisActor()

    private var taskQueue: [() async -> Void] = []
    private var shouldWait = false
    private var idleTask: Task<Void, Never>?
    private var idleContinuation: CheckedContinuation<Void, Never>?

    private init() {}

    func scoreImage(_ image: Data) async -> Result {
        await enqueue { () -> Result in
            var request = CalculateImageAestheticsScoresRequest()
            #if USE_APPLE_INTERNAL_SDK
            let supportedComputeStageDevices = request.supportedComputeStageDevices
            let gpuDevice = supportedComputeStageDevices[.main]?
                .first {
                    if case .gpu = $0 {
                        return true
                    }
                    return false
                }

            guard let gpuDevice else { return nil }

            // We don't want to add to the ANE contention.
            request.setComputeDevice(gpuDevice, for: .main)
            #endif

            return try? await request.perform(on: image, orientation: .up)
        }
    }

    func isIdle() async {
        guard !taskQueue.isEmpty || shouldWait else {
            return
        }

        if let existingTask = idleTask {
            await existingTask.value
            return
        }

        let task = Task<Void, Never> {
            await withCheckedContinuation { continuation in
                idleContinuation = continuation
            }
        }
        idleTask = task

        await task.value
    }

    private func enqueue(_ task: @escaping () async -> Result) async -> Result {
        await withCheckedContinuation { continuation in
            Task {
                taskQueue.append {
                    let result = await task()
                    continuation.resume(returning: result)
                }
                await processQueue()
            }
        }
    }

    private func processQueue() async {
        guard !shouldWait else { return }

        shouldWait = true
        while !taskQueue.isEmpty {
            let nextTask = taskQueue.removeFirst()
            await nextTask()
        }
        shouldWait = false

        idleContinuation?.resume()
        idleContinuation = nil
        idleTask = nil
    }
}

extension CGImage {
    fileprivate var hasCompatibleDimensions: Bool {
        let minSmallerDimension = 512
        let minLargerDimension = 1024
        let maxRatio: Float = 2.0

        let smallerDimension = min(width, height)
        let largerDimension = max(width, height)
        return smallerDimension >= minSmallerDimension && largerDimension >= minLargerDimension
            && (Float(largerDimension) / Float(smallerDimension)) <= maxRatio
    }
}

#endif
