/*
 * Copyright (C) 2005, 2006, 2013 Apple Inc.  All rights reserved.
 * Copyright (C) 2009 Torch Mobile, Inc.
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

#include "config.h"
#include "TransformationMatrix.h"

#include "AffineTransform.h"
#include "FloatQuad.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "LayoutRect.h"
#include <cmath>
#include <float.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

#if CPU(X86_64)
#include <emmintrin.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

namespace WebCore {

//
// Adapted from Matrix Inversion by Richard Carling, Graphics Gems <http://tog.acm.org/GraphicsGems/index.html>.

// EULA: The Graphics Gems code is copyright-protected. In other words, you cannot claim the text of the code
// as your own and resell it. Using the code is permitted in any program, product, or library, non-commercial
// or commercial. Giving credit is not required, though is a nice gesture. The code comes as-is, and if there
// are any flaws or problems with any Gems code, nobody involved with Gems - authors, editors, publishers, or
// webmasters - are to be held responsible. Basically, don't be a jerk, and remember that anything free comes
// with no guarantee.

// A clarification about the storage of matrix elements
//
// This class uses a 2 dimensional array internally to store the elements of the matrix. The first index into
// the array refers to the column that the element lies in; the second index refers to the row.
//
// In other words, this is the layout of the matrix:
//
// | m_matrix[0][0] m_matrix[1][0] m_matrix[2][0] m_matrix[3][0] |
// | m_matrix[0][1] m_matrix[1][1] m_matrix[2][1] m_matrix[3][1] |
// | m_matrix[0][2] m_matrix[1][2] m_matrix[2][2] m_matrix[3][2] |
// | m_matrix[0][3] m_matrix[1][3] m_matrix[2][3] m_matrix[3][3] |

WTF_MAKE_TZONE_ALLOCATED_IMPL(TransformationMatrix);

typedef double Vector4[4];
typedef double Vector3[3];

const TransformationMatrix TransformationMatrix::identity { };

// inverse(original_matrix, inverse_matrix)
//
// calculate the inverse of a 4x4 matrix
//
// -1
// A  = ___1__ adjoint A
//       det A

//  double = determinant2x2(double a, double b, double c, double d)
//
//  calculate the determinant of a 2x2 matrix.

static double determinant2x2(double a, double b, double c, double d)
{
    return a * d - b * c;
}

//  double = determinant3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3)
//
//  Calculate the determinant of a 3x3 matrix
//  in the form
//
//      | a1,  b1,  c1 |
//      | a2,  b2,  c2 |
//      | a3,  b3,  c3 |

static double determinant3x3(double a1, double a2, double a3, double b1, double b2, double b3, double c1, double c2, double c3)
{
    return a1 * determinant2x2(b2, b3, c2, c3)
         - b1 * determinant2x2(a2, a3, c2, c3)
         + c1 * determinant2x2(a2, a3, b2, b3);
}

//  double = determinant4x4(matrix)
//
//  calculate the determinant of a 4x4 matrix.

static double determinant4x4(const TransformationMatrix::Matrix4& m)
{
    // Assign to individual variable names to aid selecting
    // correct elements

    double a1 = m[0][0];
    double b1 = m[0][1];
    double c1 = m[0][2];
    double d1 = m[0][3];

    double a2 = m[1][0];
    double b2 = m[1][1];
    double c2 = m[1][2];
    double d2 = m[1][3];

    double a3 = m[2][0];
    double b3 = m[2][1];
    double c3 = m[2][2];
    double d3 = m[2][3];

    double a4 = m[3][0];
    double b4 = m[3][1];
    double c4 = m[3][2];
    double d4 = m[3][3];

    return a1 * determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4)
         - b1 * determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4)
         + c1 * determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4)
         - d1 * determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);
}

// adjoint( original_matrix, inverse_matrix )
//
//   calculate the adjoint of a 4x4 matrix
//
//    Let  a   denote the minor determinant of matrix A obtained by
//         ij
//
//    deleting the ith row and jth column from A.
//
//                  i+j
//   Let  b   = (-1)    a
//        ij            ji
//
//  The matrix B = (b  ) is the adjoint of A
//                   ij

static void adjoint(const TransformationMatrix::Matrix4& matrix, TransformationMatrix::Matrix4& result)
{
    // Assign to individual variable names to aid
    // selecting correct values
    double a1 = matrix[0][0];
    double b1 = matrix[0][1];
    double c1 = matrix[0][2];
    double d1 = matrix[0][3];

    double a2 = matrix[1][0];
    double b2 = matrix[1][1];
    double c2 = matrix[1][2];
    double d2 = matrix[1][3];

    double a3 = matrix[2][0];
    double b3 = matrix[2][1];
    double c3 = matrix[2][2];
    double d3 = matrix[2][3];

    double a4 = matrix[3][0];
    double b4 = matrix[3][1];
    double c4 = matrix[3][2];
    double d4 = matrix[3][3];

    // Row column labeling reversed since we transpose rows & columns
    result[0][0]  =   determinant3x3(b2, b3, b4, c2, c3, c4, d2, d3, d4);
    result[1][0]  = - determinant3x3(a2, a3, a4, c2, c3, c4, d2, d3, d4);
    result[2][0]  =   determinant3x3(a2, a3, a4, b2, b3, b4, d2, d3, d4);
    result[3][0]  = - determinant3x3(a2, a3, a4, b2, b3, b4, c2, c3, c4);

    result[0][1]  = - determinant3x3(b1, b3, b4, c1, c3, c4, d1, d3, d4);
    result[1][1]  =   determinant3x3(a1, a3, a4, c1, c3, c4, d1, d3, d4);
    result[2][1]  = - determinant3x3(a1, a3, a4, b1, b3, b4, d1, d3, d4);
    result[3][1]  =   determinant3x3(a1, a3, a4, b1, b3, b4, c1, c3, c4);

    result[0][2]  =   determinant3x3(b1, b2, b4, c1, c2, c4, d1, d2, d4);
    result[1][2]  = - determinant3x3(a1, a2, a4, c1, c2, c4, d1, d2, d4);
    result[2][2]  =   determinant3x3(a1, a2, a4, b1, b2, b4, d1, d2, d4);
    result[3][2]  = - determinant3x3(a1, a2, a4, b1, b2, b4, c1, c2, c4);

    result[0][3]  = - determinant3x3(b1, b2, b3, c1, c2, c3, d1, d2, d3);
    result[1][3]  =   determinant3x3(a1, a2, a3, c1, c2, c3, d1, d2, d3);
    result[2][3]  = - determinant3x3(a1, a2, a3, b1, b2, b3, d1, d2, d3);
    result[3][3]  =   determinant3x3(a1, a2, a3, b1, b2, b3, c1, c2, c3);
}

// Returns false if the matrix is not invertible
static bool inverse(const TransformationMatrix::Matrix4& matrix, TransformationMatrix::Matrix4& result)
{
    // Calculate the adjoint matrix
    adjoint(matrix, result);

    // Calculate the 4x4 determinant
    // If the determinant is zero,
    // then the inverse matrix is not unique.
    double determinant = determinant4x4(matrix);
    if (!std::isnormal(determinant))
        return false;

    // Scale the adjoint matrix to get the inverse

    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            result[i][j] = result[i][j] / determinant;

    return true;
}

// End of code adapted from Matrix Inversion by Richard Carling

// Perform a decomposition on the passed matrix, return false if unsuccessful
// From Graphics Gems: unmatrix.c

// Transpose rotation portion of matrix a, return b
static void transposeMatrix4(const TransformationMatrix::Matrix4& a, TransformationMatrix::Matrix4& b)
{
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            b[i][j] = a[j][i];
}

// Multiply a homogeneous point by a matrix and return the transformed point
static void v4MulPointByMatrix(const Vector4 p, const TransformationMatrix::Matrix4& m, Vector4 result)
{
    result[0] = (p[0] * m[0][0]) + (p[1] * m[1][0]) +
                (p[2] * m[2][0]) + (p[3] * m[3][0]);
    result[1] = (p[0] * m[0][1]) + (p[1] * m[1][1]) +
                (p[2] * m[2][1]) + (p[3] * m[3][1]);
    result[2] = (p[0] * m[0][2]) + (p[1] * m[1][2]) +
                (p[2] * m[2][2]) + (p[3] * m[3][2]);
    result[3] = (p[0] * m[0][3]) + (p[1] * m[1][3]) +
                (p[2] * m[2][3]) + (p[3] * m[3][3]);
}

static double v3Length(Vector3 a)
{
    return std::hypot(a[0], a[1], a[2]);
}

static void v3Scale(Vector3 v, double desiredLength)
{
    double len = v3Length(v);
    if (len != 0) {
        double l = desiredLength / len;
        v[0] *= l;
        v[1] *= l;
        v[2] *= l;
    }
}

static double v3Dot(const Vector3 a, const Vector3 b)
{
    return (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]);
}

// Make a linear combination of two vectors and return the result.
// result = (a * ascl) + (b * bscl)
static void v3Combine(const Vector3 a, const Vector3 b, Vector3 result, double ascl, double bscl)
{
    result[0] = (ascl * a[0]) + (bscl * b[0]);
    result[1] = (ascl * a[1]) + (bscl * b[1]);
    result[2] = (ascl * a[2]) + (bscl * b[2]);
}

// Return the cross product result = a cross b */
static void v3Cross(const Vector3 a, const Vector3 b, Vector3 result)
{
    result[0] = (a[1] * b[2]) - (a[2] * b[1]);
    result[1] = (a[2] * b[0]) - (a[0] * b[2]);
    result[2] = (a[0] * b[1]) - (a[1] * b[0]);
}

