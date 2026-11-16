#ifndef _XETESTCASERESULT_HPP
#define _XETESTCASERESULT_HPP
/*-------------------------------------------------------------------------
 * drawElements Quality Program Test Executor
 * ------------------------------------------
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
 * \brief Test case result models.
 *//*--------------------------------------------------------------------*/

#include "xeDefs.hpp"
#include "xeTestCase.hpp"

#include <string>
#include <vector>
#include <ostream>

namespace xe
{

enum TestStatusCode
{
    TESTSTATUSCODE_PASS,                  //!< Test case passed.
    TESTSTATUSCODE_FAIL,                  //!< Test case failed (not passed).
    TESTSTATUSCODE_QUALITY_WARNING,       //!< Result within specification, but suspicious quality wise
    TESTSTATUSCODE_COMPATIBILITY_WARNING, //!< Result within specification, but likely to cause fragmentation
    TESTSTATUSCODE_PENDING,               //!< Not yet started.
    TESTSTATUSCODE_RUNNING,               //!< Currently running (not stored in database).
    TESTSTATUSCODE_NOT_SUPPORTED,         //!< Some feature was not supported in the implementation.
    TESTSTATUSCODE_RESOURCE_ERROR,        //!< A resource error has occurred.
    TESTSTATUSCODE_INTERNAL_ERROR,        //!< An internal error has occurred.
    TESTSTATUSCODE_CANCELED,              //!< User canceled the execution
    TESTSTATUSCODE_TIMEOUT,               //!< Test was killed because of watch dog timeout.
    TESTSTATUSCODE_CRASH,                 //!< Test executable crashed before finishing the test.
    TESTSTATUSCODE_DISABLED,              //!< Test case disabled (for current target)
    TESTSTATUSCODE_TERMINATED,            //!< Terminated for other reason.
    TESTSTATUSCODE_WAIVER,                //!< Test case waived.

    TESTSTATUSCODE_LAST
};

const char *getTestStatusCodeName(TestStatusCode statusCode);

namespace ri
{

class Item;
class Result;
class Text;
class Number;
class Image;
class ImageSet;
class VertexShader;
class FragmentShader;
class ShaderProgram;
class ShaderSource;
class InfoLog;
class EglConfig;
class EglConfigSet;
class Section;
class KernelSource;
class CompileInfo;
class SampleList;
class SampleInfo;
class ValueInfo;
class Sample;
class SampleValue;

// \todo [2014-02-28 pyry] Make List<T> for items that have only specific subitems.

class List
{
public:
    List(void);
    ~List(void);

    int getNumItems(void) const
    {
        return (int)m_items.size();
    }
    const Item &getItem(int ndx) const
    {
        return *m_items[ndx];
    }
    Item &getItem(int ndx)
    {
        return *m_items[ndx];
    }

    template <typename T>
    T *allocItem(void);

private:
    std::vector<Item *> m_items;
};

template <typename T>
T *List::allocItem(void)
{
    m_items.reserve(m_items.size() + 1);
    T *item = new T();
    m_items.push_back(static_cast<ri::Item *>(item));
    return item;
}

} // namespace ri

class TestCaseResultHeader
{
public:
    TestCaseResultHeader(void) : caseType(TESTCASETYPE_LAST), statusCode(TESTSTATUSCODE_LAST)
    {
    }

    std::string caseVersion;   //!< Test case version.
    std::string casePath;      //!< Full test case path.
    TestCaseType caseType;     //!< Test case type.
    TestStatusCode statusCode; //!< Test status code.
    std::string statusDetails; //!< Status description.
};

class TestCaseResult : public TestCaseResultHeader
{
public:
    ri::List resultItems; //!< Test log items.
};

// Result items.
namespace ri
{

// Result item type.
enum Type
{
    TYPE_RESULT = 0,
    TYPE_TEXT,
    TYPE_NUMBER,
    TYPE_IMAGE,
    TYPE_IMAGESET,
    TYPE_SHADER,
    TYPE_SHADERPROGRAM,
    TYPE_SHADERSOURCE,
    TYPE_SPIRVSOURCE,
    TYPE_INFOLOG,
    TYPE_EGLCONFIG,
    TYPE_EGLCONFIGSET,
    TYPE_SECTION,
    TYPE_KERNELSOURCE,
    TYPE_COMPILEINFO,
    TYPE_SAMPLELIST,
    TYPE_SAMPLEINFO,
    TYPE_VALUEINFO,
    TYPE_SAMPLE,
    TYPE_SAMPLEVALUE,

