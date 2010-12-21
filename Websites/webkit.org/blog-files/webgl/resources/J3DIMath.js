/*
 * Copyright (C) 2009 Apple Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

 // J3DI (Jedi) - A support library for WebGL.

/*
    J3DI Math Classes. Currently includes:

        J3DIMatrix4 - A 4x4 Matrix
*/

/*
    J3DIMatrix4 class

    This class implements a 4x4 matrix. It has functions which duplicate the
    functionality of the OpenGL matrix stack and glut functions. On browsers
    that support it, CSSMatrix is used to accelerate operations.

    IDL:

    [
        Constructor(in J3DIMatrix4 matrix),                 // copy passed matrix into new J3DIMatrix4
        Constructor(in sequence<float> array)               // create new J3DIMatrix4 with 16 floats (row major)
        Constructor()                                       // create new J3DIMatrix4 with identity matrix
    ]
    interface J3DIMatrix4 {
        void load(in J3DIMatrix4 matrix);                   // copy the values from the passed matrix
        void load(in sequence<float> array);                // copy 16 floats into the matrix
        sequence<float> getAsArray();                       // return the matrix as an array of 16 floats
        Float32Array getAsFloat32Array();             // return the matrix as a Float32Array with 16 values
        void setUniform(in WebGLRenderingContext ctx,       // Send the matrix to the passed uniform location in the passed context
                        in WebGLUniformLocation loc,
                        in boolean transpose);
        void makeIdentity();                                // replace the matrix with identity
        void transpose();                                   // replace the matrix with its transpose
        void invert();                                      // replace the matrix with its inverse

        void translate(in float x, in float y, in float z); // multiply the matrix by passed translation values on the right
        void translate(in J3DVector3 v);                    // multiply the matrix by passed translation values on the right
        void scale(in float x, in float y, in float z);     // multiply the matrix by passed scale values on the right
        void scale(in J3DVector3 v);                        // multiply the matrix by passed scale values on the right
        void rotate(in float angle,                         // multiply the matrix by passed rotation values on the right
                    in float x, in float y, in float z);    // (angle is in degrees)
        void rotate(in float angle, in J3DVector3 v);       // multiply the matrix by passed rotation values on the right
                                                            // (angle is in degrees)
        void multiply(in CanvasMatrix matrix);              // multiply the matrix by the passed matrix on the right
        void divide(in float divisor);                      // divide the matrix by the passed divisor
        void ortho(in float left, in float right,           // multiply the matrix by the passed ortho values on the right
                   in float bottom, in float top,
                   in float near, in float far);
        void frustum(in float left, in float right,         // multiply the matrix by the passed frustum values on the right
                     in float bottom, in float top,
                     in float near, in float far);
        void perspective(in float fovy, in float aspect,    // multiply the matrix by the passed perspective values on the right
                         in float zNear, in float zFar);
        void lookat(in J3DVector3 eye,                      // multiply the matrix by the passed lookat
                in J3DVector3 center,  in J3DVector3 up);   // values on the right
         bool decompose(in J3DVector3 translate,            // decompose the matrix into the passed vector
                        in J3DVector3 rotate,
                        in J3DVector3 scale,
                        in J3DVector3 skew,
                        in sequence<float> perspective);
    }

    [
        Constructor(in J3DVector3 vector),                  // copy passed vector into new J3DVector3
        Constructor(in sequence<float> array)               // create new J3DVector3 with 3 floats from array
        Constructor(in float x, in float y, in float z)     // create new J3DVector3 with 3 floats
        Constructor()                                       // create new J3DVector3 with (0,0,0)
    ]
    interface J3DVector3 {
        void load(in J3DVector3 vector);                    // copy the values from the passed vector
        void load(in sequence<float> array);                // copy 3 floats into the vector from array
        void load(in float x, in float y, in float z);      // copy 3 floats into the vector
        sequence<float> getAsArray();                       // return the vector as an array of 3 floats
        Float32Array getAsFloat32Array();             // return the matrix as a Float32Array with 16 values
        void multMatrix(in J3DIMatrix4 matrix);             // multiply the vector by the passed matrix (on the right)
        float vectorLength();                               // return the length of the vector
        float dot();                                        // return the dot product of the vector
        void cross(in J3DVector3 v);                        // replace the vector with vector x v
        void divide(in float divisor);                      // divide the vector by the passed divisor
    }
*/

J3DIHasCSSMatrix = false;
J3DIHasCSSMatrixCopy = false;
/*
if ("WebKitCSSMatrix" in window && ("media" in window && window.media.matchMedium("(-webkit-transform-3d)")) ||
                                   ("styleMedia" in window && window.styleMedia.matchMedium("(-webkit-transform-3d)"))) {
    J3DIHasCSSMatrix = true;
    if ("copy" in WebKitCSSMatrix.prototype)
        J3DIHasCSSMatrixCopy = true;
}
*/

//  console.log("J3DIHasCSSMatrix="+J3DIHasCSSMatrix);
//  console.log("J3DIHasCSSMatrixCopy="+J3DIHasCSSMatrixCopy);

