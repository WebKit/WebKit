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
load("call_sign.js");
load("vector_2d.js");
load("vector_3d.js");
load("motion.js");

var epsilon = 0.000001;

function checkDoesIntersect(motionOne, motionTwo, expected) {
    print(motionOne + " and " + motionTwo + ":");
    var actual = motionOne.findIntersection(motionTwo);
    if (!actual)
        throw new Error("Was supposed to intersect at " + expected + " but didn't");
    
    var delta = actual.minus(expected).magnitude();
    if (delta > epsilon) {
        throw new Error("Was supposed to intersect at " + expected + " but intersected at " +
                        actual + ", which is " + delta + " away");
    }
    
    print("    Intersected at " + actual + ", which is " + delta + " away from " + expected + ".");
}

function checkDoesNotIntersect(motionOne, motionTwo) {
    print(motionOne + " and " + motionTwo + ":");
    var actual = motionOne.findIntersection(motionTwo);
    if (actual)
        throw new Error("Was not supposed to intersect but intersected at " + actual);
    
    print("    No intersection, as expected.");
}

var makeMotion = (function() {
    var counter = 0;
    return function(x1, y1, z1, x2, y2, z2) {
        return new Motion(new CallSign("foo" + (++counter)),
                          new Vector3D(x1, y1, z1),
                          new Vector3D(x2, y2, z2));
    }
})();

checkDoesNotIntersect(
    makeMotion(0, 0, 0, 10, 0, 0),
    makeMotion(0, 10, 0, 10, 10, 0));

checkDoesNotIntersect(
    makeMotion(0, 0, 0, 10, 0, 0),
    makeMotion(0, 1 + epsilon, 0, 10, 1 + epsilon, 0));

checkDoesIntersect(
    makeMotion(0, 0, 0, 10, 0, 0),
    makeMotion(0, 1, 0, 10, 1, 0),
    new Vector3D(0, 0.5, 0));

checkDoesIntersect(
    makeMotion(0, 0, 0, 10, 0, 0),
    makeMotion(0, 10, 0, 10, 0, 0),
    new Vector3D(9, 0.5, 0));

checkDoesIntersect(
    makeMotion(0, 0, 0, 9.5, 0, 0),
    makeMotion(0, 10, 0, 9.65, 0.35, 0),
    new Vector3D(8.939830722446178, 0.49507224691354423, 0));

// FIXME: Add more tests here. Sadly, the original Java code for this benchmark had an extensive
// JUnit test suite for intersections, but this didn't get included in any of the releases and I no
// longer have access to the original CVS repository (yes, CVS).
