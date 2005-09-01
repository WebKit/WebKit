/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
                  2004, 2005 Rob Buis <buis@kde.org>
                  2005 Eric Seidel <eric.seidel@kdemail.net>

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

#ifndef KCanvasFilters_H
#define KCanvasFilters_H

#include "KCanvasResources.h"

// Enumerations
typedef enum
{
    FE_DISTANT_LIGHT = 0,
    FE_POINT_LIGHT = 1,
    FE_SPOT_LIGHT = 2,
    FE_BLEND = 3,
    FE_COLOR_MATRIX = 4,
    FE_COMPONENT_TRANSFER = 5,
    FE_COMPOSITE = 6,
    FE_CONVOLVE_MATRIX = 7,
    FE_DIFFUSE_LIGHTING = 8,
    FE_DISPLACEMENT_MAP = 9,
    FE_FLOOD = 10,
    FE_GAUSSIAN_BLUR = 11,
    FE_IMAGE = 12,
    FE_MERGE = 13,
    FE_MORPHOLOGY = 14,
    FE_OFFSET = 15,
    FE_SPECULAR_LIGHTING = 16,
    FE_TILE = 17,
    FE_TURBULENCE = 18
} KCFilterEffectType;

#include <qcolor.h>
#include <qstringlist.h>

class KCanvasFilterEffect;
class KRenderingDeviceContext;

class KCanvasFilter : public KCanvasResource
{
public:
    KCanvasFilter() { };
    virtual ~KCanvasFilter() { };
    
    bool filterBoundingBoxMode() const { return m_filterBBoxMode; }
    void setFilterBoundingBoxMode(bool bboxMode) { m_filterBBoxMode = bboxMode; }
    
    bool effectBoundingBoxMode() const { return m_effectBBoxMode; }
    void setEffectBoundingBoxMode(bool bboxMode) { m_effectBBoxMode = bboxMode; }    

    QRect filterRect() const { return m_filterRect; }
    void setFilterRect(const QRect &rect) { m_filterRect = rect; }

    void addFilterEffect(KCanvasFilterEffect *effect);

    virtual void prepareFilter(KRenderingDeviceContext *context, const QRect &bbox) = 0;
    virtual void applyFilter(KRenderingDeviceContext *context, const KCanvasCommonArgs &args, const QRect &bbox) = 0;

    QTextStream &externalRepresentation(QTextStream &) const;

protected:
    QRect m_filterRect;
    QValueList<KCanvasFilterEffect *> m_effects;
    bool m_filterBBoxMode;
    bool m_effectBBoxMode;
};

#ifdef APPLE_CHANGES
// FIXME: this strikes me as a total hack...
#ifdef __OBJC__
@class CIFilter;
#else
class CIFilter;
#endif
class KCanvasFilterQuartz;
#endif

class KCanvasFilterEffect
{
public:
    KCanvasFilterEffect() { };
    virtual ~KCanvasFilterEffect() { };

    virtual KCFilterEffectType effectType() const { return FE_TURBULENCE; }

    QRect subRegion() const;
    void setSubRegion(const QRect &subregion);

    QString in() const;
    void setIn(const QString &in);

    QString result() const;
    void setResult(const QString &result);
    
#ifdef APPLE_CHANGES
    virtual CIFilter *getCIFilter(KCanvasFilterQuartz *quartzFilter) const = 0;
#endif

    virtual QTextStream &externalRepresentation(QTextStream &) const;

private:
    QRect m_subregion;
    QString m_in;
    QString m_result;
};

QTextStream &operator<<(QTextStream &, const KCanvasFilterEffect &);

class KCanvasFEDistantLight : public KCanvasFilterEffect
{
public:
    float azimuth() const { return m_azimuth; }
    void setAzimuth(float azimuth) { m_azimuth = azimuth; }

    float elevation() const { return m_elevation; }
    void setElevation(float elevation) { m_elevation = elevation; }
    
    // FIXME, more here...
     QTextStream &externalRepresentation(QTextStream &) const;
private:
    float m_azimuth;
    float m_elevation;
};

class KCanvasFEPointLight : public KCanvasFilterEffect
{
public:

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    float m_x;
    float m_y;
    float m_z;
};

class KCanvasFESpotLight : public KCanvasFilterEffect
{
public:

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    float m_x;
    float m_y;
    float m_z;
    float m_pointsAtX;
    float m_pointsAtY;
    float m_pointsAtZ;
    float m_specularExponent;
    float m_limitingConeAngle;
};

typedef enum {
    BM_NORMAL = 0,
    BM_MULTIPLY = 1,
    BM_SCREEN = 2,
    BM_DARKEN = 3,
    BM_LIGHTEN = 4
} KCBlendModeType;