static bool decompose2(const TransformationMatrix::Matrix4& matrix, TransformationMatrix::Decomposed2Type& result)
{
    double row0x = matrix[0][0];
    double row0y = matrix[0][1];
    double row1x = matrix[1][0];
    double row1y = matrix[1][1];
    result.translateX = matrix[3][0];
    result.translateY = matrix[3][1];

    // Compute scaling factors.
    result.scaleX = std::hypot(row0x, row0y);
    result.scaleY = std::hypot(row1x, row1y);

    // If determinant is negative, one axis was flipped.
    double determinant = row0x * row1y - row0y * row1x;
    if (determinant < 0) {
        // Flip axis with minimum unit vector dot product.
        if (row0x < row1y)
            result.scaleX = -result.scaleX;
        else
            result.scaleY = -result.scaleY;
    }

    // Renormalize matrix to remove scale.
    if (result.scaleX) {
        row0x *= 1 / result.scaleX;
        row0y *= 1 / result.scaleX;
    }
    if (result.scaleY) {
        row1x *= 1 / result.scaleY;
        row1y *= 1 / result.scaleY;
    }

    // Compute rotation and renormalize matrix.
    result.angle = atan2(row0y, row0x);

    if (result.angle) {
        // Rotate(-angle) = [cos(angle), sin(angle), -sin(angle), cos(angle)]
        //                = [row0x, -row0y, row0y, row0x]
        // Thanks to the normalization above.
        double sn = -row0y;
        double cs = row0x;
        double m11 = row0x, m12 = row0y;
        double m21 = row1x, m22 = row1y;

        row0x = cs * m11 + sn * m21;
        row0y = cs * m12 + sn * m22;
        row1x = -sn * m11 + cs * m21;
        row1y = -sn * m12 + cs * m22;
    }

    result.m11 = row0x;
    result.m12 = row0y;
    result.m21 = row1x;
    result.m22 = row1y;

    // Convert into degrees because our rotation functions expect it.
    result.angle = rad2deg(result.angle);

    return true;
}

static bool decompose4(const TransformationMatrix::Matrix4& mat, TransformationMatrix::Decomposed4Type& result)
{
    TransformationMatrix::Matrix4 localMatrix;
    memcpySpan(std::span { localMatrix }, std::span { mat });

    // Normalize the matrix.
    if (localMatrix[3][3] == 0)
        return false;

    int i, j;
    for (i = 0; i < 4; i++)
        for (j = 0; j < 4; j++)
            localMatrix[i][j] /= localMatrix[3][3];

    // perspectiveMatrix is used to solve for perspective, but it also provides
    // an easy way to test for singularity of the upper 3x3 component.
    TransformationMatrix::Matrix4 perspectiveMatrix;
    memcpySpan(std::span { perspectiveMatrix }, std::span { localMatrix });
    for (i = 0; i < 3; i++)
        perspectiveMatrix[i][3] = 0;
    perspectiveMatrix[3][3] = 1;

    if (determinant4x4(perspectiveMatrix) == 0)
        return false;

    // First, isolate perspective. This is the messiest.
    if (localMatrix[0][3] != 0 || localMatrix[1][3] != 0 || localMatrix[2][3] != 0) {
        // rightHandSide is the right hand side of the equation.
        Vector4 rightHandSide;
        rightHandSide[0] = localMatrix[0][3];
        rightHandSide[1] = localMatrix[1][3];
        rightHandSide[2] = localMatrix[2][3];
        rightHandSide[3] = localMatrix[3][3];

        // Solve the equation by inverting perspectiveMatrix and multiplying
        // rightHandSide by the inverse. (This is the easiest way, not
        // necessarily the best.)
        TransformationMatrix::Matrix4 inversePerspectiveMatrix, transposedInversePerspectiveMatrix;
        if (!inverse(perspectiveMatrix, inversePerspectiveMatrix))
            return false;
        transposeMatrix4(inversePerspectiveMatrix, transposedInversePerspectiveMatrix);

        Vector4 perspectivePoint;
        v4MulPointByMatrix(rightHandSide, transposedInversePerspectiveMatrix, perspectivePoint);

        result.perspectiveX = perspectivePoint[0];
        result.perspectiveY = perspectivePoint[1];
        result.perspectiveZ = perspectivePoint[2];
        result.perspectiveW = perspectivePoint[3];

        // Clear the perspective partition
        localMatrix[0][3] = localMatrix[1][3] = localMatrix[2][3] = 0;
        localMatrix[3][3] = 1;
    } else {
        // No perspective.
        result.perspectiveX = result.perspectiveY = result.perspectiveZ = 0;
        result.perspectiveW = 1;
    }

    // Next take care of translation (easy).
    result.translateX = localMatrix[3][0];
    localMatrix[3][0] = 0;
    result.translateY = localMatrix[3][1];
    localMatrix[3][1] = 0;
    result.translateZ = localMatrix[3][2];
    localMatrix[3][2] = 0;

    // Note: Deviating from the spec in terms of variable naming. The matrix is
    // stored on column major order and not row major. Using the variable 'row'
    // instead of 'column' in the spec pseudocode has been the source of
    // confusion, specifically in sorting out rotations.
    Vector3 column[3], pdum3;

    // Now get scale and shear.
    for (i = 0; i < 3; i++) {
        column[i][0] = localMatrix[i][0];
        column[i][1] = localMatrix[i][1];
        column[i][2] = localMatrix[i][2];
    }

    // Compute X scale factor and normalize the first column.
    result.scaleX = v3Length(column[0]);
    v3Scale(column[0], 1.0);

    // Compute XY shear factor and make 2nd column orthogonal to 1st.
    result.skewXY = v3Dot(column[0], column[1]);
    v3Combine(column[1], column[0], column[1], 1.0, -result.skewXY);

    // Now, compute Y scale and normalize 2nd column.
    result.scaleY = v3Length(column[1]);
    v3Scale(column[1], 1.0);
    result.skewXY /= result.scaleY;

    // Compute XZ and YZ shears, orthogonalize 3rd column.
    result.skewXZ = v3Dot(column[0], column[2]);
    v3Combine(column[2], column[0], column[2], 1.0, -result.skewXZ);
    result.skewYZ = v3Dot(column[1], column[2]);
    v3Combine(column[2], column[1], column[2], 1.0, -result.skewYZ);

    // Next, get Z scale and normalize 3rd column.
    result.scaleZ = v3Length(column[2]);
    v3Scale(column[2], 1.0);
    result.skewXZ /= result.scaleZ;
    result.skewYZ /= result.scaleZ;

    // At this point, the matrix (in column[]) is orthonormal.
    // Check for a coordinate system flip. If the determinant
    // is -1, then negate the matrix and the scaling factors.
    v3Cross(column[1], column[2], pdum3);
    if (v3Dot(column[0], pdum3) < 0) {

        result.scaleX *= -1;
        result.scaleY *= -1;
        result.scaleZ *= -1;

        for (i = 0; i < 3; i++) {
            column[i][0] *= -1;
            column[i][1] *= -1;
            column[i][2] *= -1;
        }
    }

    // Lastly, compute the quaternions.
    // See https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion.
    // Note: deviating from spec (http://www.w3.org/TR/css3-transforms/)
    // which has a degenerate case when the trace (t) of the orthonormal matrix
    // (Q) approaches -1. In the Wikipedia article, Q_ij is indexing on row then
    // column. Thus, Q_ij = column[j][i].

    // The following are equivalent represnetations of the rotation matrix:
    //
    // Axis-angle form:
    //
    //      [ c+(1-c)x^2  (1-c)xy-sz  (1-c)xz+sy ]    c = cos theta
    // R =  [ (1-c)xy+sz  c+(1-c)y^2  (1-c)yz-sx ]    s = sin theta
    //      [ (1-c)xz-sy  (1-c)yz+sx  c+(1-c)z^2 ]    [x,y,z] = axis or rotation
    //
    // The sum of the diagonal elements (trace) is a simple function of the cosine
    // of the angle. The w component of the quaternion is cos(theta/2), and we
    // make use of the double angle formula to directly compute w from the
    // trace. Differences between pairs of skew symmetric elements in this matrix
    // isolate the remaining components. Since w can be zero (also numerically
    // unstable if near zero), we cannot rely solely on this approach to compute
    // the quaternion components.
    //
    // Quaternion form:
    //
    //       [ 1-2(y^2+z^2)    2(xy-zw)      2(xz+yw)   ]
    //  r =  [   2(xy+zw)    1-2(x^2+z^2)    2(yz-xw)   ]    q = (x,y,y,w)
    //       [   2(xz-yw)      2(yz+xw)    1-2(x^2+y^2) ]
    //
    // Different linear combinations of the diagonal elements isolates x, y or z.
    // Sums or differences between skew symmetric elements isolate the remainder.

    double r, s, t, x, y, z, w;

    t = column[0][0] + column[1][1] + column[2][2];

    // https://en.wikipedia.org/wiki/Rotation_matrix#Quaternion
    if (1 + t > 0.001) {
        // Numerically stable as long as 1+t is not close to zero. Otherwise use the
        // diagonal element with the greatest value to compute the quaternions.
        r = std::sqrt(1.0 + t);
        s = 0.5 / r;
        w = 0.5 * r;
        x = (column[1][2] - column[2][1]) * s;
        y = (column[2][0] - column[0][2]) * s;
        z = (column[0][1] - column[1][0]) * s;
    } else if (column[0][0] > column[1][1] && column[0][0] > column[2][2]) {
        // Q_xx is largest.
        r = std::sqrt(1.0 + column[0][0] - column[1][1] - column[2][2]);
        s = 0.5 / r;
        x = 0.5 * r;
        y = (column[1][0] - column[0][1]) * s;
        z = (column[2][0] + column[0][2]) * s;
        w = (column[1][2] - column[2][1]) * s;
    } else if (column[1][1] > column[2][2]) {
        // Q_yy is largest.
        r = std::sqrt(1.0 - column[0][0] + column[1][1] - column[2][2]);
        s = 0.5 / r;
        x = (column[1][0] + column[0][1]) * s;
        y = 0.5 * r;
        z = (column[2][1] + column[1][2]) * s;
        w = (column[2][0] - column[0][2]) * s;
    } else {
        // Q_zz is largest.
        r = std::sqrt(1.0 - column[0][0] - column[1][1] + column[2][2]);
        s = 0.5 / r;
        x = (column[2][0] + column[0][2]) * s;
        y = (column[2][1] + column[1][2]) * s;
        z = 0.5 * r;
        w = (column[0][1] - column[1][0]) * s;
    }

    result.quaternion.x = x;
    result.quaternion.y = y;
    result.quaternion.z = z;
    result.quaternion.w = w;

    return true;
}

