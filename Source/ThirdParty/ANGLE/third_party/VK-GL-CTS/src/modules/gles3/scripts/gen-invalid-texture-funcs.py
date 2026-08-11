# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2015 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
#-------------------------------------------------------------------------

import sys
import string
from genutil import *

# Templates

INVALID_TEXTURE_FUNC_TEMPLATE = """
case ${{NAME}}
    expect compile_fail
    values {}
    version 300 es

    both ""
        #version 300 es
        precision mediump float;
        ${DECLARATIONS}
        uniform mediump ${{SAMPLERTYPE}} s;

        void main()
        {
            ${SETUP}
            ${POSITION_FRAG_COLOR} = vec4(${{LOOKUP}});
            ${OUTPUT}
        }
    ""
end
"""[1:-1]

# Classes

def getValueExpr (argType):
    return "%s(0)" % argType

class InvalidTexFuncCase(ShaderCase):
    def __init__(self, funcname, args):
        self.name = string.join([s.lower() for s in [funcname] + args], "_")
        self.funcname = funcname
        self.args = args

    def __str__(self):
        samplerType = self.args[0]

        lookup = self.funcname + "(s"
        for arg in self.args[1:]:
            lookup += ", %s" % getValueExpr(arg)
        lookup += ")"

        params = { "NAME": self.name, "SAMPLERTYPE": samplerType, "LOOKUP": lookup }
        return fillTemplate(INVALID_TEXTURE_FUNC_TEMPLATE, params)

# Invalid lookup cases
# \note Does not include cases that don't make sense