class KCanvasFEBlend : public KCanvasFilterEffect
{
public:
    QString in2() const { return m_in2; }
    void setIn2(const QString &in2) { m_in2 = in2; }
    
    KCBlendModeType blendMode() const { return m_mode; }
    void setBlendMode(KCBlendModeType mode) { m_mode = mode; }

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    KCBlendModeType m_mode;
    QString m_in2;
};

typedef enum {
    CMT_MATRIX = 0,
    CMT_SATURATE = 1,
    CMT_HUE_ROTATE = 2,
    CMT_LUMINANCE_TO_ALPHA = 3
} KCColorMatrixType;

class KCanvasFEColorMatrix : public KCanvasFilterEffect
{
public:
    KCColorMatrixType type() const { return m_type; }
    void setType(KCColorMatrixType type) { m_type = type; }
    
    QValueList<float> values() const { return m_values; }
    void setValues(const QValueList<float> &values) { m_values = values; };

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    KCColorMatrixType m_type;
    QValueList<float> m_values;
};

typedef enum {
    CT_IDENTITY = 0,
    CT_TABLE = 1,
    CT_DISCRETE = 2,
    CT_LINEAR = 3,
    CT_GAMMA = 4
} KCComponentTransferType;

/*
 // Keep this around in case we want to make this a class:
     KCComponentTransferType type() const;
    void setType(KCComponentTransferType type) { m_type = type; }
    
    QValueList<float> tableValues() const;
    void setTableValues(QValueList<float> tableValues);
    
    float slope() const { return m_slope; }
    void setSlope(float slope) { m_slope = slope; }
    
    float intercept() const { return m_intercept; }
    void setIntercept(float intercept) { m_intercept = intercept; }
    
    float amplitude() const { return m_amplitude; }
    void setAmplitude(float amplitude) { m_amplitude = amplitude; }
    
    float exponent() const { return m_exponent; }
    void setExponent(float exponent) { m_exponent = exponent; }
    
    float offset() const { return m_offset; }
    void setOffset(float offset) { m_offset = offset; }
*/

typedef struct {
    KCComponentTransferType type;
    QValueList<float> tableValues;
    float slope;
    float intercept;
    float amplitude;
    float exponent;
    float offset;
} KCComponentTransferFunction;

class KCanvasFEComponentTransfer : public KCanvasFilterEffect
{
public:
    KCComponentTransferFunction redFunction() const { return m_redFunc; }
    void setRedFunction(const KCComponentTransferFunction &func) { m_redFunc = func; }
    
    KCComponentTransferFunction greenFunction() const { return m_greenFunc; }
    void setGreenFunction(const KCComponentTransferFunction &func) { m_greenFunc = func; }
    
    KCComponentTransferFunction blueFunction() const { return m_blueFunc; }
    void setBlueFunction(const KCComponentTransferFunction &func) { m_blueFunc = func; }
    
    KCComponentTransferFunction alphaFunction() const { return m_alphaFunc; }
    void setAlphaFunction(const KCComponentTransferFunction &func) { m_alphaFunc = func; }

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    KCComponentTransferFunction m_redFunc;
    KCComponentTransferFunction m_greenFunc;
    KCComponentTransferFunction m_blueFunc;
    KCComponentTransferFunction m_alphaFunc;
};

typedef enum {
    CO_OVER = 0,
    CO_IN = 1,
    CO_OUT = 2,
    CO_ATOP = 3,
    CO_XOR = 4,
    CO_ARITHMETIC = 5,
} KCCompositeOperationType;

class KCanvasFEComposite : public KCanvasFilterEffect
{
public:
    QString in2() const { return m_in2; }
    void setIn2(const QString &in2) { m_in2 = in2; }
    
    KCCompositeOperationType operation() const { return m_operation; }
    void setOperation(KCCompositeOperationType oper) { m_operation = oper; }
    
    float k1() const { return m_k1; }
    void setK1(float k1) { m_k1 = k1; }
    float k2() const { return m_k2;}
    void setK2(float k2) { m_k2 = k2; }
    float k3() const { return m_k3; }
    void setK3(float k3) { m_k3 = k3; }
    float k4() const { return m_k4; }
    void setK4(float k4) { m_k4 = k4; }

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    QString m_in2;
    KCCompositeOperationType m_operation;
    float m_k1;
    float m_k2;
    float m_k3;
    float m_k4;
};

typedef enum {
    EM_DUPLICATE = 0,
    EM_WRAP = 1,
    EM_NONE = 2
} KCEdgeModeType;

class KCanvasFEConvolveMatrix : public KCanvasFilterEffect
{
public:

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    int m_orderX;
    int m_orderY;
    QValueList<float> m_kernelMatrix; // maybe should be a real matrix?
    float m_divisor;
    float m_bias;
    int m_targetX;
    int m_targetY;
    KCEdgeModeType m_edgeMode;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
    bool m_preserveAlpha;
};

