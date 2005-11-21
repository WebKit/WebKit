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

#include "config.h"
#include "KCanvasFilters.h"

#include <qtextstream.h>
#include "KCanvasTreeDebug.h"

// Filters

void KCanvasFilter::addFilterEffect(KCanvasFilterEffect *effect)
{
    m_effects.append(effect);
}

QTextStream &KCanvasFilter::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=FILTER] "
        << " [bounding box=" << filterRect() << "]";
    if (!filterBoundingBoxMode())        
        ts << " [bounding box mode=" << filterBoundingBoxMode() << "]";
    if (!effectBoundingBoxMode())
        ts << " [effect bounding box mode=" << effectBoundingBoxMode() << "]";
    if (m_effects.count() > 0)
        ts << " [effects=" << m_effects << "]";
    return ts;
}

KCanvasFilter *getFilterById(KDOM::DocumentImpl *document, const KDOM::DOMString &id)
{
    KCanvasResource *resource = getResourceById(document, id);
    if (resource && resource->isFilter())
        return static_cast<KCanvasFilter *>(resource);
    return 0;
}

QTextStream &operator<<(QTextStream &ts, const KCanvasFilterEffect &e)
{
    return e.externalRepresentation(ts);
}

QRect KCanvasFilterEffect::subRegion() const
{
    return m_subregion;
}

void KCanvasFilterEffect::setSubRegion(const QRect &subregion)
{
    m_subregion = subregion;
}

QString KCanvasFilterEffect::in() const
{
    return m_in;
}
void KCanvasFilterEffect::setIn(const QString &in)
{
    m_in = in;
}

QString KCanvasFilterEffect::result() const
{
    return m_result;
}

void KCanvasFilterEffect::setResult(const QString &result)
{
    m_result = result;
}

QTextStream &KCanvasFilterEffect::externalRepresentation(QTextStream &ts) const
{
    if (!in().isEmpty())
        ts << "[in=\"" << in() << "\"]";
    if (!result().isEmpty())
        ts << " [result=\"" << result() << "\"]";
    if (!subRegion().isEmpty())
        ts << " [subregion=\"" << subRegion() << "\"]";
    return ts;    
}

QTextStream &KCanvasFEDistantLight::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=DISTANT-LIGHT] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << "[azimuth=" << azimuth() << "]"
        << "[elevation=" << elevation() << "]";
    return ts;
}

QTextStream &KCanvasFEPointLight::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=POINT-LIGHT] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << "[x=" << m_x << " y=" << m_y << " z=" << m_z << "]";
    return ts;
}

QTextStream &KCanvasFESpotLight::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=SPOT-LIGHT] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << "[x=" << m_x << " y=" << m_y << " z=" << m_z << "]"
        << "[points at x=" << m_pointsAtX << " y=" << m_pointsAtY << " z=" << m_pointsAtZ << "]";
    return ts;
}

static QTextStream &operator<<(QTextStream &ts, KCBlendModeType t)
{
    switch (t) 
    {
        case BM_NORMAL:
            ts << "NORMAL"; break;
        case BM_MULTIPLY:    
            ts << "MULTIPLY"; break;
        case BM_SCREEN:    
            ts << "SCREEN"; break;
        case BM_DARKEN:
            ts << "DARKEN"; break;
        case BM_LIGHTEN:
            ts << "LIGHTEN"; break;            
    }
    return ts;        
}

QTextStream &KCanvasFEBlend::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=BLEND] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    if (!m_in2.isEmpty())
        ts << " [in2=\"" << m_in2 << "\"]";
    ts << " [blend mode=" << m_mode << "]";
    return ts;
}

static QTextStream &operator<<(QTextStream &ts, KCColorMatrixType t)
{
    switch (t) 
    {
        case CMT_MATRIX:
            ts << "CMT_MATRIX"; break;
        case CMT_SATURATE:    
            ts << "CMT_SATURATE"; break;
        case CMT_HUE_ROTATE:    
            ts << "HUE-ROTATE"; break;
        case CMT_LUMINANCE_TO_ALPHA:
            ts << "LUMINANCE-TO-ALPHA"; break;
    }
    return ts;        
}

QTextStream &KCanvasFEColorMatrix::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=COLOR-MATRIX] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [color matrix type=" << type() << "]"
        << " [values=" << values() << "]";
    return ts;
}

static QTextStream &operator<<(QTextStream &ts, KCComponentTransferType t)
{
    switch (t) 
    {
        case CT_IDENTITY:
            ts << "IDENTITY";break;
        case CT_TABLE:
            ts << "TABLE"; break;
        case CT_DISCRETE:
            ts << "DISCRETE"; break;
        case CT_LINEAR:             
            ts << "LINEAR"; break;
        case CT_GAMMA:
            ts << "GAMMA"; break;
    }
    return ts;
}

static QTextStream &operator<<(QTextStream &ts, const KCComponentTransferFunction &func)
{
    ts << "[type=" << func.type << "]" 
        << " [table values=" << func.tableValues << "]"
        << " [slope=" << func.slope << "]"
        << " [intercept=" << func.intercept << "]"
        << " [amplitude=" << func.amplitude << "]"
        << " [exponent=" << func.exponent << "]"
        << " [offset=" << func.offset << "]";
    return ts;        
}

QTextStream &KCanvasFEComponentTransfer::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=COMPONENT-TRANSFER] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [red func=" << redFunction() << "]"
        << " [green func=" << greenFunction() << "]"
        << " [blue func=" << blueFunction() << "]"
        << " [alpha func=" << alphaFunction() << "]";
    return ts;  
}