// End of Supporting Math Functions

TransformationMatrix::TransformationMatrix(const AffineTransform& t)
{
    setMatrix(t.a(), t.b(), t.c(), t.d(), t.e(), t.f());
}

TransformationMatrix TransformationMatrix::fromQuaternion(const Quaternion& q)
{
    double xx = q.x * q.x;
    double yy = q.y * q.y;
    double zz = q.z * q.z;
    double xz = q.x * q.z;
    double xy = q.x * q.y;
    double yz = q.y * q.z;
    double xw = q.w * q.x;
    double yw = q.w * q.y;
    double zw = q.w * q.z;

    return TransformationMatrix(1 - 2 * (yy + zz), 2 * (xy + zw), 2 * (xz - yw), 0,
        2 * (xy - zw), 1 - 2 * (xx + zz), 2 * (yz + xw), 0,
        2 * (xz + yw), 2 * (yz - xw), 1 - 2 * (xx + yy), 0,
        0, 0, 0, 1);
}


TransformationMatrix TransformationMatrix::fromProjection(double fovUp, double fovDown, double fovLeft, double fovRight, double depthNear, double depthFar)
{
    double upTan = tan(fovUp);
    double downTan = tan(fovDown);
    double leftTan = tan(fovLeft);
    double rightTan = tan(fovRight);
    double xScale = 2.0 / (leftTan + rightTan);
    double yScale = 2.0 / (upTan + downTan);
    double invDepth = 1.0 / (depthNear - depthFar);

    return TransformationMatrix(xScale, 0.0f, 0.0f, 0.0f,
        0.0f, yScale, 0.0f, 0.0f,
        (leftTan - rightTan) * xScale * -0.5, (upTan - downTan) * yScale * 0.5, (depthNear + depthFar) * invDepth, -1.0f,
        0.0f, 0.0f, (2.0f * depthFar * depthNear) * invDepth, 0.0f);
}

TransformationMatrix TransformationMatrix::fromProjection(double fovy, double aspect, double depthNear, double depthFar)
{
    double f = 1.0f / tanf(fovy / 2);
    double invDepth = 1.0f / (depthNear - depthFar);

    return TransformationMatrix(f / aspect, 0.0f, 0.0f, 0.0f,
        0.0f, f, 0.0f, 0.0f,
        0.0f, 0.0f, (depthFar + depthNear) * invDepth, -1.0f,
        0.0f, 0.0f, (2.0f * depthFar * depthNear) * invDepth, 0.0f);
}

TransformationMatrix& TransformationMatrix::scale(double s)
{
    return scaleNonUniform(s, s);
}

TransformationMatrix& TransformationMatrix::rotateFromVector(double x, double y)
{
    return rotateRadians(atan2(y, x));
}

TransformationMatrix& TransformationMatrix::flipX()
{
    return scaleNonUniform(-1.0, 1.0);
}

TransformationMatrix& TransformationMatrix::flipY()
{
    return scaleNonUniform(1.0, -1.0);
}

FloatPoint TransformationMatrix::projectPoint(const FloatPoint& p, bool* clamped) const
{
    // This is basically raytracing. We have a point in the destination
    // plane with z=0, and we cast a ray parallel to the z-axis from that
    // point to find the z-position at which it intersects the z=0 plane
    // with the transform applied. Once we have that point we apply the
    // inverse transform to find the corresponding point in the source
    // space.
    //
    // Given a plane with normal Pn, and a ray starting at point R0 and
    // with direction defined by the vector Rd, we can find the
    // intersection point as a distance d from R0 in units of Rd by:
    //
    // d = -dot (Pn', R0) / dot (Pn', Rd)
    if (clamped)
        *clamped = false;

    if (m33() == 0) {
        // In this case, the projection plane is parallel to the ray we are trying to
        // trace, and there is no well-defined value for the projection.
        return FloatPoint();
    }

    double x = p.x();
    double y = p.y();
    double z = -(m13() * x + m23() * y + m43()) / m33();

    // FIXME: use multVecMatrix()
    double outX = x * m11() + y * m21() + z * m31() + m41();
    double outY = x * m12() + y * m22() + z * m32() + m42();

    double w = x * m14() + y * m24() + z * m34() + m44();
    if (w <= 0) {
        // Using int max causes overflow when other code uses the projected point. To
        // represent infinity yet reduce the risk of overflow, we use a large but
        // not-too-large number here when clamping.
        const int largeNumber = 100000000 / kFixedPointDenominator;
        outX = copysign(largeNumber, outX);
        outY = copysign(largeNumber, outY);
        if (clamped)
            *clamped = true;
    } else if (w != 1) {
        outX /= w;
        outY /= w;
    }

    return FloatPoint(static_cast<float>(outX), static_cast<float>(outY));
}

FloatQuad TransformationMatrix::projectQuad(const FloatQuad& q, bool* clamped) const
{
    FloatQuad projectedQuad;

    bool clamped1 = false;
    bool clamped2 = false;
    bool clamped3 = false;
    bool clamped4 = false;

    projectedQuad.setP1(projectPoint(q.p1(), &clamped1));
    projectedQuad.setP2(projectPoint(q.p2(), &clamped2));
    projectedQuad.setP3(projectPoint(q.p3(), &clamped3));
    projectedQuad.setP4(projectPoint(q.p4(), &clamped4));

    if (clamped)
        *clamped = clamped1 || clamped2 || clamped3 || clamped4;

    // If all points on the quad had w < 0, then the entire quad would not be visible to the projected surface.
    bool everythingWasClipped = clamped1 && clamped2 && clamped3 && clamped4;
    if (everythingWasClipped)
        return FloatQuad();

    return projectedQuad;
}

static float clampEdgeValue(float f)
{
    ASSERT(!std::isnan(f));
    return std::min<float>(std::max<float>(f, -LayoutUnit::max() / 2), LayoutUnit::max() / 2);
}

LayoutRect TransformationMatrix::clampedBoundsOfProjectedQuad(const FloatQuad& q) const
{
    FloatRect mappedQuadBounds = projectQuad(q).boundingBox();

    float left = clampEdgeValue(floorf(mappedQuadBounds.x()));
    float top = clampEdgeValue(floorf(mappedQuadBounds.y()));

    float right;
    if (std::isinf(mappedQuadBounds.x()) && std::isinf(mappedQuadBounds.width()))
        right = LayoutUnit::max() / 2;
    else
        right = clampEdgeValue(ceilf(mappedQuadBounds.maxX()));

    float bottom;
    if (std::isinf(mappedQuadBounds.y()) && std::isinf(mappedQuadBounds.height()))
        bottom = LayoutUnit::max() / 2;
    else
        bottom = clampEdgeValue(ceilf(mappedQuadBounds.maxY()));

    return LayoutRect(LayoutUnit::clamp(left), LayoutUnit::clamp(top),  LayoutUnit::clamp(right - left), LayoutUnit::clamp(bottom - top));
}

void TransformationMatrix::map4ComponentPoint(double& x, double& y, double& z, double& w) const
{
    if (isIdentityOrTranslation()) {
        x += m_matrix[3][0];
        y += m_matrix[3][1];
        z += m_matrix[3][2];
        return;
    }

    Vector4 input = { x, y, z, w };
    Vector4 result;
    v4MulPointByMatrix(input, m_matrix, result);

    x = result[0];
    y = result[1];
    z = result[2];
    w = result[3];
}

FloatPoint TransformationMatrix::mapPoint(const FloatPoint& p) const
{
    if (isIdentityOrTranslation())
        return FloatPoint(p.x() + static_cast<float>(m_matrix[3][0]), p.y() + static_cast<float>(m_matrix[3][1]));

    return internalMapPoint(p);
}

FloatPoint3D TransformationMatrix::mapPoint(const FloatPoint3D& p) const
{
    if (isIdentityOrTranslation())
        return FloatPoint3D(p.x() + static_cast<float>(m_matrix[3][0]),
                            p.y() + static_cast<float>(m_matrix[3][1]),
                            p.z() + static_cast<float>(m_matrix[3][2]));

    return internalMapPoint(p);
}

IntRect TransformationMatrix::mapRect(const IntRect &rect) const
{
    return enclosingIntRect(mapRect(FloatRect(rect)));
}

LayoutRect TransformationMatrix::mapRect(const LayoutRect& r) const
{
    return enclosingLayoutRect(mapRect(FloatRect(r)));
}

FloatRect TransformationMatrix::mapRect(const FloatRect& r) const
{
    auto type = this->type();
    if (type == Type::IdentityOrTranslation) {
        FloatRect mappedRect(r);
        mappedRect.move(static_cast<float>(m_matrix[3][0]), static_cast<float>(m_matrix[3][1]));
        return mappedRect;
    }

    float minX = r.x();
    float minY = r.y();
    float maxX = r.maxX();
    float maxY = r.maxY();

    if (type == Type::Affine) {
        double a = m11();
        double b = m12();
        double c = m21();
        double d = m22();

        double minResultX;
        double minResultY;
        double maxResultX;
        double maxResultY;

        if (a > 0) {
            maxResultX = a * maxX;
            minResultX = a * minX;
        } else {
            maxResultX = a * minX;
            minResultX = a * maxX;
        }

        if (b > 0) {
            maxResultY = b * maxX;
            minResultY = b * minX;
        } else {
            maxResultY = b * minX;
            minResultY = b * maxX;
        }

        if (c > 0) {
            maxResultX += c * maxY;
            minResultX += c * minY;
        } else {
            maxResultX += c * minY;
            minResultX += c * maxY;
        }

        if (d > 0) {
            maxResultY += d * maxY;
            minResultY += d * minY;
        } else {
            maxResultY += d * minY;
            minResultY += d * maxY;
        }

        return FloatRect(minResultX + m41(), minResultY + m42(), maxResultX - minResultX, maxResultY - minResultY);
    }

    FloatQuad result;

    result.setP1(internalMapPoint(FloatPoint(minX, minY)));
    result.setP2(internalMapPoint(FloatPoint(maxX, minY)));
    result.setP3(internalMapPoint(FloatPoint(maxX, maxY)));
    result.setP4(internalMapPoint(FloatPoint(minX, maxY)));

    return result.boundingBox();
}

