//
// Copyright 2021 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// trace_fixture.h:
//   Common code for the ANGLE trace replays.
//

#ifndef ANGLE_TRACE_FIXTURE_H_
#define ANGLE_TRACE_FIXTURE_H_

#include <EGL/egl.h>
#include "angle_gl.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <unordered_map>
#include <vector>

#if !defined(ANGLE_REPLAY_EXPORT)
#    if defined(_WIN32)
#        if defined(ANGLE_REPLAY_IMPLEMENTATION)
#            define ANGLE_REPLAY_EXPORT __declspec(dllexport)
#        else
#            define ANGLE_REPLAY_EXPORT __declspec(dllimport)
#        endif
#    elif defined(__GNUC__)
#        define ANGLE_REPLAY_EXPORT __attribute__((visibility("default")))
#    else
#        define ANGLE_REPLAY_EXPORT
#    endif
#endif  // !defined(ANGLE_REPLAY_EXPORT)

using DecompressCallback              = uint8_t *(*)(const std::vector<uint8_t> &);
using ValidateSerializedStateCallback = void (*)(const char *, const char *, uint32_t);

extern "C" {
ANGLE_REPLAY_EXPORT void SetBinaryDataDecompressCallback(DecompressCallback callback);
ANGLE_REPLAY_EXPORT void SetBinaryDataDir(const char *dataDir);
ANGLE_REPLAY_EXPORT void SetupReplay();
ANGLE_REPLAY_EXPORT void ReplayFrame(uint32_t frameIndex);
ANGLE_REPLAY_EXPORT void ResetReplay();
ANGLE_REPLAY_EXPORT void FinishReplay();
ANGLE_REPLAY_EXPORT void SetValidateSerializedStateCallback(
    ValidateSerializedStateCallback callback);

// Only defined if serialization is enabled.
ANGLE_REPLAY_EXPORT const char *GetSerializedContextState(uint32_t frameIndex);
}  // extern "C"

// Maps from <captured Program ID, captured location> to run-time location.
using LocationsMap = std::unordered_map<GLuint, std::unordered_map<GLint, GLint>>;
extern LocationsMap gUniformLocations;
extern GLint **gUniformLocations2;
using BlockIndexesMap = std::unordered_map<GLuint, std::unordered_map<GLuint, GLuint>>;
extern BlockIndexesMap gUniformBlockIndexes;
extern GLuint gCurrentProgram;
void UpdateUniformLocation(GLuint program, const char *name, GLint location);
void UpdateUniformLocation(GLuint program, const char *name, GLint location, GLint count);
void UpdateUniformLocation2(GLuint program, const char *name, GLint location, GLint count);
void DeleteUniformLocations(GLuint program);
void DeleteUniformLocations2(GLuint program);
void UpdateUniformBlockIndex(GLuint program, const char *name, GLuint index);
void UpdateCurrentProgram(GLuint program);

// Maps from captured Resource ID to run-time Resource ID.
class ResourceMap
{
  public:
    ResourceMap() {}
    GLuint &operator[](GLuint index)
    {
        if (mIDs.size() <= static_cast<size_t>(index))
            mIDs.resize(index + 1, 0);
        return mIDs[index];
    }

  private:
    std::vector<GLuint> mIDs;
};

void InitializeReplay(const char *binaryDataFileName,
                      size_t maxClientArraySize,
                      size_t readBufferSize);

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
                       uint32_t maxVertexArray);

// Global state

constexpr size_t kMaxClientArrays = 16;

extern uint8_t *gBinaryData;
extern uint8_t *gReadBuffer;
extern uint8_t *gClientArrays[kMaxClientArrays];
extern ResourceMap gBufferMap;
extern ResourceMap gFenceNVMap;
extern ResourceMap gFramebufferMap;
extern ResourceMap gMemoryObjectMap;
extern ResourceMap gProgramPipelineMap;
extern ResourceMap gQueryMap;
extern ResourceMap gRenderbufferMap;
extern ResourceMap gSamplerMap;
extern ResourceMap gSemaphoreMap;
extern ResourceMap gShaderProgramMap;
extern ResourceMap gTextureMap;
extern ResourceMap gTransformFeedbackMap;
extern ResourceMap gVertexArrayMap;

