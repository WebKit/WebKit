#ifndef _TCUTESTLOG_HPP
#define _TCUTESTLOG_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Tester Core
 * ----------------------------------------
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
 * \brief Test Log C++ Wrapper.
 *//*--------------------------------------------------------------------*/

#include "tcuDefs.hpp"
#include "qpTestLog.h"
#include "tcuTexture.hpp"

#include <sstream>

namespace tcu
{

class Surface;
class MessageBuilder;
class LogImageSet;
class LogImage;
class LogSection;
class LogShaderProgram;
class LogShader;
class LogSpirVAssemblySource;
class LogKernelSource;
class LogSampleList;
class LogValueInfo;
class SampleBuilder;
template <typename T>
class LogNumber;

/*--------------------------------------------------------------------*//*!
 * \brief Test log
 *
 * TestLog provides convinient C++ API for logging. The API has been designed
 * around stream operators much like STL iostream library. The following
 * examples demonstrate how to use TestLog.
 *
 * \code
 * TestLog& log = m_testCtx.getLog();
 *
 * // Write message to log.
 * log << TestLog::Message << "Hello, World!" << TestLog::EndMessage;
 * int myNumber = 3;
 * log << TestLog::Message << "Diff is " << myNumber << TestLog::EndMessage;
 *
 * // Write image
 * Surface myImage(256, 256);
 * log << TestLog::Image("TestImage", "My test image", myImage);
 *
 * // Multiple commands can be combined:
 * log << TestLog::Section("Details", "Test case details")
 *     << TestLog::Message << "Here be dragons" << TestLog::EndMessage
 *     << TestLog::ImageSet("Result", "Result images")
 *     << TestLog::Image("ImageA", "Image A", imageA)
 *     << TestLog::Image("ImageB", "Image B", imageB)
 *     << TestLog::EndImageSet << TestLog::EndSection;
 * \endcode
 *//*--------------------------------------------------------------------*/
class TestLog
{
public:
    // Tokens
    static const class BeginMessageToken
    {
    } Message;
    static const class EndMessageToken
    {
    } EndMessage;
    static const class EndImageSetToken
    {
    } EndImageSet;
    static const class EndSectionToken
    {
    } EndSection;
    static const class EndShaderProgramToken
    {
    } EndShaderProgram;
    static const class SampleInfoToken
    {
    } SampleInfo;
    static const class EndSampleInfoToken
    {
    } EndSampleInfo;
    static const class BeginSampleToken
    {
    } Sample;
    static const class EndSampleToken
    {
    } EndSample;
    static const class EndSampleListToken
    {
    } EndSampleList;

    // Typedefs.
    typedef LogImageSet ImageSet;
    typedef LogImage Image;
    typedef LogSection Section;
    typedef LogShaderProgram ShaderProgram;
    typedef LogShader Shader;
    typedef LogSpirVAssemblySource SpirVAssemblySource;
    typedef LogKernelSource KernelSource;
    typedef LogSampleList SampleList;
    typedef LogValueInfo ValueInfo;
    typedef LogNumber<float> Float;
    typedef LogNumber<int64_t> Integer;

    explicit TestLog(const char *fileName, uint32_t flags = 0);
    ~TestLog(void);

    void writeSessionInfo(std::string additionalInfo = "");

    MessageBuilder operator<<(const BeginMessageToken &);
    MessageBuilder message(void);

    TestLog &operator<<(const ImageSet &imageSet);
    TestLog &operator<<(const Image &image);
    TestLog &operator<<(const EndImageSetToken &);

    TestLog &operator<<(const Section &section);
    TestLog &operator<<(const EndSectionToken &);

    TestLog &operator<<(const ShaderProgram &shaderProgram);
    TestLog &operator<<(const EndShaderProgramToken &);
    TestLog &operator<<(const Shader &shader);
    TestLog &operator<<(const SpirVAssemblySource &module);

    TestLog &operator<<(const KernelSource &kernelSrc);

    template <typename T>
    TestLog &operator<<(const LogNumber<T> &number);

