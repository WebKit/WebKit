# -*- coding: utf-8 -*-

#-------------------------------------------------------------------------
# drawElements Quality Program utilities
# --------------------------------------
#
# Copyright 2017 The Android Open Source Project
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

import random
from genutil import *

random.seed(0x1234)

DATA_TYPES = ["float", "vec4"]
ARRAY_SIZES = [16, 32, 64, 128]

s_largeArrayCaseTemplate = """
case ${{NAME}}
    version 300 es
    values
    {
        ${{VALUES}}
    }

    both ""
        #version 300 es
        precision mediump float;

        ${DECLARATIONS}

        void main()
        {
            ${{ARRAY_DECL}}

            ${SETUP}
            ${{OP}}
            ${OUTPUT}
        }
    ""
end
"""[1:]


class LargeConstantArrayCase(ShaderCase):
    def __init__(self, name, array, inputs, outputs):
        self.name = name
        self.array = array
        self.inputs = inputs
        self.outputs = outputs
        self.op = "out0 = array[in0];"

    def __str__(self):
        params = {
            "NAME": self.name,
            "VALUES": genValues(self.inputs, self.outputs),
            "ARRAY_DECL": self.array,
            "OP": self.op
        }
        return fillTemplate(s_largeArrayCaseTemplate, params)


def genArray(dataType, size):
    elements = []
    for i in xrange(size):
        if dataType == "float":
            elements.append(Scalar(round(random.uniform(-1.0, 1.0), 6)))
        if dataType == "vec4":
            elements.append(Vec4(*[round(random.uniform(-1.0, 1.0), 6) for x in range(4)]))

    return elements


def arrayToString(elements):
    array = ('const {TYPE} array[{LENGTH}] = {TYPE}[](\n'
        .format(TYPE=elements[0].typeString(), LENGTH=len(elements)))

    array += "\n".join(str(e) + ',' for e in elements[:-1])
    array += "\n" + str(elements[-1])
    array += ");"

    return array

allCases = []
largeConstantArrayCases = []

for dataType in DATA_TYPES:
    for arraySize in ARRAY_SIZES:
        indexes = random.sample(range(arraySize-1), 10)
        array = genArray(dataType, arraySize)
        outputs = [array[index] for index in indexes]
        outType = outputs[0].typeString()
        caseName = "%s_%s" % (dataType, arraySize)

        case = LargeConstantArrayCase(caseName,
                          arrayToString(array),
                          [("int in0", indexes)],
                          [("%s out0" % outType, outputs)])

        largeConstantArrayCases.append(case)

allCases.append(CaseGroup("indexing", "Large constant array indexing", largeConstantArrayCases))

if __name__ == "__main__":
    print("Generating shader case files.")
    writeAllCases("large_constant_arrays.test", allCases)
