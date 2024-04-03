//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramExecutableWgpu.cpp: Implementation of ProgramExecutableWgpu.

#include "libANGLE/renderer/wgpu/ProgramExecutableWgpu.h"

namespace rx
{
ProgramExecutableWgpu::ProgramExecutableWgpu(const gl::ProgramExecutable *executable)
    : ProgramExecutableImpl(executable)
{}

ProgramExecutableWgpu::~ProgramExecutableWgpu() = default;

void ProgramExecutableWgpu::destroy(const gl::Context *context) {}

void ProgramExecutableWgpu::setUniform1fv(GLint location, GLsizei count, const GLfloat *v) {}

void ProgramExecutableWgpu::setUniform2fv(GLint location, GLsizei count, const GLfloat *v) {}

void ProgramExecutableWgpu::setUniform3fv(GLint location, GLsizei count, const GLfloat *v) {}

void ProgramExecutableWgpu::setUniform4fv(GLint location, GLsizei count, const GLfloat *v) {}

void ProgramExecutableWgpu::setUniform1iv(GLint location, GLsizei count, const GLint *v) {}

void ProgramExecutableWgpu::setUniform2iv(GLint location, GLsizei count, const GLint *v) {}

void ProgramExecutableWgpu::setUniform3iv(GLint location, GLsizei count, const GLint *v) {}

void ProgramExecutableWgpu::setUniform4iv(GLint location, GLsizei count, const GLint *v) {}

void ProgramExecutableWgpu::setUniform1uiv(GLint location, GLsizei count, const GLuint *v) {}

void ProgramExecutableWgpu::setUniform2uiv(GLint location, GLsizei count, const GLuint *v) {}

void ProgramExecutableWgpu::setUniform3uiv(GLint location, GLsizei count, const GLuint *v) {}

void ProgramExecutableWgpu::setUniform4uiv(GLint location, GLsizei count, const GLuint *v) {}

void ProgramExecutableWgpu::setUniformMatrix2fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix3fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix4fv(GLint location,
                                                GLsizei count,
                                                GLboolean transpose,
                                                const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix2x3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix3x2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix2x4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix4x2fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix3x4fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{}

void ProgramExecutableWgpu::setUniformMatrix4x3fv(GLint location,
                                                  GLsizei count,
                                                  GLboolean transpose,
                                                  const GLfloat *value)
{}

void ProgramExecutableWgpu::getUniformfv(const gl::Context *context,
                                         GLint location,
                                         GLfloat *params) const
{
    // TODO: Write some values.
}

void ProgramExecutableWgpu::getUniformiv(const gl::Context *context,
                                         GLint location,
                                         GLint *params) const
{
    // TODO: Write some values.
}

void ProgramExecutableWgpu::getUniformuiv(const gl::Context *context,
                                          GLint location,
                                          GLuint *params) const
{
    // TODO: Write some values.
}
}  // namespace rx
