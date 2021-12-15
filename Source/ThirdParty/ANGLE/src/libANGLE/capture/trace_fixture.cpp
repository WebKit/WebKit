//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace_fixture.cpp:
//   Common code for the ANGLE trace replays.
//

#include "trace_fixture.h"

#include "angle_trace_gl.h"

#include <string>

namespace
{
void UpdateResourceMap(ResourceMap *resourceMap, GLuint id, GLsizei readBufferOffset)
{
    GLuint returnedID;
    memcpy(&returnedID, &gReadBuffer[readBufferOffset], sizeof(GLuint));
    (*resourceMap)[id] = returnedID;
}

void UpdateResourceMap2(GLuint *resourceMap, GLuint id, GLsizei readBufferOffset)
{
    GLuint returnedID;
    memcpy(&returnedID, &gReadBuffer[readBufferOffset], sizeof(GLuint));
    resourceMap[id] = returnedID;
}

DecompressCallback gDecompressCallback;
std::string gBinaryDataDir = ".";

void LoadBinaryData(const char *fileName)
{
    // TODO(b/179188489): Fix cross-module deallocation.
    if (gBinaryData != nullptr)
    {
        delete[] gBinaryData;
    }
    char pathBuffer[1000] = {};

    sprintf(pathBuffer, "%s/%s", gBinaryDataDir.c_str(), fileName);
    FILE *fp = fopen(pathBuffer, "rb");
    if (fp == 0)
    {
        fprintf(stderr, "Error loading binary data file: %s\n", fileName);
        return;
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (gDecompressCallback)
    {
        if (!strstr(fileName, ".gz"))
        {
            fprintf(stderr, "Filename does not end in .gz");
            exit(1);
        }
        std::vector<uint8_t> compressedData(size);
        (void)fread(compressedData.data(), 1, size, fp);
        gBinaryData = gDecompressCallback(compressedData);
    }
    else
    {
        if (!strstr(fileName, ".angledata"))
        {
            fprintf(stderr, "Filename does not end in .angledata");
            exit(1);
        }
        gBinaryData = new uint8_t[size];
        (void)fread(gBinaryData, 1, size, fp);
    }
    fclose(fp);
}

ValidateSerializedStateCallback gValidateSerializedStateCallback;
std::unordered_map<GLuint, std::vector<GLint>> gInternalUniformLocationsMap;
}  // namespace

LocationsMap gUniformLocations;
GLint **gUniformLocations2;
BlockIndexesMap gUniformBlockIndexes;
GLuint gCurrentProgram = 0;

// TODO (http://anglebug.com/6234): Remove three parameter UpdateUniformLocation on next retrace
void UpdateUniformLocation(GLuint program, const char *name, GLint location)
{
    gUniformLocations[program][location] = glGetUniformLocation(program, name);
}

void UpdateUniformLocation(GLuint program, const char *name, GLint location, GLint count)
{
    for (GLint i = 0; i < count; i++)
    {
        gUniformLocations[program][location + i] = glGetUniformLocation(program, name) + i;
    }
}

void UpdateUniformLocation2(GLuint program, const char *name, GLint location, GLint count)
{
    std::vector<GLint> &programLocations = gInternalUniformLocationsMap[program];
    if (static_cast<GLint>(programLocations.size()) < location + count)
    {
        programLocations.resize(location + count, 0);
    }
    for (GLint arrayIndex = 0; arrayIndex < count; ++arrayIndex)
    {
        programLocations[location + arrayIndex] = glGetUniformLocation(program, name) + arrayIndex;
    }
    gUniformLocations2[program] = programLocations.data();
}

void DeleteUniformLocations(GLuint program)
{
    gUniformLocations.erase(program);
}

void DeleteUniformLocations2(GLuint program)
{
    // No-op. We leave uniform locations around so deleted current programs can still use them.
}

void UpdateUniformBlockIndex(GLuint program, const char *name, GLuint index)
{
    gUniformBlockIndexes[program][index] = glGetUniformBlockIndex(program, name);
}
void UpdateCurrentProgram(GLuint program)
{
    gCurrentProgram = program;
}

uint8_t *gBinaryData;
uint8_t *gReadBuffer;
uint8_t *gClientArrays[kMaxClientArrays];
ResourceMap gBufferMap;
ResourceMap gFenceNVMap;
ResourceMap gFramebufferMap;
ResourceMap gMemoryObjectMap;
ResourceMap gProgramPipelineMap;
ResourceMap gQueryMap;
ResourceMap gRenderbufferMap;
ResourceMap gSamplerMap;
ResourceMap gSemaphoreMap;
ResourceMap gShaderProgramMap;
ResourceMap gTextureMap;
ResourceMap gTransformFeedbackMap;
ResourceMap gVertexArrayMap;
SyncResourceMap gSyncMap;
ContextMap gContextMap;

GLuint *gBufferMap2;
GLuint *gFenceNVMap2;
GLuint *gFramebufferMap2;
GLuint *gMemoryObjectMap2;
GLuint *gProgramPipelineMap2;
GLuint *gQueryMap2;
GLuint *gRenderbufferMap2;
GLuint *gSamplerMap2;
GLuint *gSemaphoreMap2;
GLuint *gShaderProgramMap2;
GLuint *gTextureMap2;
GLuint *gTransformFeedbackMap2;
GLuint *gVertexArrayMap2;

void SetBinaryDataDecompressCallback(DecompressCallback callback)
{
    gDecompressCallback = callback;
}

void SetBinaryDataDir(const char *dataDir)
{
    gBinaryDataDir = dataDir;
}

void InitializeReplay(const char *binaryDataFileName,
                      size_t maxClientArraySize,
                      size_t readBufferSize)
{
    LoadBinaryData(binaryDataFileName);

    for (uint8_t *&clientArray : gClientArrays)
    {
        clientArray = new uint8_t[maxClientArraySize];
    }

    gReadBuffer = new uint8_t[readBufferSize];
}

GLuint *AllocateZeroedUints(size_t count)
{
    GLuint *mem = new GLuint[count + 1];
    memset(mem, 0, sizeof(GLuint) * (count + 1));
    return mem;
}

void InitializeReplay2(const char *binaryDataFileName,
                       size_t maxClientArraySize,
                       size_t readBufferSize,
                       uint32_t maxBuffer,
                       uint32_t maxFenceNV,
                       uint32_t maxFramebuffer,
                       uint32_t maxMemoryObject,
                       uint32_t maxProgramPipeline,
                       uint32_t maxQuery,
                       uint32_t maxRenderbuffer,
                       uint32_t maxSampler,
                       uint32_t maxSemaphore,
                       uint32_t maxShaderProgram,
                       uint32_t maxTexture,
                       uint32_t maxTransformFeedback,
                       uint32_t maxVertexArray)
{
    InitializeReplay(binaryDataFileName, maxClientArraySize, readBufferSize);

    gBufferMap2            = AllocateZeroedUints(maxBuffer);
    gFenceNVMap2           = AllocateZeroedUints(maxFenceNV);
    gFramebufferMap2       = AllocateZeroedUints(maxFramebuffer);
    gMemoryObjectMap2      = AllocateZeroedUints(maxMemoryObject);
    gProgramPipelineMap2   = AllocateZeroedUints(maxProgramPipeline);
    gQueryMap2             = AllocateZeroedUints(maxQuery);
    gRenderbufferMap2      = AllocateZeroedUints(maxRenderbuffer);
    gSamplerMap2           = AllocateZeroedUints(maxSampler);
    gSemaphoreMap2         = AllocateZeroedUints(maxSemaphore);
    gShaderProgramMap2     = AllocateZeroedUints(maxShaderProgram);
    gTextureMap2           = AllocateZeroedUints(maxTexture);
    gTransformFeedbackMap2 = AllocateZeroedUints(maxTransformFeedback);
    gVertexArrayMap2       = AllocateZeroedUints(maxVertexArray);

    gUniformLocations2 = new GLint *[maxShaderProgram + 1];
    memset(gUniformLocations2, 0, sizeof(GLint *) * (maxShaderProgram + 1));
}

void FinishReplay()
{
    for (uint8_t *&clientArray : gClientArrays)
    {
        delete[] clientArray;
    }
    delete[] gReadBuffer;

    delete[] gBufferMap2;
    delete[] gRenderbufferMap2;
    delete[] gTextureMap2;
    delete[] gFramebufferMap2;
    delete[] gShaderProgramMap2;
    delete[] gFenceNVMap2;
    delete[] gMemoryObjectMap2;
    delete[] gProgramPipelineMap2;
    delete[] gQueryMap2;
    delete[] gSamplerMap2;
    delete[] gSemaphoreMap2;
    delete[] gTransformFeedbackMap2;
    delete[] gVertexArrayMap2;
}

void SetValidateSerializedStateCallback(ValidateSerializedStateCallback callback)
{
    gValidateSerializedStateCallback = callback;
}

void UpdateClientArrayPointer(int arrayIndex, const void *data, uint64_t size)
{
    memcpy(gClientArrays[arrayIndex], data, static_cast<size_t>(size));
}
BufferHandleMap gMappedBufferData;

void UpdateClientBufferData(GLuint bufferID, const void *source, GLsizei size)
{
    memcpy(gMappedBufferData[gBufferMap[bufferID]], source, size);
}

void UpdateClientBufferData2(GLuint bufferID, const void *source, GLsizei size)
{
    memcpy(gMappedBufferData[gBufferMap2[bufferID]], source, size);
}

void UpdateClientBufferData2WithOffset(GLuint bufferID,
                                       const void *source,
                                       GLsizei size,
                                       GLsizei offset)
{
    uintptr_t dest = reinterpret_cast<uintptr_t>(gMappedBufferData[gBufferMap2[bufferID]]) + offset;
    memcpy(reinterpret_cast<void *>(dest), source, size);
}

void UpdateBufferID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gBufferMap, id, readBufferOffset);
}

