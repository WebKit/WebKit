// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

// ---------------------------------------------------------------------------
// avifROStream

const uint8_t * avifROStreamCurrent(avifROStream * stream)
{
    return stream->raw->data + stream->offset;
}

void avifROStreamStart(avifROStream * stream, avifROData * raw, avifDiagnostics * diag, const char * diagContext)
{
    stream->raw = raw;
    stream->offset = 0;
    stream->numUsedBitsInPartialByte = 0;
    stream->diag = diag;
    stream->diagContext = diagContext;

    // If diag is non-NULL, diagContext must also be non-NULL
    assert(!stream->diag || stream->diagContext);
}

avifBool avifROStreamHasBytesLeft(const avifROStream * stream, size_t byteCount)
{
    return byteCount <= (stream->raw->size - stream->offset);
}

size_t avifROStreamRemainingBytes(const avifROStream * stream)
{
    return stream->raw->size - stream->offset;
}

size_t avifROStreamOffset(const avifROStream * stream)
{
    return stream->offset;
}

void avifROStreamSetOffset(avifROStream * stream, size_t offset)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    stream->offset = offset;
    if (stream->offset > stream->raw->size) {
        stream->offset = stream->raw->size;
    }
}

avifBool avifROStreamSkip(avifROStream * stream, size_t byteCount)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    if (!avifROStreamHasBytesLeft(stream, byteCount)) {
        avifDiagnosticsPrintf(stream->diag, "%s: Failed to skip %zu bytes, truncated data?", stream->diagContext, byteCount);
        return AVIF_FALSE;
    }
    stream->offset += byteCount;
    return AVIF_TRUE;
}

avifBool avifROStreamRead(avifROStream * stream, uint8_t * data, size_t size)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    if (!avifROStreamHasBytesLeft(stream, size)) {
        avifDiagnosticsPrintf(stream->diag, "%s: Failed to read %zu bytes, truncated data?", stream->diagContext, size);
        return AVIF_FALSE;
    }

    memcpy(data, stream->raw->data + stream->offset, size);
    stream->offset += size;
    return AVIF_TRUE;
}

avifBool avifROStreamReadUX8(avifROStream * stream, uint64_t * v, uint64_t factor)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    if (factor == 0) {
        // Don't read anything, just set to 0
        *v = 0;
    } else if (factor == 1) {
        uint8_t tmp;
        AVIF_CHECK(avifROStreamRead(stream, &tmp, 1));
        *v = tmp;
    } else if (factor == 2) {
        uint16_t tmp;
        AVIF_CHECK(avifROStreamReadU16(stream, &tmp));
        *v = tmp;
    } else if (factor == 4) {
        uint32_t tmp;
        AVIF_CHECK(avifROStreamReadU32(stream, &tmp));
        *v = tmp;
    } else if (factor == 8) {
        uint64_t tmp;
        AVIF_CHECK(avifROStreamReadU64(stream, &tmp));
        *v = tmp;
    } else {
        // Unsupported factor
        avifDiagnosticsPrintf(stream->diag, "%s: Failed to read UX8 value; Unsupported UX8 factor [%" PRIu64 "]", stream->diagContext, factor);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

avifBool avifROStreamReadU16(avifROStream * stream, uint16_t * v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint16_t)));
    *v = avifNTOHS(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU16Endianness(avifROStream * stream, uint16_t * v, avifBool littleEndian)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint16_t)));
    *v = littleEndian ? avifCTOHS(*v) : avifNTOHS(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU32(avifROStream * stream, uint32_t * v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint32_t)));
    *v = avifNTOHL(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU32Endianness(avifROStream * stream, uint32_t * v, avifBool littleEndian)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint32_t)));
    *v = littleEndian ? avifCTOHL(*v) : avifNTOHL(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU64(avifROStream * stream, uint64_t * v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint64_t)));
    *v = avifNTOH64(*v);
    return AVIF_TRUE;
}

// Override of avifROStreamReadBits() for convenient uint8_t output.
avifBool avifROStreamReadBits8(avifROStream * stream, uint8_t * v, size_t bitCount)
{
    assert(bitCount <= sizeof(*v) * 8);
    uint32_t v32;
    if (!avifROStreamReadBits(stream, &v32, bitCount)) {
        return AVIF_FALSE;
    }
    *v = (uint8_t)v32;
    return AVIF_TRUE;
}