//
// J3DIMatrix4
//
J3DIMatrix4 = function(m)
{
    if (J3DIHasCSSMatrix)
        this.$matrix = new WebKitCSSMatrix;
    else
        this.$matrix = new Object;

    if (typeof m == 'object') {
        if ("length" in m && m.length >= 16) {
            this.load(m);
            return;
        }
        else if (m instanceof J3DIMatrix4) {
            this.load(m);
            return;
        }
    }
    this.makeIdentity();
}

J3DIMatrix4.prototype.load = function()
{
    if (arguments.length == 1 && typeof arguments[0] == 'object') {
        var matrix;

        if (arguments[0] instanceof J3DIMatrix4) {
            matrix = arguments[0].$matrix;

            this.$matrix.m11 = matrix.m11;
            this.$matrix.m12 = matrix.m12;
            this.$matrix.m13 = matrix.m13;
            this.$matrix.m14 = matrix.m14;

            this.$matrix.m21 = matrix.m21;
            this.$matrix.m22 = matrix.m22;
            this.$matrix.m23 = matrix.m23;
            this.$matrix.m24 = matrix.m24;

            this.$matrix.m31 = matrix.m31;
            this.$matrix.m32 = matrix.m32;
            this.$matrix.m33 = matrix.m33;
            this.$matrix.m34 = matrix.m34;

            this.$matrix.m41 = matrix.m41;
            this.$matrix.m42 = matrix.m42;
            this.$matrix.m43 = matrix.m43;
            this.$matrix.m44 = matrix.m44;
            return;
        }
        else
            matrix = arguments[0];

        if ("length" in matrix && matrix.length >= 16) {
            this.$matrix.m11 = matrix[0];
            this.$matrix.m12 = matrix[1];
            this.$matrix.m13 = matrix[2];
            this.$matrix.m14 = matrix[3];

            this.$matrix.m21 = matrix[4];
            this.$matrix.m22 = matrix[5];
            this.$matrix.m23 = matrix[6];
            this.$matrix.m24 = matrix[7];

            this.$matrix.m31 = matrix[8];
            this.$matrix.m32 = matrix[9];
            this.$matrix.m33 = matrix[10];
            this.$matrix.m34 = matrix[11];

            this.$matrix.m41 = matrix[12];
            this.$matrix.m42 = matrix[13];
            this.$matrix.m43 = matrix[14];
            this.$matrix.m44 = matrix[15];
            return;
        }
    }

    this.makeIdentity();
}

J3DIMatrix4.prototype.getAsArray = function()
{
    return [
        this.$matrix.m11, this.$matrix.m12, this.$matrix.m13, this.$matrix.m14,
        this.$matrix.m21, this.$matrix.m22, this.$matrix.m23, this.$matrix.m24,
        this.$matrix.m31, this.$matrix.m32, this.$matrix.m33, this.$matrix.m34,
        this.$matrix.m41, this.$matrix.m42, this.$matrix.m43, this.$matrix.m44
    ];
}

J3DIMatrix4.prototype.getAsFloat32Array = function()
{
    if (J3DIHasCSSMatrixCopy) {
        var array = new Float32Array(16);
        this.$matrix.copy(array);
        return array;
    }
    return new Float32Array(this.getAsArray());
}

J3DIMatrix4.prototype.setUniform = function(ctx, loc, transpose)
{
    if (J3DIMatrix4.setUniformArray == undefined) {
        J3DIMatrix4.setUniformWebGLArray = new Float32Array(16);
        J3DIMatrix4.setUniformArray = new Array(16);
    }

    if (J3DIHasCSSMatrixCopy)
        this.$matrix.copy(J3DIMatrix4.setUniformWebGLArray);
    else {
        J3DIMatrix4.setUniformArray[0] = this.$matrix.m11;
        J3DIMatrix4.setUniformArray[1] = this.$matrix.m12;
        J3DIMatrix4.setUniformArray[2] = this.$matrix.m13;
        J3DIMatrix4.setUniformArray[3] = this.$matrix.m14;
        J3DIMatrix4.setUniformArray[4] = this.$matrix.m21;
        J3DIMatrix4.setUniformArray[5] = this.$matrix.m22;
        J3DIMatrix4.setUniformArray[6] = this.$matrix.m23;
        J3DIMatrix4.setUniformArray[7] = this.$matrix.m24;
        J3DIMatrix4.setUniformArray[8] = this.$matrix.m31;
        J3DIMatrix4.setUniformArray[9] = this.$matrix.m32;
        J3DIMatrix4.setUniformArray[10] = this.$matrix.m33;
        J3DIMatrix4.setUniformArray[11] = this.$matrix.m34;
        J3DIMatrix4.setUniformArray[12] = this.$matrix.m41;
        J3DIMatrix4.setUniformArray[13] = this.$matrix.m42;
        J3DIMatrix4.setUniformArray[14] = this.$matrix.m43;
        J3DIMatrix4.setUniformArray[15] = this.$matrix.m44;

        J3DIMatrix4.setUniformWebGLArray.set(J3DIMatrix4.setUniformArray);
    }

    ctx.uniformMatrix4fv(loc, transpose, J3DIMatrix4.setUniformWebGLArray);
}

