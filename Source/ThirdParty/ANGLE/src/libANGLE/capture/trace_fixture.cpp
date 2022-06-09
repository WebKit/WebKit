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
void UpdateResourceMap(GLuint *resourceMap, GLuint id, GLsizei readBufferOffset)
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

GLint **gUniformLocations;
BlockIndexesMap gUniformBlockIndexes;
GLuint gCurrentProgram = 0;

void UpdateUniformLocation(GLuint program, const char *name, GLint location, GLint count)
{
    std::vector<GLint> &programLocations = gInternalUniformLocationsMap[program];
    if (static_cast<GLint>(programLocations.size()) < location + count)
    {
        programLocations.resize(location + count, 0);
    }
    for (GLint arrayIndex = 0; arrayIndex < count; ++arrayIndex)
    {
        GLuint mappedProgramID = gShaderProgramMap[program];
        programLocations[location + arrayIndex] =
            glGetUniformLocation(mappedProgramID, name) + arrayIndex;
    }
    gUniformLocations[program] = programLocations.data();
}

void DeleteUniformLocations(GLuint program)
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
SyncResourceMap gSyncMap;
ContextMap gContextMap;

GLuint *gBufferMap;
GLuint *gFenceNVMap;
GLuint *gFramebufferMap;
GLuint *gMemoryObjectMap;
GLuint *gProgramPipelineMap;
GLuint *gQueryMap;
GLuint *gRenderbufferMap;
GLuint *gSamplerMap;
GLuint *gSemaphoreMap;
GLuint *gShaderProgramMap;
GLuint *gTextureMap;
GLuint *gTransformFeedbackMap;
GLuint *gVertexArrayMap;

void SetBinaryDataDecompressCallback(DecompressCallback callback)
{
    gDecompressCallback = callback;
}

void SetBinaryDataDir(const char *dataDir)
{
    gBinaryDataDir = dataDir;
}

GLuint *AllocateZeroedUints(size_t count)
{
    GLuint *mem = new GLuint[count + 1];
    memset(mem, 0, sizeof(GLuint) * (count + 1));
    return mem;
}

void InitializeReplay(const char *binaryDataFileName,
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
    LoadBinaryData(binaryDataFileName);

    for (uint8_t *&clientArray : gClientArrays)
    {
        clientArray = new uint8_t[maxClientArraySize];
    }

    gReadBuffer = new uint8_t[readBufferSize];

    gBufferMap            = AllocateZeroedUints(maxBuffer);
    gFenceNVMap           = AllocateZeroedUints(maxFenceNV);
    gFramebufferMap       = AllocateZeroedUints(maxFramebuffer);
    gMemoryObjectMap      = AllocateZeroedUints(maxMemoryObject);
    gProgramPipelineMap   = AllocateZeroedUints(maxProgramPipeline);
    gQueryMap             = AllocateZeroedUints(maxQuery);
    gRenderbufferMap      = AllocateZeroedUints(maxRenderbuffer);
    gSamplerMap           = AllocateZeroedUints(maxSampler);
    gSemaphoreMap         = AllocateZeroedUints(maxSemaphore);
    gShaderProgramMap     = AllocateZeroedUints(maxShaderProgram);
    gTextureMap           = AllocateZeroedUints(maxTexture);
    gTransformFeedbackMap = AllocateZeroedUints(maxTransformFeedback);
    gVertexArrayMap       = AllocateZeroedUints(maxVertexArray);

    gUniformLocations = new GLint *[maxShaderProgram + 1];
    memset(gUniformLocations, 0, sizeof(GLint *) * (maxShaderProgram + 1));
}

void FinishReplay()
{
    for (uint8_t *&clientArray : gClientArrays)
    {
        delete[] clientArray;
    }
    delete[] gReadBuffer;

    delete[] gBufferMap;
    delete[] gRenderbufferMap;
    delete[] gTextureMap;
    delete[] gFramebufferMap;
    delete[] gShaderProgramMap;
    delete[] gFenceNVMap;
    delete[] gMemoryObjectMap;
    delete[] gProgramPipelineMap;
    delete[] gQueryMap;
    delete[] gSamplerMap;
    delete[] gSemaphoreMap;
    delete[] gTransformFeedbackMap;
    delete[] gVertexArrayMap;
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

void UpdateClientBufferDataWithOffset(GLuint bufferID,
                                      const void *source,
                                      GLsizei size,
                                      GLsizei offset)
{
    uintptr_t dest = reinterpret_cast<uintptr_t>(gMappedBufferData[gBufferMap[bufferID]]) + offset;
    memcpy(reinterpret_cast<void *>(dest), source, size);
}

void UpdateBufferID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gBufferMap, id, readBufferOffset);
}

void UpdateFenceNVID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gFenceNVMap, id, readBufferOffset);
}

void UpdateFramebufferID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gFramebufferMap, id, readBufferOffset);
}

void UpdateMemoryObjectID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gMemoryObjectMap, id, readBufferOffset);
}

void UpdateProgramPipelineID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gProgramPipelineMap, id, readBufferOffset);
}

void UpdateQueryID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gQueryMap, id, readBufferOffset);
}

void UpdateRenderbufferID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gRenderbufferMap, id, readBufferOffset);
}

void UpdateSamplerID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gSamplerMap, id, readBufferOffset);
}

void UpdateSemaphoreID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gSemaphoreMap, id, readBufferOffset);
}

void UpdateShaderProgramID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gShaderProgramMap, id, readBufferOffset);
}

void UpdateTextureID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gTextureMap, id, readBufferOffset);
}

void UpdateTransformFeedbackID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gTransformFeedbackMap, id, readBufferOffset);
}

void UpdateVertexArrayID(GLuint id, GLsizei readBufferOffset)
{
    UpdateResourceMap(gVertexArrayMap, id, readBufferOffset);
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
    SetResourceID(gFramebufferMap, id);
}

void SetBufferID(GLuint id)
{
    SetResourceID(gBufferMap, id);
}

void SetRenderbufferID(GLuint id)
{
    SetResourceID(gRenderbufferMap, id);
}

void SetTextureID(GLuint id)
{
    SetResourceID(gTextureMap, id);
}

void ValidateSerializedState(const char *serializedState, const char *fileName, uint32_t line)
{
    if (gValidateSerializedStateCallback)
    {
        gValidateSerializedStateCallback(serializedState, fileName, line);
    }
}