QTextStream &KCanvasFEComposite::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=COMPOSITE] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    if (!in2().isEmpty())
        ts << " [in2=\"" << in2() << "\"]";    
    ts << " [k1=" << k1() << " k2=" << k2() << " k3=" << k3() << " k4=" << k4() << "]";
    return ts;
}

static QTextStream &operator<<(QTextStream &ts, KCEdgeModeType t)
{   
    switch (t) 
    {
        case EM_DUPLICATE:
            ts << "DUPLICATE";break;
        case EM_WRAP:
            ts << "WRAP"; break;
        case EM_NONE:
            ts << "NONE"; break;
    }
    return ts;    
}

QTextStream &KCanvasFEConvolveMatrix::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=CONVOLVE-MATRIX] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [order x= " << m_orderX << " y=" << m_orderY << "]"
        << " [kernel matrix=" << m_kernelMatrix  << "]"
        << " [divisor=" << m_divisor << "]"
        << " [bias=" << m_bias << "]"
        << " [target x= " << m_targetX << " y=" << m_targetY << "]"
        << " [edge mode=" << m_edgeMode << "]"
        << " [kernel unit length x=" << m_kernelUnitLengthX << " y=" << m_kernelUnitLengthY << "]"
        << " [preserve alpha=" << m_preserveAlpha << "]";        
   return ts;
}

QTextStream &KCanvasFEDiffuseLighting::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=DIFFUSE-LIGHTING] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [surface scale=" << m_surfaceScale << "]"
        << " [diffuse constant=" << m_diffuseConstant << "]"
        << " [kernel unit length x=" << m_kernelUnitLengthX << " y=" << m_kernelUnitLengthY << "]";
   return ts;
}

static QTextStream &operator<<(QTextStream &ts, KCChannelSelectorType t)
{
    switch (t)
    {
        case CS_RED:
            ts << "RED"; break;
        case CS_GREEN:
            ts << "GREEN"; break;
        case CS_BLUE:
            ts << "BLUE"; break;
        case CS_ALPHA:
            ts << "ALPHA"; break;
    }
    return ts;
}

QTextStream &KCanvasFEDisplacementMap::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=DISPLACEMENT-MAP] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    if (!in2().isEmpty())
        ts << " [in2=" << in2() << "]";        
    ts << " [scale=" << m_scale << "]"
        << " [x channel selector=" << m_XChannelSelector << "]"
        << " [y channel selector=" << m_XChannelSelector << "]";
   return ts;
}

QTextStream &KCanvasFEFlood::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=FLOOD] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [color=" << floodColor() << "]"
        << " [opacity=" << floodOpacity() << "]";
   return ts;
}

float KCanvasFEGaussianBlur::stdDeviationX() const
{
    return m_x;
}

void KCanvasFEGaussianBlur::setStdDeviationX(float x)
{
    m_x = x;
}

float KCanvasFEGaussianBlur::stdDeviationY() const
{
    return m_y;
}

void KCanvasFEGaussianBlur::setStdDeviationY(float y)
{
    m_y = y;
}

QTextStream &KCanvasFEGaussianBlur::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=GAUSSIAN-BLUR] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [std dev. x=" << stdDeviationX() << " y=" << stdDeviationY() << "]";
    return ts;
}

KCanvasFEImage::~KCanvasFEImage()
{
    delete m_image;
}

QTextStream &KCanvasFEImage::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=IMAGE] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    //FIXME: should this dump also object returned by KCanvasFEImage::image() ?
    return ts;
}

QTextStream &KCanvasFEMerge::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=MERGE] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << "[merge inputs=" << m_mergeInputs << "]";    
    return ts;
}

static QTextStream &operator<<(QTextStream &ts, KCMorphologyOperatorType t)
{
    switch (t)
    {
        case MO_ERODE:
            ts << "ERODE"; break;
        case MO_DIALATE:
            ts << "DIALATE"; break;
    }
    return ts;
}

QTextStream &KCanvasFEMorphology::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=MORPHOLOGY-OPERATOR] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [operator type=" << morphologyOperator() << "]"
        << " [radius x=" << radiusX() << " y=" << radiusY() << "]";
   return ts;
}

QTextStream &KCanvasFEOffset::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=OFFSET] "; KCanvasFilterEffect::externalRepresentation(ts)
        << " [dx=" << dx() << " dy=" << dy() << "]";    
   return ts;
}

QTextStream &KCanvasFESpecularLighting::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=SPECULAR-LIGHTING] ";
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [surface scale=" << m_surfaceScale << "]"
        << " [specual constant=" << m_specularConstant << "]"
        << " [specular exponent=" << m_specularExponent << "]";        
   return ts;
}

#if 0
///FIXME: KCanvasFETile doesn't have any properties
QTextStream &KCanvasFETile::externalRepresentation(QTextStream &ts) const
{
   KCanvasFilterEffect::externalRepresentation(ts);
   return ts;
}
#endif

static QTextStream &operator<<(QTextStream &ts, KCTurbulanceType t)
{
    switch (t)
    {
        case TT_TURBULANCE:
            ts << "TURBULANCE";break;
        case TT_NOISE:
            ts << "NOISE"; break;
    }
    return ts;
}

QTextStream &KCanvasFETurbulence::externalRepresentation(QTextStream &ts) const
{
    ts << "[type=TURBULENCE] "; 
    KCanvasFilterEffect::externalRepresentation(ts);
    ts << " [turbulence type=" << type() << "]"
        << " [base frequency x=" << baseFrequencyX() << " y=" << baseFrequencyY() << "]"
        << " [seed=" << seed() << "]"
        << " [num octaves=" << numOctaves() << "]"
        << " [stitch tiles=" << stitchTiles() << "]";
   return ts;
}