J3DIMatrix4.prototype.makeIdentity = function()
{
    this.$matrix.m11 = 1;
    this.$matrix.m12 = 0;
    this.$matrix.m13 = 0;
    this.$matrix.m14 = 0;

    this.$matrix.m21 = 0;
    this.$matrix.m22 = 1;
    this.$matrix.m23 = 0;
    this.$matrix.m24 = 0;

    this.$matrix.m31 = 0;
    this.$matrix.m32 = 0;
    this.$matrix.m33 = 1;
    this.$matrix.m34 = 0;

    this.$matrix.m41 = 0;
    this.$matrix.m42 = 0;
    this.$matrix.m43 = 0;
    this.$matrix.m44 = 1;
}

J3DIMatrix4.prototype.transpose = function()
{
    var tmp = this.$matrix.m12;
    this.$matrix.m12 = this.$matrix.m21;
    this.$matrix.m21 = tmp;

    tmp = this.$matrix.m13;
    this.$matrix.m13 = this.$matrix.m31;
    this.$matrix.m31 = tmp;

    tmp = this.$matrix.m14;
    this.$matrix.m14 = this.$matrix.m41;
    this.$matrix.m41 = tmp;

    tmp = this.$matrix.m23;
    this.$matrix.m23 = this.$matrix.m32;
    this.$matrix.m32 = tmp;

    tmp = this.$matrix.m24;
    this.$matrix.m24 = this.$matrix.m42;
    this.$matrix.m42 = tmp;

    tmp = this.$matrix.m34;
    this.$matrix.m34 = this.$matrix.m43;
    this.$matrix.m43 = tmp;
}

J3DIMatrix4.prototype.invert = function()
{
    if (J3DIHasCSSMatrix) {
        this.$matrix = this.$matrix.inverse();
        return;
    }

    // Calculate the 4x4 determinant
    // If the determinant is zero,
    // then the inverse matrix is not unique.
    var det = this._determinant4x4();

    if (Math.abs(det) < 1e-8)
        return null;

    this._makeAdjoint();

    // Scale the adjoint matrix to get the inverse
    this.$matrix.m11 /= det;
    this.$matrix.m12 /= det;
    this.$matrix.m13 /= det;
    this.$matrix.m14 /= det;

    this.$matrix.m21 /= det;
    this.$matrix.m22 /= det;
    this.$matrix.m23 /= det;
    this.$matrix.m24 /= det;

    this.$matrix.m31 /= det;
    this.$matrix.m32 /= det;
    this.$matrix.m33 /= det;
    this.$matrix.m34 /= det;

    this.$matrix.m41 /= det;
    this.$matrix.m42 /= det;
    this.$matrix.m43 /= det;
    this.$matrix.m44 /= det;
}

J3DIMatrix4.prototype.translate = function(x,y,z)
{
    if (typeof x == 'object' && "length" in x) {
        var t = x;
        x = t[0];
        y = t[1];
        z = t[2];
    }
    else {
        if (x == undefined)
            x = 0;
        if (y == undefined)
            y = 0;
        if (z == undefined)
            z = 0;
    }

    if (J3DIHasCSSMatrix) {
        this.$matrix = this.$matrix.translate(x, y, z);
        return;
    }

    var matrix = new J3DIMatrix4();
    matrix.$matrix.m41 = x;
    matrix.$matrix.m42 = y;
    matrix.$matrix.m43 = z;

    this.multiply(matrix);
}

J3DIMatrix4.prototype.scale = function(x,y,z)
{
    if (typeof x == 'object' && "length" in x) {
        var t = x;
        x = t[0];
        y = t[1];
        z = t[2];
    }
    else {
        if (x == undefined)
            x = 1;
        if (z == undefined) {
            if (y == undefined) {
                y = x;
                z = x;
            }
            else
                z = 1;
        }
        else if (y == undefined)
            y = x;
    }

    if (J3DIHasCSSMatrix) {
        this.$matrix = this.$matrix.scale(x, y, z);
        return;
    }

    var matrix = new J3DIMatrix4();
    matrix.$matrix.m11 = x;
    matrix.$matrix.m22 = y;
    matrix.$matrix.m33 = z;

    this.multiply(matrix);
}