class KCanvasFEDiffuseLighting : public KCanvasFilterEffect
{
public:

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    float m_surfaceScale;
    float m_diffuseConstant;
    float m_kernelUnitLengthX;
    float m_kernelUnitLengthY;
};

typedef enum {
    CS_RED = 0,
    CS_GREEN = 1,
    CS_BLUE = 2,
    CS_ALPHA = 3
} KCChannelSelectorType;

class KCanvasFEDisplacementMap : public KCanvasFilterEffect
{
public:
    QString in2() const { return m_in2; }
    void setIn2(const QString &in2) { m_in2 = in2; }

    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    float m_scale;
    KCChannelSelectorType m_XChannelSelector;
    KCChannelSelectorType m_YChannelSelector;
    QString m_in2;
};

class KCanvasFEFlood : public KCanvasFilterEffect
{
public:
    QColor floodColor() const { return m_floodColor; }
    void setFloodColor(const QColor &color) { m_floodColor = color; }


    float floodOpacity() const { return m_floodOpacity; }
    void setFloodOpacity(float floodOpacity) { m_floodOpacity = floodOpacity; }

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    QColor m_floodColor;
    float m_floodOpacity;
};

class KCanvasFEGaussianBlur : public KCanvasFilterEffect
{
public:
    float stdDeviationX() const;
    void setStdDeviationX(float x);

    float stdDeviationY() const;
    void setStdDeviationY(float y);

    QTextStream &externalRepresentation(QTextStream &) const;

private:
    float m_x;
    float m_y;
};

class KCanvasFEImage : public KCanvasFilterEffect
{
public:
    KCanvasFEImage() : m_image(0) {}
    virtual ~KCanvasFEImage();

    KCanvasItem *image() const { return m_image; }
    void setImage(KCanvasItem *image) { m_image = image; }

    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    KCanvasItem *m_image;
};

class KCanvasFEMerge : public KCanvasFilterEffect
{
public:
    QStringList mergeInputs() const { return m_mergeInputs; }
    void setMergeInputs(const QStringList &mergeInputs) { m_mergeInputs = mergeInputs; }

    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    QStringList m_mergeInputs;
};

typedef enum {
    MO_ERODE = 0,
    MO_DIALATE = 1
} KCMorphologyOperatorType;

class KCanvasFEMorphology : public KCanvasFilterEffect
{
public:
    KCMorphologyOperatorType morphologyOperator() const { return m_operator; }
    void setMorphologyOperator(KCMorphologyOperatorType _operator) { m_operator = _operator; }

    float radiusX() const { return m_radiusX; }
    void setRadiusX(float radiusX) { m_radiusX = radiusX; }

    float radiusY() const { return m_radiusY; }
    void setRadiusY(float radiusY) { m_radiusY = radiusY; }
    
    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    KCMorphologyOperatorType m_operator;
    float m_radiusX;
    float m_radiusY;
};

class KCanvasFEOffset : public KCanvasFilterEffect
{
public:
    float dx() const { return m_dx; }
    void setDx(float dx) { m_dx = dx; }

    float dy() const { return m_dy; }
    void setDy(float dy) { m_dy = dy; }
    
    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    float m_dx;
    float m_dy;
};

class KCanvasFESpecularLighting : public KCanvasFilterEffect
{
public:

    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    float m_surfaceScale;
    float m_specularConstant;
    float m_specularExponent;
};

class KCanvasFETile : public KCanvasFilterEffect {};

typedef enum {
    TT_TURBULANCE = 0,
    TT_NOISE = 1
} KCTurbulanceType;

class KCanvasFETurbulence : public KCanvasFilterEffect
{
public:
    KCTurbulanceType type() const { return m_type; }
    void setType(KCTurbulanceType type) { m_type = type; }

    float baseFrequencyY() const { return m_baseFrequencyY; }
    void setBaseFrequencyY(float baseFrequencyY) { m_baseFrequencyY = baseFrequencyY; }

    float baseFrequencyX() const { return m_baseFrequencyX; }
    void setBaseFrequencyX(float baseFrequencyX) { m_baseFrequencyX = baseFrequencyX; }

    float seed() const { return m_seed; }
    void setSeed(float seed) { m_seed = seed; }

    int numOctaves() const { return m_numOctaves; }
    void setNumOctaves(bool numOctaves) { m_numOctaves = numOctaves; }

    bool stitchTiles() const { return m_stitchTiles; }
    void setStitchTiles(bool stitch) { m_stitchTiles = stitch; }

    QTextStream &externalRepresentation(QTextStream &) const;
    
private:
    float m_baseFrequencyX;
    float m_baseFrequencyY;
    int m_numOctaves;
    float m_seed;
    bool m_stitchTiles;
    KCTurbulanceType m_type;
};

#endif
