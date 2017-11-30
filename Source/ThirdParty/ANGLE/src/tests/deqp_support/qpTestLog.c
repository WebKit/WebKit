/*-------------------------------------------------------------------------
 * drawElements TestLog Library
 * ----------------------------
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
 * \brief Test case result logging
 *//*--------------------------------------------------------------------*/

#include "qpTestLog.h"
#include "qpXmlWriter.h"
#include "qpInfo.h"
#include "qpDebugOut.h"

#include "deMemory.h"
#include "deInt32.h"
#include "deString.h"

#include "deMutex.h"

#if defined(QP_SUPPORT_PNG)
#   include <png.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#if (DE_OS == DE_OS_WIN32)
#   include <windows.h>
#   include <io.h>
#endif

#if defined(DE_DEBUG)

/* Utils for verifying container (Section, ImageSet, EglConfigSet) usage in debug builds. */

typedef enum ContainerType_e
{
    CONTAINERTYPE_SECTION = 0,
    CONTAINERTYPE_IMAGESET,
    CONTAINERTYPE_EGLCONFIGSET,
    CONTAINERTYPE_SHADERPROGRAM,
    CONTAINERTYPE_SAMPLELIST,
    CONTAINERTYPE_SAMPLEINFO,
    CONTAINERTYPE_SAMPLE,

    CONTAINERTYPE_LAST
} ContainerType;

DE_INLINE deBool childContainersOk (ContainerType type)
{
    return type == CONTAINERTYPE_SECTION || type == CONTAINERTYPE_SAMPLELIST;
}

enum
{
    MAX_CONTAINER_STACK_DEPTH       = 32
};

typedef struct ContainerStack_s
{
    int             numElements;
    ContainerType   elements[MAX_CONTAINER_STACK_DEPTH];
} ContainerStack;

DE_INLINE void ContainerStack_reset (ContainerStack* stack)
{
    deMemset(stack, 0, sizeof(ContainerStack));
}

DE_INLINE deBool ContainerStack_isEmpty (const ContainerStack* stack)
{
    return stack->numElements == 0;
}

DE_INLINE deBool ContainerStack_push (ContainerStack* stack, ContainerType type)
{
    if (stack->numElements == MAX_CONTAINER_STACK_DEPTH)
        return DE_FALSE;

    if (stack->numElements > 0 && !childContainersOk(stack->elements[stack->numElements-1]))
        return DE_FALSE;

    stack->elements[stack->numElements]  = type;
    stack->numElements                  += 1;

    return DE_TRUE;
}

DE_INLINE ContainerType ContainerStack_pop (ContainerStack* stack)
{
    DE_ASSERT(stack->numElements > 0);
    stack->numElements -= 1;
    return stack->elements[stack->numElements];
}

DE_INLINE ContainerType ContainerStack_getTop (const ContainerStack* stack)
{
    if (stack->numElements > 0)
        return stack->elements[stack->numElements-1];
    else
        return CONTAINERTYPE_LAST;
}

#endif

/* qpTestLog instance */
struct qpTestLog_s
{
    deUint32                flags;              /*!< Logging flags.                     */

    deMutex                 lock;               /*!< Lock for mutable state below.      */

    /* State protected by lock. */
    FILE*                   outputFile;
    qpXmlWriter*            writer;
    deBool                  isSessionOpen;
    deBool                  isCaseOpen;

#if defined(DE_DEBUG)
    ContainerStack          containerStack;     /*!< For container usage verification.  */
#endif
};

/* Maps integer to string. */
typedef struct qpKeyStringMap_s
{
    int     key;
    char*   string;
} qpKeyStringMap;

static const char* LOG_FORMAT_VERSION = "0.3.4";

/* Mapping enum to above strings... */
static const qpKeyStringMap s_qpTestTypeMap[] =
{
    { QP_TEST_CASE_TYPE_SELF_VALIDATE,      "SelfValidate"  },
    { QP_TEST_CASE_TYPE_PERFORMANCE,        "Performance"   },
    { QP_TEST_CASE_TYPE_CAPABILITY,         "Capability"    },
    { QP_TEST_CASE_TYPE_ACCURACY,           "Accuracy"      },

    { QP_TEST_CASE_TYPE_LAST,               DE_NULL         }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpTestTypeMap) == QP_TEST_CASE_TYPE_LAST + 1);

static const qpKeyStringMap s_qpTestResultMap[] =
{
    { QP_TEST_RESULT_PASS,                      "Pass"                  },
    { QP_TEST_RESULT_FAIL,                      "Fail"                  },
    { QP_TEST_RESULT_QUALITY_WARNING,           "QualityWarning"        },
    { QP_TEST_RESULT_COMPATIBILITY_WARNING,     "CompatibilityWarning"  },
    { QP_TEST_RESULT_PENDING,                   "Pending"               },  /* should not be needed here */
    { QP_TEST_RESULT_NOT_SUPPORTED,             "NotSupported"          },
    { QP_TEST_RESULT_RESOURCE_ERROR,            "ResourceError"         },
    { QP_TEST_RESULT_INTERNAL_ERROR,            "InternalError"         },
    { QP_TEST_RESULT_CRASH,                     "Crash"                 },
    { QP_TEST_RESULT_TIMEOUT,                   "Timeout"               },

    /* Add new values here if needed, remember to update qpTestResult enumeration. */

    { QP_TEST_RESULT_LAST,                      DE_NULL                 }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpTestResultMap) == QP_TEST_RESULT_LAST + 1);

/* Key tag to string mapping. */

static const qpKeyStringMap s_qpTagMap[] =
{
    { QP_KEY_TAG_NONE,          DE_NULL         },
    { QP_KEY_TAG_PERFORMANCE,   "Performance"   },
    { QP_KEY_TAG_QUALITY,       "Quality"       },
    { QP_KEY_TAG_PRECISION,     "Precision"     },
    { QP_KEY_TAG_TIME,          "Time"          },

    { QP_KEY_TAG_LAST,          DE_NULL         }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpTagMap) == QP_KEY_TAG_LAST + 1);