J3DIMatrix4.prototype.rotate = function(angle,x,y,z)
{
    // Forms are (angle, x,y,z), (angle,vector), (angleX, angleY, angleZ), (angle)
    if (typeof x == 'object' && "length" in x) {
        var t = x;
        x = t[0];
        y = t[1];
        z = t[2];
    }
    else {
        if (arguments.length == 1) {
            x = 0;
            y = 0;
            z = 1;
        }
        else if (arguments.length == 3) {
            this.rotate(angle, 1,0,0); // about X axis
            this.rotate(x, 0,1,0); // about Y axis
            this.rotate(y, 0,0,1); // about Z axis
            return;
        }
    }

    if (J3DIHasCSSMatrix) {
        this.$matrix = this.$matrix.rotateAxisAngle(x, y, z, angle);
        return;
    }

    // angles are in degrees. Switch to radians
    angle = angle / 180 * Math.PI;

    angle /= 2;
    var sinA = Math.sin(angle);
    var cosA = Math.cos(angle);
    var sinA2 = sinA * sinA;

    // normalize
    var len = Math.sqrt(x * x + y * y + z * z);
    if (len == 0) {
        // bad vector, just use something reasonable
        x = 0;
        y = 0;
        z = 1;
    } else if (len != 1) {
        x /= len;
        y /= len;
        z /= len;
    }

    var mat = new J3DIMatrix4();

    // optimize case where axis is along major axis
    if (x == 1 && y == 0 && z == 0) {
        mat.$matrix.m11 = 1;
        mat.$matrix.m12 = 0;
        mat.$matrix.m13 = 0;
        mat.$matrix.m21 = 0;
        mat.$matrix.m22 = 1 - 2 * sinA2;
        mat.$matrix.m23 = 2 * sinA * cosA;
        mat.$matrix.m31 = 0;
        mat.$matrix.m32 = -2 * sinA * cosA;
        mat.$matrix.m33 = 1 - 2 * sinA2;
        mat.$matrix.m14 = mat.$matrix.m24 = mat.$matrix.m34 = 0;
        mat.$matrix.m41 = mat.$matrix.m42 = mat.$matrix.m43 = 0;
        mat.$matrix.m44 = 1;
    } else if (x == 0 && y == 1 && z == 0) {
        mat.$matrix.m11 = 1 - 2 * sinA2;
        mat.$matrix.m12 = 0;
        mat.$matrix.m13 = -2 * sinA * cosA;
        mat.$matrix.m21 = 0;
        mat.$matrix.m22 = 1;
        mat.$matrix.m23 = 0;
        mat.$matrix.m31 = 2 * sinA * cosA;
        mat.$matrix.m32 = 0;
        mat.$matrix.m33 = 1 - 2 * sinA2;
        mat.$matrix.m14 = mat.$matrix.m24 = mat.$matrix.m34 = 0;
        mat.$matrix.m41 = mat.$matrix.m42 = mat.$matrix.m43 = 0;
        mat.$matrix.m44 = 1;
    } else if (x == 0 && y == 0 && z == 1) {
        mat.$matrix.m11 = 1 - 2 * sinA2;
        mat.$matrix.m12 = 2 * sinA * cosA;
        mat.$matrix.m13 = 0;
        mat.$matrix.m21 = -2 * sinA * cosA;
        mat.$matrix.m22 = 1 - 2 * sinA2;
        mat.$matrix.m23 = 0;
        mat.$matrix.m31 = 0;
        mat.$matrix.m32 = 0;
        mat.$matrix.m33 = 1;
        mat.$matrix.m14 = mat.$matrix.m24 = mat.$matrix.m34 = 0;
        mat.$matrix.m41 = mat.$matrix.m42 = mat.$matrix.m43 = 0;
        mat.$matrix.m44 = 1;
    } else {
        var x2 = x*x;
        var y2 = y*y;
        var z2 = z*z;

        mat.$matrix.m11 = 1 - 2 * (y2 + z2) * sinA2;
        mat.$matrix.m12 = 2 * (x * y * sinA2 + z * sinA * cosA);
        mat.$matrix.m13 = 2 * (x * z * sinA2 - y * sinA * cosA);
        mat.$matrix.m21 = 2 * (y * x * sinA2 - z * sinA * cosA);
        mat.$matrix.m22 = 1 - 2 * (z2 + x2) * sinA2;
        mat.$matrix.m23 = 2 * (y * z * sinA2 + x * sinA * cosA);
        mat.$matrix.m31 = 2 * (z * x * sinA2 + y * sinA * cosA);
        mat.$matrix.m32 = 2 * (z * y * sinA2 - x * sinA * cosA);
        mat.$matrix.m33 = 1 - 2 * (x2 + y2) * sinA2;
        mat.$matrix.m14 = mat.$matrix.m24 = mat.$matrix.m34 = 0;
        mat.$matrix.m41 = mat.$matrix.m42 = mat.$matrix.m43 = 0;
        mat.$matrix.m44 = 1;
    }
    this.multiply(mat);
}