    TYPE_LAST
};

class NumericValue
{
public:
    enum Type
    {
        NUMVALTYPE_EMPTY = 0,
        NUMVALTYPE_INT64,
        NUMVALTYPE_FLOAT64,

        NUMVALTYPE_LAST
    };

    NumericValue(void) : m_type(NUMVALTYPE_EMPTY)
    {
    }
    NumericValue(int64_t value) : m_type(NUMVALTYPE_INT64)
    {
        m_value.int64 = value;
    }
    NumericValue(double value) : m_type(NUMVALTYPE_FLOAT64)
    {
        m_value.float64 = value;
    }

    Type getType(void) const
    {
        return m_type;
    }
    int64_t getInt64(void) const
    {
        DE_ASSERT(getType() == NUMVALTYPE_INT64);
        return m_value.int64;
    }
    double getFloat64(void) const
    {
        DE_ASSERT(getType() == NUMVALTYPE_FLOAT64);
        return m_value.float64;
    }

private:
    Type m_type;
    union
    {
        int64_t int64;
        double float64;
    } m_value;
};

std::ostream &operator<<(std::ostream &str, const NumericValue &value);

class Item
{
public:
    virtual ~Item(void)
    {
    }

    Type getType(void) const
    {
        return m_type;
    }

protected:
    Item(Type type) : m_type(type)
    {
    }

private:
    Item(const Item &other);
    Item &operator=(const Item &other);

    Type m_type;
};

class Result : public Item
{
public:
    Result(void) : Item(TYPE_RESULT), statusCode(TESTSTATUSCODE_LAST)
    {
    }
    ~Result(void)
    {
    }

    TestStatusCode statusCode;
    std::string details;
};

class Text : public Item
{
public:
    Text(void) : Item(TYPE_TEXT)
    {
    }
    ~Text(void)
    {
    }

    std::string text;
};

class Number : public Item
{
public:
    Number(void) : Item(TYPE_NUMBER)
    {
    }
    ~Number(void)
    {
    }

    std::string name;
    std::string description;
    std::string unit;
    std::string tag;
    NumericValue value;
};

class Image : public Item
{
public:
    enum Format
    {
        FORMAT_RGB888,
        FORMAT_RGBA8888,

        FORMAT_LAST
    };

    enum Compression
    {
        COMPRESSION_NONE = 0,
        COMPRESSION_PNG,

        COMPRESSION_LAST
    };

    Image(void) : Item(TYPE_IMAGE), width(0), height(0), format(FORMAT_LAST), compression(COMPRESSION_LAST)
    {
    }
    ~Image(void)
    {
    }

    std::string name;
    std::string description;
    int width;
    int height;
    Format format;
    Compression compression;
    std::vector<uint8_t> data;
};

class ImageSet : public Item
{
public:
    ImageSet(void) : Item(TYPE_IMAGESET)
    {
    }
    ~ImageSet(void)
    {
    }

    std::string name;
    std::string description;
    List images;
};

class ShaderSource : public Item
{
public:
    ShaderSource(void) : Item(TYPE_SHADERSOURCE)
    {
    }
    ~ShaderSource(void)
    {
    }

    std::string source;
};

class SpirVSource : public Item
{
public:
    SpirVSource(void) : Item(TYPE_SPIRVSOURCE)
    {
    }
    ~SpirVSource(void)
    {
    }

    std::string source;
};

class InfoLog : public Item
{
public:
    InfoLog(void) : Item(TYPE_INFOLOG)
    {
    }
    ~InfoLog(void)
    {
    }

    std::string log;
};

class Shader : public Item
{
public:
    enum ShaderType
    {
        SHADERTYPE_VERTEX = 0,
        SHADERTYPE_FRAGMENT,
        SHADERTYPE_GEOMETRY,
        SHADERTYPE_TESS_CONTROL,
        SHADERTYPE_TESS_EVALUATION,
        SHADERTYPE_COMPUTE,
        SHADERTYPE_RAYGEN,
        SHADERTYPE_ANY_HIT,
        SHADERTYPE_CLOSEST_HIT,
        SHADERTYPE_MISS,
        SHADERTYPE_INTERSECTION,
        SHADERTYPE_CALLABLE,
        SHADERTYPE_TASK,
        SHADERTYPE_MESH,

        SHADERTYPE_LAST
    };

    Shader(void) : Item(TYPE_SHADER), shaderType(SHADERTYPE_LAST), compileStatus(false)
    {
    }
    ~Shader(void)
    {
    }