/* Sample value tag to string mapping. */

static const qpKeyStringMap s_qpSampleValueTagMap[] =
{
    { QP_SAMPLE_VALUE_TAG_PREDICTOR,    "Predictor" },
    { QP_SAMPLE_VALUE_TAG_RESPONSE,     "Response"  },

    { QP_SAMPLE_VALUE_TAG_LAST,         DE_NULL     }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpSampleValueTagMap) == QP_SAMPLE_VALUE_TAG_LAST + 1);

/* Image compression mode to string mapping. */

static const qpKeyStringMap s_qpImageCompressionModeMap[] =
{
    { QP_IMAGE_COMPRESSION_MODE_NONE,   "None"  },
    { QP_IMAGE_COMPRESSION_MODE_PNG,    "PNG"   },

    { QP_IMAGE_COMPRESSION_MODE_BEST,   DE_NULL },  /* not allowed to be written! */

    { QP_IMAGE_COMPRESSION_MODE_LAST,   DE_NULL }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpImageCompressionModeMap) == QP_IMAGE_COMPRESSION_MODE_LAST + 1);

/* Image format to string mapping. */

static const qpKeyStringMap s_qpImageFormatMap[] =
{
    { QP_IMAGE_FORMAT_RGB888,   "RGB888"    },
    { QP_IMAGE_FORMAT_RGBA8888, "RGBA8888"  },

    { QP_IMAGE_FORMAT_LAST,     DE_NULL     }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpImageFormatMap) == QP_IMAGE_FORMAT_LAST + 1);

/* Shader type to string mapping. */

static const qpKeyStringMap s_qpShaderTypeMap[] =
{
    { QP_SHADER_TYPE_VERTEX,            "VertexShader"          },
    { QP_SHADER_TYPE_FRAGMENT,          "FragmentShader"        },
    { QP_SHADER_TYPE_GEOMETRY,          "GeometryShader"        },
    { QP_SHADER_TYPE_TESS_CONTROL,      "TessControlShader"     },
    { QP_SHADER_TYPE_TESS_EVALUATION,   "TessEvaluationShader"  },
    { QP_SHADER_TYPE_COMPUTE,           "ComputeShader"         },

    { QP_SHADER_TYPE_LAST,              DE_NULL                 }
};

DE_STATIC_ASSERT(DE_LENGTH_OF_ARRAY(s_qpShaderTypeMap) == QP_SHADER_TYPE_LAST + 1);

static void qpTestLog_flushFile (qpTestLog* log)
{
    DE_ASSERT(log && log->outputFile);
    fflush(log->outputFile);
#if (DE_OS == DE_OS_WIN32) && (DE_COMPILER == DE_COMPILER_MSC)
    /* \todo [petri] Is this really necessary? */
    FlushFileBuffers((HANDLE)_get_osfhandle(_fileno(log->outputFile)));
#endif
}

#define QP_LOOKUP_STRING(KEYMAP, KEY)   qpLookupString(KEYMAP, DE_LENGTH_OF_ARRAY(KEYMAP), (int)(KEY))

static const char* qpLookupString (const qpKeyStringMap* keyMap, int keyMapSize, int key)
{
    DE_ASSERT(keyMap);
    DE_ASSERT(deInBounds32(key, 0, keyMapSize));
    DE_ASSERT(keyMap[key].key == key);
    DE_UNREF(keyMapSize); /* for asserting only */
    return keyMap[key].string;
}

DE_INLINE void int32ToString (int val, char buf[32])
{
    deSprintf(&buf[0], 32, "%d", val);
}

DE_INLINE void int64ToString (deInt64 val, char buf[32])
{
    deSprintf(&buf[0], 32, "%lld", (long long int)val);
}

DE_INLINE void floatToString (float value, char* buf, size_t bufSize)
{
    deSprintf(buf, bufSize, "%f", value);
}

DE_INLINE void doubleToString (double value, char* buf, size_t bufSize)
{
    deSprintf(buf, bufSize, "%f", value);
}

static deBool beginSession (qpTestLog* log)
{
    DE_ASSERT(log && !log->isSessionOpen);

    /* Write session info. */
    fprintf(log->outputFile, "#sessionInfo releaseName %s\n", qpGetReleaseName());
    fprintf(log->outputFile, "#sessionInfo releaseId 0x%08x\n", qpGetReleaseId());
    fprintf(log->outputFile, "#sessionInfo targetName \"%s\"\n", qpGetTargetName());

    /* Write out #beginSession. */
    fprintf(log->outputFile, "#beginSession\n");
    qpTestLog_flushFile(log);

    log->isSessionOpen = DE_TRUE;

    return DE_TRUE;
}