avifBool avifROStreamReadBits(avifROStream * stream, uint32_t * v, size_t bitCount)
{
    assert(bitCount <= sizeof(*v) * 8);
    *v = 0;
    while (bitCount) {
        if (stream->numUsedBitsInPartialByte == 0) {
            AVIF_CHECK(avifROStreamSkip(stream, sizeof(uint8_t))); // Book a new partial byte in the stream.
        }
        assert(stream->offset > 0);
        const uint8_t * packedBits = stream->raw->data + stream->offset - 1;

        const size_t numBits = AVIF_MIN(bitCount, 8 - stream->numUsedBitsInPartialByte);
        stream->numUsedBitsInPartialByte += numBits;
        bitCount -= numBits;
        // The stream bits are packed starting with the most significant bit of the first input byte.
        // This way, packed bits can be found in the same order in the bit stream.
        const uint32_t bits = (*packedBits >> (8 - stream->numUsedBitsInPartialByte)) & ((1 << numBits) - 1);
        // The value bits are ordered from the most significant bit to the least significant bit.
        // In the case where avifROStreamReadBits() is used to parse the unsigned integer value *v
        // over multiple aligned bytes, this order corresponds to big endianness.
        *v |= bits << bitCount;

        if (stream->numUsedBitsInPartialByte == 8) {
            // Start a new partial byte the next time a bit is needed.
            stream->numUsedBitsInPartialByte = 0;
        }
    }
    return AVIF_TRUE;
}

static const int VARINT_DEPTH_0 = 7; // +1 bit to stop or continue.
static const int VARINT_DEPTH_1 = 3; // +1 bit to stop or continue.
static const int VARINT_DEPTH_2 = 18;

avifBool avifROStreamReadVarInt(avifROStream * stream, uint32_t * v)
{
    AVIF_CHECK(avifROStreamReadBits(stream, v, VARINT_DEPTH_0));
    uint32_t extended, extension;

    AVIF_CHECK(avifROStreamReadBits(stream, &extended, 1));
    if (extended) {
        AVIF_CHECK(avifROStreamReadBits(stream, &extension, VARINT_DEPTH_1));
        *v += (extension + 1) << VARINT_DEPTH_0;

        AVIF_CHECK(avifROStreamReadBits(stream, &extended, 1));
        if (extended) {
            AVIF_CHECK(avifROStreamReadBits(stream, &extension, VARINT_DEPTH_2));
            *v += (extension + 1) << (VARINT_DEPTH_0 + VARINT_DEPTH_1);
        }
    }
    return AVIF_TRUE;
}

avifBool avifROStreamReadString(avifROStream * stream, char * output, size_t outputSize)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.

    // Check for the presence of a null terminator in the stream.
    size_t remainingBytes = avifROStreamRemainingBytes(stream);
    const uint8_t * p = avifROStreamCurrent(stream);
    avifBool foundNullTerminator = AVIF_FALSE;
    for (size_t i = 0; i < remainingBytes; ++i) {
        if (p[i] == 0) {
            foundNullTerminator = AVIF_TRUE;
            break;
        }
    }
    if (!foundNullTerminator) {
        avifDiagnosticsPrintf(stream->diag, "%s: Failed to find a NULL terminator when reading a string", stream->diagContext);
        return AVIF_FALSE;
    }

    const char * streamString = (const char *)p;
    size_t stringLen = strlen(streamString);
    stream->offset += stringLen + 1; // update the stream to have read the "whole string" in

    if (output && outputSize) {
        // clamp to our output buffer
        if (stringLen >= outputSize) {
            stringLen = outputSize - 1;
        }
        memcpy(output, streamString, stringLen);
        output[stringLen] = 0;
    }
    return AVIF_TRUE;
}

avifBool avifROStreamReadBoxHeaderPartial(avifROStream * stream, avifBoxHeader * header)
{
    size_t startOffset = stream->offset;

    uint32_t smallSize;
    AVIF_CHECK(avifROStreamReadU32(stream, &smallSize));
    AVIF_CHECK(avifROStreamRead(stream, header->type, 4));

    uint64_t size = smallSize;
    if (size == 1) {
        AVIF_CHECK(avifROStreamReadU64(stream, &size));
    }

    if (!memcmp(header->type, "uuid", 4)) {
        AVIF_CHECK(avifROStreamSkip(stream, 16));
    }

    size_t bytesRead = stream->offset - startOffset;
    if ((size < bytesRead) || ((size - bytesRead) > SIZE_MAX)) {
        avifDiagnosticsPrintf(stream->diag, "%s: Header size overflow check failure", stream->diagContext);
        return AVIF_FALSE;
    }
    header->size = (size_t)(size - bytesRead);
    return AVIF_TRUE;
}

