//
// Copyright 2015 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// validationES unit tests:
//   Unit tests for general ES validation functions.
//

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "libANGLE/ContextState.h"
#include "libANGLE/ErrorStrings.h"
#include "libANGLE/VaryingPacking.h"
#include "libANGLE/renderer/FramebufferImpl_mock.h"
#include "libANGLE/renderer/ProgramImpl_mock.h"
#include "libANGLE/renderer/TextureImpl_mock.h"
#include "libANGLE/validationES.h"
#include "tests/angle_unittests_utils.h"

using namespace gl;
using namespace rx;
using testing::_;
using testing::NiceMock;
using testing::Return;

namespace
{

class MockValidationContext : public ValidationContext
{
  public:
    MockValidationContext(const ValidationContext *shareContext,
                          TextureManager *shareTextures,
                          const Version &version,
                          State *state,
                          const Caps &caps,
                          const TextureCapsMap &textureCaps,
                          const Extensions &extensions,
                          const Limitations &limitations,
                          bool skipValidation)
        : ValidationContext(shareContext,
                            shareTextures,
                            version,
                            state,
                            caps,
                            textureCaps,
                            extensions,
                            limitations,
                            skipValidation)
    {
    }

    MOCK_METHOD1(handleError, void(const Error &));
};

// Test that ANGLE generates an INVALID_OPERATION when validating index data that uses a value
// larger than MAX_ELEMENT_INDEX and robust access is not enabled. Not specified in the GLES 3 spec,
// it's undefined behaviour, but we want a test to ensure we maintain this behaviour. TODO(jmadill):
// Re-enable when framebuffer sync state doesn't happen in validation. Also broken because of change
// of api of the state initialize method.
TEST(ValidationESTest, DISABLED_DrawElementsWithMaxIndexGivesError)
{
    auto framebufferImpl = MakeFramebufferMock();
    auto programImpl     = MakeProgramMock();

    // TODO(jmadill): Generalize some of this code so we can re-use it for other tests.
    NiceMock<MockGLFactory> mockFactory;
    EXPECT_CALL(mockFactory, createFramebuffer(_)).WillOnce(Return(framebufferImpl));
    EXPECT_CALL(mockFactory, createProgram(_)).WillOnce(Return(programImpl));
    EXPECT_CALL(mockFactory, createVertexArray(_)).WillOnce(Return(nullptr));

    State state;
    Caps caps;
    TextureCapsMap textureCaps;
    Extensions extensions;
    Limitations limitations;

    // Set some basic caps.
    caps.maxElementIndex     = 100;
    caps.maxDrawBuffers      = 1;
    caps.maxColorAttachments = 1;
    state.initialize(nullptr, false, true, true, false, false);

    NiceMock<MockTextureImpl> *textureImpl = new NiceMock<MockTextureImpl>();
    EXPECT_CALL(mockFactory, createTexture(_)).WillOnce(Return(textureImpl));
    EXPECT_CALL(*textureImpl, setStorage(_, _, _, _, _)).WillOnce(Return(gl::NoError()));
    EXPECT_CALL(*textureImpl, destructor()).Times(1).RetiresOnSaturation();

    Texture *texture = new Texture(&mockFactory, 0, GL_TEXTURE_2D);
    texture->addRef();
    EXPECT_FALSE(
        texture->setStorage(nullptr, GL_TEXTURE_2D, 1, GL_RGBA8, Extents(1, 1, 0)).isError());

    VertexArray *vertexArray = new VertexArray(&mockFactory, 0, 1, 1);
    Framebuffer *framebuffer = new Framebuffer(caps, &mockFactory, 1);
    framebuffer->setAttachment(nullptr, GL_FRAMEBUFFER_DEFAULT, GL_BACK, ImageIndex::Make2D(0),
                               texture);

    Program *program = new Program(&mockFactory, nullptr, 1);

    state.setVertexArrayBinding(vertexArray);
    state.setDrawFramebufferBinding(framebuffer);
    state.setProgram(nullptr, program);

    NiceMock<MockValidationContext> testContext(nullptr, nullptr, Version(3, 0), &state, caps,
                                                textureCaps, extensions, limitations, false);

    // Set the expectation for the validation error here.

    Error expectedError(gl::InvalidOperation() << kErrorExceedsMaxElement);
    EXPECT_CALL(testContext, handleError(expectedError)).Times(1);

    // Call once with maximum index, and once with an excessive index.
    GLuint indexData[] = {0, 1, static_cast<GLuint>(caps.maxElementIndex - 1),
                          3, 4, static_cast<GLuint>(caps.maxElementIndex)};
    EXPECT_TRUE(
        ValidateDrawElementsCommon(&testContext, GL_TRIANGLES, 3, GL_UNSIGNED_INT, indexData, 1));
    if (!testContext.getExtensions().robustBufferAccessBehavior)
    {
        EXPECT_FALSE(ValidateDrawElementsCommon(&testContext, GL_TRIANGLES, 6, GL_UNSIGNED_INT,
                                                indexData, 2));
    }

    texture->release(nullptr);

    state.setVertexArrayBinding(nullptr);
    state.setDrawFramebufferBinding(nullptr);
    state.setProgram(nullptr, nullptr);

    vertexArray->onDestroy(nullptr);
    framebuffer->onDestroy(nullptr);
    program->onDestroy(nullptr);
}

}  // anonymous namespace