J3DIMatrix4.prototype.multiply = function(mat)
{
    if (J3DIHasCSSMatrix) {
        this.$matrix = this.$matrix.multiply(mat.$matrix);
        return;
    }

    var m11 = (mat.$matrix.m11 * this.$matrix.m11 + mat.$matrix.m12 * this.$matrix.m21
               + mat.$matrix.m13 * this.$matrix.m31 + mat.$matrix.m14 * this.$matrix.m41);
    var m12 = (mat.$matrix.m11 * this.$matrix.m12 + mat.$matrix.m12 * this.$matrix.m22
               + mat.$matrix.m13 * this.$matrix.m32 + mat.$matrix.m14 * this.$matrix.m42);
    var m13 = (mat.$matrix.m11 * this.$matrix.m13 + mat.$matrix.m12 * this.$matrix.m23
               + mat.$matrix.m13 * this.$matrix.m33 + mat.$matrix.m14 * this.$matrix.m43);
    var m14 = (mat.$matrix.m11 * this.$matrix.m14 + mat.$matrix.m12 * this.$matrix.m24
               + mat.$matrix.m13 * this.$matrix.m34 + mat.$matrix.m14 * this.$matrix.m44);

    var m21 = (mat.$matrix.m21 * this.$matrix.m11 + mat.$matrix.m22 * this.$matrix.m21
               + mat.$matrix.m23 * this.$matrix.m31 + mat.$matrix.m24 * this.$matrix.m41);
    var m22 = (mat.$matrix.m21 * this.$matrix.m12 + mat.$matrix.m22 * this.$matrix.m22
               + mat.$matrix.m23 * this.$matrix.m32 + mat.$matrix.m24 * this.$matrix.m42);
    var m23 = (mat.$matrix.m21 * this.$matrix.m13 + mat.$matrix.m22 * this.$matrix.m23
               + mat.$matrix.m23 * this.$matrix.m33 + mat.$matrix.m24 * this.$matrix.m43);
    var m24 = (mat.$matrix.m21 * this.$matrix.m14 + mat.$matrix.m22 * this.$matrix.m24
               + mat.$matrix.m23 * this.$matrix.m34 + mat.$matrix.m24 * this.$matrix.m44);

    var m31 = (mat.$matrix.m31 * this.$matrix.m11 + mat.$matrix.m32 * this.$matrix.m21
               + mat.$matrix.m33 * this.$matrix.m31 + mat.$matrix.m34 * this.$matrix.m41);
    var m32 = (mat.$matrix.m31 * this.$matrix.m12 + mat.$matrix.m32 * this.$matrix.m22
               + mat.$matrix.m33 * this.$matrix.m32 + mat.$matrix.m34 * this.$matrix.m42);
    var m33 = (mat.$matrix.m31 * this.$matrix.m13 + mat.$matrix.m32 * this.$matrix.m23
               + mat.$matrix.m33 * this.$matrix.m33 + mat.$matrix.m34 * this.$matrix.m43);
    var m34 = (mat.$matrix.m31 * this.$matrix.m14 + mat.$matrix.m32 * this.$matrix.m24
               + mat.$matrix.m33 * this.$matrix.m34 + mat.$matrix.m34 * this.$matrix.m44);

    var m41 = (mat.$matrix.m41 * this.$matrix.m11 + mat.$matrix.m42 * this.$matrix.m21
               + mat.$matrix.m43 * this.$matrix.m31 + mat.$matrix.m44 * this.$matrix.m41);
    var m42 = (mat.$matrix.m41 * this.$matrix.m12 + mat.$matrix.m42 * this.$matrix.m22
               + mat.$matrix.m43 * this.$matrix.m32 + mat.$matrix.m44 * this.$matrix.m42);
    var m43 = (mat.$matrix.m41 * this.$matrix.m13 + mat.$matrix.m42 * this.$matrix.m23
               + mat.$matrix.m43 * this.$matrix.m33 + mat.$matrix.m44 * this.$matrix.m43);
    var m44 = (mat.$matrix.m41 * this.$matrix.m14 + mat.$matrix.m42 * this.$matrix.m24
               + mat.$matrix.m43 * this.$matrix.m34 + mat.$matrix.m44 * this.$matrix.m44);

    this.$matrix.m11 = m11;
    this.$matrix.m12 = m12;
    this.$matrix.m13 = m13;
    this.$matrix.m14 = m14;

    this.$matrix.m21 = m21;
    this.$matrix.m22 = m22;
    this.$matrix.m23 = m23;
    this.$matrix.m24 = m24;

    this.$matrix.m31 = m31;
    this.$matrix.m32 = m32;
    this.$matrix.m33 = m33;
    this.$matrix.m34 = m34;

    this.$matrix.m41 = m41;
    this.$matrix.m42 = m42;
    this.$matrix.m43 = m43;
    this.$matrix.m44 = m44;
}

J3DIMatrix4.prototype.divide = function(divisor)
{
    this.$matrix.m11 /= divisor;
    this.$matrix.m12 /= divisor;
    this.$matrix.m13 /= divisor;
    this.$matrix.m14 /= divisor;

    this.$matrix.m21 /= divisor;
    this.$matrix.m22 /= divisor;
    this.$matrix.m23 /= divisor;
    this.$matrix.m24 /= divisor;

    this.$matrix.m31 /= divisor;
    this.$matrix.m32 /= divisor;
    this.$matrix.m33 /= divisor;
    this.$matrix.m34 /= divisor;

    this.$matrix.m41 /= divisor;
    this.$matrix.m42 /= divisor;
    this.$matrix.m43 /= divisor;
    this.$matrix.m44 /= divisor;

}

J3DIMatrix4.prototype.ortho = function(left, right, bottom, top, near, far)
{
    var tx = (left + right) / (left - right);
    var ty = (top + bottom) / (top - bottom);
    var tz = (far + near) / (far - near);

    var matrix = new J3DIMatrix4();
    matrix.$matrix.m11 = 2 / (left - right);
    matrix.$matrix.m12 = 0;
    matrix.$matrix.m13 = 0;
    matrix.$matrix.m14 = 0;
    matrix.$matrix.m21 = 0;
    matrix.$matrix.m22 = 2 / (top - bottom);
    matrix.$matrix.m23 = 0;
    matrix.$matrix.m24 = 0;
    matrix.$matrix.m31 = 0;
    matrix.$matrix.m32 = 0;
    matrix.$matrix.m33 = -2 / (far - near);
    matrix.$matrix.m34 = 0;
    matrix.$matrix.m41 = tx;
    matrix.$matrix.m42 = ty;
    matrix.$matrix.m43 = tz;
    matrix.$matrix.m44 = 1;

    this.multiply(matrix);
}

