/*
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include "FormData.h"
#include <wtf/Forward.h>

namespace WebCore {

class TextEncoding;

namespace FormDataBuilder {

// Helper functions used by HTMLFormElement for multi-part form data.
Vector<char> generateUniqueBoundaryString();
void beginMultiPartHeader(Vector<char>&, const CString& boundary, const Vector<uint8_t>& name);
void addBoundaryToMultiPartHeader(Vector<char>&, const CString& boundary, bool isLastBoundary = false);
void addFilenameToMultiPartHeader(Vector<char>&, const TextEncoding&, const String& filename);
void addContentTypeToMultiPartHeader(Vector<char>&, const CString& mimeType);
void finishMultiPartHeader(Vector<char>&);

// Helper functions used by HTMLFormElement for non-multi-part form data.
void addKeyValuePairAsFormData(Vector<char>&, const Vector<uint8_t>& key, const Vector<uint8_t>& value, FormData::EncodingType = FormData::FormURLEncoded);
void encodeStringAsFormData(Vector<char>&, const CString&);

}

}
