// Copyright (c) 2001-2010, Purdue University. All rights reserved.
// Copyright (C) 2015 Apple Inc. All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//  * Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
//  * Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//  * Neither the name of the Purdue University nor the
//    names of its contributors may be used to endorse or promote products
//    derived from this software without specific prior written permission.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE FOR ANY
// DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
// (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
// LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
// ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

load("constants.js");
load("util.js");
load("red_black_tree.js");
load("call_sign.js");
load("vector_2d.js");
load("vector_3d.js");
load("motion.js");
load("reduce_collision_set.js");

var makeMotion = (function() {
    var counter = 0;
    return function(x1, y1, z1, x2, y2, z2) {
        return new Motion(new CallSign("foo" + (++counter)),
                          new Vector3D(x1, y1, z1),
                          new Vector3D(x2, y2, z2));
    }
})();

function drawMotion(motion) {
    var voxelMap = new RedBlackTree();
    drawMotionOnVoxelMap(voxelMap, motion);
    return voxelMap;
}

function keys(voxelMap) {
    var result = "[";
    var first = true;
    voxelMap.forEach(function(key, value) {
        if (first)
            first = false;
        else
            result += ", ";
        result += key;
    });
    return result + "]";
}

function test(x1, y1, z1, x2, y2, z2, expected) {
    var motion = makeMotion(x1, y1, z1, x2, y2, z2);
    print(motion + ":");
    var actual = keys(drawMotion(motion));
    if (expected != actual)
        throw new Error("Wrong voxel map: " + actual);
    print("    Got: " + actual);
}

test(0, 0, 0, 1, 1, 1, "[[0, 0]]");
test(0, 0, 0, 2, 2, 2, "[[0, 0], [0, 2], [2, 0], [2, 2]]");
test(0, 0, 0, 4, 4, 4, "[[0, 0], [0, 2], [2, 0], [2, 2], [2, 4], [4, 2], [4, 4]]");
test(0, 0, 0, 10, 0, 0, "[[0, 0], [2, 0], [4, 0], [6, 0], [8, 0], [10, 0]]");
test(2, 0, 0, 0, 2, 2, "[[0, 0], [0, 2], [2, 0]]");
test(0, 2, 0, 2, 0, 2, "[[0, 0], [0, 2], [2, 0]]");
test(2, 2, 0, 0, 0, 2, "[[0, 0], [0, 2], [2, 0], [2, 2]]");
test(0, 0, 0, 2, 0, 0, "[[0, 0], [2, 0]]");
test(0, 0, 0, 0, 2, 0, "[[0, 0], [0, 2]]");
test(2, 0, 0, 0, 0, 0, "[[0, 0], [2, 0]]");
test(0, 2, 0, 0, 0, 0, "[[0, 0], [0, 2]]");
