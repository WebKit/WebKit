//
// Copyright (c) 2014 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef SAMPLE_UTIL_SHADER_UTILS_H
#define SAMPLE_UTIL_SHADER_UTILS_H

#include <export.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <string>
#include <vector>

ANGLE_EXPORT GLuint CompileShader(GLenum type, const std::string &source);
ANGLE_EXPORT GLuint CompileShaderFromFile(GLenum type, const std::string &sourcePath);

ANGLE_EXPORT GLuint
CompileProgramWithTransformFeedback(const std::string &vsSource,
                                    const std::string &fsSource,
                                    const std::vector<std::string> &transformFeedbackVaryings,
                                    GLenum bufferMode);
ANGLE_EXPORT GLuint CompileProgram(const std::string &vsSource, const std::string &fsSource);
ANGLE_EXPORT GLuint CompileProgramFromFiles(const std::string &vsPath, const std::string &fsPath);
ANGLE_EXPORT GLuint CompileComputeProgram(const std::string &csSource,
                                          bool outputErrorMessages = true);
ANGLE_EXPORT bool LinkAttachedProgram(GLuint program);

ANGLE_EXPORT GLuint LoadBinaryProgramOES(const std::vector<uint8_t> &binary, GLenum binaryFormat);
ANGLE_EXPORT GLuint LoadBinaryProgramES3(const std::vector<uint8_t> &binary, GLenum binaryFormat);

#endif // SAMPLE_UTIL_SHADER_UTILS_H