    ShaderType shaderType;
    bool compileStatus;
    ShaderSource source;
    InfoLog infoLog;
};

class ShaderProgram : public Item
{
public:
    ShaderProgram(void) : Item(TYPE_SHADERPROGRAM), linkStatus(false)
    {
    }
    ~ShaderProgram(void)
    {
    }

    List shaders;
    bool linkStatus;
    InfoLog linkInfoLog;
};

class EglConfig : public Item
{
public:
    EglConfig(void);
    ~EglConfig(void)
    {
    }

    int bufferSize;
    int redSize;
    int greenSize;
    int blueSize;
    int luminanceSize;
    int alphaSize;
    int alphaMaskSize;
    bool bindToTextureRGB;
    bool bindToTextureRGBA;
    std::string colorBufferType;
    std::string configCaveat;
    int configID;
    std::string conformant;
    int depthSize;
    int level;
    int maxPBufferWidth;
    int maxPBufferHeight;
    int maxPBufferPixels;
    int maxSwapInterval;
    int minSwapInterval;
    bool nativeRenderable;
    std::string renderableType;
    int sampleBuffers;
    int samples;
    int stencilSize;
    std::string surfaceTypes;
    std::string transparentType;
    int transparentRedValue;
    int transparentGreenValue;
    int transparentBlueValue;
};

inline EglConfig::EglConfig(void)
    : Item(TYPE_EGLCONFIG)
    , bufferSize(0)
    , redSize(0)
    , greenSize(0)
    , blueSize(0)
    , luminanceSize(0)
    , alphaSize(0)
    , alphaMaskSize(0)
    , bindToTextureRGB(false)
    , bindToTextureRGBA(false)
    , configID(0)
    , depthSize(0)
    , level(0)
    , maxPBufferWidth(0)
    , maxPBufferHeight(0)
    , maxPBufferPixels(0)
    , maxSwapInterval(0)
    , minSwapInterval(0)
    , nativeRenderable(false)
    , sampleBuffers(0)
    , samples(0)
    , stencilSize(0)
    , transparentRedValue(0)
    , transparentGreenValue(0)
    , transparentBlueValue(0)
{
}

class EglConfigSet : public Item
{
public:
    EglConfigSet(void) : Item(TYPE_EGLCONFIGSET)
    {
    }
    ~EglConfigSet(void)
    {
    }

    std::string name;
    std::string description;
    List configs;
};

class Section : public Item
{
public:
    Section(void) : Item(TYPE_SECTION)
    {
    }
    ~Section(void)
    {
    }

    std::string name;
    std::string description;
    List items;
};

class KernelSource : public Item
{
public:
    KernelSource(void) : Item(TYPE_KERNELSOURCE)
    {
    }
    ~KernelSource(void)
    {
    }

    std::string source;
};

class CompileInfo : public Item
{
public:
    CompileInfo(void) : Item(TYPE_COMPILEINFO), compileStatus(false)
    {
    }
    ~CompileInfo(void)
    {
    }

    std::string name;
    std::string description;
    bool compileStatus;
    InfoLog infoLog;
};

class ValueInfo : public Item
{
public:
    enum ValueTag
    {
        VALUETAG_PREDICTOR,
        VALUETAG_RESPONSE,

        VALUETAG_LAST
    };

    ValueInfo(void) : Item(TYPE_VALUEINFO), tag(VALUETAG_LAST)
    {
    }
    ~ValueInfo(void)
    {
    }

    std::string name;
    std::string description;
    std::string unit;
    ValueTag tag;
};

class SampleInfo : public Item
{
public:
    SampleInfo(void) : Item(TYPE_SAMPLEINFO)
    {
    }
    ~SampleInfo(void)
    {
    }

    List valueInfos;
};

class SampleValue : public Item
{
public:
    SampleValue(void) : Item(TYPE_SAMPLEVALUE)
    {
    }
    ~SampleValue(void)
    {
    }

    NumericValue value;
};

class Sample : public Item
{
public:
    Sample(void) : Item(TYPE_SAMPLE)
    {
    }
    ~Sample(void)
    {
    }

    List values;
};

class SampleList : public Item
{
public:
    SampleList(void) : Item(TYPE_SAMPLELIST)
    {
    }
    ~SampleList(void)
    {
    }

    std::string name;
    std::string description;
    SampleInfo sampleInfo;
    List samples;
};

} // namespace ri
} // namespace xe

#endif // _XETESTCASERESULT_HPP
