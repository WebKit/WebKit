/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

// ImageDocument.cpp : implementation of the ImageDocument class
//

#include "stdafx.h"
#include "Image Viewer.h"

#include "ImageDocument.h"
#include "ImageView.h"

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

using WebCore::Image;
using WebCore::ByteArray;

// ImageDocument

IMPLEMENT_DYNCREATE(ImageDocument, CDocument)

BEGIN_MESSAGE_MAP(ImageDocument, CDocument)
END_MESSAGE_MAP()


// ImageDocument construction/destruction

ImageDocument::ImageDocument()
{
    m_image = 0;
}

ImageDocument::~ImageDocument()
{
    delete m_image;
}

// ImageDocument serialization

void ImageDocument::Serialize(CArchive& ar)
{
    if (ar.IsStoring())
        ar.Write(m_buffer.data(), m_buffer.size());
    else {
	// Pull the image into a buffer.
        CFile* file = ar.GetFile();
        if (!file)
            return;
        UINT len = (UINT)file->GetLength();
        if (len == 0)
            return;
        m_buffer.resize(len);
        ar.Read(m_buffer.data(), len);
        if (m_image)
            delete m_image;
        m_image = new Image(this);
        m_image->setData(m_buffer, true);
        UpdateAllViews(0);
    }
}


// ImageDocument diagnostics

#ifdef _DEBUG
void ImageDocument::AssertValid() const
{
	CDocument::AssertValid();
}

void ImageDocument::Dump(CDumpContext& dc) const
{
	CDocument::Dump(dc);
}
#endif //_DEBUG


// ImageDocument commands

void ImageDocument::animationAdvanced(const Image*)
{
    UpdateAllViews(0);
}