INVALID_TEX_FUNC_CASES = [
    # texture
    InvalidTexFuncCase("texture", ["sampler3DShadow", "vec4"]),
    InvalidTexFuncCase("texture", ["sampler2DArrayShadow", "vec4", "float"]),

    # textureProj
    InvalidTexFuncCase("textureProj", ["samplerCube", "vec4"]),
    InvalidTexFuncCase("textureProj", ["isamplerCube", "vec4"]),
    InvalidTexFuncCase("textureProj", ["usamplerCube", "vec4"]),
    InvalidTexFuncCase("textureProj", ["samplerCube", "vec4", "float"]),
    InvalidTexFuncCase("textureProj", ["isamplerCube", "vec4", "float"]),
    InvalidTexFuncCase("textureProj", ["usamplerCube", "vec4", "float"]),
    InvalidTexFuncCase("textureProj", ["sampler2DArrayShadow", "vec4"]),
    InvalidTexFuncCase("textureProj", ["sampler2DArrayShadow", "vec4", "float"]),

    # textureLod
    InvalidTexFuncCase("textureLod", ["samplerCubeShadow", "vec4", "float"]),
    InvalidTexFuncCase("textureLod", ["sampler2DArrayShadow", "vec4", "float"]),

    # textureOffset
    InvalidTexFuncCase("textureOffset", ["samplerCube", "vec3", "ivec2"]),
    InvalidTexFuncCase("textureOffset", ["isamplerCube", "vec3", "ivec2"]),
    InvalidTexFuncCase("textureOffset", ["usamplerCube", "vec3", "ivec2"]),
    InvalidTexFuncCase("textureOffset", ["samplerCube", "vec3", "ivec3"]),
    InvalidTexFuncCase("textureOffset", ["isamplerCube", "vec3", "ivec3"]),
    InvalidTexFuncCase("textureOffset", ["usamplerCube", "vec3", "ivec3"]),
    InvalidTexFuncCase("textureOffset", ["samplerCube", "vec3", "ivec2", "float"]),
    InvalidTexFuncCase("textureOffset", ["samplerCube", "vec3", "ivec3", "float"]),
    InvalidTexFuncCase("textureOffset", ["sampler2DArray", "vec3", "ivec3"]),
    InvalidTexFuncCase("textureOffset", ["sampler2DArray", "vec3", "ivec3", "float"]),
    InvalidTexFuncCase("textureOffset", ["samplerCubeShadow", "vec4", "ivec2"]),
    InvalidTexFuncCase("textureOffset", ["samplerCubeShadow", "vec4", "ivec3"]),
    InvalidTexFuncCase("textureOffset", ["sampler2DArrayShadow", "vec4", "ivec2"]),
    InvalidTexFuncCase("textureOffset", ["sampler2DArrayShadow", "vec4", "ivec2", "float"]),

    # texelFetch
    InvalidTexFuncCase("texelFetch", ["samplerCube", "ivec3", "int"]),
    InvalidTexFuncCase("texelFetch", ["isamplerCube", "ivec3", "int"]),
    InvalidTexFuncCase("texelFetch", ["usamplerCube", "ivec3", "int"]),
    InvalidTexFuncCase("texelFetch", ["sampler2DShadow", "ivec2", "int"]),
    InvalidTexFuncCase("texelFetch", ["samplerCubeShadow", "ivec3", "int"]),
    InvalidTexFuncCase("texelFetch", ["sampler2DArrayShadow", "ivec3", "int"]),

    # texelFetchOffset
    InvalidTexFuncCase("texelFetch", ["samplerCube", "ivec3", "int", "ivec3"]),
    InvalidTexFuncCase("texelFetch", ["sampler2DShadow", "ivec2", "int", "ivec2"]),
    InvalidTexFuncCase("texelFetch", ["samplerCubeShadow", "ivec3", "int", "ivec3"]),
    InvalidTexFuncCase("texelFetch", ["sampler2DArrayShadow", "ivec3", "int", "ivec3"]),

    # textureProjOffset
    InvalidTexFuncCase("textureProjOffset", ["samplerCube", "vec4", "ivec2"]),
    InvalidTexFuncCase("textureProjOffset", ["samplerCube", "vec4", "ivec3"]),
    InvalidTexFuncCase("textureProjOffset", ["samplerCubeShadow", "vec4", "ivec3"]),
    InvalidTexFuncCase("textureProjOffset", ["sampler2DArrayShadow", "vec4", "ivec2"]),
    InvalidTexFuncCase("textureProjOffset", ["sampler2DArrayShadow", "vec4", "ivec3"]),

    # textureLodOffset
    InvalidTexFuncCase("textureLodOffset", ["samplerCube", "vec3", "float", "ivec2"]),
    InvalidTexFuncCase("textureLodOffset", ["samplerCube", "vec3", "float", "ivec3"]),
    InvalidTexFuncCase("textureLodOffset", ["samplerCubeShadow", "vec3", "float", "ivec3"]),
    InvalidTexFuncCase("textureLodOffset", ["sampler2DArrayShadow", "vec3", "float", "ivec2"]),
    InvalidTexFuncCase("textureLodOffset", ["sampler2DArrayShadow", "vec3", "float", "ivec3"]),

    # textureProjLod
    InvalidTexFuncCase("textureProjLod", ["samplerCube", "vec4", "float"]),
    InvalidTexFuncCase("textureProjLod", ["sampler2DArray", "vec4", "float"]),
    InvalidTexFuncCase("textureProjLod", ["sampler2DArrayShadow", "vec4", "float"]),

    # textureGrad
    InvalidTexFuncCase("textureGrad", ["sampler2DArray", "vec3", "vec3", "vec3"]),

    # textureGradOffset
    InvalidTexFuncCase("textureGradOffset", ["samplerCube", "vec3", "vec3", "vec3", "ivec2"]),
    InvalidTexFuncCase("textureGradOffset", ["samplerCube", "vec3", "vec3", "vec3", "ivec3"]),
    InvalidTexFuncCase("textureGradOffset", ["samplerCubeShadow", "vec4", "vec3", "vec3", "ivec2"]),
    InvalidTexFuncCase("textureGradOffset", ["samplerCubeShadow", "vec4", "vec3", "vec3", "ivec3"]),

    # textureProjGrad
    InvalidTexFuncCase("textureProjGrad", ["samplerCube", "vec4", "vec3", "vec3"]),
    InvalidTexFuncCase("textureProjGrad", ["sampler2DArray", "vec4", "vec2", "vec2"]),

    # textureProjGradOffset
    InvalidTexFuncCase("textureProjGradOffset", ["samplerCube", "vec4", "vec3", "vec3", "ivec2"]),
    InvalidTexFuncCase("textureProjGradOffset", ["samplerCube", "vec4", "vec3", "vec3", "ivec3"]),
    InvalidTexFuncCase("textureProjGradOffset", ["sampler2DArray", "vec4", "vec2", "vec2", "ivec2"]),
    InvalidTexFuncCase("textureProjGradOffset", ["sampler2DArray", "vec4", "vec2", "vec2", "ivec3"])
]

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("invalid_texture_functions.test", INVALID_TEX_FUNC_CASES)