void UpdateFenceNVID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gFenceNVMap, id, readBufferOffset);
}

void UpdateFramebufferID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gFramebufferMap, id, readBufferOffset);
}

void UpdateMemoryObjectID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gMemoryObjectMap, id, readBufferOffset);
}

void UpdateProgramPipelineID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gProgramPipelineMap, id, readBufferOffset);
}

void UpdateQueryID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gQueryMap, id, readBufferOffset);
}

void UpdateRenderbufferID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gRenderbufferMap, id, readBufferOffset);
}

void UpdateSamplerID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gSamplerMap, id, readBufferOffset);
}

void UpdateSemaphoreID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gSemaphoreMap, id, readBufferOffset);
}

void UpdateShaderProgramID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gShaderProgramMap, id, readBufferOffset);
}

void UpdateTextureID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gTextureMap, id, readBufferOffset);
}

void UpdateTransformFeedbackID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gTransformFeedbackMap, id, readBufferOffset);
}

void UpdateVertexArrayID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(&gVertexArrayMap, id, readBufferOffset);
}

void UpdateBufferID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gBufferMap2, id, readBufferOffset);
}

void UpdateFenceNVID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gFenceNVMap2, id, readBufferOffset);
}

void UpdateFramebufferID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gFramebufferMap2, id, readBufferOffset);
}

