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
extern GLint **gUniformLocations;
using BlockIndexesMap = std::unordered_map<GLuint, std::unordered_map<GLuint, GLuint>>;
extern BlockIndexesMap gUniformBlockIndexes;
extern GLuint gCurrentProgram;
void UpdateUniformLocation(GLuint program, const char *name, GLint location, GLint count);
void DeleteUniformLocations(GLuint program);
void UpdateUniformBlockIndex(GLuint program, const char *name, GLuint index);
void UpdateCurrentProgram(GLuint program);

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
                      uint32_t maxVertexArray);

// Global state

constexpr size_t kMaxClientArrays = 16;

extern uint8_t *gBinaryData;
extern uint8_t *gReadBuffer;
extern uint8_t *gClientArrays[kMaxClientArrays];

extern GLuint *gBufferMap;
extern GLuint *gFenceNVMap;
extern GLuint *gFramebufferMap;
extern GLuint *gMemoryObjectMap;
extern GLuint *gProgramPipelineMap;
extern GLuint *gQueryMap;
extern GLuint *gRenderbufferMap;
extern GLuint *gSamplerMap;
extern GLuint *gSemaphoreMap;
extern GLuint *gShaderProgramMap;
extern GLuint *gTextureMap;
extern GLuint *gTransformFeedbackMap;
extern GLuint *gVertexArrayMap;

// TODO(http://www.anglebug.com/5878): avoid std::unordered_map, it's slow
using SyncResourceMap = std::unordered_map<uintptr_t, GLsync>;
extern SyncResourceMap gSyncMap;
using ContextMap = std::unordered_map<uint32_t, EGLContext>;
extern ContextMap gContextMap;

void UpdateClientArrayPointer(int arrayIndex, const void *data, uint64_t size);
using BufferHandleMap = std::unordered_map<GLuint, void *>;
extern BufferHandleMap gMappedBufferData;
void UpdateClientBufferData(GLuint bufferID, const void *source, GLsizei size);
void UpdateClientBufferDataWithOffset(GLuint bufferID,
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

void SetFramebufferID(GLuint id);
void SetBufferID(GLuint id);
void SetRenderbufferID(GLuint id);
void SetTextureID(GLuint id);

void ValidateSerializedState(const char *serializedState, const char *fileName, uint32_t line);
#define VALIDATE_CHECKPOINT(STATE) ValidateSerializedState(STATE, __FILE__, __LINE__)

#endif  // ANGLE_TRACE_FIXTURE_H_
