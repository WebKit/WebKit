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

from genutil import *

allCases = []

VERTEX = "VERTEX"
FRAGMENT = "FRAGMENT"

CASE_FRAGMENT_SHADER_TEMPLATE = """
case ${{NAME}}
    version 300 es
    expect ${{EXPECT}}

    vertex ""
        #version 300 es
        precision highp float;

        in vec4 a_pos;

        void main()
        {
            gl_Position = a_pos;
        }
    ""

    fragment ""
        ${{SOURCE}}
    ""
end"""[1:]

CASE_VERTEX_SHADER_TEMPLATE = """
case ${{NAME}}
    version 300 es
    expect ${{EXPECT}}

    vertex ""
        ${{SOURCE}}
    ""

    fragment ""
        #version 300 es
        precision highp float;

        layout(location=0) out vec4 o_color;

        void main()
        {
            o_color = vec4(1.0);
        }
    ""
end"""[1:]

class UniformBlockCase(ShaderCase):
    def __init__(self, name, shaderType, source, valid):
        self.name = name
        self.shaderType = shaderType
        self.source = source
        self.valid = valid

    def __str__(self):
        if self.shaderType == FRAGMENT:
            sourceParams = {
                "OUTPUT": "o_color",
                "OUTPUT_DECLARATION": "layout(location=0) out vec4 o_color;"
            }

            source = fillTemplate(self.source, sourceParams)

            testCaseParams = {
                "NAME": self.name,
                "SOURCE": source,
                "EXPECT": ("build_successful" if self.valid else "compile_fail")
            }

            return fillTemplate(CASE_FRAGMENT_SHADER_TEMPLATE, testCaseParams)
        elif self.shaderType == VERTEX:
            sourceParams = {
                "OUTPUT": "gl_Position",
                "OUTPUT_DECLARATION": ""
            }

            source = fillTemplate(self.source, sourceParams)

            testCaseParams = {
                "NAME": self.name,
                "SOURCE": source,
                "EXPECT": ("build_successful" if self.valid else "compile_fail")
            }

            return fillTemplate(CASE_VERTEX_SHADER_TEMPLATE, testCaseParams)

        assert False

def createCases(name, source, valid):
    return [UniformBlockCase(name + "_vertex", VERTEX, source, valid),
            UniformBlockCase(name + "_fragment", FRAGMENT, source, valid)]

repeatShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    uniform vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

layoutQualifierShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

layout(%s) uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

layoutGlobalQualifierShaderTemplate = """
#version 300 es
precision highp float;

layout(%s) uniform;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

layoutMemberQualifierShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    layout(%s) mat4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember[0];
}"""[1:]

layoutMemberVec4QualifierShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    layout(%s) vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

noInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
};

void main()
{
    ${{OUTPUT}} = uniformMember;
}"""[1:]

sameVariableAndInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    vec4 uniformBlock = vec4(0.0);
    ${{OUTPUT}} = uniformBlock;
}"""[1:]

sameVariableAndBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    vec4 UniformBlock = vec4(0.0);
    ${{OUTPUT}} = UniformBlock + uniformBlock.uniformMember;
}"""[1:]

repeatedBlockShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlockA;

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlockB;

void main()
{
    ${{OUTPUT}} = uniformBlockA.uniformMember + uniformBlockB.uniformMember;
}"""[1:]

repeatedBlockNoInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

uniform UniformBlock
{
    vec4 uniformMember;
};

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember + uniformMember;
}"""[1:]

structMemberShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

struct Struct
{
    vec4 uniformMember;
};

uniform UniformBlock
{
    Struct st;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.st.uniformMember;
}"""[1:]

layoutStructMemberQualifierShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

struct Struct
{
    vec4 uniformMember;
};

uniform UniformBlock
{
    layout(%s) Struct st;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.st.uniformMember;
}"""[1:]

longIdentifierBlockNameShaderTemplate = ("""
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

// Total of 1024 characters
uniform """ + ("a" * 1024) + """
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}""")[1:]

longIdentifierInstanceNameShaderTemplate = ("""
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} """ + ("a" * 1024) + """;
// Total of 1024 characters

void main()
{
    ${{OUTPUT}} = """ + ("a" * 1024) + """.uniformMember;
}""")[1:]

underscoreIdentifierInstanceNameShaderTemplate = ("""
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} _;