FloatQuad TransformationMatrix::mapQuad(const FloatQuad& q) const
{
    if (isIdentityOrTranslation()) {
        FloatQuad mappedQuad(q);
        mappedQuad.move(static_cast<float>(m_matrix[3][0]), static_cast<float>(m_matrix[3][1]));
        return mappedQuad;
    }

    FloatQuad result;
    result.setP1(internalMapPoint(q.p1()));
    result.setP2(internalMapPoint(q.p2()));
    result.setP3(internalMapPoint(q.p3()));
    result.setP4(internalMapPoint(q.p4()));
    return result;
}

TransformationMatrix& TransformationMatrix::scaleNonUniform(double sx, double sy)
{
    m_matrix[0][0] *= sx;
    m_matrix[0][1] *= sx;
    m_matrix[0][2] *= sx;
    m_matrix[0][3] *= sx;

    m_matrix[1][0] *= sy;
    m_matrix[1][1] *= sy;
    m_matrix[1][2] *= sy;
    m_matrix[1][3] *= sy;
    return *this;
}

TransformationMatrix& TransformationMatrix::scale3d(double sx, double sy, double sz)
{
    scaleNonUniform(sx, sy);

    m_matrix[2][0] *= sz;
    m_matrix[2][1] *= sz;
    m_matrix[2][2] *= sz;
    m_matrix[2][3] *= sz;
    return *this;
}

static double roundEpsilonToZero(double val)
{
    if (-DBL_EPSILON < val && val < DBL_EPSILON)
        return 0.0f;
    return val;
}

TransformationMatrix& TransformationMatrix::rotate3d(double x, double y, double z, double angle, RotationSnapping snapping)
{
    // Normalize the axis of rotation
    double length = std::hypot(x, y, z);
    if (length == 0) {
        // A direction vector that cannot be normalized, such as [0, 0, 0], will cause the rotation to not be applied.
        return *this;
    } else if (length != 1) {
        x /= length;
        y /= length;
        z /= length;
    }

    // Angles are in degrees. Switch to radians.
    angle = deg2rad(angle);

    double sinTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(sin(angle)) : sin(angle);
    double cosTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(cos(angle)) : cos(angle);

    TransformationMatrix mat;

    // Optimize cases where the axis is along a major axis
    if (x == 1.0 && y == 0.0 && z == 0.0) {
        mat.m_matrix[0][0] = 1.0;
        mat.m_matrix[0][1] = 0.0;
        mat.m_matrix[0][2] = 0.0;
        mat.m_matrix[1][0] = 0.0;
        mat.m_matrix[1][1] = cosTheta;
        mat.m_matrix[1][2] = sinTheta;
        mat.m_matrix[2][0] = 0.0;
        mat.m_matrix[2][1] = -sinTheta;
        mat.m_matrix[2][2] = cosTheta;
        mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
        mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
        mat.m_matrix[3][3] = 1.0;
    } else if (x == 0.0 && y == 1.0 && z == 0.0) {
        mat.m_matrix[0][0] = cosTheta;
        mat.m_matrix[0][1] = 0.0;
        mat.m_matrix[0][2] = -sinTheta;
        mat.m_matrix[1][0] = 0.0;
        mat.m_matrix[1][1] = 1.0;
        mat.m_matrix[1][2] = 0.0;
        mat.m_matrix[2][0] = sinTheta;
        mat.m_matrix[2][1] = 0.0;
        mat.m_matrix[2][2] = cosTheta;
        mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
        mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
        mat.m_matrix[3][3] = 1.0;
    } else if (x == 0.0 && y == 0.0 && z == 1.0) {
        mat.m_matrix[0][0] = cosTheta;
        mat.m_matrix[0][1] = sinTheta;
        mat.m_matrix[0][2] = 0.0;
        mat.m_matrix[1][0] = -sinTheta;
        mat.m_matrix[1][1] = cosTheta;
        mat.m_matrix[1][2] = 0.0;
        mat.m_matrix[2][0] = 0.0;
        mat.m_matrix[2][1] = 0.0;
        mat.m_matrix[2][2] = 1.0;
        mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
        mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
        mat.m_matrix[3][3] = 1.0;
    } else {
        // This case is the rotation about an arbitrary unit vector.
        //
        // Formula is adapted from Wikipedia article on Rotation matrix,
        // http://en.wikipedia.org/wiki/Rotation_matrix#Rotation_matrix_from_axis_and_angle
        //
        // An alternate resource with the same matrix: http://www.fastgraph.com/makegames/3drotation/
        //
        double oneMinusCosTheta = 1 - cosTheta;
        mat.m_matrix[0][0] = cosTheta + x * x * oneMinusCosTheta;
        mat.m_matrix[0][1] = y * x * oneMinusCosTheta + z * sinTheta;
        mat.m_matrix[0][2] = z * x * oneMinusCosTheta - y * sinTheta;
        mat.m_matrix[1][0] = x * y * oneMinusCosTheta - z * sinTheta;
        mat.m_matrix[1][1] = cosTheta + y * y * oneMinusCosTheta;
        mat.m_matrix[1][2] = z * y * oneMinusCosTheta + x * sinTheta;
        mat.m_matrix[2][0] = x * z * oneMinusCosTheta + y * sinTheta;
        mat.m_matrix[2][1] = y * z * oneMinusCosTheta - x * sinTheta;
        mat.m_matrix[2][2] = cosTheta + z * z * oneMinusCosTheta;
        mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
        mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
        mat.m_matrix[3][3] = 1.0;
    }
    multiply(mat);
    return *this;
}

TransformationMatrix& TransformationMatrix::rotate(double angle, RotationSnapping snapping)
{
    if (!std::fmod(angle, 360))
        return *this;

    return rotateRadians(deg2rad(angle), snapping);
}

TransformationMatrix& TransformationMatrix::rotateRadians(double angle, RotationSnapping snapping)
{
    double sinZ = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(sin(angle)) : sin(angle);
    double cosZ = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(cos(angle)) : cos(angle);
    multiply({ cosZ, sinZ, -sinZ, cosZ, 0, 0 });
    return *this;
}

TransformationMatrix& TransformationMatrix::rotate3d(double rx, double ry, double rz, RotationSnapping snapping)
{
    // Angles are in degrees. Switch to radians.
    rx = deg2rad(rx);
    ry = deg2rad(ry);
    rz = deg2rad(rz);

    TransformationMatrix mat;

    double sinTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(sin(rz)) : sin(rz);
    double cosTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(cos(rz)) : cos(rz);

    mat.m_matrix[0][0] = cosTheta;
    mat.m_matrix[0][1] = sinTheta;
    mat.m_matrix[0][2] = 0.0;
    mat.m_matrix[1][0] = -sinTheta;
    mat.m_matrix[1][1] = cosTheta;
    mat.m_matrix[1][2] = 0.0;
    mat.m_matrix[2][0] = 0.0;
    mat.m_matrix[2][1] = 0.0;
    mat.m_matrix[2][2] = 1.0;
    mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
    mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
    mat.m_matrix[3][3] = 1.0;

    TransformationMatrix rmat(mat);

    sinTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(sin(ry)) : sin(ry);
    cosTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(cos(ry)) : cos(ry);

    mat.m_matrix[0][0] = cosTheta;
    mat.m_matrix[0][1] = 0.0;
    mat.m_matrix[0][2] = -sinTheta;
    mat.m_matrix[1][0] = 0.0;
    mat.m_matrix[1][1] = 1.0;
    mat.m_matrix[1][2] = 0.0;
    mat.m_matrix[2][0] = sinTheta;
    mat.m_matrix[2][1] = 0.0;
    mat.m_matrix[2][2] = cosTheta;
    mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
    mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
    mat.m_matrix[3][3] = 1.0;

    rmat.multiply(mat);

    sinTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(sin(rx)) : sin(rx);
    cosTheta = snapping == RotationSnapping::Snap90degRotations ? roundEpsilonToZero(cos(rx)) : cos(rx);

    mat.m_matrix[0][0] = 1.0;
    mat.m_matrix[0][1] = 0.0;
    mat.m_matrix[0][2] = 0.0;
    mat.m_matrix[1][0] = 0.0;
    mat.m_matrix[1][1] = cosTheta;
    mat.m_matrix[1][2] = sinTheta;
    mat.m_matrix[2][0] = 0.0;
    mat.m_matrix[2][1] = -sinTheta;
    mat.m_matrix[2][2] = cosTheta;
    mat.m_matrix[0][3] = mat.m_matrix[1][3] = mat.m_matrix[2][3] = 0.0;
    mat.m_matrix[3][0] = mat.m_matrix[3][1] = mat.m_matrix[3][2] = 0.0;
    mat.m_matrix[3][3] = 1.0;

    rmat.multiply(mat);

    multiply(rmat);
    return *this;
}

TransformationMatrix& TransformationMatrix::translate(double tx, double ty)
{
    m_matrix[3][0] += tx * m_matrix[0][0] + ty * m_matrix[1][0];
    m_matrix[3][1] += tx * m_matrix[0][1] + ty * m_matrix[1][1];
    m_matrix[3][2] += tx * m_matrix[0][2] + ty * m_matrix[1][2];
    m_matrix[3][3] += tx * m_matrix[0][3] + ty * m_matrix[1][3];
    return *this;
}

