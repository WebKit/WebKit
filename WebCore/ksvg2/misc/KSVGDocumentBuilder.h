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
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#ifndef KSVG_DocumentBuilder_H
#define KSVG_DocumentBuilder_H

#include <ksvg2/KSVGView.h>

#include <kdom/parser/KDOMDocumentBuilder.h>

class KURL;

namespace KSVG
{
    //class SVGDocument;
    class SVGDocumentImpl;

    // KSVG Document builder 
    class DocumentBuilder : public KDOM::DocumentBuilder
    {
    public:
        DocumentBuilder(KSVGView *view);
        virtual ~DocumentBuilder();

        // Helper function for in-memory created documents.
        // Use this function to link a newly created 'KDOM::Document'
        // with the passed 'KCanvas', and the other for indicate that
        // you are done creating your tree (invokes rendering in non-progr. mode)
//        static void linkDocumentToCanvas(const SVGDocument &doc, KSVGView *view);
        static void linkDocumentToCanvas(SVGDocumentImpl *docImpl, KSVGView *view);

        static void finishedDocument(SVGDocumentImpl *doc);

    protected:
        virtual bool startDocument(const KURL &uri);
        virtual bool endDocument();

    private:
        KSVGView *m_view;
    };
};

#endif

// vim:ts=4:noet