extern GLuint *gBufferMap2;
extern GLuint *gFenceNVMap2;
extern GLuint *gFramebufferMap2;
extern GLuint *gMemoryObjectMap2;
extern GLuint *gProgramPipelineMap2;
extern GLuint *gQueryMap2;
extern GLuint *gRenderbufferMap2;
extern GLuint *gSamplerMap2;
extern GLuint *gSemaphoreMap2;
extern GLuint *gShaderProgramMap2;
extern GLuint *gTextureMap2;
extern GLuint *gTransformFeedbackMap2;
extern GLuint *gVertexArrayMap2;

// TODO(http://www.anglebug.com/5878): avoid std::unordered_map, it's slow
using SyncResourceMap = std::unordered_map<uintptr_t, GLsync>;
extern SyncResourceMap gSyncMap;
using ContextMap = std::unordered_map<uint32_t, EGLContext>;
extern ContextMap gContextMap;

void UpdateClientArrayPointer(int arrayIndex, const void *data, uint64_t size);
using BufferHandleMap = std::unordered_map<GLuint, void *>;
extern BufferHandleMap gMappedBufferData;
void UpdateClientBufferData(GLuint bufferID, const void *source, GLsizei size);
void UpdateClientBufferData2(GLuint bufferID, const void *source, GLsizei size);
void UpdateClientBufferData2WithOffset(GLuint bufferID,
                                       const void *source,
                                       GLsizei size,
                                       GLsizei offset);
void UpdateBufferID(GLuint id, GLsizei readBufferOffset);
void UpdateFenceNVID(GLuint id, GLsizei readBufferOffset);
void UpdateFramebufferID(GLuint id, GLsizei readBufferOffset);
void UpdateMemoryObjectID(GLuint id, GLsizei readBufferOffset);
void UpdateProgramPipelineID(GLuint id, GLsizei readBufferOffset);
void UpdateQueryID(GLuint id, GLsizei readBufferOffset);
void UpdateRenderbufferID(GLuint id, GLsizei readBufferOffset);
void UpdateSamplerID(GLuint id, GLsizei readBufferOffset);
void UpdateSemaphoreID(GLuint id, GLsizei readBufferOffset);
void UpdateShaderProgramID(GLuint id, GLsizei readBufferOffset);
void UpdateTextureID(GLuint id, GLsizei readBufferOffset);
void UpdateTransformFeedbackID(GLuint id, GLsizei readBufferOffset);
void UpdateVertexArrayID(GLuint id, GLsizei readBufferOffset);
void UpdateBufferID2(GLuint id, GLsizei readBufferOffset);
void UpdateFenceNVID2(GLuint id, GLsizei readBufferOffset);
void UpdateFramebufferID2(GLuint id, GLsizei readBufferOffset);
void UpdateMemoryObjectID2(GLuint id, GLsizei readBufferOffset);
void UpdateProgramPipelineID2(GLuint id, GLsizei readBufferOffset);
void UpdateQueryID2(GLuint id, GLsizei readBufferOffset);
void UpdateRenderbufferID2(GLuint id, GLsizei readBufferOffset);
void UpdateSamplerID2(GLuint id, GLsizei readBufferOffset);
void UpdateSemaphoreID2(GLuint id, GLsizei readBufferOffset);
void UpdateShaderProgramID2(GLuint id, GLsizei readBufferOffset);
void UpdateTextureID2(GLuint id, GLsizei readBufferOffset);
void UpdateTransformFeedbackID2(GLuint id, GLsizei readBufferOffset);
void UpdateVertexArrayID2(GLuint id, GLsizei readBufferOffset);

void SetFramebufferID(GLuint id);

void ValidateSerializedState(const char *serializedState, const char *fileName, uint32_t line);
#define VALIDATE_CHECKPOINT(STATE) ValidateSerializedState(STATE, __FILE__, __LINE__)

#endif  // ANGLE_TRACE_FIXTURE_H_