TransformationMatrix& TransformationMatrix::translate3d(double tx, double ty, double tz)
{
    m_matrix[3][0] += tx * m_matrix[0][0] + ty * m_matrix[1][0] + tz * m_matrix[2][0];
    m_matrix[3][1] += tx * m_matrix[0][1] + ty * m_matrix[1][1] + tz * m_matrix[2][1];
    m_matrix[3][2] += tx * m_matrix[0][2] + ty * m_matrix[1][2] + tz * m_matrix[2][2];
    m_matrix[3][3] += tx * m_matrix[0][3] + ty * m_matrix[1][3] + tz * m_matrix[2][3];
    return *this;
}

TransformationMatrix& TransformationMatrix::translateRight(double tx, double ty)
{
    if (tx != 0) {
        m_matrix[0][0] +=  m_matrix[0][3] * tx;
        m_matrix[1][0] +=  m_matrix[1][3] * tx;
        m_matrix[2][0] +=  m_matrix[2][3] * tx;
        m_matrix[3][0] +=  m_matrix[3][3] * tx;
    }

    if (ty != 0) {
        m_matrix[0][1] +=  m_matrix[0][3] * ty;
        m_matrix[1][1] +=  m_matrix[1][3] * ty;
        m_matrix[2][1] +=  m_matrix[2][3] * ty;
        m_matrix[3][1] +=  m_matrix[3][3] * ty;
    }

    return *this;
}

TransformationMatrix& TransformationMatrix::translateRight3d(double tx, double ty, double tz)
{
    translateRight(tx, ty);
    if (tz != 0) {
        m_matrix[0][2] +=  m_matrix[0][3] * tz;
        m_matrix[1][2] +=  m_matrix[1][3] * tz;
        m_matrix[2][2] +=  m_matrix[2][3] * tz;
        m_matrix[3][2] +=  m_matrix[3][3] * tz;
    }

    return *this;
}

TransformationMatrix& TransformationMatrix::skew(double sx, double sy)
{
    // angles are in degrees. Switch to radians
    sx = deg2rad(sx);
    sy = deg2rad(sy);

    TransformationMatrix mat;
    mat.m_matrix[0][1] = tan(sy); // note that the y shear goes in the first row
    mat.m_matrix[1][0] = tan(sx); // and the x shear in the second row

    multiply(mat);
    return *this;
}

TransformationMatrix& TransformationMatrix::applyPerspective(double p)
{
    TransformationMatrix mat;
    if (p != 0)
        mat.m_matrix[2][3] = -1/p;

    multiply(mat);
    return *this;
}

TransformationMatrix TransformationMatrix::rectToRect(const FloatRect& from, const FloatRect& to)
{
    ASSERT(!from.isEmpty());
    return TransformationMatrix(to.width() / from.width(),
                                0, 0,
                                to.height() / from.height(),
                                to.x() - from.x(),
                                to.y() - from.y());
}

TransformationMatrix& TransformationMatrix::zoom(double zoomFactor)
{
    m_matrix[0][3] /= zoomFactor;
    m_matrix[1][3] /= zoomFactor;
    m_matrix[2][3] /= zoomFactor;
    m_matrix[3][0] *= zoomFactor;
    m_matrix[3][1] *= zoomFactor;
    m_matrix[3][2] *= zoomFactor;
    return *this;
}

