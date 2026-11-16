#ifndef _QPTESTLOG_H
#define _QPTESTLOG_H
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
 * \defgroup TestLog
 * \ingroup TestLog
 * \{
 * \file
 * \brief Test log library
 *
 * qpTestLog Conventions:
 *
 * Each function takes qpTestLog pointer. Operations are done on that log
 * instance.
 *
 * When function takes a 'name' parameter, that name is expected to
 * be a unique identifier within the scope of one test case. Test case
 * begins with a call to qpTestLog_startCase and ends with a call to
 * qpTestLog_endCase or qpTestLog_terminateCase. The human readable
 * "name" for a piece of information is given with the parameter
 * called 'description'.
 *
 * All functions writing to the log return a boolean value. False
 * means that the current write operation failed and the current log
 * instance should be abandoned.
 *
 *//*--------------------------------------------------------------------*/

#include "deDefs.h"

typedef struct qpTestLog_s qpTestLog;

/* Test results supported by current report version */
/* \note Keep in sync with TestCaseStatus in Candy project. */
typedef enum qpTestResult_e
{
    QP_TEST_RESULT_PASS =
        0,               /*!< Test case passed.                                                                    */
    QP_TEST_RESULT_FAIL, /*!< Implementation produced incorrect results                                            */
    QP_TEST_RESULT_QUALITY_WARNING, /*!< Result is within specification, but is not of high quality                            */
    QP_TEST_RESULT_COMPATIBILITY_WARNING, /*!< Result is within specification, but likely to cause fragmentation in the market    */
    QP_TEST_RESULT_PENDING, /*!< The test is still running. Not a valid end result                                    */
    QP_TEST_RESULT_NOT_SUPPORTED, /*!< Implementation does not support functionality needed by this test case                */
    QP_TEST_RESULT_RESOURCE_ERROR, /*!< Implementation fails to pass the test due to lack of resources                        */
    QP_TEST_RESULT_INTERNAL_ERROR, /*!< Error occurred within Tester Core                                                    */
    QP_TEST_RESULT_CRASH,   /*!< Crash occurred in test execution.                                                    */
    QP_TEST_RESULT_TIMEOUT, /*!< Timeout occurred in test execution.                                                */
    QP_TEST_RESULT_WAIVER,  /*!< Status code reported by waived test.                                                */
    QP_TEST_RESULT_DEVICE_LOST, /*!< Test caused a Device Lost error                                                    */
    QP_TEST_RESULT_ENFORCE_DEFAULT_CONTEXT, /*!< Enforces creation of default Context as a compability with existsing code  */
    QP_TEST_RESULT_ENFORCE_DEFAULT_INSTANCE, /*!< Enforces creation of default instance as a compability with existsing code  */
    QP_TEST_RESULT_CAPABILITY_WARNING, /*!< Device returned incomplete or incorrect capability information */

    QP_TEST_RESULT_LAST
} qpTestResult;

/* Test case types. */
typedef enum qpTestCaseType_e
{
    QP_TEST_CASE_TYPE_SELF_VALIDATE = 0, /*!< Self-validating test case            */
    QP_TEST_CASE_TYPE_PERFORMANCE,       /*!< Performace test case                */
    QP_TEST_CASE_TYPE_CAPABILITY,        /*!< Capability score case                */
    QP_TEST_CASE_TYPE_ACCURACY,          /*!< Accuracy test case                    */

    QP_TEST_CASE_TYPE_LAST
} qpTestCaseType;

/*--------------------------------------------------------------------*//*!
 * \brief Tag key-value pairs to give cues on proper visualization in GUI
 *//*--------------------------------------------------------------------*/
typedef enum qpKeyValueTag_e
{
    QP_KEY_TAG_NONE = 0,
    QP_KEY_TAG_PERFORMANCE,
    QP_KEY_TAG_QUALITY,
    QP_KEY_TAG_PRECISION,
    QP_KEY_TAG_TIME,

    /* Add new values here if needed, remember to update relevant code in qpTestLog.c and change format revision */

    QP_KEY_TAG_LAST /* Do not remove */
} qpKeyValueTag;

/*--------------------------------------------------------------------*//*!
 * \brief Sample value tag for giving hints for analysis
 *//*--------------------------------------------------------------------*/
typedef enum qpSampleValueTag_e
{
    QP_SAMPLE_VALUE_TAG_PREDICTOR = 0, /*!< Predictor for sample, such as number of operations.    */
    QP_SAMPLE_VALUE_TAG_RESPONSE,      /*!< Response, i.e. measured value, such as render time.    */

    /* Add new values here if needed, remember to update relevant code in qpTestLog.c and change format revision */

    QP_SAMPLE_VALUE_TAG_LAST /* Do not remove */
} qpSampleValueTag;

/* Image compression type. */
typedef enum qpImageCompressionMode_e
{
    QP_IMAGE_COMPRESSION_MODE_NONE = 0, /*!< Do not compress images.                    */
    QP_IMAGE_COMPRESSION_MODE_PNG,      /*!< Compress images using lossless libpng.        */
    QP_IMAGE_COMPRESSION_MODE_BEST,     /*!< Choose the best image compression mode.    */

    QP_IMAGE_COMPRESSION_MODE_LAST
} qpImageCompressionMode;

/*--------------------------------------------------------------------*//*!
 * \brief Image formats.
 *
 * Pixels are handled as a byte stream, i.e., endianess does not
 * affect component ordering.
 *//*--------------------------------------------------------------------*/
typedef enum qpImageFormat_e
{
    QP_IMAGE_FORMAT_RGB888 = 0,
    QP_IMAGE_FORMAT_RGBA8888,

    QP_IMAGE_FORMAT_LAST
} qpImageFormat;

/* Test log flags. */
typedef enum qpTestLogFlag_e
{
    QP_TEST_LOG_EXCLUDE_IMAGES         = (1 << 0), //!< Do not log images. This reduces log size considerably.
    QP_TEST_LOG_EXCLUDE_SHADER_SOURCES = (1 << 1), //!< Do not log shader sources. Helps to reduce log size further.
    QP_TEST_LOG_NO_FLUSH               = (1 << 2), //!< Do not do a fflush after writing the log.
    QP_TEST_LOG_EXCLUDE_EMPTY_LOGINFO  = (1 << 3), //!< Do not log empty shader compile or link loginfo.
    QP_TEST_LOG_NO_INITIAL_OUTPUT      = (1 << 4), //!< Do not push data to cout when initializing log.
    QP_TEST_LOG_COMPACT                = (1 << 5), //!< Only write test case status.
    QP_TEST_LOG_SPLIT_SLICES           = (1 << 6), //!< Log each image slice separately.
    QP_TEST_LOG_ALL_IMAGES             = (1 << 7)  //!< Log all images
} qpTestLogFlag;

/* Shader type. */
typedef enum qpShaderType_e
{
    QP_SHADER_TYPE_VERTEX = 0,
    QP_SHADER_TYPE_FRAGMENT,
    QP_SHADER_TYPE_GEOMETRY,
    QP_SHADER_TYPE_TESS_CONTROL,
    QP_SHADER_TYPE_TESS_EVALUATION,
    QP_SHADER_TYPE_COMPUTE,
    QP_SHADER_TYPE_RAYGEN,
    QP_SHADER_TYPE_ANY_HIT,
    QP_SHADER_TYPE_CLOSEST_HIT,
    QP_SHADER_TYPE_MISS,
    QP_SHADER_TYPE_INTERSECTION,
    QP_SHADER_TYPE_CALLABLE,
    QP_SHADER_TYPE_TASK,
    QP_SHADER_TYPE_MESH,

    QP_SHADER_TYPE_LAST
} qpShaderType;

DE_BEGIN_EXTERN_C

/* \todo [2013-04-13 pyry] Clean up & document. Do we actually want this? */
typedef struct qpEglConfigInfo_s
{
    int bufferSize;
    int redSize;
    int greenSize;
    int blueSize;
    int luminanceSize;
    int alphaSize;
    int alphaMaskSize;
    bool bindToTextureRGB;
    bool bindToTextureRGBA;
    const char *colorBufferType;
    const char *configCaveat;
    int configID;
    const char *conformant;
    int depthSize;
    int level;
    int maxPBufferWidth;
    int maxPBufferHeight;
    int maxPBufferPixels;
    int maxSwapInterval;
    int minSwapInterval;
    bool nativeRenderable;
    const char *renderableType;
    int sampleBuffers;
    int samples;
    int stencilSize;
    const char *surfaceTypes;
    const char *transparentType;
    int transparentRedValue;
    int transparentGreenValue;
    int transparentBlueValue;
    bool recordableAndroid;
} qpEglConfigInfo;

qpTestLog *qpTestLog_createFileLog(const char *fileName, uint32_t flags);
bool qpTestLog_beginSession(qpTestLog *log, const char *additionalSessionInfo);
void qpTestLog_destroy(qpTestLog *log);
bool qpTestLog_isCompact(qpTestLog *log);

bool qpTestLog_startCase(qpTestLog *log, const char *testCasePath, qpTestCaseType testCaseType,
                         const char *testCaseSource);
bool qpTestLog_endCase(qpTestLog *log, qpTestResult result, const char *description);

bool qpTestLog_startTestsCasesTime(qpTestLog *log);
bool qpTestLog_endTestsCasesTime(qpTestLog *log);

bool qpTestLog_terminateCase(qpTestLog *log, qpTestResult result);

bool qpTestLog_startSection(qpTestLog *log, const char *name, const char *description);
bool qpTestLog_endSection(qpTestLog *log);
bool qpTestLog_writeText(qpTestLog *log, const char *name, const char *description, qpKeyValueTag tag,
                         const char *value);
bool qpTestLog_writeInteger(qpTestLog *log, const char *name, const char *description, const char *unit,
                            qpKeyValueTag tag, int64_t value);
bool qpTestLog_writeFloat(qpTestLog *log, const char *name, const char *description, const char *unit,
                          qpKeyValueTag tag, float value);

bool qpTestLog_startImageSet(qpTestLog *log, const char *name, const char *description);
bool qpTestLog_endImageSet(qpTestLog *log);
bool qpTestLog_writeImage(qpTestLog *log, const char *name, const char *description,
                          qpImageCompressionMode compressionMode, qpImageFormat format, int width, int height,
                          int stride, const void *data);

bool qpTestLog_startEglConfigSet(qpTestLog *log, const char *key, const char *description);
bool qpTestLog_writeEglConfig(qpTestLog *log, const qpEglConfigInfo *config);
bool qpTestLog_endEglConfigSet(qpTestLog *log);

/* \todo [2013-08-26 pyry] Unify ShaderProgram & KernelSource & CompileInfo. */

bool qpTestLog_startShaderProgram(qpTestLog *log, bool linkOk, const char *linkInfoLog);
bool qpTestLog_endShaderProgram(qpTestLog *log);
bool qpTestLog_writeShader(qpTestLog *log, qpShaderType type, const char *source, bool compileOk, const char *infoLog);

bool qpTestLog_writeKernelSource(qpTestLog *log, const char *source);
bool qpTestLog_writeSpirVAssemblySource(qpTestLog *log, const char *source);
bool qpTestLog_writeCompileInfo(qpTestLog *log, const char *name, const char *description, bool compileOk,
                                const char *infoLog);

bool qpTestLog_startSampleList(qpTestLog *log, const char *name, const char *description);
bool qpTestLog_startSampleInfo(qpTestLog *log);
bool qpTestLog_writeValueInfo(qpTestLog *log, const char *name, const char *description, const char *unit,
                              qpSampleValueTag tag);
bool qpTestLog_endSampleInfo(qpTestLog *log);
bool qpTestLog_startSample(qpTestLog *log);
bool qpTestLog_writeValueFloat(qpTestLog *log, double value);
bool qpTestLog_writeValueInteger(qpTestLog *log, int64_t value);
bool qpTestLog_endSample(qpTestLog *log);
bool qpTestLog_endSampleList(qpTestLog *log);

bool qpTestLog_writeRaw(qpTestLog *log, const char *rawContents);

void qpTestLog_setSplitSlices(qpTestLog *log, bool value);
uint32_t qpTestLog_getLogFlags(const qpTestLog *log);

const char *qpGetTestResultName(qpTestResult result);

DE_END_EXTERN_C

/*! \} */

#endif /* _QPTESTLOG_H */