avifBool avifROStreamReadBoxHeader(avifROStream * stream, avifBoxHeader * header)
{
    AVIF_CHECK(avifROStreamReadBoxHeaderPartial(stream, header));
    if (header->size > avifROStreamRemainingBytes(stream)) {
        avifDiagnosticsPrintf(stream->diag, "%s: Child box too large, possibly truncated data", stream->diagContext);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

avifBool avifROStreamReadVersionAndFlags(avifROStream * stream, uint8_t * version, uint32_t * flags)
{
    uint8_t versionAndFlags[4];
    AVIF_CHECK(avifROStreamRead(stream, versionAndFlags, 4));
    if (version) {
        *version = versionAndFlags[0];
    }
    if (flags) {
        *flags = (versionAndFlags[1] << 16) + (versionAndFlags[2] << 8) + (versionAndFlags[3] << 0);
    }
    return AVIF_TRUE;
}

avifBool avifROStreamReadAndEnforceVersion(avifROStream * stream, uint8_t enforcedVersion)
{
    uint8_t version;
    AVIF_CHECK(avifROStreamReadVersionAndFlags(stream, &version, NULL));
    if (version != enforcedVersion) {
        avifDiagnosticsPrintf(stream->diag, "%s: Expecting box version %u, got version %u", stream->diagContext, enforcedVersion, version);
        return AVIF_FALSE;
    }
    return AVIF_TRUE;
}

// ---------------------------------------------------------------------------
// avifRWStream

#define AVIF_STREAM_BUFFER_INCREMENT (1024 * 1024)
static avifResult makeRoom(avifRWStream * stream, size_t size)
{
    size_t neededSize = stream->offset + size;
    size_t newSize = stream->raw->size;
    while (newSize < neededSize) {
        newSize += AVIF_STREAM_BUFFER_INCREMENT;
    }
    return avifRWDataRealloc(stream->raw, newSize);
}

void avifRWStreamStart(avifRWStream * stream, avifRWData * raw)
{
    stream->raw = raw;
    stream->offset = 0;
    stream->numUsedBitsInPartialByte = 0;
}

size_t avifRWStreamOffset(const avifRWStream * stream)
{
    return stream->offset;
}

void avifRWStreamSetOffset(avifRWStream * stream, size_t offset)
{
    stream->offset = offset;
    if (stream->offset > stream->raw->size) {
        stream->offset = stream->raw->size;
    }
}

void avifRWStreamFinishWrite(avifRWStream * stream)
{
    if (stream->raw->size != stream->offset) {
        if (stream->offset) {
            stream->raw->size = stream->offset;
        } else {
            avifRWDataFree(stream->raw);
        }
    }
}

avifResult avifRWStreamWrite(avifRWStream * stream, const void * data, size_t size)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    if (size) {
        AVIF_CHECKRES(makeRoom(stream, size));
        memcpy(stream->raw->data + stream->offset, data, size);
        stream->offset += size;
    }
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteChars(avifRWStream * stream, const char * chars, size_t size)
{
    return avifRWStreamWrite(stream, chars, size);
}

avifResult avifRWStreamWriteFullBox(avifRWStream * stream, const char * type, size_t contentSize, int version, uint32_t flags, avifBoxMarker * marker)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    if (marker) {
        *marker = stream->offset;
    }
    size_t headerSize = sizeof(uint32_t) + 4 /* size of type */;
    if (version != -1) {
        headerSize += 4;
    }

    AVIF_CHECKRES(makeRoom(stream, headerSize));
    memset(stream->raw->data + stream->offset, 0, headerSize);
    uint32_t noSize = avifHTONL((uint32_t)(headerSize + contentSize));
    memcpy(stream->raw->data + stream->offset, &noSize, sizeof(uint32_t));
    memcpy(stream->raw->data + stream->offset + 4, type, 4);
    if (version != -1) {
        stream->raw->data[stream->offset + 8] = (uint8_t)version;
        stream->raw->data[stream->offset + 9] = (uint8_t)((flags >> 16) & 0xff);
        stream->raw->data[stream->offset + 10] = (uint8_t)((flags >> 8) & 0xff);
        stream->raw->data[stream->offset + 11] = (uint8_t)((flags >> 0) & 0xff);
    }
    stream->offset += headerSize;

    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteBox(avifRWStream * stream, const char * type, size_t contentSize, avifBoxMarker * marker)
{
    return avifRWStreamWriteFullBox(stream, type, contentSize, -1, 0, marker);
}

void avifRWStreamFinishBox(avifRWStream * stream, avifBoxMarker marker)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    uint32_t noSize = avifHTONL((uint32_t)(stream->offset - marker));
    memcpy(stream->raw->data + marker, &noSize, sizeof(uint32_t));
}

avifResult avifRWStreamWriteU8(avifRWStream * stream, uint8_t v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECKRES(makeRoom(stream, 1));
    stream->raw->data[stream->offset] = v;
    stream->offset += 1;
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteU16(avifRWStream * stream, uint16_t v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    const size_t size = sizeof(uint16_t);
    AVIF_CHECKRES(makeRoom(stream, size));
    v = avifHTONS(v);
    memcpy(stream->raw->data + stream->offset, &v, size);
    stream->offset += size;
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteU32(avifRWStream * stream, uint32_t v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    const size_t size = sizeof(uint32_t);
    AVIF_CHECKRES(makeRoom(stream, size));
    v = avifHTONL(v);
    memcpy(stream->raw->data + stream->offset, &v, size);
    stream->offset += size;
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteU64(avifRWStream * stream, uint64_t v)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    const size_t size = sizeof(uint64_t);
    AVIF_CHECKRES(makeRoom(stream, size));
    v = avifHTON64(v);
    memcpy(stream->raw->data + stream->offset, &v, size);
    stream->offset += size;
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteZeros(avifRWStream * stream, size_t byteCount)
{
    assert(stream->numUsedBitsInPartialByte == 0); // Byte alignment is required.
    AVIF_CHECKRES(makeRoom(stream, byteCount));
    memset(stream->raw->data + stream->offset, 0, byteCount);
    stream->offset += byteCount;
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteBits(avifRWStream * stream, uint32_t v, size_t bitCount)
{
    assert(((uint64_t)v >> bitCount) == 0); // (uint32_t >> 32 is undefined behavior)
    while (bitCount) {
        if (stream->numUsedBitsInPartialByte == 0) {
            AVIF_CHECKRES(makeRoom(stream, 1)); // Book a new partial byte in the stream.
            stream->raw->data[stream->offset] = 0;
            stream->offset += 1;
        }
        assert(stream->offset > 0);
        uint8_t * packedBits = stream->raw->data + stream->offset - 1;

        const size_t numBits = AVIF_MIN(bitCount, 8 - stream->numUsedBitsInPartialByte);
        stream->numUsedBitsInPartialByte += numBits;
        bitCount -= numBits;
        // Order the input bits from the most significant bit to the least significant bit.
        // In the case where avifRWStreamWriteBits() is used to write the unsigned integer value v
        // over multiple aligned bytes, this order corresponds to big endianness.
        const uint32_t bits = (v >> bitCount) & ((1 << numBits) - 1);
        // Pack bits starting with the most significant bit of the first output byte.
        // This way, packed bits can be found in the same order in the bit stream.
        *packedBits |= bits << (8 - stream->numUsedBitsInPartialByte);

        if (stream->numUsedBitsInPartialByte == 8) {
            // Start a new partial byte the next time a bit is needed.
            stream->numUsedBitsInPartialByte = 0;
        }
    }
    return AVIF_RESULT_OK;
}

avifResult avifRWStreamWriteVarInt(avifRWStream * stream, uint32_t v)
{
    AVIF_CHECKERR(v < (1u << (VARINT_DEPTH_0 + VARINT_DEPTH_1 + VARINT_DEPTH_2)) + (1u << (VARINT_DEPTH_0 + VARINT_DEPTH_1)) +
                          (1u << VARINT_DEPTH_0),
                  AVIF_RESULT_INVALID_ARGUMENT);

    AVIF_CHECKRES(avifRWStreamWriteBits(stream, v & ((1u << VARINT_DEPTH_0) - 1), VARINT_DEPTH_0)); // value
    v >>= VARINT_DEPTH_0;
    AVIF_CHECKRES(avifRWStreamWriteBits(stream, v > 0, 1)); // extended
    if (v > 0) {
        v -= 1;
        AVIF_CHECKRES(avifRWStreamWriteBits(stream, v & ((1u << VARINT_DEPTH_1) - 1), VARINT_DEPTH_1)); // extension
        v >>= VARINT_DEPTH_1;
        AVIF_CHECKRES(avifRWStreamWriteBits(stream, v > 0, 1)); // extended
        if (v > 0) {
            v -= 1;
            AVIF_CHECKRES(avifRWStreamWriteBits(stream, v, VARINT_DEPTH_2)); // extension
        }
    }
    return AVIF_RESULT_OK;
}