// this = mat * this.
TransformationMatrix& TransformationMatrix::multiply(const TransformationMatrix& mat)
{
#if CPU(ARM64) && defined(_LP64)
    double* leftMatrix = &(m_matrix[0][0]);
    const double* rightMatrix = &(mat.m_matrix[0][0]);
    asm volatile (
        // First, load the leftMatrix completely in memory. The leftMatrix is in v16-v23.
        "mov   x4, %[leftMatrix]\n\t"
        "ld1   {v16.2d, v17.2d, v18.2d, v19.2d}, [%[leftMatrix]], #64\n\t"
        "ld1   {v20.2d, v21.2d, v22.2d, v23.2d}, [%[leftMatrix]]\n\t"

        // First row.
        "ld4r  {v24.2d, v25.2d, v26.2d, v27.2d}, [%[rightMatrix]], #32\n\t"
        "fmul  v28.2d, v24.2d, v16.2d\n\t"
        "fmul  v29.2d, v24.2d, v17.2d\n\t"
        "fmla  v28.2d, v25.2d, v18.2d\n\t"
        "fmla  v29.2d, v25.2d, v19.2d\n\t"
        "fmla  v28.2d, v26.2d, v20.2d\n\t"
        "fmla  v29.2d, v26.2d, v21.2d\n\t"
        "fmla  v28.2d, v27.2d, v22.2d\n\t"
        "fmla  v29.2d, v27.2d, v23.2d\n\t"

        "ld4r  {v0.2d, v1.2d, v2.2d, v3.2d}, [%[rightMatrix]], #32\n\t"
        "st1  {v28.2d, v29.2d}, [x4], #32\n\t"

        // Second row.
        "fmul  v30.2d, v0.2d, v16.2d\n\t"
        "fmul  v31.2d, v0.2d, v17.2d\n\t"
        "fmla  v30.2d, v1.2d, v18.2d\n\t"
        "fmla  v31.2d, v1.2d, v19.2d\n\t"
        "fmla  v30.2d, v2.2d, v20.2d\n\t"
        "fmla  v31.2d, v2.2d, v21.2d\n\t"
        "fmla  v30.2d, v3.2d, v22.2d\n\t"
        "fmla  v31.2d, v3.2d, v23.2d\n\t"

        "ld4r  {v24.2d, v25.2d, v26.2d, v27.2d}, [%[rightMatrix]], #32\n\t"
        "st1   {v30.2d, v31.2d}, [x4], #32\n\t"

        // Third row.
        "fmul  v28.2d, v24.2d, v16.2d\n\t"
        "fmul  v29.2d, v24.2d, v17.2d\n\t"
        "fmla  v28.2d, v25.2d, v18.2d\n\t"
        "fmla  v29.2d, v25.2d, v19.2d\n\t"
        "fmla  v28.2d, v26.2d, v20.2d\n\t"
        "fmla  v29.2d, v26.2d, v21.2d\n\t"
        "fmla  v28.2d, v27.2d, v22.2d\n\t"
        "fmla  v29.2d, v27.2d, v23.2d\n\t"

        "ld4r  {v0.2d, v1.2d, v2.2d, v3.2d}, [%[rightMatrix]], #32\n\t"
        "st1   {v28.2d, v29.2d}, [x4], #32\n\t"

        // Fourth row.
        "fmul  v30.2d, v0.2d, v16.2d\n\t"
        "fmul  v31.2d, v0.2d, v17.2d\n\t"
        "fmla  v30.2d, v1.2d, v18.2d\n\t"
        "fmla  v31.2d, v1.2d, v19.2d\n\t"
        "fmla  v30.2d, v2.2d, v20.2d\n\t"
        "fmla  v31.2d, v2.2d, v21.2d\n\t"
        "fmla  v30.2d, v3.2d, v22.2d\n\t"
        "fmla  v31.2d, v3.2d, v23.2d\n\t"

        "st1  {v30.2d, v31.2d}, [x4]\n\t"

        : [leftMatrix]"+r"(leftMatrix), [rightMatrix]"+r"(rightMatrix)
        :
        : "memory", "x4", "v0", "v1", "v2", "v3", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
#elif CPU(APPLE_ARMV7S)
    double* leftMatrix = &(m_matrix[0][0]);
    const double* rightMatrix = &(mat.m_matrix[0][0]);
    asm volatile (// First row of leftMatrix.
        "mov        r3, %[leftMatrix]\n\t"
        "vld1.64    { d16-d19 }, [%[leftMatrix], :128]!\n\t"
        "vld1.64    { d0-d3}, [%[rightMatrix], :128]!\n\t"
        "vmul.f64   d4, d0, d16\n\t"
        "vld1.64    { d20-d23 }, [%[leftMatrix], :128]!\n\t"
        "vmla.f64   d4, d1, d20\n\t"
        "vld1.64    { d24-d27 }, [%[leftMatrix], :128]!\n\t"
        "vmla.f64   d4, d2, d24\n\t"
        "vld1.64    { d28-d31 }, [%[leftMatrix], :128]!\n\t"
        "vmla.f64   d4, d3, d28\n\t"

        "vmul.f64   d5, d0, d17\n\t"
        "vmla.f64   d5, d1, d21\n\t"
        "vmla.f64   d5, d2, d25\n\t"
        "vmla.f64   d5, d3, d29\n\t"

        "vmul.f64   d6, d0, d18\n\t"
        "vmla.f64   d6, d1, d22\n\t"
        "vmla.f64   d6, d2, d26\n\t"
        "vmla.f64   d6, d3, d30\n\t"

        "vmul.f64   d7, d0, d19\n\t"
        "vmla.f64   d7, d1, d23\n\t"
        "vmla.f64   d7, d2, d27\n\t"
        "vmla.f64   d7, d3, d31\n\t"
        "vld1.64    { d0-d3}, [%[rightMatrix], :128]!\n\t"
        "vst1.64    { d4-d7 }, [r3, :128]!\n\t"

        // Second row of leftMatrix.
        "vmul.f64   d4, d0, d16\n\t"
        "vmla.f64   d4, d1, d20\n\t"
        "vmla.f64   d4, d2, d24\n\t"
        "vmla.f64   d4, d3, d28\n\t"

        "vmul.f64   d5, d0, d17\n\t"
        "vmla.f64   d5, d1, d21\n\t"
        "vmla.f64   d5, d2, d25\n\t"
        "vmla.f64   d5, d3, d29\n\t"

        "vmul.f64   d6, d0, d18\n\t"
        "vmla.f64   d6, d1, d22\n\t"
        "vmla.f64   d6, d2, d26\n\t"
        "vmla.f64   d6, d3, d30\n\t"

        "vmul.f64   d7, d0, d19\n\t"
        "vmla.f64   d7, d1, d23\n\t"
        "vmla.f64   d7, d2, d27\n\t"
        "vmla.f64   d7, d3, d31\n\t"
        "vld1.64    { d0-d3}, [%[rightMatrix], :128]!\n\t"
        "vst1.64    { d4-d7 }, [r3, :128]!\n\t"

        // Third row of leftMatrix.
        "vmul.f64   d4, d0, d16\n\t"
        "vmla.f64   d4, d1, d20\n\t"
        "vmla.f64   d4, d2, d24\n\t"
        "vmla.f64   d4, d3, d28\n\t"

        "vmul.f64   d5, d0, d17\n\t"
        "vmla.f64   d5, d1, d21\n\t"
        "vmla.f64   d5, d2, d25\n\t"
        "vmla.f64   d5, d3, d29\n\t"

        "vmul.f64   d6, d0, d18\n\t"
        "vmla.f64   d6, d1, d22\n\t"
        "vmla.f64   d6, d2, d26\n\t"
        "vmla.f64   d6, d3, d30\n\t"

        "vmul.f64   d7, d0, d19\n\t"
        "vmla.f64   d7, d1, d23\n\t"
        "vmla.f64   d7, d2, d27\n\t"
        "vmla.f64   d7, d3, d31\n\t"
        "vld1.64    { d0-d3}, [%[rightMatrix], :128]\n\t"
        "vst1.64    { d4-d7 }, [r3, :128]!\n\t"

        // Fourth and last row of leftMatrix.
        "vmul.f64   d4, d0, d16\n\t"
        "vmla.f64   d4, d1, d20\n\t"
        "vmla.f64   d4, d2, d24\n\t"
        "vmla.f64   d4, d3, d28\n\t"

        "vmul.f64   d5, d0, d17\n\t"
        "vmla.f64   d5, d1, d21\n\t"
        "vmla.f64   d5, d2, d25\n\t"
        "vmla.f64   d5, d3, d29\n\t"

        "vmul.f64   d6, d0, d18\n\t"
        "vmla.f64   d6, d1, d22\n\t"
        "vmla.f64   d6, d2, d26\n\t"
        "vmla.f64   d6, d3, d30\n\t"

        "vmul.f64   d7, d0, d19\n\t"
        "vmla.f64   d7, d1, d23\n\t"
        "vmla.f64   d7, d2, d27\n\t"
        "vmla.f64   d7, d3, d31\n\t"
        "vst1.64    { d4-d7 }, [r3, :128]\n\t"
        : [leftMatrix]"+r"(leftMatrix), [rightMatrix]"+r"(rightMatrix)
        :
        : "memory", "r3", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7", "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31");
#elif CPU(ARM_VFP) && PLATFORM(IOS_FAMILY)

#define MATRIX_MULTIPLY_ONE_LINE \
    "vldmia.64  %[rightMatrix]!, { d0-d3}\n\t" \
    "vmul.f64   d4, d0, d16\n\t" \
    "vmla.f64   d4, d1, d20\n\t" \
    "vmla.f64   d4, d2, d24\n\t" \
    "vmla.f64   d4, d3, d28\n\t" \
    \
    "vmul.f64   d5, d0, d17\n\t" \
    "vmla.f64   d5, d1, d21\n\t" \
    "vmla.f64   d5, d2, d25\n\t" \
    "vmla.f64   d5, d3, d29\n\t" \
    \
    "vmul.f64   d6, d0, d18\n\t" \
    "vmla.f64   d6, d1, d22\n\t" \
    "vmla.f64   d6, d2, d26\n\t" \
    "vmla.f64   d6, d3, d30\n\t" \
    \
    "vmul.f64   d7, d0, d19\n\t" \
    "vmla.f64   d7, d1, d23\n\t" \
    "vmla.f64   d7, d2, d27\n\t" \
    "vmla.f64   d7, d3, d31\n\t" \
    "vstmia.64  %[leftMatrix]!, { d4-d7 }\n\t"

    double* leftMatrix = &(m_matrix[0][0]);
    const double* rightMatrix = &(mat.m_matrix[0][0]);
    // We load the full m_matrix at once in d16-d31.
    asm volatile("vldmia.64  %[leftMatrix], { d16-d31 }\n\t"
                 :
                 : [leftMatrix]"r"(leftMatrix)
                 : "d16", "d17", "d18", "d19", "d20", "d21", "d22", "d23", "d24", "d25", "d26", "d27", "d28", "d29", "d30", "d31");
    for (unsigned i = 0; i < 4; ++i) {
        asm volatile(MATRIX_MULTIPLY_ONE_LINE
                     : [leftMatrix]"+r"(leftMatrix), [rightMatrix]"+r"(rightMatrix)
                     :
                     : "memory", "d0", "d1", "d2", "d3", "d4", "d5", "d6", "d7");
    }
#undef MATRIX_MULTIPLY_ONE_LINE

#elif defined(TRANSFORMATION_MATRIX_USE_X86_64_SSE2)
    // x86_64 has 16 XMM registers which is enough to do the multiplication fully in registers.
    __m128d matrixBlockA = _mm_load_pd(&(m_matrix[0][0]));
    __m128d matrixBlockC = _mm_load_pd(&(m_matrix[1][0]));
    __m128d matrixBlockE = _mm_load_pd(&(m_matrix[2][0]));
    __m128d matrixBlockG = _mm_load_pd(&(m_matrix[3][0]));

    // First row.
    __m128d otherMatrixFirstParam = _mm_set1_pd(mat.m_matrix[0][0]);
    __m128d otherMatrixSecondParam = _mm_set1_pd(mat.m_matrix[0][1]);
    __m128d otherMatrixThirdParam = _mm_set1_pd(mat.m_matrix[0][2]);
    __m128d otherMatrixFourthParam = _mm_set1_pd(mat.m_matrix[0][3]);

    // output00 and output01.
    __m128d accumulator = _mm_mul_pd(matrixBlockA, otherMatrixFirstParam);
    __m128d temp1 = _mm_mul_pd(matrixBlockC, otherMatrixSecondParam);
    __m128d temp2 = _mm_mul_pd(matrixBlockE, otherMatrixThirdParam);
    __m128d temp3 = _mm_mul_pd(matrixBlockG, otherMatrixFourthParam);

    __m128d matrixBlockB = _mm_load_pd(&(m_matrix[0][2]));
    __m128d matrixBlockD = _mm_load_pd(&(m_matrix[1][2]));
    __m128d matrixBlockF = _mm_load_pd(&(m_matrix[2][2]));
    __m128d matrixBlockH = _mm_load_pd(&(m_matrix[3][2]));

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[0][0], accumulator);

    // output02 and output03.
    accumulator = _mm_mul_pd(matrixBlockB, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockD, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockF, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockH, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[0][2], accumulator);

    // Second row.
    otherMatrixFirstParam = _mm_set1_pd(mat.m_matrix[1][0]);
    otherMatrixSecondParam = _mm_set1_pd(mat.m_matrix[1][1]);
    otherMatrixThirdParam = _mm_set1_pd(mat.m_matrix[1][2]);
    otherMatrixFourthParam = _mm_set1_pd(mat.m_matrix[1][3]);

    // output10 and output11.
    accumulator = _mm_mul_pd(matrixBlockA, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockC, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockE, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockG, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[1][0], accumulator);

    // output12 and output13.
    accumulator = _mm_mul_pd(matrixBlockB, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockD, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockF, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockH, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[1][2], accumulator);

    // Third row.
    otherMatrixFirstParam = _mm_set1_pd(mat.m_matrix[2][0]);
    otherMatrixSecondParam = _mm_set1_pd(mat.m_matrix[2][1]);
    otherMatrixThirdParam = _mm_set1_pd(mat.m_matrix[2][2]);
    otherMatrixFourthParam = _mm_set1_pd(mat.m_matrix[2][3]);

    // output20 and output21.
    accumulator = _mm_mul_pd(matrixBlockA, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockC, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockE, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockG, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[2][0], accumulator);

    // output22 and output23.
    accumulator = _mm_mul_pd(matrixBlockB, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockD, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockF, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockH, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[2][2], accumulator);

    // Fourth row.
    otherMatrixFirstParam = _mm_set1_pd(mat.m_matrix[3][0]);
    otherMatrixSecondParam = _mm_set1_pd(mat.m_matrix[3][1]);
    otherMatrixThirdParam = _mm_set1_pd(mat.m_matrix[3][2]);
    otherMatrixFourthParam = _mm_set1_pd(mat.m_matrix[3][3]);

    // output30 and output31.
    accumulator = _mm_mul_pd(matrixBlockA, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockC, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockE, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockG, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[3][0], accumulator);

    // output32 and output33.
    accumulator = _mm_mul_pd(matrixBlockB, otherMatrixFirstParam);
    temp1 = _mm_mul_pd(matrixBlockD, otherMatrixSecondParam);
    temp2 = _mm_mul_pd(matrixBlockF, otherMatrixThirdParam);
    temp3 = _mm_mul_pd(matrixBlockH, otherMatrixFourthParam);

    accumulator = _mm_add_pd(accumulator, temp1);
    accumulator = _mm_add_pd(accumulator, temp2);
    accumulator = _mm_add_pd(accumulator, temp3);
    _mm_store_pd(&m_matrix[3][2], accumulator);
#else
    Matrix4 tmp;

    tmp[0][0] = (mat.m_matrix[0][0] * m_matrix[0][0] + mat.m_matrix[0][1] * m_matrix[1][0]
               + mat.m_matrix[0][2] * m_matrix[2][0] + mat.m_matrix[0][3] * m_matrix[3][0]);
    tmp[0][1] = (mat.m_matrix[0][0] * m_matrix[0][1] + mat.m_matrix[0][1] * m_matrix[1][1]
               + mat.m_matrix[0][2] * m_matrix[2][1] + mat.m_matrix[0][3] * m_matrix[3][1]);
    tmp[0][2] = (mat.m_matrix[0][0] * m_matrix[0][2] + mat.m_matrix[0][1] * m_matrix[1][2]
               + mat.m_matrix[0][2] * m_matrix[2][2] + mat.m_matrix[0][3] * m_matrix[3][2]);
    tmp[0][3] = (mat.m_matrix[0][0] * m_matrix[0][3] + mat.m_matrix[0][1] * m_matrix[1][3]
               + mat.m_matrix[0][2] * m_matrix[2][3] + mat.m_matrix[0][3] * m_matrix[3][3]);

    tmp[1][0] = (mat.m_matrix[1][0] * m_matrix[0][0] + mat.m_matrix[1][1] * m_matrix[1][0]
               + mat.m_matrix[1][2] * m_matrix[2][0] + mat.m_matrix[1][3] * m_matrix[3][0]);
    tmp[1][1] = (mat.m_matrix[1][0] * m_matrix[0][1] + mat.m_matrix[1][1] * m_matrix[1][1]
               + mat.m_matrix[1][2] * m_matrix[2][1] + mat.m_matrix[1][3] * m_matrix[3][1]);
    tmp[1][2] = (mat.m_matrix[1][0] * m_matrix[0][2] + mat.m_matrix[1][1] * m_matrix[1][2]
               + mat.m_matrix[1][2] * m_matrix[2][2] + mat.m_matrix[1][3] * m_matrix[3][2]);
    tmp[1][3] = (mat.m_matrix[1][0] * m_matrix[0][3] + mat.m_matrix[1][1] * m_matrix[1][3]
               + mat.m_matrix[1][2] * m_matrix[2][3] + mat.m_matrix[1][3] * m_matrix[3][3]);

    tmp[2][0] = (mat.m_matrix[2][0] * m_matrix[0][0] + mat.m_matrix[2][1] * m_matrix[1][0]
               + mat.m_matrix[2][2] * m_matrix[2][0] + mat.m_matrix[2][3] * m_matrix[3][0]);
    tmp[2][1] = (mat.m_matrix[2][0] * m_matrix[0][1] + mat.m_matrix[2][1] * m_matrix[1][1]
               + mat.m_matrix[2][2] * m_matrix[2][1] + mat.m_matrix[2][3] * m_matrix[3][1]);
    tmp[2][2] = (mat.m_matrix[2][0] * m_matrix[0][2] + mat.m_matrix[2][1] * m_matrix[1][2]
               + mat.m_matrix[2][2] * m_matrix[2][2] + mat.m_matrix[2][3] * m_matrix[3][2]);
    tmp[2][3] = (mat.m_matrix[2][0] * m_matrix[0][3] + mat.m_matrix[2][1] * m_matrix[1][3]
               + mat.m_matrix[2][2] * m_matrix[2][3] + mat.m_matrix[2][3] * m_matrix[3][3]);

    tmp[3][0] = (mat.m_matrix[3][0] * m_matrix[0][0] + mat.m_matrix[3][1] * m_matrix[1][0]
               + mat.m_matrix[3][2] * m_matrix[2][0] + mat.m_matrix[3][3] * m_matrix[3][0]);
    tmp[3][1] = (mat.m_matrix[3][0] * m_matrix[0][1] + mat.m_matrix[3][1] * m_matrix[1][1]
               + mat.m_matrix[3][2] * m_matrix[2][1] + mat.m_matrix[3][3] * m_matrix[3][1]);
    tmp[3][2] = (mat.m_matrix[3][0] * m_matrix[0][2] + mat.m_matrix[3][1] * m_matrix[1][2]
               + mat.m_matrix[3][2] * m_matrix[2][2] + mat.m_matrix[3][3] * m_matrix[3][2]);
    tmp[3][3] = (mat.m_matrix[3][0] * m_matrix[0][3] + mat.m_matrix[3][1] * m_matrix[1][3]
               + mat.m_matrix[3][2] * m_matrix[2][3] + mat.m_matrix[3][3] * m_matrix[3][3]);

    memcpySpan(std::span { m_matrix }, std::span { tmp });
#endif
    return *this;
}

TransformationMatrix& TransformationMatrix::multiplyAffineTransform(const AffineTransform& matrix)
{
    if (matrix.isIdentity())
        return *this;

    if (matrix.isIdentityOrTranslation())
        return translate(matrix.e(), matrix.f());

    return multiply(matrix.toTransformationMatrix());
}

void TransformationMatrix::multVecMatrix(double x, double y, double& resultX, double& resultY) const
{
    resultX = m_matrix[3][0] + x * m_matrix[0][0] + y * m_matrix[1][0];
    resultY = m_matrix[3][1] + x * m_matrix[0][1] + y * m_matrix[1][1];
    double w = m_matrix[3][3] + x * m_matrix[0][3] + y * m_matrix[1][3];
    if (w != 1 && w != 0) {
        resultX /= w;
        resultY /= w;
    }
}

void TransformationMatrix::multVecMatrix(double x, double y, double z, double& resultX, double& resultY, double& resultZ) const
{
    resultX = m_matrix[3][0] + x * m_matrix[0][0] + y * m_matrix[1][0] + z * m_matrix[2][0];
    resultY = m_matrix[3][1] + x * m_matrix[0][1] + y * m_matrix[1][1] + z * m_matrix[2][1];
    resultZ = m_matrix[3][2] + x * m_matrix[0][2] + y * m_matrix[1][2] + z * m_matrix[2][2];
    double w = m_matrix[3][3] + x * m_matrix[0][3] + y * m_matrix[1][3] + z * m_matrix[2][3];
    if (w != 1 && w != 0) {
        resultX /= w;
        resultY /= w;
        resultZ /= w;
    }
}

bool TransformationMatrix::isInvertible() const
{
    auto type = this->type();
    if (type == Type::IdentityOrTranslation)
        return true;

    return std::isnormal(type == Type::Affine ? (m11() * m22() - m12() * m21()) : WebCore::determinant4x4(m_matrix));
}

std::optional<TransformationMatrix> TransformationMatrix::inverse() const
{
    auto type = this->type();
    if (type == Type::IdentityOrTranslation) {
        // identity matrix
        if (m_matrix[3][0] == 0 && m_matrix[3][1] == 0 && m_matrix[3][2] == 0)
            return TransformationMatrix();

        // translation
        return TransformationMatrix(1, 0, 0, 0,
                                    0, 1, 0, 0,
                                    0, 0, 1, 0,
                                    -m_matrix[3][0], -m_matrix[3][1], -m_matrix[3][2], 1);
    }

    if (type == Type::Affine) {
        double a = m11();
        double b = m12();
        double c = m21();
        double d = m22();
        double e = m41();
        double f = m42();
        double determinant = a * d - b * c;
        if (!std::isnormal(determinant))
            return std::nullopt;

        double inverseDeterminant = 1 / determinant;
        return {{
            d * inverseDeterminant,
            -b * inverseDeterminant,
            -c * inverseDeterminant,
            a * inverseDeterminant,
            (c * f - d * e) * inverseDeterminant,
            (b * e - a * f) * inverseDeterminant
        }};
    }

    TransformationMatrix invMat;
    // FIXME: Use LU decomposition to apply the inverse instead of calculating the inverse explicitly.
    // Calculating the inverse of a 4x4 matrix using cofactors is numerically unstable and unnecessary to apply the inverse transformation to a point.
    if (!WebCore::inverse(m_matrix, invMat.m_matrix))
        return std::nullopt;

    return invMat;
}

void TransformationMatrix::makeAffine()
{
    m_matrix[0][2] = 0;
    m_matrix[0][3] = 0;

    m_matrix[1][2] = 0;
    m_matrix[1][3] = 0;

    m_matrix[2][0] = 0;
    m_matrix[2][1] = 0;
    m_matrix[2][2] = 1;
    m_matrix[2][3] = 0;

    m_matrix[3][2] = 0;
    m_matrix[3][3] = 1;
}

void TransformationMatrix::flatten()
{
    m_matrix[0][2] = 0;

    m_matrix[1][2] = 0;

    m_matrix[2][0] = 0;
    m_matrix[2][1] = 0;
    m_matrix[2][2] = 1;
    m_matrix[2][3] = 0;

    m_matrix[3][2] = 0;
}

AffineTransform TransformationMatrix::toAffineTransform() const
{
    return AffineTransform(m_matrix[0][0], m_matrix[0][1], m_matrix[1][0],
                           m_matrix[1][1], m_matrix[3][0], m_matrix[3][1]);
}

static inline void blendFloat(double& from, double to, double progress, CompositeOperation compositeOperation)
{
    switch (compositeOperation) {
    case CompositeOperation::Replace:
        from = from + (to - from) * progress;
        break;
    case CompositeOperation::Accumulate:
        ASSERT(progress == 1.0);
        from += to - 1;
        break;
    case CompositeOperation::Add:
        ASSERT(progress == 1.0);
        from += to;
        break;
    }
}

void TransformationMatrix::blend2(const TransformationMatrix& from, double progress, CompositeOperation compositeOperation)
{
    Decomposed2Type fromDecomp;
    Decomposed2Type toDecomp;
    if (!from.decompose2(fromDecomp) || !decompose2(toDecomp)) {
        if (progress < 0.5)
            *this = from;
        return;
    }

    // If x-axis of one is flipped, and y-axis of the other, convert to an unflipped rotation.
    if ((fromDecomp.scaleX < 0 && toDecomp.scaleY < 0) || (fromDecomp.scaleY < 0 && toDecomp.scaleX < 0)) {
        fromDecomp.scaleX = -fromDecomp.scaleX;
        fromDecomp.scaleY = -fromDecomp.scaleY;
        fromDecomp.angle += fromDecomp.angle < 0 ? 180 : -180;
    }

    // Don't rotate the long way around.
    if (!fromDecomp.angle)
        fromDecomp.angle = 360;
    if (!toDecomp.angle)
        toDecomp.angle = 360;

    if (std::abs(fromDecomp.angle - toDecomp.angle) > 180) {
        if (fromDecomp.angle > toDecomp.angle)
            fromDecomp.angle -= 360;
        else
            toDecomp.angle -= 360;
    }
    
    // When compositeOperation is accumulate, if the decomposed function is a 1-based value (for affine matrix these properties are
    // m11, m22, scaleX and scaleY), use one based accumulation. Otherwise, use behavior for add.
    auto operationForNonOneBasedValues = compositeOperation == CompositeOperation::Accumulate ? CompositeOperation::Add : compositeOperation;
    blendFloat(fromDecomp.m11, toDecomp.m11, progress, compositeOperation);
    blendFloat(fromDecomp.m12, toDecomp.m12, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.m21, toDecomp.m21, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.m22, toDecomp.m22, progress, compositeOperation);
    blendFloat(fromDecomp.translateX, toDecomp.translateX, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.translateY, toDecomp.translateY, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.scaleX, toDecomp.scaleX, progress, compositeOperation);
    blendFloat(fromDecomp.scaleY, toDecomp.scaleY, progress, compositeOperation);
    blendFloat(fromDecomp.angle, toDecomp.angle, progress, operationForNonOneBasedValues);
    recompose2(fromDecomp);
}

void TransformationMatrix::blend4(const TransformationMatrix& from, double progress, CompositeOperation compositeOperation)
{
    Decomposed4Type fromDecomp;
    Decomposed4Type toDecomp;
    if (!from.decompose4(fromDecomp) || !decompose4(toDecomp)) {
        if (progress < 0.5)
            *this = from;
        return;
    }
    
    // When compositeOperation is accumulate, if the decomposed function is a 1-based value (for non-affine matrix these properties are
    // scaleX, scaleY, scaleZ, and perspectiveW), use one based accumulation. Otherwise, use behavior for add.
    auto operationForNonOneBasedValues = compositeOperation == CompositeOperation::Accumulate ? CompositeOperation::Add : compositeOperation;

    blendFloat(fromDecomp.scaleX, toDecomp.scaleX, progress, compositeOperation);
    blendFloat(fromDecomp.scaleY, toDecomp.scaleY, progress, compositeOperation);
    blendFloat(fromDecomp.scaleZ, toDecomp.scaleZ, progress, compositeOperation);
    blendFloat(fromDecomp.skewXY, toDecomp.skewXY, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.skewXZ, toDecomp.skewXZ, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.skewYZ, toDecomp.skewYZ, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.translateX, toDecomp.translateX, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.translateY, toDecomp.translateY, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.translateZ, toDecomp.translateZ, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.perspectiveX, toDecomp.perspectiveX, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.perspectiveY, toDecomp.perspectiveY, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.perspectiveZ, toDecomp.perspectiveZ, progress, operationForNonOneBasedValues);
    blendFloat(fromDecomp.perspectiveW, toDecomp.perspectiveW, progress, compositeOperation);
    fromDecomp.quaternion = fromDecomp.quaternion.interpolate(toDecomp.quaternion, progress, compositeOperation);

    recompose4(fromDecomp);
}

void TransformationMatrix::blend(const TransformationMatrix& from, double progress, CompositeOperation compositeOperation)
{
    
    if (!progress && compositeOperation == CompositeOperation::Replace) {
        *this = from;
        return;
    }

    if (progress == 1 && compositeOperation == CompositeOperation::Replace)
        return;

    if (from.isIdentity() && isIdentity())
        return;

    if (from.isAffine() && isAffine())
        blend2(from, progress, compositeOperation);
    else
        blend4(from, progress, compositeOperation);
}

bool TransformationMatrix::decompose2(Decomposed2Type& decomp) const
{
    if (isIdentity()) {
        memset(&decomp, 0, sizeof(decomp));
        decomp.scaleX = 1;
        decomp.scaleY = 1;
        decomp.m11 = 1;
        decomp.m22 = 1;
        return true;
    }

    return WebCore::decompose2(m_matrix, decomp);
}

bool TransformationMatrix::decompose4(Decomposed4Type& decomp) const
{
    if (isIdentity()) {
        memset(&decomp, 0, sizeof(decomp));
        decomp.perspectiveW = 1;
        decomp.scaleX = 1;
        decomp.scaleY = 1;
        decomp.scaleZ = 1;
        return true;
    }

    return WebCore::decompose4(m_matrix, decomp);
}

void TransformationMatrix::recompose2(const Decomposed2Type& decomp)
{
    makeIdentity();

    m_matrix[0][0] = decomp.m11;
    m_matrix[0][1] = decomp.m12;
    m_matrix[1][0] = decomp.m21;
    m_matrix[1][1] = decomp.m22;

    translateRight(decomp.translateX, decomp.translateY);
    rotate(decomp.angle);
    scale3d(decomp.scaleX, decomp.scaleY, 1);
}

void TransformationMatrix::recompose4(const Decomposed4Type& decomp)
{
    makeIdentity();

    // First apply perspective.
    m_matrix[0][3] = decomp.perspectiveX;
    m_matrix[1][3] = decomp.perspectiveY;
    m_matrix[2][3] = decomp.perspectiveZ;
    m_matrix[3][3] = decomp.perspectiveW;

    // Next, translate.
    translate3d(decomp.translateX, decomp.translateY, decomp.translateZ);

    // Apply rotation.
    TransformationMatrix rotationMatrix = TransformationMatrix::fromQuaternion(decomp.quaternion);

    multiply(rotationMatrix);

    // Apply skew.
    if (decomp.skewYZ) {
        TransformationMatrix tmp;
        tmp.setM32(decomp.skewYZ);
        multiply(tmp);
    }

    if (decomp.skewXZ) {
        TransformationMatrix tmp;
        tmp.setM31(decomp.skewXZ);
        multiply(tmp);
    }

    if (decomp.skewXY) {
        TransformationMatrix tmp;
        tmp.setM21(decomp.skewXY);
        multiply(tmp);
    }

    // Finally, apply scale.
    scale3d(decomp.scaleX, decomp.scaleY, decomp.scaleZ);
}

bool TransformationMatrix::isIntegerTranslation() const
{
    if (!isIdentityOrTranslation())
        return false;

    // Check for translate Z.
    if (m_matrix[3][2])
        return false;

    // Check for non-integer translate X/Y.
    if (static_cast<int>(m_matrix[3][0]) != m_matrix[3][0] || static_cast<int>(m_matrix[3][1]) != m_matrix[3][1])
        return false;

    return true;
}

bool TransformationMatrix::containsOnlyFiniteValues() const
{
    return std::isfinite(m_matrix[0][0]) && std::isfinite(m_matrix[0][1]) && std::isfinite(m_matrix[0][2]) && std::isfinite(m_matrix[0][3])
        && std::isfinite(m_matrix[1][0]) && std::isfinite(m_matrix[1][1]) && std::isfinite(m_matrix[1][2]) && std::isfinite(m_matrix[1][3])
        && std::isfinite(m_matrix[2][0]) && std::isfinite(m_matrix[2][1]) && std::isfinite(m_matrix[2][2]) && std::isfinite(m_matrix[2][3])
        && std::isfinite(m_matrix[3][0]) && std::isfinite(m_matrix[3][1]) && std::isfinite(m_matrix[3][2]) && std::isfinite(m_matrix[3][3]);
}

TransformationMatrix TransformationMatrix::to2dTransform() const
{
    return TransformationMatrix(m_matrix[0][0], m_matrix[0][1], 0, m_matrix[0][3],
                                m_matrix[1][0], m_matrix[1][1], 0, m_matrix[1][3],
                                0, 0, 1, 0,
                                m_matrix[3][0], m_matrix[3][1], 0, m_matrix[3][3]);
}

auto TransformationMatrix::toColumnMajorFloatArray() const -> FloatMatrix4
{
    return { {
        float(m11()), float(m12()), float(m13()), float(m14()),
        float(m21()), float(m22()), float(m23()), float(m24()),
        float(m31()), float(m32()), float(m33()), float(m34()),
        float(m41()), float(m42()), float(m43()), float(m44()) } };
}

bool TransformationMatrix::isBackFaceVisible() const
{
    // Back-face visibility is determined by transforming the normal vector (0, 0, 1) and
    // checking the sign of the resulting z component. However, normals cannot be
    // transformed by the original matrix, they require being transformed by the
    // inverse-transpose.
    //
    // Since we know we will be using (0, 0, 1), and we only care about the z-component of
    // the transformed normal, then we only need the m33() element of the
    // inverse-transpose. Therefore we do not need the transpose.
    //
    // Additionally, if we only need the m33() element, we do not need to compute a full
    // inverse. Instead, knowing the inverse of a matrix is adjoint(matrix) / determinant,
    // we can simply compute the m33() of the adjoint (adjugate) matrix, without computing
    // the full adjoint.

    double determinant = WebCore::determinant4x4(m_matrix);

    // If the matrix is not invertible, then we assume its backface is not visible.
    if (!std::isnormal(determinant))
        return false;

    double cofactor33 = determinant3x3(m11(), m12(), m14(), m21(), m22(), m24(), m41(), m42(), m44());
    double zComponentOfTransformedNormal = cofactor33 / determinant;

    return zComponentOfTransformedNormal < 0;
}

TransformationMatrix TransformationMatrix::transpose() const
{
    TransformationMatrix transpose;
    transposeMatrix4(m_matrix, transpose.m_matrix);
    return transpose;
}

TextStream& operator<<(TextStream& ts, const TransformationMatrix& transform)
{
    TextStream::IndentScope indentScope(ts);
    ts << "\n";
    ts << indent << "[" << transform.m11() << " " << transform.m12() << " " << transform.m13() << " " << transform.m14() << "]\n";
    ts << indent << "[" << transform.m21() << " " << transform.m22() << " " << transform.m23() << " " << transform.m24() << "]\n";
    ts << indent << "[" << transform.m31() << " " << transform.m32() << " " << transform.m33() << " " << transform.m34() << "]\n";
    ts << indent << "[" << transform.m41() << " " << transform.m42() << " " << transform.m43() << " " << transform.m44() << "]";
    return ts;
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