    TestLog &operator<<(const SampleList &sampleList);
    TestLog &operator<<(const SampleInfoToken &);
    TestLog &operator<<(const ValueInfo &valueInfo);
    TestLog &operator<<(const EndSampleInfoToken &);
    SampleBuilder operator<<(const BeginSampleToken &);
    TestLog &operator<<(const EndSampleListToken &);

    // Raw api
    void writeMessage(const char *message);

    void startImageSet(const char *name, const char *description);
    void endImageSet(void);
    void writeImage(const char *name, const char *description, const ConstPixelBufferAccess &surface, const Vec4 &scale,
                    const Vec4 &bias, qpImageCompressionMode compressionMode = QP_IMAGE_COMPRESSION_MODE_BEST);
    void writeImage(const char *name, const char *description, qpImageCompressionMode compressionMode,
                    qpImageFormat format, int width, int height, int stride, const void *data);

    void startSection(const char *name, const char *description);
    void endSection(void);

    void startShaderProgram(bool linkOk, const char *linkInfoLog);
    void endShaderProgram(void);
    void writeShader(qpShaderType type, const char *source, bool compileOk, const char *infoLog);
    void writeSpirVAssemblySource(const char *source);
    void writeKernelSource(const char *source);
    void writeCompileInfo(const char *name, const char *description, bool compileOk, const char *infoLog);

    void writeFloat(const char *name, const char *description, const char *unit, qpKeyValueTag tag, float value);
    void writeInteger(const char *name, const char *description, const char *unit, qpKeyValueTag tag, int64_t value);

    void startEglConfigSet(const char *name, const char *description);
    void writeEglConfig(const qpEglConfigInfo *config);
    void endEglConfigSet(void);

    void startCase(const char *testCasePath, qpTestCaseType testCaseType, const char *testCaseSource = nullptr);
    void endCase(qpTestResult result, const char *description);
    void terminateCase(qpTestResult result);

    void startTestsCasesTime(void);
    void endTestsCasesTime(void);

    void startSampleList(const std::string &name, const std::string &description);
    void startSampleInfo(void);
    void writeValueInfo(const std::string &name, const std::string &description, const std::string &unit,
                        qpSampleValueTag tag);
    void endSampleInfo(void);
    void startSample(void);
    void writeSampleValue(double value);
    void writeSampleValue(int64_t value);
    void endSample(void);
    void endSampleList(void);

    void writeRaw(const char *rawContents);

    bool isShaderLoggingEnabled(void);

    void supressLogging(bool value);
    bool isSupressLogging(void);

    void separateSlices(bool value = true);
    bool isSeparateSlices(void) const;
    bool logAllImages(void);

private:
    TestLog(const TestLog &other);            // Not allowed!
    TestLog &operator=(const TestLog &other); // Not allowed!

    qpTestLog *m_log;
    bool m_logSupressed;
    bool m_skipAdditionalDataInLog;
};

class MessageBuilder
{
public:
    explicit MessageBuilder(TestLog *log) : m_log(log)
    {
    }
    ~MessageBuilder(void)
    {
    }

    std::string toString(void) const
    {
        return m_str.str();
    }

    TestLog &operator<<(const TestLog::EndMessageToken &);

    template <typename T>
    MessageBuilder &operator<<(const T &value);

    MessageBuilder(const MessageBuilder &other);
    MessageBuilder &operator=(const MessageBuilder &other);

private:
    TestLog *m_log;
    std::ostringstream m_str;
};

class SampleBuilder
{
public:
    SampleBuilder(TestLog *log) : m_log(log)
    {
    }

    SampleBuilder &operator<<(int v)
    {
        m_values.push_back(Value((int64_t)v));
        return *this;
    }
    SampleBuilder &operator<<(int64_t v)
    {
        m_values.push_back(Value(v));
        return *this;
    }
    SampleBuilder &operator<<(float v)
    {
        m_values.push_back(Value((double)v));
        return *this;
    }
    SampleBuilder &operator<<(double v)
    {
        m_values.push_back(Value(v));
        return *this;
    }

