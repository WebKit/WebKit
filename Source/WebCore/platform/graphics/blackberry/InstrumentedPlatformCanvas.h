/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef InstrumentedPlatformCanvas_h
#define InstrumentedPlatformCanvas_h

#include <skia/ext/platform_canvas.h>

#define DEBUG_SKIA_DRAWING 0
#if DEBUG_SKIA_DRAWING
#define WRAPCANVAS_LOG_ENTRY(...) do { \
    fprintf(stderr, "%s ", __FUNCTION__); \
    fprintf(stderr, __VA_ARGS__); \
    fprintf(stderr, "\n"); \
} while (0)
#else
#define WRAPCANVAS_LOG_ENTRY(...) ((void)0)
#endif

namespace WebCore {

class InstrumentedPlatformCanvas : public SkCanvas {
public:
    InstrumentedPlatformCanvas(int width, int height)
        : m_size(width, height)
        , m_isSolidColor(true)
        , m_solidColor(0, 0, 0, 0)
    {
        setDevice(new SkDevice(SkBitmap::kARGB_8888_Config, width, height))->unref();
    }

    virtual ~InstrumentedPlatformCanvas() { }

    bool isSolidColor() const { return m_isSolidColor; }
    Color solidColor() const { return m_solidColor; }

    // overrides from SkCanvas
    virtual int save(SaveFlags flags)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::save(flags);
    }

    virtual int saveLayer(const SkRect* bounds, const SkPaint* paint, SaveFlags flags)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        return SkCanvas::saveLayer(bounds, paint, flags);
    }

    virtual void restore()
    {
        WRAPCANVAS_LOG_ENTRY("");
        SkCanvas::restore();
    }

    virtual bool translate(SkScalar dx, SkScalar dy)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::translate(dx, dy);
    }

    virtual bool scale(SkScalar sx, SkScalar sy)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::scale(sx, sy);
    }

    virtual bool rotate(SkScalar degrees)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::rotate(degrees);
    }

    virtual bool skew(SkScalar sx, SkScalar sy)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::skew(sx, sy);
    }

    virtual bool concat(const SkMatrix& matrix)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::concat(matrix);
    }

    virtual void setMatrix(const SkMatrix& matrix)
    {
        WRAPCANVAS_LOG_ENTRY("");
        SkCanvas::setMatrix(matrix);
    }

    virtual bool clipRect(const SkRect& rect, SkRegion::Op op)
    {
        WRAPCANVAS_LOG_ENTRY("");
        return SkCanvas::clipRect(rect, op);
    }

    virtual bool clipPath(const SkPath& path, SkRegion::Op op) 
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        return SkCanvas::clipPath(path, op);
    }

    virtual bool clipRegion(const SkRegion& region, SkRegion::Op op)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        return SkCanvas::clipRegion(region, op);
    }

    virtual void clear(SkColor color)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = true;
        m_solidColor = Color(color);
        SkCanvas::clear(color);
    }

    virtual void drawPaint(const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawPaint(paint);
    }

    virtual void drawPoints(PointMode mode, size_t count, const SkPoint pts[],
            const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawPoints(mode, count, pts, paint);
    }

    virtual void drawRect(const SkRect& rect, const SkPaint& paint)
    {
        IntRect rectToDraw(rect);
        WRAPCANVAS_LOG_ENTRY("rect = (x=%d,y=%d,width=%d,height=%d)", rectToDraw.x(), rectToDraw.y(), rectToDraw.width(), rectToDraw.height());
        IntRect canvasRect(IntPoint(), m_size);
        if (m_isSolidColor && getTotalMatrix().rectStaysRect() && getTotalClip().contains(canvasRect)) {
            const SkMatrix& matrix = getTotalMatrix();
            SkRect mapped;
            matrix.mapRect(&mapped, rect);
            if (mapped.contains(canvasRect)) {
                Color color = solidColor(paint);
                m_isSolidColor = color.isValid();
                m_solidColor = color;
             } else
                 m_isSolidColor = false;
        } else
            m_isSolidColor = false;
        SkCanvas::drawRect(rect, paint);
    }

    virtual void drawPath(const SkPath& path, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawPath(path, paint);
    }

    virtual void drawBitmap(const SkBitmap& bitmap, SkScalar left,
            SkScalar top, const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawBitmap(bitmap, left, top, paint);
    }

    virtual void drawBitmapRect(const SkBitmap& bitmap, const SkIRect* src,
            const SkRect& dst, const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawBitmapRect(bitmap, src, dst, paint);
    }

    virtual void drawBitmapMatrix(const SkBitmap& bitmap,
            const SkMatrix& matrix, const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawBitmapMatrix(bitmap, matrix, paint);
    }

    virtual void drawBitmapNine(const SkBitmap& bitmap, const SkIRect& center,
                                const SkRect& dst, const SkPaint* paint = 0)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawBitmapNine(bitmap, center, dst, paint);
    }

    virtual void drawSprite(const SkBitmap& bitmap, int left, int top,
            const SkPaint* paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawSprite(bitmap, left, top, paint);
    }

    virtual void drawText(const void* text, size_t byteLength, SkScalar x,
            SkScalar y, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawText(text, byteLength, x, y, paint);
    }

    virtual void drawPosText(const void* text, size_t byteLength,
            const SkPoint pos[], const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawPosText(text, byteLength, pos, paint);
    }

    virtual void drawPosTextH(const void* text, size_t byteLength,
            const SkScalar xpos[], SkScalar constY, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawPosTextH(text, byteLength, xpos, constY, paint);
    }

    virtual void drawTextOnPath(const void* text, size_t byteLength,
            const SkPath& path, const SkMatrix* matrix, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawTextOnPath(text, byteLength, path, matrix, paint);
    }

    virtual void drawPicture(SkPicture& picture)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawPicture(picture);
    }

    virtual void drawVertices(VertexMode mode, int vertexCount,
            const SkPoint vertices[], const SkPoint texs[],
            const SkColor colors[], SkXfermode* xfermode,
            const uint16_t indices[], int indexCount, const SkPaint& paint)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawVertices(mode, vertexCount, vertices, texs, colors, xfermode, indices, indexCount, paint);
    }

    virtual void drawData(const void* data, size_t size)
    {
        WRAPCANVAS_LOG_ENTRY("");
        m_isSolidColor = false;
        SkCanvas::drawData(data, size);
    }

private:
    Color solidColor(const SkPaint& paint)
    {
        if (paint.getStyle() != SkPaint::kFill_Style)
            return Color();
        if (paint.getLooper() || paint.getShader())
            return Color();

        SkXfermode::Mode mode;
        SkXfermode::AsMode(paint.getXfermode(), &mode);
        if (mode == SkXfermode::kClear_Mode)
            return Color(0, 0, 0, 0);

        if ((mode == SkXfermode::kSrcOver_Mode && paint.getAlpha() == 255) || mode == SkXfermode::kSrc_Mode)
            return Color(paint.getColor());
        return Color();
    }

    IntSize m_size;
    bool m_isSolidColor;
    Color m_solidColor;
    SkPaint m_solidPaint;
};

} // namespace WebCore

#endif // InstrumentedPlatformCanvas_h
