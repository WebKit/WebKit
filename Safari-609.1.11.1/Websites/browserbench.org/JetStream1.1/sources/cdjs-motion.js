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

function Motion(callsign, posOne, posTwo) {
    this.callsign = callsign;
    this.posOne = posOne;
    this.posTwo = posTwo;
}

Motion.prototype.toString = function() {
    return "Motion(" + this.callsign + " from " + this.posOne + " to " + this.posTwo + ")";
};

Motion.prototype.delta = function() {
    return this.posTwo.minus(this.posOne);
};

Motion.prototype.findIntersection = function(other) {
    var init1 = this.posOne;
    var init2 = other.posOne;
    var vec1 = this.delta();
    var vec2 = other.delta();
    var radius = Constants.PROXIMITY_RADIUS;
    
    // this test is not geometrical 3-d intersection test, it takes the fact that the aircraft move
    // into account ; so it is more like a 4d test
    // (it assumes that both of the aircraft have a constant speed over the tested interval)
    
    // we thus have two points, each of them moving on its line segment at constant speed ; we are looking
    // for times when the distance between these two points is smaller than r 
    
    // vec1 is vector of aircraft 1
    // vec2 is vector of aircraft 2
    
    // a = (V2 - V1)^T * (V2 - V1)
    var a = vec2.minus(vec1).squaredMagnitude();
    
    if (a != 0) {
        // we are first looking for instances of time when the planes are exactly r from each other
        // at least one plane is moving ; if the planes are moving in parallel, they do not have constant speed

        // if the planes are moving in parallel, then
        //   if the faster starts behind the slower, we can have 2, 1, or 0 solutions
        //   if the faster plane starts in front of the slower, we can have 0 or 1 solutions

        // if the planes are not moving in parallel, then


        // point P1 = I1 + vV1
        // point P2 = I2 + vV2
        //   - looking for v, such that dist(P1,P2) = || P1 - P2 || = r

        // it follows that || P1 - P2 || = sqrt( < P1-P2, P1-P2 > )
        //   0 = -r^2 + < P1 - P2, P1 - P2 >
        //  from properties of dot product
        //   0 = -r^2 + <I1-I2,I1-I2> + v * 2<I1-I2, V1-V2> + v^2 *<V1-V2,V1-V2>
        //   so we calculate a, b, c - and solve the quadratic equation
        //   0 = c + bv + av^2

        // b = 2 * <I1-I2, V1-V2>

        var b = 2 * init1.minus(init2).dot(vec1.minus(vec2));

        // c = -r^2 + (I2 - I1)^T * (I2 - I1)
        var c = -radius * radius + init2.minus(init1).squaredMagnitude();

        var discr = b * b - 4 * a * c;
        if (discr < 0)
            return null;

        var v1 = (-b - Math.sqrt(discr)) / (2 * a);
        var v2 = (-b + Math.sqrt(discr)) / (2 * a);

        if (v1 <= v2 && ((v1 <= 1 && 1 <= v2) ||
                         (v1 <= 0 && 0 <= v2) ||
                         (0 <= v1 && v2 <= 1))) {
            // Pick a good "time" at which to report the collision.
            var v;
            if (v1 <= 0) {
                // The collision started before this frame. Report it at the start of the frame.
                v = 0;
            } else {
                // The collision started during this frame. Report it at that moment.
                v = v1;
            }
            
            var result1 = init1.plus(vec1.times(v));
            var result2 = init2.plus(vec2.times(v));
            
            var result = result1.plus(result2).times(0.5);
            if (result.x >= Constants.MIN_X &&
                result.x <= Constants.MAX_X &&
                result.y >= Constants.MIN_Y &&
                result.y <= Constants.MAX_Y &&
                result.z >= Constants.MIN_Z &&
                result.z <= Constants.MAX_Z)
                return result;
        }

        return null;
    }
    
    // the planes have the same speeds and are moving in parallel (or they are not moving at all)
    // they  thus have the same distance all the time ; we calculate it from the initial point
    
    // dist = || i2 - i1 || = sqrt(  ( i2 - i1 )^T * ( i2 - i1 ) )
    
    var dist = init2.minus(init1).magnitude();
    if (dist <= radius)
        return init1.plus(init2).times(0.5);
    
    return null;
};

