// Copyright 2019 Joe Drago. All rights reserved.
// SPDX-License-Identifier: BSD-2-Clause

#include "avif/internal.h"

#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
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
    stream->offset = offset;
    if (stream->offset > stream->raw->size) {
        stream->offset = stream->raw->size;
    }
}

avifBool avifROStreamSkip(avifROStream * stream, size_t byteCount)
{
    if (!avifROStreamHasBytesLeft(stream, byteCount)) {
        avifDiagnosticsPrintf(stream->diag, "%s: Failed to skip %zu bytes, truncated data?", stream->diagContext, byteCount);
        return AVIF_FALSE;
    }
    stream->offset += byteCount;
    return AVIF_TRUE;
}

avifBool avifROStreamRead(avifROStream * stream, uint8_t * data, size_t size)
{
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
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint16_t)));
    *v = avifNTOHS(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU16Endianness(avifROStream * stream, uint16_t * v, avifBool littleEndian)
{
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint16_t)));
    *v = littleEndian ? avifCTOHS(*v) : avifNTOHS(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU32(avifROStream * stream, uint32_t * v)
{
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint32_t)));
    *v = avifNTOHL(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU32Endianness(avifROStream * stream, uint32_t * v, avifBool littleEndian)
{
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint32_t)));
    *v = littleEndian ? avifCTOHL(*v) : avifNTOHL(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadU64(avifROStream * stream, uint64_t * v)
{
    AVIF_CHECK(avifROStreamRead(stream, (uint8_t *)v, sizeof(uint64_t)));
    *v = avifNTOH64(*v);
    return AVIF_TRUE;
}

avifBool avifROStreamReadString(avifROStream * stream, char * output, size_t outputSize)
{
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
static void makeRoom(avifRWStream * stream, size_t size)
{
    size_t neededSize = stream->offset + size;
    size_t newSize = stream->raw->size;
    while (newSize < neededSize) {
        newSize += AVIF_STREAM_BUFFER_INCREMENT;
    }
    if (stream->raw->size != newSize) {
        avifRWDataRealloc(stream->raw, newSize);
    }
}

void avifRWStreamStart(avifRWStream * stream, avifRWData * raw)
{
    stream->raw = raw;
    stream->offset = 0;
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

void avifRWStreamWrite(avifRWStream * stream, const void * data, size_t size)
{
    if (!size) {
        return;
    }

    makeRoom(stream, size);
    memcpy(stream->raw->data + stream->offset, data, size);
    stream->offset += size;
}

void avifRWStreamWriteChars(avifRWStream * stream, const char * chars, size_t size)
{
    avifRWStreamWrite(stream, chars, size);
}

avifBoxMarker avifRWStreamWriteFullBox(avifRWStream * stream, const char * type, size_t contentSize, int version, uint32_t flags)
{
    avifBoxMarker marker = stream->offset;
    size_t headerSize = sizeof(uint32_t) + 4 /* size of type */;
    if (version != -1) {
        headerSize += 4;
    }

    makeRoom(stream, headerSize);
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

    return marker;
}

avifBoxMarker avifRWStreamWriteBox(avifRWStream * stream, const char * type, size_t contentSize)
{
    return avifRWStreamWriteFullBox(stream, type, contentSize, -1, 0);
}

void avifRWStreamFinishBox(avifRWStream * stream, avifBoxMarker marker)
{
    uint32_t noSize = avifHTONL((uint32_t)(stream->offset - marker));
    memcpy(stream->raw->data + marker, &noSize, sizeof(uint32_t));
}

void avifRWStreamWriteU8(avifRWStream * stream, uint8_t v)
{
    makeRoom(stream, 1);
    stream->raw->data[stream->offset] = v;
    stream->offset += 1;
}

void avifRWStreamWriteU16(avifRWStream * stream, uint16_t v)
{
    size_t size = sizeof(uint16_t);
    v = avifHTONS(v);
    makeRoom(stream, size);
    memcpy(stream->raw->data + stream->offset, &v, size);
    stream->offset += size;
}

void avifRWStreamWriteU32(avifRWStream * stream, uint32_t v)
{
    size_t size = sizeof(uint32_t);
    v = avifHTONL(v);
    makeRoom(stream, size);
    memcpy(stream->raw->data + stream->offset, &v, size);
    stream->offset += size;
}

void avifRWStreamWriteU64(avifRWStream * stream, uint64_t v)
{
    size_t size = sizeof(uint64_t);
    v = avifHTON64(v);
    makeRoom(stream, size);
    memcpy(stream->raw->data + stream->offset, &v, size);
    stream->offset += size;
}

void avifRWStreamWriteZeros(avifRWStream * stream, size_t byteCount)
{
    makeRoom(stream, byteCount);
    uint8_t * p = stream->raw->data + stream->offset;
    uint8_t * end = p + byteCount;
    while (p != end) {
        *p = 0;
        ++p;
    }
    stream->offset += byteCount;
}