    TestLog &operator<<(const TestLog::EndSampleToken &);

private:
    struct Value
    {
        enum Type
        {
            TYPE_INT64 = 0,
            TYPE_FLOAT64,
            TYPE_LAST
        };

        Type type;
        union
        {
            int64_t int64;
            double float64;
        } value;

        Value(void) : type(TYPE_LAST)
        {
            value.int64 = 0;
        }
        Value(double v) : type(TYPE_FLOAT64)
        {
            value.float64 = v;
        }
        Value(int64_t v) : type(TYPE_INT64)
        {
            value.int64 = v;
        }
    };

    TestLog *m_log;
    std::vector<Value> m_values;
};

class LogImageSet
{
public:
    LogImageSet(const std::string &name, const std::string &description) : m_name(name), m_description(description)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_name;
    std::string m_description;
};

// \note Doesn't take copy of surface contents
class LogImage
{
public:
    LogImage(const std::string &name, const std::string &description, const Surface &surface,
             qpImageCompressionMode compression = QP_IMAGE_COMPRESSION_MODE_BEST);

    LogImage(const std::string &name, const std::string &description, const ConstPixelBufferAccess &access,
             qpImageCompressionMode compression = QP_IMAGE_COMPRESSION_MODE_BEST);

    LogImage(const std::string &name, const std::string &description, const ConstPixelBufferAccess &access,
             const Vec4 &scale, const Vec4 &bias, qpImageCompressionMode compression = QP_IMAGE_COMPRESSION_MODE_BEST);

    void write(TestLog &log) const;

private:
    std::string m_name;
    std::string m_description;
    ConstPixelBufferAccess m_access;
    Vec4 m_scale;
    Vec4 m_bias;
    qpImageCompressionMode m_compression;
};

class LogSection
{
public:
    LogSection(const std::string &name, const std::string &description) : m_name(name), m_description(description)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_name;
    std::string m_description;
};

class LogShaderProgram
{
public:
    LogShaderProgram(bool linkOk, const std::string &linkInfoLog) : m_linkOk(linkOk), m_linkInfoLog(linkInfoLog)
    {
    }

    void write(TestLog &log) const;

private:
    bool m_linkOk;
    std::string m_linkInfoLog;
};

class LogShader
{
public:
    LogShader(qpShaderType type, const std::string &source, bool compileOk, const std::string &infoLog)
        : m_type(type)
        , m_source(source)
        , m_compileOk(compileOk)
        , m_infoLog(infoLog)
    {
    }

    void write(TestLog &log) const;

private:
    qpShaderType m_type;
    std::string m_source;
    bool m_compileOk;
    std::string m_infoLog;
};

class LogSpirVAssemblySource
{
public:
    LogSpirVAssemblySource(const std::string &source) : m_source(source)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_source;
};

class LogKernelSource
{
public:
    explicit LogKernelSource(const std::string &source) : m_source(source)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_source;
};

class LogSampleList
{
public:
    LogSampleList(const std::string &name, const std::string &description) : m_name(name), m_description(description)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_name;
    std::string m_description;
};

class LogValueInfo
{
public:
    LogValueInfo(const std::string &name, const std::string &description, const std::string &unit, qpSampleValueTag tag)
        : m_name(name)
        , m_description(description)
        , m_unit(unit)
        , m_tag(tag)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_name;
    std::string m_description;
    std::string m_unit;
    qpSampleValueTag m_tag;
};

template <typename T>
class LogNumber
{
public:
    LogNumber(const std::string &name, const std::string &desc, const std::string &unit, qpKeyValueTag tag, T value)
        : m_name(name)
        , m_desc(desc)
        , m_unit(unit)
        , m_tag(tag)
        , m_value(value)
    {
    }

    void write(TestLog &log) const;

private:
    std::string m_name;
    std::string m_desc;
    std::string m_unit;
    qpKeyValueTag m_tag;
    T m_value;
};

// Section helper that closes section when leaving scope.
class ScopedLogSection
{
public:
    ScopedLogSection(TestLog &log, const std::string &name, const std::string &description) : m_log(log)
    {
        m_log << TestLog::Section(name, description);
    }

