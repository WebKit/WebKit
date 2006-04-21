// ImageView.cpp : implementation of the ImageView class
//

#include "stdafx.h"
#include "Image Viewer.h"
#include "ImageDocument.h"
#include "ImageView.h"
#include <cairo.h>
#include "Image.h"
#include "FloatRect.h"
#include "IntRect.h"
#include "GraphicsContext.h"
#include "FrameWin.h"

using namespace WebCore;

#ifdef _DEBUG
#define new DEBUG_NEW
#endif


// ImageView

IMPLEMENT_DYNCREATE(ImageView, CView)

BEGIN_MESSAGE_MAP(ImageView, CView)
	// Standard printing commands
	ON_COMMAND(ID_FILE_PRINT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_DIRECT, &CView::OnFilePrint)
	ON_COMMAND(ID_FILE_PRINT_PREVIEW, &CView::OnFilePrintPreview)
        ON_WM_ERASEBKGND()
END_MESSAGE_MAP()

// ImageView construction/destruction

ImageView::ImageView()
{
    // Needed to make sure everything is properly initialized;
    static FrameWin *dummyFrame = new FrameWin(0, 0, 0);
}

ImageView::~ImageView()
{
}

BOOL ImageView::PreCreateWindow(CREATESTRUCT& cs)
{
	// TODO: Modify the Window class or styles here by modifying
	//  the CREATESTRUCT cs

	return CView::PreCreateWindow(cs);
}

// ImageView drawing

BOOL ImageView::OnEraseBkgnd(CDC* pDC)
{
    // Don't ever let windows erase the background.
    ImageDocument* pDoc = GetDocument();
    if (!pDoc || !pDoc->image())
        return CView::OnEraseBkgnd(pDC);
    return FALSE;
}

void ImageView::OnDraw(CDC* pDC)
{
    ImageDocument* pDoc = GetDocument();
    ASSERT_VALID(pDoc);
    if (!pDoc)
        return;

    Image* image = pDoc->image();
    if (!image)
        return;

    // Make a win32 cairo surface for painting.
    HDC dc = pDC->m_hDC;
    
    // Center the image.
    RECT rect;
    GetClientRect(&rect);

    cairo_surface_t* finalSurface = cairo_win32_surface_create(dc);
    cairo_surface_t* surface = cairo_surface_create_similar(finalSurface,
                                                            CAIRO_CONTENT_COLOR_ALPHA,
                                                            rect.right, rect.bottom);

    cairo_t* context = cairo_create(surface);
    GraphicsContext gc(context);
    // Fill with white.
    gc.fillRect(IntRect(0, 0, rect.right, rect.bottom), Color::white);

    // Comment in to test overlapping.
    bool overlapping = false; // true;

    // Comment in to test tiling.
    bool tile = false; // true;
    FloatPoint srcPoint; // The src point to use when tiling.  Change value to test offset tiling.

    // Comment the multiplicative factor in to test scaling (doubles the size).
    float width = tile ? rect.right : image->size().width(); // * 2;
    float height = tile ? rect.bottom : image->size().height(); // * 2;

    float left = tile ? 0 : (rect.right - width) / 2;
    float top = tile ? 0 : (rect.bottom - height) / 2;
    if (left < 0) left = 0;
    if (top < 0) top = 0;

    FloatRect dstRect(FloatPoint(left, top), FloatSize(width, height));
    FloatRect imageRect(srcPoint, image->size());
    if (tile)
        gc.drawTiledImage(image, IntRect(left, top, width, height), IntPoint(srcPoint.x(), srcPoint.y()), image->size());
    else {
        gc.drawImage(image, dstRect, imageRect);
    //    if (overlapping)
     //       image->drawInRect(FloatRect(dstRect.x() + dstRect.width()/2, dstRect.y(), dstRect.width(), dstRect.height()),
     //                        imageRect, Image::CompositeSourceOver, context);
    }
    
    context = cairo_create(finalSurface);
    cairo_set_operator(context, CAIRO_OPERATOR_SOURCE);
    cairo_set_source_surface(context, surface, 0, 0);
    cairo_rectangle(context, 0, 0, rect.right, rect.bottom);
    cairo_fill(context);
    cairo_destroy(context);

    cairo_surface_destroy(surface);
    cairo_surface_destroy(finalSurface);   
}


// ImageView printing

BOOL ImageView::OnPreparePrinting(CPrintInfo* pInfo)
{
	// default preparation
	return DoPreparePrinting(pInfo);
}

void ImageView::OnBeginPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add extra initialization before printing
}

void ImageView::OnEndPrinting(CDC* /*pDC*/, CPrintInfo* /*pInfo*/)
{
	// TODO: add cleanup after printing
}


// ImageView diagnostics

#ifdef _DEBUG
void ImageView::AssertValid() const
{
	CView::AssertValid();
}

void ImageView::Dump(CDumpContext& dc) const
{
	CView::Dump(dc);
}

ImageDocument* ImageView::GetDocument() const // non-debug version is inline
{
	ASSERT(m_pDocument->IsKindOf(RUNTIME_CLASS(ImageDocument)));
	return (ImageDocument*)m_pDocument;
}
#endif //_DEBUG


// ImageView message handlers