void UpdateMemoryObjectID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gMemoryObjectMap2, id, readBufferOffset);
}

void UpdateProgramPipelineID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gProgramPipelineMap2, id, readBufferOffset);
}

void UpdateQueryID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gQueryMap2, id, readBufferOffset);
}

void UpdateRenderbufferID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gRenderbufferMap2, id, readBufferOffset);
}

void UpdateSamplerID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gSamplerMap2, id, readBufferOffset);
}

void UpdateSemaphoreID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gSemaphoreMap2, id, readBufferOffset);
}

void UpdateShaderProgramID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gShaderProgramMap2, id, readBufferOffset);
}

void UpdateTextureID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gTextureMap2, id, readBufferOffset);
}

void UpdateTransformFeedbackID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gTransformFeedbackMap2, id, readBufferOffset);
}

void UpdateVertexArrayID2(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap2(gVertexArrayMap2, id, readBufferOffset);
}

void SetResourceID(GLuint *map, GLuint id)
{
    if (map[id] != 0)
    {
        fprintf(stderr, "%s: resource ID %d is already reserved\n", __func__, id);
        exit(1);
    }
    map[id] = id;
}

void SetFramebufferID(GLuint id)
{
    SetResourceID(gFramebufferMap2, id);
}

void ValidateSerializedState(const char *serializedState, const char *fileName, uint32_t line)
{
    if (gValidateSerializedStateCallback)
    {
        gValidateSerializedStateCallback(serializedState, fileName, line);
    }
}