    ~ScopedLogSection(void)
    {
        m_log << TestLog::EndSection;
    }

private:
    TestLog &m_log;
};

// TestLog stream operators.

inline TestLog &TestLog::operator<<(const ImageSet &imageSet)
{
    imageSet.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const Image &image)
{
    image.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const EndImageSetToken &)
{
    endImageSet();
    return *this;
}
inline TestLog &TestLog::operator<<(const Section &section)
{
    section.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const EndSectionToken &)
{
    endSection();
    return *this;
}
inline TestLog &TestLog::operator<<(const ShaderProgram &shaderProg)
{
    shaderProg.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const EndShaderProgramToken &)
{
    endShaderProgram();
    return *this;
}
inline TestLog &TestLog::operator<<(const Shader &shader)
{
    shader.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const SpirVAssemblySource &module)
{
    module.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const KernelSource &kernelSrc)
{
    kernelSrc.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const SampleList &sampleList)
{
    sampleList.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const SampleInfoToken &)
{
    startSampleInfo();
    return *this;
}
inline TestLog &TestLog::operator<<(const ValueInfo &valueInfo)
{
    valueInfo.write(*this);
    return *this;
}
inline TestLog &TestLog::operator<<(const EndSampleInfoToken &)
{
    endSampleInfo();
    return *this;
}
inline TestLog &TestLog::operator<<(const EndSampleListToken &)
{
    endSampleList();
    return *this;
}

template <typename T>
inline TestLog &TestLog::operator<<(const LogNumber<T> &number)
{
    number.write(*this);
    return *this;
}

inline TestLog &operator<<(TestLog &log, const std::exception &e)
{
    // \todo [2012-10-18 pyry] Print type info?
    return log << TestLog::Message << e.what() << TestLog::EndMessage;
}

inline bool TestLog::logAllImages(void)
{
    if ((qpTestLog_getLogFlags(m_log) & QP_TEST_LOG_ALL_IMAGES) != 0)
        return true;

    return false;
}

// Utility class inline implementations.

template <typename T>
inline MessageBuilder &MessageBuilder::operator<<(const T &value)
{
    // Overload stream operator to implement custom format
    m_str << value;
    return *this;
}

inline MessageBuilder TestLog::operator<<(const BeginMessageToken &)
{
    return MessageBuilder(this);
}

inline MessageBuilder TestLog::message(void)
{
    return MessageBuilder(this);
}

inline SampleBuilder TestLog::operator<<(const BeginSampleToken &)
{
    return SampleBuilder(this);
}

inline void LogImageSet::write(TestLog &log) const
{
    log.startImageSet(m_name.c_str(), m_description.c_str());
}

inline void LogSection::write(TestLog &log) const
{
    log.startSection(m_name.c_str(), m_description.c_str());
}

inline void LogShaderProgram::write(TestLog &log) const
{
    log.startShaderProgram(m_linkOk, m_linkInfoLog.c_str());
}

inline void LogShader::write(TestLog &log) const
{
    log.writeShader(m_type, m_source.c_str(), m_compileOk, m_infoLog.c_str());
}

inline void LogSpirVAssemblySource::write(TestLog &log) const
{
    log.writeSpirVAssemblySource(m_source.c_str());
}

inline void LogKernelSource::write(TestLog &log) const
{
    log.writeKernelSource(m_source.c_str());
}

inline void LogSampleList::write(TestLog &log) const
{
    log.startSampleList(m_name, m_description);
}

inline void LogValueInfo::write(TestLog &log) const
{
    log.writeValueInfo(m_name, m_description, m_unit, m_tag);
}

template <>
inline void LogNumber<float>::write(TestLog &log) const
{
    log.writeFloat(m_name.c_str(), m_desc.c_str(), m_unit.c_str(), m_tag, m_value);
}

template <>
inline void LogNumber<int64_t>::write(TestLog &log) const
{
    log.writeInteger(m_name.c_str(), m_desc.c_str(), m_unit.c_str(), m_tag, m_value);
}

} // namespace tcu

#endif // _TCUTESTLOG_HPP