J3DIMatrix4.prototype.frustum = function(left, right, bottom, top, near, far)
{
    var matrix = new J3DIMatrix4();
    var A = (right + left) / (right - left);
    var B = (top + bottom) / (top - bottom);
    var C = -(far + near) / (far - near);
    var D = -(2 * far * near) / (far - near);

    matrix.$matrix.m11 = (2 * near) / (right - left);
    matrix.$matrix.m12 = 0;
    matrix.$matrix.m13 = 0;
    matrix.$matrix.m14 = 0;

    matrix.$matrix.m21 = 0;
    matrix.$matrix.m22 = 2 * near / (top - bottom);
    matrix.$matrix.m23 = 0;
    matrix.$matrix.m24 = 0;

    matrix.$matrix.m31 = A;
    matrix.$matrix.m32 = B;
    matrix.$matrix.m33 = C;
    matrix.$matrix.m34 = -1;

    matrix.$matrix.m41 = 0;
    matrix.$matrix.m42 = 0;
    matrix.$matrix.m43 = D;
    matrix.$matrix.m44 = 0;

    this.multiply(matrix);
}

J3DIMatrix4.prototype.perspective = function(fovy, aspect, zNear, zFar)
{
    var top = Math.tan(fovy * Math.PI / 360) * zNear;
    var bottom = -top;
    var left = aspect * bottom;
    var right = aspect * top;
    this.frustum(left, right, bottom, top, zNear, zFar);
}

J3DIMatrix4.prototype.lookat = function(eyex, eyey, eyez, centerx, centery, centerz, upx, upy, upz)
{
    if (typeof eyez == 'object' && "length" in eyez) {
        var t = eyez;
        upx = t[0];
        upy = t[1];
        upz = t[2];

        t = eyey;
        centerx = t[0];
        centery = t[1];
        centerz = t[2];

        t = eyex;
        eyex = t[0];
        eyey = t[1];
        eyez = t[2];
    }

    var matrix = new J3DIMatrix4();

    // Make rotation matrix

    // Z vector
    var zx = eyex - centerx;
    var zy = eyey - centery;
    var zz = eyez - centerz;
    var mag = Math.sqrt(zx * zx + zy * zy + zz * zz);
    if (mag) {
        zx /= mag;
        zy /= mag;
        zz /= mag;
    }

    // Y vector
    var yx = upx;
    var yy = upy;
    var yz = upz;

    // X vector = Y cross Z
    xx =  yy * zz - yz * zy;
    xy = -yx * zz + yz * zx;
    xz =  yx * zy - yy * zx;

    // Recompute Y = Z cross X
    yx = zy * xz - zz * xy;
    yy = -zx * xz + zz * xx;
    yx = zx * xy - zy * xx;

    // cross product gives area of parallelogram, which is < 1.0 for
    // non-perpendicular unit-length vectors; so normalize x, y here

    mag = Math.sqrt(xx * xx + xy * xy + xz * xz);
    if (mag) {
        xx /= mag;
        xy /= mag;
        xz /= mag;
    }

    mag = Math.sqrt(yx * yx + yy * yy + yz * yz);
    if (mag) {
        yx /= mag;
        yy /= mag;
        yz /= mag;
    }

    matrix.$matrix.m11 = xx;
    matrix.$matrix.m12 = xy;
    matrix.$matrix.m13 = xz;
    matrix.$matrix.m14 = 0;

    matrix.$matrix.m21 = yx;
    matrix.$matrix.m22 = yy;
    matrix.$matrix.m23 = yz;
    matrix.$matrix.m24 = 0;

    matrix.$matrix.m31 = zx;
    matrix.$matrix.m32 = zy;
    matrix.$matrix.m33 = zz;
    matrix.$matrix.m34 = 0;

    matrix.$matrix.m41 = 0;
    matrix.$matrix.m42 = 0;
    matrix.$matrix.m43 = 0;
    matrix.$matrix.m44 = 1;
    matrix.translate(-eyex, -eyey, -eyez);

    this.multiply(matrix);
}

