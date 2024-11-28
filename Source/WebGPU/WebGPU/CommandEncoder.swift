/*
 * Copyright (c) 2021-2024 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

public func clearBuffer(
    commandEncoder: WebGPU.CommandEncoder, buffer: WebGPU.Buffer, offset: UInt64, size: inout UInt64
) {
    commandEncoder.clearBuffer(buffer: buffer, offset: offset, size: &size)
}
public func resolveQuerySet(commandEncoder: WebGPU.CommandEncoder, querySet: WebGPU.QuerySet, firstQuery: UInt32, queryCount:UInt32, destination: WebGPU.Buffer, destinationOffset: UInt64)
{
    commandEncoder.resolveQuerySet(querySet, firstQuery: firstQuery, queryCount: queryCount, destination: destination, destinationOffset: destinationOffset)
}
extension WebGPU.CommandEncoder {
    public func clearBuffer(buffer: WebGPU.Buffer, offset: UInt64, size: inout UInt64) {
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        if size == UInt64.max {
            let initialSize = buffer.initialSize()
            let (subtractionResult, didOverflow) = initialSize.subtractingReportingOverflow(offset)
            if didOverflow {
                self.device().generateAValidationError(
                    "CommandEncoder::clearBuffer(): offset > buffer.size")
                return
            }
            size = subtractionResult
        }

        if !self.validateClearBuffer(buffer, offset, size) {
            self.makeInvalid("GPUCommandEncoder.clearBuffer validation failed")
            return
        }
        // FIXME: rdar://138042799 need to pass in the default argument.
        buffer.setCommandEncoder(self, false)
        buffer.indirectBufferInvalidated()
        guard let offsetInt = Int(exactly: offset), let sizeInt = Int(exactly: size) else {
            return
        }
        let range = offsetInt..<(offsetInt + sizeInt)
        if buffer.isDestroyed() || sizeInt == 0 || range.upperBound > buffer.buffer().length {
            return
        }
        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        blitCommandEncoder.fill(buffer: buffer.buffer(), range: range, value: 0)
    }

    public func resolveQuerySet(_ querySet: WebGPU.QuerySet, firstQuery: UInt32, queryCount: UInt32, destination: WebGPU.Buffer, destinationOffset:UInt64)
    {
        guard self.prepareTheEncoderState() else {
            self.generateInvalidEncoderStateError()
            return
        }
        guard self.validateResolveQuerySet(querySet: querySet, firstQuery: firstQuery, queryCount: queryCount, destination: destination, destinationOffset: destinationOffset) else {
            self.makeInvalid("GPUCommandEncoder.resolveQuerySet validation failed")
            return
        }
        querySet.setCommandEncoder(self)
        // FIXME: rdar://138042799 need to pass in the default argument.
        destination.setCommandEncoder(self, false)
        destination.indirectBufferInvalidated();
        guard !(querySet.isDestroyed() || destination.isDestroyed() || queryCount == 0) else {
            return
        }
        guard let blitCommandEncoder = ensureBlitCommandEncoder() else {
            return
        }
        if querySet.type().rawValue == WGPUQueryType_Occlusion.rawValue {
            guard let sourceOffset = Int(exactly: 8 * firstQuery), let destinationOffsetChecked = Int(exactly: destinationOffset), let size = Int(exactly: 8 * queryCount) else {
                return
            }
            blitCommandEncoder.copy(from: querySet.visibilityBuffer(), sourceOffset: sourceOffset, to: destination.buffer(), destinationOffset: destinationOffsetChecked, size: size)
        }
    }
    public func validateResolveQuerySet(querySet: WebGPU.QuerySet, firstQuery: UInt32, queryCount: UInt32, destination: WebGPU.Buffer, destinationOffset: UInt64) -> Bool
    {
        guard (destinationOffset % 256) == 0 else {
            return false
        }
        guard querySet.isDestroyed() || querySet.isValid() else {
            return false
        }
        guard destination.isDestroyed() || destination.isValid() else {
            return false
        }
        guard (destination.usage() & WGPUBufferUsage_QueryResolve.rawValue) != 0 else {
            return false
        }
        guard firstQuery < querySet.count() else {
            return false
        }
        let countEnd: UInt32 = firstQuery
        var (additionResult, didOverflow) = countEnd.addingReportingOverflow(queryCount)
        guard !didOverflow && additionResult <= querySet.count() else {
            return false
        }
        let countTimes8PlusOffset = destinationOffset
        let additionResult64: UInt64
        (additionResult64, didOverflow) = countTimes8PlusOffset.addingReportingOverflow(8 * UInt64(queryCount))
        guard !didOverflow && destination.initialSize() >= additionResult64 else {
            return false
        }
        return true
    }
}