void main()
{
    ${{OUTPUT}} = _.uniformMember;
}""")[1:]

underscoreIdentifierBlockNameShaderTemplate = ("""
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform _
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}""")[1:]

validCases = (createCases("repeat_interface_qualifier", repeatShaderTemplate, True)
            + sum([createCases("layout_%s" % qualifier, layoutQualifierShaderTemplate % qualifier, True)
                        for qualifier in ["shared", "packed", "std140", "row_major", "column_major"]], [])
            + createCases("layout_all", layoutQualifierShaderTemplate % "shared, packed, std140, row_major, column_major", True)
            + createCases("layout_all_8_times", layoutQualifierShaderTemplate % str.join(", ", ["shared", "packed", "std140", "row_major", "column_major"] * 8), True)
            + sum([createCases("global_layout_%s" % qualifier, layoutGlobalQualifierShaderTemplate % qualifier, True)
                        for qualifier in ["shared", "packed", "std140", "row_major", "column_major"]], [])
            + createCases("global_layout_all", layoutGlobalQualifierShaderTemplate % "shared, packed, std140, row_major, column_major", True)
            + createCases("global_layout_all_8_times", layoutGlobalQualifierShaderTemplate % str.join(", ", ["shared", "packed", "std140", "row_major", "column_major"] * 8), True)
            + sum([createCases("member_layout_%s" % qualifier, layoutMemberQualifierShaderTemplate % qualifier, True)
                        for qualifier in ["row_major", "column_major"]], [])
            + sum([createCases("member_layout_%s_vec4" % qualifier, layoutMemberVec4QualifierShaderTemplate % qualifier, True)
                        for qualifier in ["row_major", "column_major"]], [])
            + createCases("member_layout_all", layoutMemberQualifierShaderTemplate % "row_major, column_major", True)
            + createCases("member_layout_all_8_times", layoutMemberQualifierShaderTemplate % str.join(", ", ["row_major", "column_major"] * 8), True)
            + createCases("no_instance_name", noInstanceNameShaderTemplate, True)
            + createCases("same_variable_and_block_name", sameVariableAndBlockNameShaderTemplate, True)
            + createCases("same_variable_and_instance_name", sameVariableAndInstanceNameShaderTemplate, True)
            + createCases("struct_member", structMemberShaderTemplate, True)
            + sum([createCases("struct_member_layout_%s" % qualifier, layoutStructMemberQualifierShaderTemplate % qualifier, True)
                        for qualifier in ["row_major", "column_major"]], [])
            + createCases("struct_member_layout_all", layoutStructMemberQualifierShaderTemplate % "row_major, column_major", True)
            + createCases("struct_member_layout_all_8_times", layoutStructMemberQualifierShaderTemplate % str.join(", ", ["row_major", "column_major"] * 8), True)
            + createCases("long_block_name", longIdentifierBlockNameShaderTemplate, True)
            + createCases("long_instance_name", longIdentifierInstanceNameShaderTemplate, True)
            + createCases("underscore_block_name", underscoreIdentifierBlockNameShaderTemplate, True)
            + createCases("underscore_instance_name", underscoreIdentifierInstanceNameShaderTemplate, True))

invalidMemberInterfaceQualifierShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    %s vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

conflictingInstanceNamesShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlockA
{
    vec4 uniformMember;
} uniformBlock;

uniform UniformBlockB
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

conflictingFunctionAndInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

float uniformBlock (float x)
{
    return x;
}

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

conflictingFunctionAndBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

float UniformBlock (float x)
{
    return x;
}

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

conflictingVariableAndInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

%s vec4 uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

conflictingVariableAndBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

%s vec4 UniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]


matchingInstanceAndBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} UniformBlock;

void main()
{
    ${{OUTPUT}} = UniformBlock.uniformMember;
}"""[1:]

referenceUsingBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = UniformBlock.uniformMember;
}"""[1:]

emptyBlockShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
} uniformBlock;

void main()
{
    ${{OUTPUT}} = vec4(0.0);
}"""[1:]

emptyLayoutShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

layout() uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

emptyGlobalLayoutShaderTemplate = """
#version 300 es
precision highp float;

layout() uniform;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

emptyMemberLayoutShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    layout() vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

invalidMemberLayoutShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    layout(%s) vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

structureDefinitionShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    struct A
    {
        vec4 uniformMember;
    } a;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.a.uniformMember;
}"""[1:]

samplerShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    sampler2D sampler;
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

missingBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

invalidNumberBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform 0UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

invalidHashBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform #UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

invalidDollarBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform $UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

invalidIdentifierBlockNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform gl_UniformBlock
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}"""[1:]

tooLongIdentifierBlockNameShaderTemplate = ("""
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

// Total of 1025 characters
uniform """ + ("a" * 1025) + """
{
    vec4 uniformMember;
} uniformBlock;

