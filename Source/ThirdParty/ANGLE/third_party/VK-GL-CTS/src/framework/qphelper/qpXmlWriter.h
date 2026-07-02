#ifndef _QPXMLWRITER_H
#define _QPXMLWRITER_H
/*-------------------------------------------------------------------------
 * drawElements Quality Program Helper Library
 * -------------------------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Test log library
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

#include <stdio.h>

DE_BEGIN_EXTERN_C

typedef struct qpXmlWriter_s qpXmlWriter;

typedef enum qpXmlAttributeType_e
{
    QP_XML_ATTRIBUTE_STRING = 0,
    QP_XML_ATTRIBUTE_INT,
    QP_XML_ATTRIBUTE_BOOL,

    QP_XML_ATTRIBUTE_LAST
} qpXmlAttributeType;

typedef struct qpXmlAttribute_s
{
    const char *name;
    qpXmlAttributeType type;
    const char *stringValue;
    int intValue;
    bool boolValue;
} qpXmlAttribute;

static inline qpXmlAttribute qpSetStringAttrib(const char *name, const char *value)
{
    qpXmlAttribute attrib;
    attrib.name        = name;
    attrib.type        = QP_XML_ATTRIBUTE_STRING;
    attrib.stringValue = value;
    attrib.intValue    = -678;
    attrib.boolValue   = (bool)0xFFFFFFFFu;
    return attrib;
}

static inline qpXmlAttribute qpSetIntAttrib(const char *name, int value)
{
    qpXmlAttribute attrib;
    attrib.name        = name;
    attrib.type        = QP_XML_ATTRIBUTE_INT;
    attrib.stringValue = "<intAttrib>";
    attrib.intValue    = value;
    attrib.boolValue   = (bool)0xFFFFFFFFu;
    return attrib;
}

static inline qpXmlAttribute qpSetBoolAttrib(const char *name, bool value)
{
    qpXmlAttribute attrib;
    attrib.name        = name;
    attrib.type        = QP_XML_ATTRIBUTE_BOOL;
    attrib.stringValue = "<boolAttrib>";
    attrib.intValue    = -679;
    attrib.boolValue   = value;
    return attrib;
}
/*--------------------------------------------------------------------*//*!
 * \brief Create a file based XML Writer instance
 * \param fileName Name of the file
 * \param useCompression Set to true to use compression, if supported by implementation
 * \param flushAfterWrite Set to true to call fflush after writing each XML token
 * \return qpXmlWriter instance, or NULL if cannot create file
 *//*--------------------------------------------------------------------*/
qpXmlWriter *qpXmlWriter_createFileWriter(FILE *outFile, bool useCompression, bool flushAfterWrite);

/*--------------------------------------------------------------------*//*!
 * \brief XML Writer instance
 * \param a    qpXmlWriter instance
 *//*--------------------------------------------------------------------*/
void qpXmlWriter_destroy(qpXmlWriter *writer);

/*--------------------------------------------------------------------*//*!
 * \brief XML Writer instance
 * \param a    qpXmlWriter instance
 *//*--------------------------------------------------------------------*/
void qpXmlWriter_flush(qpXmlWriter *writer);

/*--------------------------------------------------------------------*//*!
 * \brief Start XML document
 * \param writer qpXmlWriter instance
 * \param writeXmlHeader whether to write the <xml.. header
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_startDocument(qpXmlWriter *writer, bool writeXmlHeader);

/*--------------------------------------------------------------------*//*!
 * \brief End XML document
 * \param writer qpXmlWriter instance
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_endDocument(qpXmlWriter *writer);

/*--------------------------------------------------------------------*//*!
 * \brief Start XML element
 * \param writer qpXmlWriter instance
 * \param elementName Name of the element
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_startElement(qpXmlWriter *writer, const char *elementName, int numAttribs,
                              const qpXmlAttribute *attribs);

/*--------------------------------------------------------------------*//*!
 * \brief End XML element
 * \param writer qpXmlWriter instance
 * \param elementName Name of the element
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_endElement(qpXmlWriter *writer, const char *elementName);

/*--------------------------------------------------------------------*//*!
 * \brief Write raw string into XML document
 * \param writer qpXmlWriter instance
 * \param content String to be written
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_writeString(qpXmlWriter *writer, const char *content);

/*--------------------------------------------------------------------*//*!
 * \brief Write base64 encoded data into XML document
 * \param writer    qpXmlWriter instance
 * \param data        Pointer to data to be written
 * \param numBytes    Length of data in bytes
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_writeBase64(qpXmlWriter *writer, const uint8_t *data, size_t numBytes);

/*--------------------------------------------------------------------*//*!
 * \brief Convenience function for writing XML element
 * \param writer qpXmlWriter instance
 * \param elementName Name of the element
 * \param elementContent Contents of the element
 * \return true on success, false on error
 *//*--------------------------------------------------------------------*/
bool qpXmlWriter_writeStringElement(qpXmlWriter *writer, const char *elementName, const char *elementContent);

DE_END_EXTERN_C

#endif /* _QPXMLWRITER_H */
