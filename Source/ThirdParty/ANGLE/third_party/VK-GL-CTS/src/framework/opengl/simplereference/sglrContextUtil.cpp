/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES Utilities
 * ------------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief SGLR Context utilities.
 *//*--------------------------------------------------------------------*/

#include "sglrContextUtil.hpp"
#include "sglrContext.hpp"
#include "glwEnums.hpp"

namespace sglr
{

void drawQuad(sglr::Context &ctx, uint32_t program, const tcu::Vec3 &p0, const tcu::Vec3 &p1)
{
    const glu::ContextType ctxType = ctx.getType();

    if (glu::isContextTypeGLCore(ctxType) || (contextSupports(ctxType, glu::ApiType::es(3, 1))))
        drawQuadWithVaoBuffers(ctx, program, p0, p1);
    else
    {
        DE_ASSERT(isContextTypeES(ctxType));
        drawQuadWithClientPointers(ctx, program, p0, p1);
    }
}

void drawQuadWithVaoBuffers(sglr::Context &ctx, uint32_t program, const tcu::Vec3 &p0, const tcu::Vec3 &p1)
{
    // Vertex data.
    float hz                 = (p0.z() + p1.z()) * 0.5f;
    float position[]         = {p0.x(), p0.y(), p0.z(), 1.0f, p0.x(), p1.y(), hz,     1.0f,
                                p1.x(), p0.y(), hz,     1.0f, p1.x(), p1.y(), p1.z(), 1.0f};
    const float coord[]      = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    int32_t posLoc   = ctx.getAttribLocation(program, "a_position");
    int32_t coordLoc = ctx.getAttribLocation(program, "a_coord");
    uint32_t vaoID;
    uint32_t bufIDs[2];

    ctx.genVertexArrays(1, &vaoID);
    ctx.bindVertexArray(vaoID);

    ctx.genBuffers(2, &bufIDs[0]);

    ctx.useProgram(program);
    TCU_CHECK(posLoc >= 0);
    {
        ctx.bindBuffer(GL_ARRAY_BUFFER, bufIDs[0]);
        ctx.bufferData(GL_ARRAY_BUFFER, DE_LENGTH_OF_ARRAY(position) * sizeof(float), &position[0], GL_STATIC_DRAW);

        ctx.enableVertexAttribArray(posLoc);
        ctx.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, 0);

        ctx.bindBuffer(GL_ARRAY_BUFFER, 0);
    }

    if (coordLoc >= 0)
    {
        ctx.bindBuffer(GL_ARRAY_BUFFER, bufIDs[1]);
        ctx.bufferData(GL_ARRAY_BUFFER, DE_LENGTH_OF_ARRAY(coord) * sizeof(float), &coord[0], GL_STATIC_DRAW);

        ctx.enableVertexAttribArray(coordLoc);
        ctx.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

        ctx.bindBuffer(GL_ARRAY_BUFFER, 0);
    }

    {
        uint32_t ndxID;
        ctx.genBuffers(1, &ndxID);

        ctx.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, ndxID);
        ctx.bufferData(GL_ELEMENT_ARRAY_BUFFER, DE_LENGTH_OF_ARRAY(indices) * sizeof(uint16_t), &indices[0],
                       GL_STATIC_DRAW);

        ctx.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_SHORT, 0);

        ctx.bindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        ctx.deleteBuffers(1, &ndxID);
    }

    ctx.deleteBuffers(2, &bufIDs[0]);
    ctx.deleteVertexArrays(1, &vaoID);
}

void drawQuadWithClientPointers(sglr::Context &ctx, uint32_t program, const tcu::Vec3 &p0, const tcu::Vec3 &p1)
{
    // Vertex data.
    float hz                 = (p0.z() + p1.z()) * 0.5f;
    float position[]         = {p0.x(), p0.y(), p0.z(), 1.0f, p0.x(), p1.y(), hz,     1.0f,
                                p1.x(), p0.y(), hz,     1.0f, p1.x(), p1.y(), p1.z(), 1.0f};
    const float coord[]      = {0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f, 1.0f};
    const uint16_t indices[] = {0, 1, 2, 2, 1, 3};

    int32_t posLoc   = ctx.getAttribLocation(program, "a_position");
    int32_t coordLoc = ctx.getAttribLocation(program, "a_coord");

    ctx.useProgram(program);
    TCU_CHECK(posLoc >= 0);
    {
        ctx.enableVertexAttribArray(posLoc);
        ctx.vertexAttribPointer(posLoc, 4, GL_FLOAT, GL_FALSE, 0, &position[0]);
    }

    if (coordLoc >= 0)
    {
        ctx.enableVertexAttribArray(coordLoc);
        ctx.vertexAttribPointer(coordLoc, 2, GL_FLOAT, GL_FALSE, 0, &coord[0]);
    }

    ctx.drawElements(GL_TRIANGLES, DE_LENGTH_OF_ARRAY(indices), GL_UNSIGNED_SHORT, &indices[0]);

    if (posLoc >= 0)
        ctx.disableVertexAttribArray(posLoc);

    if (coordLoc >= 0)
        ctx.disableVertexAttribArray(coordLoc);
}

} // namespace sglr