static deBool endSession (qpTestLog* log)
{
    DE_ASSERT(log && log->isSessionOpen);

    /* Make sure xml is flushed. */
    qpXmlWriter_flush(log->writer);

    /* Write out #endSession. */
    fprintf(log->outputFile, "\n#endSession\n");
    qpTestLog_flushFile(log);

    log->isSessionOpen = DE_FALSE;

    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Create a file based logger instance
 * \param fileName Name of the file where to put logs
 * \return qpTestLog instance, or DE_NULL if cannot create file
 *//*--------------------------------------------------------------------*/
qpTestLog* qpTestLog_createFileLog (const char* fileName, deUint32 flags)
{
    qpTestLog* log = (qpTestLog*)deCalloc(sizeof(qpTestLog));
    if (!log)
        return DE_NULL;

    DE_ASSERT(fileName && fileName[0]); /* must have filename. */

#if defined(DE_DEBUG)
    ContainerStack_reset(&log->containerStack);
#endif

    qpPrintf("Writing test log into %s\n", fileName);

    /* Create output file. */
    log->outputFile = fopen(fileName, "wb");
    if (!log->outputFile)
    {
        qpPrintf("ERROR: Unable to open test log output file '%s'.\n", fileName);
        qpTestLog_destroy(log);
        return DE_NULL;
    }

    log->flags          = flags;
    log->writer         = qpXmlWriter_createFileWriter(log->outputFile, 0, !(flags & QP_TEST_LOG_NO_FLUSH));
    log->lock           = deMutex_create(DE_NULL);
    log->isSessionOpen  = DE_FALSE;
    log->isCaseOpen     = DE_FALSE;

    if (!log->writer)
    {
        qpPrintf("ERROR: Unable to create output XML writer to file '%s'.\n", fileName);
        qpTestLog_destroy(log);
        return DE_NULL;
    }

    if (!log->lock)
    {
        qpPrintf("ERROR: Unable to create mutex.\n");
        qpTestLog_destroy(log);
        return DE_NULL;
    }

    beginSession(log);

    return log;
}

/*--------------------------------------------------------------------*//*!
 * \brief Destroy a logger instance
 * \param a qpTestLog instance
 *//*--------------------------------------------------------------------*/
void qpTestLog_destroy (qpTestLog* log)
{
    DE_ASSERT(log);

    if (log->isSessionOpen)
        endSession(log);

    if (log->writer)
        qpXmlWriter_destroy(log->writer);

    if (log->outputFile)
        fclose(log->outputFile);

    if (log->lock)
        deMutex_destroy(log->lock);

    deFree(log);
}

/*--------------------------------------------------------------------*//*!
 * \brief Log start of test case
 * \param log qpTestLog instance
 * \param testCasePath  Full test case path (as seen in Candy).
 * \param testCaseType  Test case type
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_startCase (qpTestLog* log, const char* testCasePath, qpTestCaseType testCaseType)
{
    const char*     typeStr             = QP_LOOKUP_STRING(s_qpTestTypeMap, testCaseType);
    int             numResultAttribs    = 0;
    qpXmlAttribute  resultAttribs[8];

    DE_ASSERT(log && testCasePath && (testCasePath[0] != 0));
    deMutex_lock(log->lock);

    DE_ASSERT(!log->isCaseOpen);
    DE_ASSERT(ContainerStack_isEmpty(&log->containerStack));

    /* Flush XML and write out #beginTestCaseResult. */
    qpXmlWriter_flush(log->writer);
    fprintf(log->outputFile, "\n#beginTestCaseResult %s\n", testCasePath);
    if (!(log->flags & QP_TEST_LOG_NO_FLUSH))
        qpTestLog_flushFile(log);

    log->isCaseOpen = DE_TRUE;

    /* Fill in attributes. */
    resultAttribs[numResultAttribs++] = qpSetStringAttrib("Version", LOG_FORMAT_VERSION);
    resultAttribs[numResultAttribs++] = qpSetStringAttrib("CasePath", testCasePath);
    resultAttribs[numResultAttribs++] = qpSetStringAttrib("CaseType", typeStr);

    if (!qpXmlWriter_startDocument(log->writer) ||
        !qpXmlWriter_startElement(log->writer, "TestCaseResult", numResultAttribs, resultAttribs))
    {
        qpPrintf("qpTestLog_startCase(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Log end of test case
 * \param log qpTestLog instance
 * \param result Test result
 * \param description Description of a problem in case of error
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_endCase (qpTestLog* log, qpTestResult result, const char* resultDetails)
{
    const char*     statusStr       = QP_LOOKUP_STRING(s_qpTestResultMap, result);
    qpXmlAttribute  statusAttrib    = qpSetStringAttrib("StatusCode", statusStr);

    deMutex_lock(log->lock);

    DE_ASSERT(log->isCaseOpen);
    DE_ASSERT(ContainerStack_isEmpty(&log->containerStack));

    /* <Result StatusCode="Pass">Result details</Result>
     * </TestCaseResult>
     */
    if (!qpXmlWriter_startElement(log->writer, "Result", 1, &statusAttrib) ||
        (resultDetails && !qpXmlWriter_writeString(log->writer, resultDetails)) ||
        !qpXmlWriter_endElement(log->writer, "Result") ||
        !qpXmlWriter_endElement(log->writer, "TestCaseResult") ||
        !qpXmlWriter_endDocument(log->writer))      /* Close any XML elements still open */
    {
        qpPrintf("qpTestLog_endCase(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    /* Flush XML and write #endTestCaseResult. */
    qpXmlWriter_flush(log->writer);
    fprintf(log->outputFile, "\n#endTestCaseResult\n");
    if (!(log->flags & QP_TEST_LOG_NO_FLUSH))
        qpTestLog_flushFile(log);

    log->isCaseOpen = DE_FALSE;

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Abrupt termination of logging.
 * \param log       qpTestLog instance
 * \param result    Result code, only Crash and Timeout are allowed.
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_terminateCase (qpTestLog* log, qpTestResult result)
{
    const char* resultStr = QP_LOOKUP_STRING(s_qpTestResultMap, result);

    DE_ASSERT(log);
    DE_ASSERT(result == QP_TEST_RESULT_CRASH || result == QP_TEST_RESULT_TIMEOUT);

    deMutex_lock(log->lock);

    if (!log->isCaseOpen)
    {
        deMutex_unlock(log->lock);
        return DE_FALSE; /* Soft error. This is called from error handler. */
    }

    /* Flush XML and write #terminateTestCaseResult. */
    qpXmlWriter_flush(log->writer);
    fprintf(log->outputFile, "\n#terminateTestCaseResult %s\n", resultStr);
    qpTestLog_flushFile(log);

    log->isCaseOpen = DE_FALSE;

#if defined(DE_DEBUG)
    ContainerStack_reset(&log->containerStack);
#endif

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

static deBool qpTestLog_writeKeyValuePair (qpTestLog* log, const char* elementName, const char* name, const char* description, const char* unit, qpKeyValueTag tag, const char* text)
{
    const char*     tagString = QP_LOOKUP_STRING(s_qpTagMap, tag);
    qpXmlAttribute  attribs[8];
    int             numAttribs = 0;

    DE_ASSERT(log && elementName && text);
    deMutex_lock(log->lock);

    /* Fill in attributes. */
    if (name)           attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    if (description)    attribs[numAttribs++] = qpSetStringAttrib("Description", description);
    if (tagString)      attribs[numAttribs++] = qpSetStringAttrib("Tag", tagString);
    if (unit)           attribs[numAttribs++] = qpSetStringAttrib("Unit", unit);

    if (!qpXmlWriter_startElement(log->writer, elementName, numAttribs, attribs) ||
        !qpXmlWriter_writeString(log->writer, text) ||
        !qpXmlWriter_endElement(log->writer, elementName))
    {
        qpPrintf("qpTestLog_writeKeyValuePair(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write a message to output log
 * \param log       qpTestLog instance
 * \param format    Format string of message
 * \param ...       Parameters for message
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeMessage (qpTestLog* log, const char* format, ...)
{
    char    buffer[1024];
    va_list args;

    /* \todo [petri] Handle buffer overflows! */

    va_start(args, format);
    buffer[DE_LENGTH_OF_ARRAY(buffer) - 1] = 0;
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    /* <Text>text</Text> */
    return qpTestLog_writeKeyValuePair(log, "Text", DE_NULL, DE_NULL, DE_NULL, QP_KEY_TAG_LAST, buffer);
}

/*--------------------------------------------------------------------*//*!
 * \brief Write key-value-pair into log
 * \param log           qpTestLog instance
 * \param name          Unique identifier for entry
 * \param description   Human readable description
 * \param tag           Optional tag
 * \param value         Value of the key-value-pair
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeText (qpTestLog* log, const char* name, const char* description, qpKeyValueTag tag, const char* text)
{
    /* <Text Name="name" Description="description" Tag="tag">text</Text> */
    return qpTestLog_writeKeyValuePair(log, "Text", name, description, DE_NULL, tag, text);
}

/*--------------------------------------------------------------------*//*!
 * \brief Write key-value-pair into log
 * \param log           qpTestLog instance
 * \param name          Unique identifier for entry
 * \param description   Human readable description
 * \param tag           Optional tag
 * \param value         Value of the key-value-pair
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeInteger (qpTestLog* log, const char* name, const char* description, const char* unit, qpKeyValueTag tag, deInt64 value)
{
    char tmpString[64];
    int64ToString(value, tmpString);

    /* <Number Name="name" Description="description" Tag="Performance">15</Number> */
    return qpTestLog_writeKeyValuePair(log, "Number", name, description, unit, tag, tmpString);
}

/*--------------------------------------------------------------------*//*!
 * \brief Write key-value-pair into log
 * \param log           qpTestLog instance
 * \param name          Unique identifier for entry
 * \param description   Human readable description
 * \param tag           Optional tag
 * \param value         Value of the key-value-pair
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeFloat (qpTestLog* log, const char* name, const char* description, const char* unit, qpKeyValueTag tag, float value)
{
    char tmpString[64];
    floatToString(value, tmpString, sizeof(tmpString));

    /* <Number Name="name" Description="description" Tag="Performance">15</Number> */
    return qpTestLog_writeKeyValuePair(log, "Number", name, description, unit, tag, tmpString);
}

typedef struct Buffer_s
{
    size_t      capacity;
    size_t      size;
    deUint8*    data;
} Buffer;

void Buffer_init (Buffer* buffer)
{
    buffer->capacity    = 0;
    buffer->size        = 0;
    buffer->data        = DE_NULL;
}

void Buffer_deinit (Buffer* buffer)
{
    deFree(buffer->data);
    Buffer_init(buffer);
}

deBool Buffer_resize (Buffer* buffer, size_t newSize)
{
    /* Grow buffer if necessary. */
    if (newSize > buffer->capacity)
    {
        size_t      newCapacity = (size_t)deAlign32(deMax32(2*(int)buffer->capacity, (int)newSize), 512);
        deUint8*    newData     = (deUint8*)deMalloc(newCapacity);
        if (!newData)
            return DE_FALSE;

        memcpy(newData, buffer->data, buffer->size);
        deFree(buffer->data);
        buffer->data        = newData;
        buffer->capacity    = newCapacity;
    }

    buffer->size = newSize;
    return DE_TRUE;
}

deBool Buffer_append (Buffer* buffer, const deUint8* data, size_t numBytes)
{
    size_t offset = buffer->size;

    if (!Buffer_resize(buffer, buffer->size + numBytes))
        return DE_FALSE;

    /* Append bytes. */
    memcpy(&buffer->data[offset], data, numBytes);
    return DE_TRUE;
}

#if defined(QP_SUPPORT_PNG)
void pngWriteData (png_structp png, png_bytep dataPtr, png_size_t numBytes)
{
    Buffer* buffer = (Buffer*)png_get_io_ptr(png);
    if (!Buffer_append(buffer, (const deUint8*)dataPtr, numBytes))
        png_error(png, "unable to resize PNG write buffer!");
}

void pngFlushData (png_structp png)
{
    DE_UNREF(png);
    /* nada */
}

static deBool writeCompressedPNG (png_structp png, png_infop info, png_byte** rowPointers, int width, int height, int colorFormat)
{
    if (setjmp(png_jmpbuf(png)) == 0)
    {
        /* Write data. */
        png_set_IHDR(png, info, (png_uint_32)width, (png_uint_32)height,
            8,
            colorFormat,
            PNG_INTERLACE_NONE,
            PNG_COMPRESSION_TYPE_BASE,
            PNG_FILTER_TYPE_BASE);
        png_write_info(png, info);
        png_write_image(png, rowPointers);
        png_write_end(png, NULL);

        return DE_TRUE;
    }
    else
        return DE_FALSE;
}

static deBool compressImagePNG (Buffer* buffer, qpImageFormat imageFormat, int width, int height, int rowStride, const void* data)
{
    deBool          compressOk      = DE_FALSE;
    png_structp     png             = DE_NULL;
    png_infop       info            = DE_NULL;
    png_byte**      rowPointers     = DE_NULL;
    deBool          hasAlpha        = imageFormat == QP_IMAGE_FORMAT_RGBA8888;
    int             ndx;

    /* Handle format. */
    DE_ASSERT(imageFormat == QP_IMAGE_FORMAT_RGB888 || imageFormat == QP_IMAGE_FORMAT_RGBA8888);

    /* Allocate & set row pointers. */
    rowPointers = (png_byte**)deMalloc((size_t)height * sizeof(png_byte*));
    if (!rowPointers)
        return DE_FALSE;

    for (ndx = 0; ndx < height; ndx++)
        rowPointers[ndx] = (png_byte*)((const deUint8*)data + ndx*rowStride);

    /* Initialize PNG compressor. */
    png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    info = png ? png_create_info_struct(png) : DE_NULL;
    if (png && info)
    {
        /* Set our own write function. */
        png_set_write_fn(png, buffer, pngWriteData, pngFlushData);

        compressOk = writeCompressedPNG(png, info, rowPointers, width, height,
                                        hasAlpha ? PNG_COLOR_TYPE_RGBA : PNG_COLOR_TYPE_RGB);
    }

    /* Cleanup & return. */
    if (png && info)
    {
        png_destroy_info_struct(png, &info);
        png_destroy_write_struct(&png, DE_NULL);
    }
    else if (png)
        png_destroy_write_struct(&png, &info);

    deFree(rowPointers);
    return compressOk;
}
#endif /* QP_SUPPORT_PNG */

/*--------------------------------------------------------------------*//*!
 * \brief Start image set
 * \param log           qpTestLog instance
 * \param name          Unique identifier for the set
 * \param description   Human readable description
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_startImageSet (qpTestLog* log, const char* name, const char* description)
{
    qpXmlAttribute  attribs[4];
    int             numAttribs = 0;

    DE_ASSERT(log && name);
    deMutex_lock(log->lock);

    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    if (description)
        attribs[numAttribs++] = qpSetStringAttrib("Description", description);

    /* <ImageSet Name="<name>"> */
    if (!qpXmlWriter_startElement(log->writer, "ImageSet", numAttribs, attribs))
    {
        qpPrintf("qpTestLog_startImageSet(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_IMAGESET));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief End image set
 * \param log           qpTestLog instance
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_endImageSet (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    /* <ImageSet Name="<name>"> */
    if (!qpXmlWriter_endElement(log->writer, "ImageSet"))
    {
        qpPrintf("qpTestLog_endImageSet(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_IMAGESET);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write base64 encoded raw image data into log
 * \param log               qpTestLog instance
 * \param name              Unique name (matching names can be compared across BatchResults).
 * \param description       Textual description (shown in Candy).
 * \param compressionMode   Compression mode
 * \param imageFormat       Color format
 * \param width             Width in pixels
 * \param height            Height in pixels
 * \param stride            Data stride (offset between rows)
 * \param data              Pointer to pixel data
 * \return 0 if OK, otherwise <0
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeImage (
    qpTestLog*              log,
    const char*             name,
    const char*             description,
    qpImageCompressionMode  compressionMode,
    qpImageFormat           imageFormat,
    int                     width,
    int                     height,
    int                     stride,
    const void*             data)
{
    char            widthStr[32];
    char            heightStr[32];
    qpXmlAttribute  attribs[8];
    int             numAttribs          = 0;
    Buffer          compressedBuffer;
    const void*     writeDataPtr        = DE_NULL;
    size_t          writeDataBytes      = ~(size_t)0;

    DE_ASSERT(log && name);
    DE_ASSERT(deInRange32(width, 1, 16384));
    DE_ASSERT(deInRange32(height, 1, 16384));
    DE_ASSERT(data);

    if (log->flags & QP_TEST_LOG_EXCLUDE_IMAGES)
        return DE_TRUE; /* Image not logged. */

    Buffer_init(&compressedBuffer);

    /* BEST compression mode defaults to PNG. */
    if (compressionMode == QP_IMAGE_COMPRESSION_MODE_BEST)
    {
#if defined(QP_SUPPORT_PNG)
        compressionMode = QP_IMAGE_COMPRESSION_MODE_PNG;
#else
        compressionMode = QP_IMAGE_COMPRESSION_MODE_NONE;
#endif
    }

#if defined(QP_SUPPORT_PNG)
    /* Try storing with PNG compression. */
    if (compressionMode == QP_IMAGE_COMPRESSION_MODE_PNG)
    {
        deBool compressOk = compressImagePNG(&compressedBuffer, imageFormat, width, height, stride, data);
        if (compressOk)
        {
            writeDataPtr    = compressedBuffer.data;
            writeDataBytes  = compressedBuffer.size;
        }
        else
        {
            /* Fall-back to default compression. */
            qpPrintf("WARNING: PNG compression failed -- storing image uncompressed.\n");
            compressionMode = QP_IMAGE_COMPRESSION_MODE_NONE;
        }
    }
#endif

    /* Handle image compression. */
    switch (compressionMode)
    {
        case QP_IMAGE_COMPRESSION_MODE_NONE:
        {
            int pixelSize       = imageFormat == QP_IMAGE_FORMAT_RGB888 ? 3 : 4;
            int packedStride    = pixelSize*width;

            if (packedStride == stride)
                writeDataPtr = data;
            else
            {
                /* Need to re-pack pixels. */
                if (Buffer_resize(&compressedBuffer, (size_t)(packedStride*height)))
                {
                    int row;
                    for (row = 0; row < height; row++)
                        memcpy(&compressedBuffer.data[packedStride*row], &((const deUint8*)data)[row*stride], (size_t)(pixelSize*width));
                }
                else
                {
                    qpPrintf("ERROR: Failed to pack pixels for writing.\n");
                    Buffer_deinit(&compressedBuffer);
                    return DE_FALSE;
                }
            }

            writeDataBytes = (size_t)(packedStride*height);
            break;
        }

#if defined(QP_SUPPORT_PNG)
        case QP_IMAGE_COMPRESSION_MODE_PNG:
            DE_ASSERT(writeDataPtr); /* Already handled. */
            break;
#endif

        default:
            qpPrintf("qpTestLog_writeImage(): Unknown compression mode: %s\n", QP_LOOKUP_STRING(s_qpImageCompressionModeMap, compressionMode));
            Buffer_deinit(&compressedBuffer);
            return DE_FALSE;
    }

    /* Fill in attributes. */
    int32ToString(width, widthStr);
    int32ToString(height, heightStr);
    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    attribs[numAttribs++] = qpSetStringAttrib("Width", widthStr);
    attribs[numAttribs++] = qpSetStringAttrib("Height", heightStr);
    attribs[numAttribs++] = qpSetStringAttrib("Format", QP_LOOKUP_STRING(s_qpImageFormatMap, imageFormat));
    attribs[numAttribs++] = qpSetStringAttrib("CompressionMode", QP_LOOKUP_STRING(s_qpImageCompressionModeMap, compressionMode));
    if (description) attribs[numAttribs++] = qpSetStringAttrib("Description", description);

    /* \note Log lock is acquired after compression! */
    deMutex_lock(log->lock);

    /* <Image ID="result" Name="Foobar" Width="640" Height="480" Format="RGB888" CompressionMode="None">base64 data</Image> */
    if (!qpXmlWriter_startElement(log->writer, "Image", numAttribs, attribs) ||
        !qpXmlWriter_writeBase64(log->writer, (const deUint8*)writeDataPtr, writeDataBytes) ||
        !qpXmlWriter_endElement(log->writer, "Image"))
    {
        qpPrintf("qpTestLog_writeImage(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        Buffer_deinit(&compressedBuffer);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);

    /* Free compressed data if allocated. */
    Buffer_deinit(&compressedBuffer);

    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write a OpenGL ES shader program into the log.
 * \param linkOk            Shader program link result, false on failure
 * \param linkInfoLog       Implementation provided linkage log
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_startShaderProgram (qpTestLog* log, deBool linkOk, const char* linkInfoLog)
{
    qpXmlAttribute  programAttribs[4];
    int             numProgramAttribs = 0;

    DE_ASSERT(log);
    deMutex_lock(log->lock);

    programAttribs[numProgramAttribs++] = qpSetStringAttrib("LinkStatus", linkOk ? "OK" : "Fail");

    if (!qpXmlWriter_startElement(log->writer, "ShaderProgram", numProgramAttribs, programAttribs) ||
        !qpXmlWriter_writeStringElement(log->writer, "InfoLog", linkInfoLog))
    {
        qpPrintf("qpTestLog_startShaderProgram(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_SHADERPROGRAM));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief End shader program
 * \param log           qpTestLog instance
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_endShaderProgram (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    /* </ShaderProgram> */
    if (!qpXmlWriter_endElement(log->writer, "ShaderProgram"))
    {
        qpPrintf("qpTestLog_endShaderProgram(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_SHADERPROGRAM);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write a OpenGL ES shader into the log.
 * \param type              Shader type
 * \param source            Shader source
 * \param compileOk         Shader compilation result, false on failure
 * \param infoLog           Implementation provided shader compilation log
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeShader (qpTestLog* log, qpShaderType type, const char* source, deBool compileOk, const char* infoLog)
{
    const char*     tagName             = QP_LOOKUP_STRING(s_qpShaderTypeMap, type);
    const char*     sourceStr           = ((log->flags & QP_TEST_LOG_EXCLUDE_SHADER_SOURCES) == 0 || !compileOk) ? source : "";
    int             numShaderAttribs    = 0;
    qpXmlAttribute  shaderAttribs[4];

    deMutex_lock(log->lock);

    DE_ASSERT(source);
    DE_ASSERT(ContainerStack_getTop(&log->containerStack) == CONTAINERTYPE_SHADERPROGRAM);

    shaderAttribs[numShaderAttribs++]   = qpSetStringAttrib("CompileStatus", compileOk ? "OK" : "Fail");

    if (!qpXmlWriter_startElement(log->writer, tagName, numShaderAttribs, shaderAttribs) ||
        !qpXmlWriter_writeStringElement(log->writer, "ShaderSource", sourceStr) ||
        !qpXmlWriter_writeStringElement(log->writer, "InfoLog", infoLog) ||
        !qpXmlWriter_endElement(log->writer, tagName))
    {
        qpPrintf("qpTestLog_writeShader(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Start writing a set of EGL configurations into the log.
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_startEglConfigSet (qpTestLog* log, const char* name, const char* description)
{
    qpXmlAttribute  attribs[4];
    int             numAttribs = 0;

    DE_ASSERT(log && name);
    deMutex_lock(log->lock);

    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    if (description)
        attribs[numAttribs++] = qpSetStringAttrib("Description", description);

    /* <EglConfigSet Name="<name>"> */
    if (!qpXmlWriter_startElement(log->writer, "EglConfigSet", numAttribs, attribs))
    {
        qpPrintf("qpTestLog_startEglImageSet(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_EGLCONFIGSET));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief End an EGL config set
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_endEglConfigSet (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    /* <EglConfigSet Name="<name>"> */
    if (!qpXmlWriter_endElement(log->writer, "EglConfigSet"))
    {
        qpPrintf("qpTestLog_endEglImageSet(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_EGLCONFIGSET);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write an EGL config inside an EGL config set
 * \see   qpElgConfigInfo for details
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeEglConfig (qpTestLog* log, const qpEglConfigInfo* config)
{
    qpXmlAttribute  attribs[64];
    int             numAttribs = 0;

    DE_ASSERT(log && config);
    deMutex_lock(log->lock);

    attribs[numAttribs++] = qpSetIntAttrib      ("BufferSize", config->bufferSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("RedSize", config->redSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("GreenSize", config->greenSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("BlueSize", config->blueSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("LuminanceSize", config->luminanceSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("AlphaSize", config->alphaSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("AlphaMaskSize", config->alphaMaskSize);
    attribs[numAttribs++] = qpSetBoolAttrib     ("BindToTextureRGB", config->bindToTextureRGB);
    attribs[numAttribs++] = qpSetBoolAttrib     ("BindToTextureRGBA", config->bindToTextureRGBA);
    attribs[numAttribs++] = qpSetStringAttrib   ("ColorBufferType", config->colorBufferType);
    attribs[numAttribs++] = qpSetStringAttrib   ("ConfigCaveat", config->configCaveat);
    attribs[numAttribs++] = qpSetIntAttrib      ("ConfigID", config->configID);
    attribs[numAttribs++] = qpSetStringAttrib   ("Conformant", config->conformant);
    attribs[numAttribs++] = qpSetIntAttrib      ("DepthSize", config->depthSize);
    attribs[numAttribs++] = qpSetIntAttrib      ("Level", config->level);
    attribs[numAttribs++] = qpSetIntAttrib      ("MaxPBufferWidth", config->maxPBufferWidth);
    attribs[numAttribs++] = qpSetIntAttrib      ("MaxPBufferHeight", config->maxPBufferHeight);
    attribs[numAttribs++] = qpSetIntAttrib      ("MaxPBufferPixels", config->maxPBufferPixels);
    attribs[numAttribs++] = qpSetIntAttrib      ("MaxSwapInterval", config->maxSwapInterval);
    attribs[numAttribs++] = qpSetIntAttrib      ("MinSwapInterval", config->minSwapInterval);
    attribs[numAttribs++] = qpSetBoolAttrib     ("NativeRenderable", config->nativeRenderable);
    attribs[numAttribs++] = qpSetStringAttrib   ("RenderableType", config->renderableType);
    attribs[numAttribs++] = qpSetIntAttrib      ("SampleBuffers", config->sampleBuffers);
    attribs[numAttribs++] = qpSetIntAttrib      ("Samples", config->samples);
    attribs[numAttribs++] = qpSetIntAttrib      ("StencilSize", config->stencilSize);
    attribs[numAttribs++] = qpSetStringAttrib   ("SurfaceTypes", config->surfaceTypes);
    attribs[numAttribs++] = qpSetStringAttrib   ("TransparentType", config->transparentType);
    attribs[numAttribs++] = qpSetIntAttrib      ("TransparentRedValue", config->transparentRedValue);
    attribs[numAttribs++] = qpSetIntAttrib      ("TransparentGreenValue", config->transparentGreenValue);
    attribs[numAttribs++] = qpSetIntAttrib      ("TransparentBlueValue", config->transparentBlueValue);
    DE_ASSERT(numAttribs <= DE_LENGTH_OF_ARRAY(attribs));

    if (!qpXmlWriter_startElement(log->writer, "EglConfig", numAttribs, attribs) ||
        !qpXmlWriter_endElement(log->writer, "EglConfig"))
    {
        qpPrintf("qpTestLog_writeEglConfig(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Start section in log.
 * \param log           qpTestLog instance
 * \param name          Section name
 * \param description   Human readable description
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_startSection (qpTestLog* log, const char* name, const char* description)
{
    qpXmlAttribute  attribs[2];
    int             numAttribs = 0;

    DE_ASSERT(log && name);
    deMutex_lock(log->lock);

    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    if (description)
        attribs[numAttribs++] = qpSetStringAttrib("Description", description);

    /* <Section Name="<name>" Description="<description>"> */
    if (!qpXmlWriter_startElement(log->writer, "Section", numAttribs, attribs))
    {
        qpPrintf("qpTestLog_startSection(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_SECTION));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief End section in log.
 * \param log           qpTestLog instance
 * \return true if ok, false otherwise
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_endSection (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    /* </Section> */
    if (!qpXmlWriter_endElement(log->writer, "Section"))
    {
        qpPrintf("qpTestLog_endSection(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_SECTION);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write OpenCL compute kernel source into the log.
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeKernelSource (qpTestLog* log, const char* source)
{
    const char*     sourceStr   = (log->flags & QP_TEST_LOG_EXCLUDE_SHADER_SOURCES) != 0 ? "" : source;

    DE_ASSERT(log);
    deMutex_lock(log->lock);

    if (!qpXmlWriter_writeStringElement(log->writer, "KernelSource", sourceStr))
    {
        qpPrintf("qpTestLog_writeKernelSource(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write a SPIR-V module assembly source into the log.
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeSpirVAssemblySource (qpTestLog* log, const char* source)
{
    const char* const   sourceStr   = (log->flags & QP_TEST_LOG_EXCLUDE_SHADER_SOURCES) != 0 ? "" : source;

    deMutex_lock(log->lock);

    DE_ASSERT(ContainerStack_getTop(&log->containerStack) == CONTAINERTYPE_SHADERPROGRAM);

    if (!qpXmlWriter_writeStringElement(log->writer, "SpirVAssemblySource", sourceStr))
    {
        qpPrintf("qpTestLog_writeSpirVAssemblySource(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

/*--------------------------------------------------------------------*//*!
 * \brief Write OpenCL kernel compilation results into the log
 *//*--------------------------------------------------------------------*/
deBool qpTestLog_writeCompileInfo (qpTestLog* log, const char* name, const char* description, deBool compileOk, const char* infoLog)
{
    int             numAttribs = 0;
    qpXmlAttribute  attribs[3];

    DE_ASSERT(log && name && description && infoLog);
    deMutex_lock(log->lock);

    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    attribs[numAttribs++] = qpSetStringAttrib("Description", description);
    attribs[numAttribs++] = qpSetStringAttrib("CompileStatus", compileOk ? "OK" : "Fail");

    if (!qpXmlWriter_startElement(log->writer, "CompileInfo", numAttribs, attribs) ||
        !qpXmlWriter_writeStringElement(log->writer, "InfoLog", infoLog) ||
        !qpXmlWriter_endElement(log->writer, "CompileInfo"))
    {
        qpPrintf("qpTestLog_writeCompileInfo(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_startSampleList (qpTestLog* log, const char* name, const char* description)
{
    int             numAttribs = 0;
    qpXmlAttribute  attribs[2];

    DE_ASSERT(log && name && description);
    deMutex_lock(log->lock);

    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    attribs[numAttribs++] = qpSetStringAttrib("Description", description);

    if (!qpXmlWriter_startElement(log->writer, "SampleList", numAttribs, attribs))
    {
        qpPrintf("qpTestLog_startSampleList(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_SAMPLELIST));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_startSampleInfo (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    if (!qpXmlWriter_startElement(log->writer, "SampleInfo", 0, DE_NULL))
    {
        qpPrintf("qpTestLog_startSampleInfo(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_SAMPLEINFO));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_writeValueInfo (qpTestLog* log, const char* name, const char* description, const char* unit, qpSampleValueTag tag)
{
    const char*     tagName     = QP_LOOKUP_STRING(s_qpSampleValueTagMap, tag);
    int             numAttribs  = 0;
    qpXmlAttribute  attribs[4];

    DE_ASSERT(log && name && description && tagName);
    deMutex_lock(log->lock);

    DE_ASSERT(ContainerStack_getTop(&log->containerStack) == CONTAINERTYPE_SAMPLEINFO);

    attribs[numAttribs++] = qpSetStringAttrib("Name", name);
    attribs[numAttribs++] = qpSetStringAttrib("Description", description);
    attribs[numAttribs++] = qpSetStringAttrib("Tag", tagName);

    if (unit)
        attribs[numAttribs++] = qpSetStringAttrib("Unit", unit);

    if (!qpXmlWriter_startElement(log->writer, "ValueInfo", numAttribs, attribs) ||
        !qpXmlWriter_endElement(log->writer, "ValueInfo"))
    {
        qpPrintf("qpTestLog_writeValueInfo(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_endSampleInfo (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    if (!qpXmlWriter_endElement(log->writer, "SampleInfo"))
    {
        qpPrintf("qpTestLog_endSampleInfo(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_SAMPLEINFO);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_startSample (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    DE_ASSERT(ContainerStack_getTop(&log->containerStack) == CONTAINERTYPE_SAMPLELIST);

    if (!qpXmlWriter_startElement(log->writer, "Sample", 0, DE_NULL))
    {
        qpPrintf("qpTestLog_startSample(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_push(&log->containerStack, CONTAINERTYPE_SAMPLE));

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_writeValueFloat (qpTestLog* log, double value)
{
    char tmpString[512];
    doubleToString(value, tmpString, (int)sizeof(tmpString));

    deMutex_lock(log->lock);

    DE_ASSERT(ContainerStack_getTop(&log->containerStack) == CONTAINERTYPE_SAMPLE);

    if (!qpXmlWriter_writeStringElement(log->writer, "Value", &tmpString[0]))
    {
        qpPrintf("qpTestLog_writeSampleValue(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_writeValueInteger (qpTestLog* log, deInt64 value)
{
    char tmpString[64];
    int64ToString(value, tmpString);

    deMutex_lock(log->lock);

    DE_ASSERT(ContainerStack_getTop(&log->containerStack) == CONTAINERTYPE_SAMPLE);

    if (!qpXmlWriter_writeStringElement(log->writer, "Value", &tmpString[0]))
    {
        qpPrintf("qpTestLog_writeSampleValue(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_endSample (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    if (!qpXmlWriter_endElement(log->writer, "Sample"))
    {
        qpPrintf("qpTestLog_endSample(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_SAMPLE);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deBool qpTestLog_endSampleList (qpTestLog* log)
{
    DE_ASSERT(log);
    deMutex_lock(log->lock);

    if (!qpXmlWriter_endElement(log->writer, "SampleList"))
    {
        qpPrintf("qpTestLog_endSampleList(): Writing XML failed\n");
        deMutex_unlock(log->lock);
        return DE_FALSE;
    }

    DE_ASSERT(ContainerStack_pop(&log->containerStack) == CONTAINERTYPE_SAMPLELIST);

    deMutex_unlock(log->lock);
    return DE_TRUE;
}

deUint32 qpTestLog_getLogFlags (const qpTestLog* log)
{
    DE_ASSERT(log);
    return log->flags;
}

const char* qpGetTestResultName (qpTestResult result)
{
    return QP_LOOKUP_STRING(s_qpTestResultMap, result);
}
