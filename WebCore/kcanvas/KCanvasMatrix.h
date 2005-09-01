/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    aint with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KCanvasMatrix_H
#define KCanvasMatrix_H

#include <qwmatrix.h>

typedef enum
{
    OPS_PREMUL = 1, // Default mode (svg compatible)
    OPS_POSTMUL = 2
} KCMatrixOperationMode;

class KCPathDataList;
class KCanvasMatrix
{
public:
    KCanvasMatrix();
    KCanvasMatrix(const QWMatrix &qmatrix);
    KCanvasMatrix(const KCanvasMatrix &matrix);
    KCanvasMatrix(double a, double b, double c, double d, double e, double f);
    ~KCanvasMatrix();

    // Operators
    KCanvasMatrix &operator=(const QWMatrix &other);
    KCanvasMatrix &operator=(const KCanvasMatrix &other);

    bool operator==(const QWMatrix &other) const;
    bool operator!=(const QWMatrix &other) const;

    bool operator==(const KCanvasMatrix &other) const;
    bool operator!=(const KCanvasMatrix &other) const;

    void setOperationMode(KCMatrixOperationMode mode);

    double a() const;
    void setA(double a);

    double b() const;
    void setB(double b);

    double c() const;
    void setC(double c);

    double d() const;
    void setD(double d);

    double e() const;
    void setE(double e);

    double f() const;
    void setF(double f);

    // OPS_PREMUL : Pre-multiplied operations (needed for svg-like matrix operations)
    // OPS_POSTMUL: Post-multiplied operations
    KCanvasMatrix &translate(double x, double y);
    KCanvasMatrix &multiply(const KCanvasMatrix &other);
    KCanvasMatrix &scale(double scaleFactorX, double scaleFactorY);

    KCanvasMatrix &rotate(double angle);
    KCanvasMatrix &rotateFromVector(double x, double y);

    KCanvasMatrix &flipX();
    KCanvasMatrix &flipY();

    KCanvasMatrix &skewX(double angle);
    KCanvasMatrix &skewY(double angle);

    void reset();

    // Determine the scaling component of the matrix and factor it out.
    // After this operation, the matrix has x and y scale of one ('1').
    void removeScale(double *xScale, double *yScale);

    // Transform 'polydata' using current matrix.
    // (Only works for lineto/moveto operations!)
    KCPathDataList map(const KCPathDataList &pathData) const;

    QWMatrix qmatrix() const;

private:
    QWMatrix m_matrix;
    KCMatrixOperationMode m_mode;
};

#endif

// vim:ts=4:noet
