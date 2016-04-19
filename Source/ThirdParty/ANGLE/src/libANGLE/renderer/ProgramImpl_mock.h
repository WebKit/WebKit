//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// ProgramImpl_mock.h:
//   Defines a mock of the ProgramImpl class.
//

#ifndef LIBANGLE_RENDERER_PROGRAMIMPLMOCK_H_
#define LIBANGLE_RENDERER_PROGRAMIMPLMOCK_H_

#include "gmock/gmock.h"

#include "libANGLE/renderer/ProgramImpl.h"

namespace rx
{

class MockProgramImpl : public rx::ProgramImpl
{
  public:
    MockProgramImpl() : ProgramImpl(gl::Program::Data()) {}
    virtual ~MockProgramImpl() { destroy(); }

    MOCK_METHOD2(load, LinkResult(gl::InfoLog &, gl::BinaryInputStream *));
    MOCK_METHOD1(save, gl::Error(gl::BinaryOutputStream *));
    MOCK_METHOD1(setBinaryRetrievableHint, void(bool));

    MOCK_METHOD2(link, LinkResult(const gl::Data &, gl::InfoLog &));
    MOCK_METHOD2(validate, GLboolean(const gl::Caps &, gl::InfoLog *));

    MOCK_METHOD3(setUniform1fv, void(GLint, GLsizei, const GLfloat *));
    MOCK_METHOD3(setUniform2fv, void(GLint, GLsizei, const GLfloat *));
    MOCK_METHOD3(setUniform3fv, void(GLint, GLsizei, const GLfloat *));
    MOCK_METHOD3(setUniform4fv, void(GLint, GLsizei, const GLfloat *));
    MOCK_METHOD3(setUniform1iv, void(GLint, GLsizei, const GLint *));
    MOCK_METHOD3(setUniform2iv, void(GLint, GLsizei, const GLint *));
    MOCK_METHOD3(setUniform3iv, void(GLint, GLsizei, const GLint *));
    MOCK_METHOD3(setUniform4iv, void(GLint, GLsizei, const GLint *));
    MOCK_METHOD3(setUniform1uiv, void(GLint, GLsizei, const GLuint *));
    MOCK_METHOD3(setUniform2uiv, void(GLint, GLsizei, const GLuint *));
    MOCK_METHOD3(setUniform3uiv, void(GLint, GLsizei, const GLuint *));
    MOCK_METHOD3(setUniform4uiv, void(GLint, GLsizei, const GLuint *));

    MOCK_METHOD4(setUniformMatrix2fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix3fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix4fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix2x3fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix3x2fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix2x4fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix4x2fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix3x4fv, void(GLint, GLsizei, GLboolean, const GLfloat *));
    MOCK_METHOD4(setUniformMatrix4x3fv, void(GLint, GLsizei, GLboolean, const GLfloat *));

    MOCK_METHOD2(setUniformBlockBinding, void(GLuint, GLuint));
    MOCK_CONST_METHOD2(getUniformBlockSize, bool(const std::string &, size_t *));
    MOCK_CONST_METHOD2(getUniformBlockMemberInfo, bool(const std::string &, sh::BlockMemberInfo *));

    MOCK_METHOD0(destroy, void());
};

inline ::testing::NiceMock<MockProgramImpl> *MakeProgramMock()
{
    ::testing::NiceMock<MockProgramImpl> *programImpl = new ::testing::NiceMock<MockProgramImpl>();
    // TODO(jmadill): add ON_CALLS for returning methods
    // We must mock the destructor since NiceMock doesn't work for destructors.
    EXPECT_CALL(*programImpl, destroy()).Times(1).RetiresOnSaturation();

    return programImpl;
}

}  // namespace rx

#endif  // LIBANGLE_RENDERER_PROGRAMIMPLMOCK_H_