void main()
{
    ${{OUTPUT}} = uniformBlock.uniformMember;
}""")[1:]

invalidNumberInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformInstance
{
    vec4 uniformMember;
} 0uniformBlock;

void main()
{
    ${{OUTPUT}} = 0uniformBlock.uniformMember;
}"""[1:]

invalidHashInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformInstance
{
    vec4 uniformMember;
} #uniformBlock;

void main()
{
    ${{OUTPUT}} = #uniformBlock.uniformMember;
}"""[1:]

invalidDollarInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformInstance
{
    vec4 uniformMember;
} $uniformBlock;

void main()
{
    ${{OUTPUT}} = $uniformBlock.uniformMember;
}"""[1:]

invalidIdentifierInstanceNameShaderTemplate = """
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} gl_uniformBlock;

void main()
{
    ${{OUTPUT}} = gl_uniformBlock.uniformMember;
}"""[1:]

tooLongIdentifierInstanceNameShaderTemplate = ("""
#version 300 es
precision highp float;

${{OUTPUT_DECLARATION}}

uniform UniformBlock
{
    vec4 uniformMember;
} """ + ("a" * 1025) + """;
// Total of 1025 characters

void main()
{
    ${{OUTPUT}} = """ + ("a" * 1025) + """.uniformMember;
}""")[1:]

invalidCases = (
            sum([createCases("member_%s_interface_qualifier" % qualifier, invalidMemberInterfaceQualifierShaderTemplate % qualifier, False)
                    for qualifier in ["in", "out", "buffer", "attribute", "varying"]], [])
            + createCases("conflicting_instance_names", conflictingInstanceNamesShaderTemplate, False)
            + createCases("conflicting_function_and_instance_name", conflictingFunctionAndInstanceNameShaderTemplate, False)
            + createCases("conflicting_function_and_block_name", conflictingFunctionAndBlockNameShaderTemplate, False)
            + sum([createCases("conflicting_%s_and_instance_name" % qualifier, conflictingVariableAndInstanceNameShaderTemplate % qualifier, False)
                    for qualifier in ["uniform", "in", "out"]], [])
            + sum([createCases("conflicting_%s_and_block_name" % qualifier, conflictingVariableAndBlockNameShaderTemplate % qualifier, False)
                    for qualifier in ["uniform", "in", "out"]], [])
            + createCases("matching_instance_and_block_name", matchingInstanceAndBlockNameShaderTemplate, False)
            + createCases("reference_using_block_name", referenceUsingBlockNameShaderTemplate, False)
            + createCases("empty_block", emptyBlockShaderTemplate, False)
            + createCases("empty_layout", emptyLayoutShaderTemplate, False)
            + createCases("empty_member_layout", emptyMemberLayoutShaderTemplate, False)
            + createCases("empty_global_layout", emptyGlobalLayoutShaderTemplate, False)
            + createCases("structure_definition", structureDefinitionShaderTemplate, False)
            + sum([createCases("member_layout_%s" % qualifier, invalidMemberLayoutShaderTemplate % qualifier, False)
                    for qualifier in ["shared", "packed", "std140"]], [])
            + createCases("missing_block_name", missingBlockNameShaderTemplate, False)
            + createCases("invalid_number_block_name", invalidNumberBlockNameShaderTemplate, False)
            + createCases("invalid_hash_block_name", invalidHashBlockNameShaderTemplate, False)
            + createCases("invalid_dollar_block_name", invalidDollarBlockNameShaderTemplate, False)
            + createCases("invalid_identifier_block_name", invalidIdentifierBlockNameShaderTemplate, False)
            + createCases("too_long_block_name", tooLongIdentifierBlockNameShaderTemplate, False)
            + createCases("invalid_number_instance_name", invalidNumberInstanceNameShaderTemplate, False)
            + createCases("invalid_hash_instance_name", invalidDollarInstanceNameShaderTemplate, False)
            + createCases("invalid_dollar_instance_name", invalidDollarInstanceNameShaderTemplate, False)
            + createCases("invalid_identifier_instance_name", invalidIdentifierInstanceNameShaderTemplate, False)
            + createCases("repeated_block", repeatedBlockShaderTemplate, False)
            + createCases("repeated_block_no_instance_name", repeatedBlockNoInstanceNameShaderTemplate, False)
        )

allCases.append(CaseGroup("valid", "Valid uniform interface block syntax tests.", validCases))
allCases.append(CaseGroup("invalid", "Invalid uniform interface block syntax tests.", invalidCases))

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("uniform_block.test", allCases)