// Returns true on success, false otherwise. All params are Array objects
J3DIMatrix4.prototype.decompose = function(_translate, _rotate, _scale, _skew, _perspective)
{
    // Normalize the matrix.
    if (this.$matrix.m44 == 0)
        return false;

    // Gather the params
    var translate, rotate, scale, skew, perspective;

    var translate = (_translate == undefined || !("length" in _translate)) ? new J3DIVector3 : _translate;
    var rotate = (_rotate == undefined || !("length" in _rotate)) ? new J3DIVector3 : _rotate;
    var scale = (_scale == undefined || !("length" in _scale)) ? new J3DIVector3 : _scale;
    var skew = (_skew == undefined || !("length" in _skew)) ? new J3DIVector3 : _skew;
    var perspective = (_perspective == undefined || !("length" in _perspective)) ? new Array(4) : _perspective;

    var matrix = new J3DIMatrix4(this);

    matrix.divide(matrix.$matrix.m44);

    // perspectiveMatrix is used to solve for perspective, but it also provides
    // an easy way to test for singularity of the upper 3x3 component.
    var perspectiveMatrix = new J3DIMatrix4(matrix);

    perspectiveMatrix.$matrix.m14 = 0;
    perspectiveMatrix.$matrix.m24 = 0;
    perspectiveMatrix.$matrix.m34 = 0;
    perspectiveMatrix.$matrix.m44 = 1;

    if (perspectiveMatrix._determinant4x4() == 0)
        return false;

    // First, isolate perspective.
    if (matrix.$matrix.m14 != 0 || matrix.$matrix.m24 != 0 || matrix.$matrix.m34 != 0) {
        // rightHandSide is the right hand side of the equation.
        var rightHandSide = [ matrix.$matrix.m14, matrix.$matrix.m24, matrix.$matrix.m34, matrix.$matrix.m44 ];

        // Solve the equation by inverting perspectiveMatrix and multiplying
        // rightHandSide by the inverse.
        var inversePerspectiveMatrix = new J3DIMatrix4(perspectiveMatrix);
        inversePerspectiveMatrix.invert();
        var transposedInversePerspectiveMatrix = new J3DIMatrix4(inversePerspectiveMatrix);
        transposedInversePerspectiveMatrix.transpose();
        transposedInversePerspectiveMatrix.multVecMatrix(perspective, rightHandSide);

        // Clear the perspective partition
        matrix.$matrix.m14 = matrix.$matrix.m24 = matrix.$matrix.m34 = 0
        matrix.$matrix.m44 = 1;
    }
    else {
        // No perspective.
        perspective[0] = perspective[1] = perspective[2] = 0;
        perspective[3] = 1;
    }

    // Next take care of translation
    translate[0] = matrix.$matrix.m41
    matrix.$matrix.m41 = 0
    translate[1] = matrix.$matrix.m42
    matrix.$matrix.m42 = 0
    translate[2] = matrix.$matrix.m43
    matrix.$matrix.m43 = 0

    // Now get scale and shear. 'row' is a 3 element array of 3 component vectors
    var row0 = new J3DIVector3(matrix.$matrix.m11, matrix.$matrix.m12, matrix.$matrix.m13);
    var row1 = new J3DIVector3(matrix.$matrix.m21, matrix.$matrix.m22, matrix.$matrix.m23);
    var row2 = new J3DIVector3(matrix.$matrix.m31, matrix.$matrix.m32, matrix.$matrix.m33);

    // Compute X scale factor and normalize first row.
    scale[0] = row0.vectorLength();
    row0.divide(scale[0]);

    // Compute XY shear factor and make 2nd row orthogonal to 1st.
    skew[0] = row0.dot(row1);
    row1.combine(row0, 1.0, -skew[0]);

    // Now, compute Y scale and normalize 2nd row.
    scale[1] = row1.vectorLength();
    row1.divide(scale[1]);
    skew[0] /= scale[1];

    // Compute XZ and YZ shears, orthogonalize 3rd row
    skew[1] = row1.dot(row2);
    row2.combine(row0, 1.0, -skew[1]);
    skew[2] = row1.dot(row2);
    row2.combine(row1, 1.0, -skew[2]);

    // Next, get Z scale and normalize 3rd row.
    scale[2] = row2.vectorLength();
    row2.divide(scale[2]);
    skew[1] /= scale[2];
    skew[2] /= scale[2];

    // At this point, the matrix (in rows) is orthonormal.
    // Check for a coordinate system flip.  If the determinant
    // is -1, then negate the matrix and the scaling factors.
    var pdum3 = new J3DIVector3(row1);
    pdum3.cross(row2);
    if (row0.dot(pdum3) < 0) {
        for (i = 0; i < 3; i++) {
            scale[i] *= -1;
            row[0][i] *= -1;
            row[1][i] *= -1;
            row[2][i] *= -1;
        }
    }

    // Now, get the rotations out
    rotate[1] = Math.asin(-row0[2]);
    if (Math.cos(rotate[1]) != 0) {
        rotate[0] = Math.atan2(row1[2], row2[2]);
        rotate[2] = Math.atan2(row0[1], row0[0]);
    }
    else {
        rotate[0] = Math.atan2(-row2[0], row1[1]);
        rotate[2] = 0;
    }

    // Convert rotations to degrees
    var rad2deg = 180 / Math.PI;
    rotate[0] *= rad2deg;
    rotate[1] *= rad2deg;
    rotate[2] *= rad2deg;

    return true;
}

J3DIMatrix4.prototype._determinant2x2 = function(a, b, c, d)
{
    return a * d - b * c;
}

J3DIMatrix4.prototype._determinant3x3 = function(a1, a2, a3, b1, b2, b3, c1, c2, c3)
{
    return a1 * this._determinant2x2(b2, b3, c2, c3)
         - b1 * this._determinant2x2(a2, a3, c2, c3)
         + c1 * this._determinant2x2(a2, a3, b2, b3);
}

