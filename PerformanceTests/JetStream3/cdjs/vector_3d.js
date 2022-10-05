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

function Vector3D(x, y, z) {
    this.x = x;
    this.y = y;
    this.z = z;
}

Vector3D.prototype.plus = function(other) {
    return new Vector3D(this.x + other.x,
                        this.y + other.y,
                        this.z + other.z);
};

Vector3D.prototype.minus = function(other) {
    return new Vector3D(this.x - other.x,
                        this.y - other.y,
                        this.z - other.z);
};

Vector3D.prototype.dot = function(other) {
    return this.x * other.x + this.y * other.y + this.z * other.z;
};

Vector3D.prototype.squaredMagnitude = function() {
    return this.dot(this);
};

Vector3D.prototype.magnitude = function() {
    return Math.sqrt(this.squaredMagnitude());
};

Vector3D.prototype.times = function(amount) {
    return new Vector3D(this.x * amount,
                        this.y * amount,
                        this.z * amount);
};

Vector3D.prototype.as2D = function() {
    return new Vector2D(this.x, this.y);
};

Vector3D.prototype.toString = function() {
    return "[" + this.x + ", " + this.y + ", " + this.z + "]";
};


