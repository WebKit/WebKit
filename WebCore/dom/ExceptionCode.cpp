/*
 *  Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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

#include "config.h"
#include "ExceptionCode.h"

#include "EventException.h"
#include "RangeException.h"
#include "XMLHttpRequestException.h"

#if ENABLE(SVG)
#include "SVGException.h"
#endif

#if ENABLE(XPATH)
#include "XPathException.h"
#endif

namespace WebCore {

static const char* const exceptionNames[] = {
    "INDEX_SIZE_ERR",
    "DOMSTRING_SIZE_ERR",
    "HIERARCHY_REQUEST_ERR",
    "WRONG_DOCUMENT_ERR",
    "INVALID_CHARACTER_ERR",
    "NO_DATA_ALLOWED_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "NOT_FOUND_ERR",
    "NOT_SUPPORTED_ERR",
    "INUSE_ATTRIBUTE_ERR",
    "INVALID_STATE_ERR",
    "SYNTAX_ERR",
    "INVALID_MODIFICATION_ERR",
    "NAMESPACE_ERR",
    "INVALID_ACCESS_ERR",
    "VALIDATION_ERR",
    "TYPE_MISMATCH_ERR"
};

static const char* const rangeExceptionNames[] = {
    "BAD_BOUNDARYPOINTS_ERR",
    "INVALID_NODE_TYPE_ERR"
};

static const char* const eventExceptionNames[] = {
    "UNSPECIFIED_EVENT_TYPE_ERR"
};

static const char* const xmlHttpRequestExceptionNames[] = {
    "NETWORK_ERR"
};

#if ENABLE(XPATH)
static const char* const xpathExceptionNames[] = {
    "INVALID_EXPRESSION_ERR",
    "TYPE_ERR"
};
#endif

#if ENABLE(SVG)
static const char* const svgExceptionNames[] = {
    "SVG_WRONG_TYPE_ERR",
    "SVG_INVALID_VALUE_ERR",
    "SVG_MATRIX_NOT_INVERTABLE"
};
#endif

void getExceptionCodeDescription(ExceptionCode ec, ExceptionCodeDescription& description)
{
    ASSERT(ec);

    const char* typeName;
    int code = ec;
    const char* const* nameTable;
    int nameTableSize;
    int nameTableOffset;
    ExceptionType type;
    
    if (code >= RangeException::RangeExceptionOffset && code <= RangeException::RangeExceptionMax) {
        type = RangeExceptionType;
        typeName = "DOM Range";
        code -= RangeException::RangeExceptionOffset;
        nameTable = rangeExceptionNames;
        nameTableSize = sizeof(rangeExceptionNames) / sizeof(rangeExceptionNames[0]);
        nameTableOffset = RangeException::BAD_BOUNDARYPOINTS_ERR;
    } else if (code >= EventException::EventExceptionOffset && code <= EventException::EventExceptionMax) {
        type = EventExceptionType;
        typeName = "DOM Events";
        code -= EventException::EventExceptionOffset;
        nameTable = eventExceptionNames;
        nameTableSize = sizeof(eventExceptionNames) / sizeof(eventExceptionNames[0]);
        nameTableOffset = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
    } else if (code >= XMLHttpRequestException::XMLHttpRequestExceptionOffset && code <= XMLHttpRequestException::XMLHttpRequestExceptionMax) {
        type = XMLHttpRequestExceptionType;
        typeName = "XMLHttpRequest";
        code -= XMLHttpRequestException::XMLHttpRequestExceptionOffset;
        nameTable = xmlHttpRequestExceptionNames;
        nameTableSize = sizeof(xmlHttpRequestExceptionNames) / sizeof(xmlHttpRequestExceptionNames[0]);
        // XMLHttpRequest exception codes start with 101 and we don't want 100 empty elements in the name array
        nameTableOffset = XMLHttpRequestException::NETWORK_ERR;
#if ENABLE(XPATH)
    } else if (code >= XPathException::XPathExceptionOffset && code <= XPathException::XPathExceptionMax) {
        type = XPathExceptionType;
        typeName = "DOM XPath";
        code -= XPathException::XPathExceptionOffset;
        nameTable = xpathExceptionNames;
        nameTableSize = sizeof(xpathExceptionNames) / sizeof(xpathExceptionNames[0]);
        // XPath exception codes start with 51 and we don't want 51 empty elements in the name array
        nameTableOffset = XPathException::INVALID_EXPRESSION_ERR;
#endif
#if ENABLE(SVG)
    } else if (code >= SVGException::SVGExceptionOffset && code <= SVGException::SVGExceptionMax) {
        type = SVGExceptionType;
        typeName = "DOM SVG";
        code -= SVGException::SVGExceptionOffset;
        nameTable = svgExceptionNames;
        nameTableSize = sizeof(svgExceptionNames) / sizeof(svgExceptionNames[0]);
        nameTableOffset = SVGException::SVG_WRONG_TYPE_ERR;
#endif
    } else {
        type = DOMExceptionType;
        typeName = "DOM";
        nameTable = exceptionNames;
        nameTableSize = sizeof(exceptionNames) / sizeof(exceptionNames[0]);
        nameTableOffset = INDEX_SIZE_ERR;
    }

    description.typeName = typeName;
    description.name = (ec >= nameTableOffset && ec - nameTableOffset < nameTableSize) ? nameTable[ec - nameTableOffset] : 0;
    description.code = code;
    description.type = type;

    // All exceptions used in the DOM code should have names.
    ASSERT(description.name);
}

} // namespace WebCore
