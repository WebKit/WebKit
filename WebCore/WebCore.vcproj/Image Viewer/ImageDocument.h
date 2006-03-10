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

// ImageDocument.h : interface of the ImageDocument class
//


#pragma once

#include "config.h"
#include "ImageAnimationObserver.h"
#include "Array.h"
#include "Image.h"

class ImageDocument : public CDocument, public WebCore::ImageAnimationObserver
{
protected: // create from serialization only
	ImageDocument();
	DECLARE_DYNCREATE(ImageDocument)

// Attributes
public:

// Operations
public:

// Overrides
public:
	virtual void Serialize(CArchive& ar);

        virtual bool shouldStopAnimation(const WebCore::Image*) { return false; }
        virtual void animationAdvanced(const WebCore::Image*);

        WebCore::Image* image() const { return m_image; }

// Implementation
public:
	virtual ~ImageDocument();
#ifdef _DEBUG
	virtual void AssertValid() const;
	virtual void Dump(CDumpContext& dc) const;
#endif

protected:
        ByteArray m_buffer;
        WebCore::Image* m_image;

// Generated message map functions
protected:
	DECLARE_MESSAGE_MAP()
};