J3DIMatrix4.prototype._determinant4x4 = function()
{
    var a1 = this.$matrix.m11;
    var b1 = this.$matrix.m12;
    var c1 = this.$matrix.m13;
    var d1 = this.$matrix.m14;

    var a2 = this.$matrix.m21;
    var b2 = this.$matrix.m22;
    var c2 = this.$matrix.m23;
    var d2 = this.$matrix.m24;

    var a3 = this.$matrix.m31;
    var b3 = this.$matrix.m32;
    var c3 = this.$matrix.m33;
    var d3 = this.$matrix.m34;

    var a4 = this.$matrix.m41;
    var b4 = this.$matrix.m42;
    var c4 = this.$matrix.m43;
    var d4 = this.$matrix.m44;

    return a1 * this._determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4)
         - b1 * this._determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4)
         + c1 * this._determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4)
         - d1 * this._determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

J3DIMatrix4.prototype._makeAdjoint = function()
{
    var a1 = this.$matrix.m11;
    var b1 = this.$matrix.m12;
    var c1 = this.$matrix.m13;
    var d1 = this.$matrix.m14;

    var a2 = this.$matrix.m21;
    var b2 = this.$matrix.m22;
    var c2 = this.$matrix.m23;
    var d2 = this.$matrix.m24;

    var a3 = this.$matrix.m31;
    var b3 = this.$matrix.m32;
    var c3 = this.$matrix.m33;
    var d3 = this.$matrix.m34;

    var a4 = this.$matrix.m41;
    var b4 = this.$matrix.m42;
    var c4 = this.$matrix.m43;
    var d4 = this.$matrix.m44;

    // Row column labeling reversed since we transpose rows & columns
    this.$matrix.m11  =   this._determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
    this.$matrix.m21  = - this._determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
    this.$matrix.m31  =   this._determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
    this.$matrix.m41  = - this._determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

    this.$matrix.m12  = - this._determinant3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
    this.$matrix.m22  =   this._determinant3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
    this.$matrix.m32  = - this._determinant3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
    this.$matrix.m42  =   this._determinant3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

    this.$matrix.m13  =   this._determinant3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
    this.$matrix.m23  = - this._determinant3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
    this.$matrix.m33  =   this._determinant3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
    this.$matrix.m43  = - this._determinant3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

    this.$matrix.m14  = - this._determinant3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
    this.$matrix.m24  =   this._determinant3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
    this.$matrix.m34  = - this._determinant3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
    this.$matrix.m44  =   this._determinant3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

//
// J3DIVector3
//
J3DIVector3 = function(x,y,z)
{
    this.load(x,y,z);
}

J3DIVector3.prototype.load = function(x,y,z)
{
    if (typeof x == 'object' && "length" in x) {
        this[0] = x[0];
        this[1] = x[1];
        this[2] = x[2];
    }
    else if (typeof x == 'number') {
        this[0] = x;
        this[1] = y;
        this[2] = z;
    }
    else {
        this[0] = 0;
        this[1] = 0;
        this[2] = 0;
    }
}

J3DIVector3.prototype.getAsArray = function()
{
    return [ this[0], this[1], this[2] ];
}

J3DIVector3.prototype.getAsFloat32Array = function()
{
    return new Float32Array(this.getAsArray());
}

J3DIVector3.prototype.vectorLength = function()
{
    return Math.sqrt(this[0] * this[0] + this[1] * this[1] + this[2] * this[2]);
}

J3DIVector3.prototype.divide = function(divisor)
{
    this[0] /= divisor; this[1] /= divisor; this[2] /= divisor;
}

J3DIVector3.prototype.cross = function(v)
{
    this[0] =  this[1] * v[2] - this[2] * v[1];
    this[1] = -this[0] * v[2] + this[2] * v[0];
    this[2] =  this[0] * v[1] - this[1] * v[0];
}

J3DIVector3.prototype.dot = function(v)
{
    return this[0] * v[0] + this[1] * v[1] + this[2] * v[2];
}

J3DIVector3.prototype.combine = function(v, ascl, bscl)
{
    this[0] = (ascl * this[0]) + (bscl * v[0]);
    this[1] = (ascl * this[1]) + (bscl * v[1]);
    this[2] = (ascl * this[2]) + (bscl * v[2]);
}

J3DIVector3.prototype.multVecMatrix = function(matrix)
{
    var x = this[0];
    var y = this[1];
    var z = this[2];

    this[0] = matrix.$matrix.m41 + x * matrix.$matrix.m11 + y * matrix.$matrix.m21 + z * matrix.$matrix.m31;
    this[1] = matrix.$matrix.m42 + x * matrix.$matrix.m12 + y * matrix.$matrix.m22 + z * matrix.$matrix.m32;
    this[2] = matrix.$matrix.m43 + x * matrix.$matrix.m13 + y * matrix.$matrix.m23 + z * matrix.$matrix.m33;
    var w = matrix.$matrix.m44 + x * matrix.$matrix.m14 + y * matrix.$matrix.m24 + z * matrix.$matrix.m34;
    if (w != 1 && w != 0) {
        this[0] /= w;
        this[1] /= w;
        this[2] /= w;
    }
}

J3DIVector3.prototype.toString = function()
{
    return "["+this[0]+","+this[1]+","+this[2]+"]";
}
