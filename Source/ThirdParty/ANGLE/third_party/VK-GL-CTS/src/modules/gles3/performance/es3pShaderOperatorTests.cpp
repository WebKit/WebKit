/*-------------------------------------------------------------------------
 * drawElements Quality Program OpenGL ES 3.0 Module
 * -------------------------------------------------
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
 * \brief Shader operator performance tests.
 *//*--------------------------------------------------------------------*/

#include "es3pShaderOperatorTests.hpp"
#include "glsCalibration.hpp"
#include "gluShaderUtil.hpp"
#include "gluShaderProgram.hpp"
#include "gluPixelTransfer.hpp"
#include "tcuTestLog.hpp"
#include "tcuRenderTarget.hpp"
#include "tcuCommandLine.hpp"
#include "tcuSurface.hpp"
#include "deStringUtil.hpp"
#include "deSharedPtr.hpp"
#include "deClock.h"
#include "deMath.h"

#include "glwEnums.hpp"
#include "glwFunctions.hpp"

#include <map>
#include <algorithm>
#include <limits>
#include <set>

namespace deqp
{
namespace gles3
{
namespace Performance
{

using namespace gls;
using namespace glu;
using de::SharedPtr;
using tcu::TestLog;
using tcu::Vec2;
using tcu::Vec4;

using std::string;
using std::vector;

#define MEASUREMENT_FAIL() \
    throw tcu::InternalError("Unable to get sensible measurements for estimation", nullptr, __FILE__, __LINE__)

// Number of measurements in OperatorPerformanceCase for each workload size, unless specified otherwise by a command line argument.
static const int DEFAULT_NUM_MEASUREMENTS_PER_WORKLOAD = 3;
// How many different workload sizes are used by OperatorPerformanceCase.
static const int NUM_WORKLOADS = 8;
// Maximum workload size that can be attempted. In a sensible case, this most likely won't be reached.
static const int MAX_WORKLOAD_SIZE = 1 << 29;

// BinaryOpCase-specific constants for shader generation.
static const int BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS = 4;
static const int BINARY_OPERATOR_CASE_SMALL_PROGRAM_UNROLL_AMOUNT  = 2;
static const int BINARY_OPERATOR_CASE_BIG_PROGRAM_UNROLL_AMOUNT    = 4;

// FunctionCase-specific constants for shader generation.
static const int FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS = 4;

static const char *const s_swizzles[][4] = {{"x", "yx", "yzx", "wzyx"},
                                            {"y", "zy", "wyz", "xwzy"},
                                            {"z", "wy", "zxy", "yzwx"},
                                            {"w", "xw", "yxw", "zyxw"}};

template <int N>
static tcu::Vector<float, N> mean(const vector<tcu::Vector<float, N>> &data)
{
    tcu::Vector<float, N> sum(0.0f);
    for (int i = 0; i < (int)data.size(); i++)
        sum += data[i];
    return sum / tcu::Vector<float, N>((float)data.size());
}

static void uniformNfv(const glw::Functions &gl, int n, int location, int count, const float *data)
{
    switch (n)
    {
    case 1:
        gl.uniform1fv(location, count, data);
        break;
    case 2:
        gl.uniform2fv(location, count, data);
        break;
    case 3:
        gl.uniform3fv(location, count, data);
        break;
    case 4:
        gl.uniform4fv(location, count, data);
        break;
    default:
        DE_ASSERT(false);
    }
}

static void uniformNiv(const glw::Functions &gl, int n, int location, int count, const int *data)
{
    switch (n)
    {
    case 1:
        gl.uniform1iv(location, count, data);
        break;
    case 2:
        gl.uniform2iv(location, count, data);
        break;
    case 3:
        gl.uniform3iv(location, count, data);
        break;
    case 4:
        gl.uniform4iv(location, count, data);
        break;
    default:
        DE_ASSERT(false);
    }
}

static void uniformMatrixNfv(const glw::Functions &gl, int n, int location, int count, const float *data)
{
    switch (n)
    {
    case 2:
        gl.uniformMatrix2fv(location, count, GL_FALSE, &data[0]);
        break;
    case 3:
        gl.uniformMatrix3fv(location, count, GL_FALSE, &data[0]);
        break;
    case 4:
        gl.uniformMatrix4fv(location, count, GL_FALSE, &data[0]);
        break;
    default:
        DE_ASSERT(false);
    }
}

static glu::DataType getDataTypeFloatOrVec(int size)
{
    return size == 1 ? glu::TYPE_FLOAT : glu::getDataTypeFloatVec(size);
}

static int getIterationCountOrDefault(const tcu::CommandLine &cmdLine, int def)
{
    const int cmdLineVal = cmdLine.getTestIterationCount();
    return cmdLineVal > 0 ? cmdLineVal : def;
}

static string lineParamsString(const LineParameters &params)
{
    return "y = " + de::toString(params.offset) + " + " + de::toString(params.coefficient) + "*x";
}

namespace
{

/*--------------------------------------------------------------------*//*!
 * \brief Abstract class for measuring shader operator performance.
 *
 * This class draws multiple times with different workload sizes (set
 * via a uniform, by subclass). Time for each frame is measured, and the
 * slope of the workload size vs frame time data is estimated. This slope
 * tells us the estimated increase in frame time caused by a workload
 * increase of 1 unit (what 1 workload unit means is up to subclass).
 *
 * Generally, the shaders contain not just the operation we're interested
 * in (e.g. addition) but also some other stuff (e.g. loop overhead). To
 * eliminate this cost, we actually do the stuff described in the above
 * paragraph with multiple programs (usually two), which contain different
 * kinds of workload (e.g. different loop contents). Then we can (in
 * theory) compute the cost of just one operation in a subclass-dependent
 * manner.
 *
 * At this point, the result tells us the increase in frame time caused
 * by the addition of one operation. Dividing this by the amount of
 * draw calls in a frame, and further by the amount of vertices or
 * fragments in a draw call, we get the time cost of one operation.
 *
 * In reality, there sometimes isn't just a trivial linear dependence
 * between workload size and frame time. Instead, there tends to be some
 * amount of initial "free" operations. That is, it may be that all
 * workload sizes below some positive integer C yield the same frame time,
 * and only workload sizes beyond C increase the frame time in a supposedly
 * linear manner. Graphically, this means that there graph consists of two
 * parts: a horizontal left part, and a linearly increasing right part; the
 * right part starts where the left parts ends. The principal task of these
 * tests is to look at the slope of the increasing right part. Additionally
 * an estimate for the amount of initial free operations is calculated.
 * Note that it is also normal to get graphs where the horizontal left part
 * is of zero width, i.e. there are no free operations.
 *//*--------------------------------------------------------------------*/
class OperatorPerformanceCase : public tcu::TestCase
{
public:
    enum CaseType
    {
        CASETYPE_VERTEX = 0,
        CASETYPE_FRAGMENT,

        CASETYPE_LAST
    };

    struct InitialCalibration
    {
        int initialNumCalls;
        InitialCalibration(void) : initialNumCalls(1)
        {
        }
    };

    typedef SharedPtr<InitialCalibration> InitialCalibrationStorage;

    OperatorPerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx, const char *name,
                            const char *description, CaseType caseType, int numWorkloads,
                            const InitialCalibrationStorage &initialCalibrationStorage);
    ~OperatorPerformanceCase(void);

    void init(void);
    void deinit(void);

    IterateResult iterate(void);

    struct AttribSpec
    {
        AttribSpec(const char *name_, const tcu::Vec4 &p00_, const tcu::Vec4 &p01_, const tcu::Vec4 &p10_,
                   const tcu::Vec4 &p11_)
            : name(name_)
            , p00(p00_)
            , p01(p01_)
            , p10(p10_)
            , p11(p11_)
        {
        }

        AttribSpec(void)
        {
        }

        std::string name;
        tcu::Vec4 p00; //!< Bottom left.
        tcu::Vec4 p01; //!< Bottom right.
        tcu::Vec4 p10; //!< Top left.
        tcu::Vec4 p11; //!< Top right.
    };

protected:
    struct ProgramContext
    {
        string vertShaderSource;
        string fragShaderSource;
        vector<AttribSpec> attributes;

        string description;

        ProgramContext(void)
        {
        }
        ProgramContext(const string &vs, const string &fs, const vector<AttribSpec> &attrs, const string &desc)
            : vertShaderSource(vs)
            , fragShaderSource(fs)
            , attributes(attrs)
            , description(desc)
        {
        }
    };

    virtual vector<ProgramContext> generateProgramData(void) const = 0;
    //! Sets program-specific uniforms that don't depend on the workload size.
    virtual void setGeneralUniforms(uint32_t program) const = 0;
    //! Sets the uniform(s) that specifies the workload size in the shader.
    virtual void setWorkloadSizeUniform(uint32_t program, int workload) const = 0;
    //! Computes the cost of a single operation, given the workload costs per program.
    virtual float computeSingleOperationTime(const vector<float> &perProgramWorkloadCosts) const = 0;
    //! Logs a human-readable description of what computeSingleOperationTime does.
    virtual void logSingleOperationCalculationInfo(void) const = 0;

    glu::RenderContext &m_renderCtx;

    CaseType m_caseType;

private:
    enum State
    {
        STATE_CALIBRATING = 0, //!< Calibrate draw call count, using first program in m_programs, with workload size 1.
        STATE_FIND_HIGH_WORKLOAD, //!< Find an appropriate lower bound for the highest workload size we intend to use (one with high-enough frame time compared to workload size 1) for each program.
        STATE_MEASURING,          //!< Do actual measurements, for each program in m_programs.
        STATE_REPORTING,          //!< Measurements are done; calculate results and log.
        STATE_FINISHED,           //!< All done.

        STATE_LAST
    };

    struct WorkloadRecord
    {
        int workloadSize;
        vector<float> frameTimes; //!< In microseconds.

        WorkloadRecord(int workloadSize_) : workloadSize(workloadSize_)
        {
        }
        bool operator<(const WorkloadRecord &other) const
        {
            return this->workloadSize < other.workloadSize;
        }
        void addFrameTime(float time)
        {
            frameTimes.push_back(time);
        }
        float getMedianTime(void) const
        {
            vector<float> times = frameTimes;
            std::sort(times.begin(), times.end());
            return times.size() % 2 == 0 ? (times[times.size() / 2 - 1] + times[times.size() / 2]) * 0.5f :
                                           times[times.size() / 2];
        }
    };

    void prepareProgram(int progNdx); //!< Sets attributes and uniforms for m_programs[progNdx].
    void prepareWorkload(
        int progNdx,
        int workload); //!< Calls setWorkloadSizeUniform and draws, in case the implementation does some draw-time compilation.
    void prepareNextRound(void); //!< Increases workload and/or updates m_state.
    void render(int numDrawCalls);
    uint64_t renderAndMeasure(int numDrawCalls);
    void adjustAndLogGridAndViewport(
        void); //!< Log grid and viewport sizes, after possibly reducing them to reduce draw time.

    vector<Vec2> getWorkloadMedianDataPoints(
        int progNdx) const; //!< [ Vec2(r.workloadSize, r.getMedianTime()) for r in m_workloadRecords[progNdx] ]

    const int m_numMeasurementsPerWorkload;
    const int m_numWorkloads; //!< How many different workload sizes are used for measurement for each program.

    int m_workloadNdx; //!< Runs from 0 to m_numWorkloads-1.

    int m_workloadMeasurementNdx;
    vector<vector<WorkloadRecord>>
        m_workloadRecordsFindHigh; //!< The measurements done during STATE_FIND_HIGH_WORKLOAD.
    vector<vector<WorkloadRecord>>
        m_workloadRecords; //!< The measurements of each program in m_programs. Generated during STATE_MEASURING, into index specified by m_measureProgramNdx.

    State m_state;
    int m_measureProgramNdx; //!< When m_state is STATE_FIND_HIGH_WORKLOAD or STATE_MEASURING, this tells which program in m_programs is being measured.

    vector<int>
        m_highWorkloadSizes; //!< The first workload size encountered during STATE_FIND_HIGH_WORKLOAD that was determined suitable, for each program.

    TheilSenCalibrator m_calibrator;
    InitialCalibrationStorage m_initialCalibrationStorage;

    int m_viewportWidth;
    int m_viewportHeight;
    int m_gridSizeX;
    int m_gridSizeY;

    vector<ProgramContext> m_programData;
    vector<SharedPtr<ShaderProgram>> m_programs;

    std::vector<uint32_t> m_attribBuffers;
};

static inline float triangleInterpolate(float v0, float v1, float v2, float x, float y)
{
    return v0 + (v2 - v0) * x + (v1 - v0) * y;
}

static inline float triQuadInterpolate(float x, float y, const tcu::Vec4 &quad)
{
    // \note Top left fill rule.
    if (x + y < 1.0f)
        return triangleInterpolate(quad.x(), quad.y(), quad.z(), x, y);
    else
        return triangleInterpolate(quad.w(), quad.z(), quad.y(), 1.0f - x, 1.0f - y);
}

static inline int getNumVertices(int gridSizeX, int gridSizeY)
{
    return gridSizeX * gridSizeY * 2 * 3;
}

static void generateVertices(std::vector<float> &dst, int gridSizeX, int gridSizeY,
                             const OperatorPerformanceCase::AttribSpec &spec)
{
    const int numComponents = 4;

    DE_ASSERT(gridSizeX >= 1 && gridSizeY >= 1);
    dst.resize(getNumVertices(gridSizeX, gridSizeY) * numComponents);

    {
        int dstNdx = 0;

        for (int baseY = 0; baseY < gridSizeY; baseY++)
            for (int baseX = 0; baseX < gridSizeX; baseX++)
            {
                const float xf0 = (float)(baseX + 0) / (float)gridSizeX;
                const float yf0 = (float)(baseY + 0) / (float)gridSizeY;
                const float xf1 = (float)(baseX + 1) / (float)gridSizeX;
                const float yf1 = (float)(baseY + 1) / (float)gridSizeY;

#define ADD_VERTEX(XF, YF)                                    \
    for (int compNdx = 0; compNdx < numComponents; compNdx++) \
    dst[dstNdx++] = triQuadInterpolate(                       \
        (XF), (YF), tcu::Vec4(spec.p00[compNdx], spec.p01[compNdx], spec.p10[compNdx], spec.p11[compNdx]))

                ADD_VERTEX(xf0, yf0);
                ADD_VERTEX(xf1, yf0);
                ADD_VERTEX(xf0, yf1);

                ADD_VERTEX(xf1, yf0);
                ADD_VERTEX(xf1, yf1);
                ADD_VERTEX(xf0, yf1);

#undef ADD_VERTEX
            }
    }
}

static float intersectionX(const gls::LineParameters &a, const gls::LineParameters &b)
{
    return (a.offset - b.offset) / (b.coefficient - a.coefficient);
}

static int numDistinctX(const vector<Vec2> &data)
{
    std::set<float> xs;
    for (int i = 0; i < (int)data.size(); i++)
        xs.insert(data[i].x());
    return (int)xs.size();
}

static gls::LineParameters simpleLinearRegression(const vector<Vec2> &data)
{
    const Vec2 mid = mean(data);

    float slopeNumerator   = 0.0f;
    float slopeDenominator = 0.0f;

    for (int i = 0; i < (int)data.size(); i++)
    {
        const Vec2 diff = data[i] - mid;

        slopeNumerator += diff.x() * diff.y();
        slopeDenominator += diff.x() * diff.x();
    }

    const float slope  = slopeNumerator / slopeDenominator;
    const float offset = mid.y() - slope * mid.x();

    return gls::LineParameters(offset, slope);
}

static float simpleLinearRegressionError(const vector<Vec2> &data)
{
    if (numDistinctX(data) <= 2)
        return 0.0f;
    else
    {
        const gls::LineParameters estimator = simpleLinearRegression(data);
        float error                         = 0.0f;

        for (int i = 0; i < (int)data.size(); i++)
        {
            const float estY = estimator.offset + estimator.coefficient * data[i].x();
            const float diff = estY - data[i].y();
            error += diff * diff;
        }

        return error / (float)data.size();
    }
}

static float verticalVariance(const vector<Vec2> &data)
{
    if (numDistinctX(data) <= 2)
        return 0.0f;
    else
    {
        const float meanY = mean(data).y();
        float error       = 0.0f;

        for (int i = 0; i < (int)data.size(); i++)
        {
            const float diff = meanY - data[i].y();
            error += diff * diff;
        }

        return error / (float)data.size();
    }
}

/*--------------------------------------------------------------------*//*!
 * \brief Find the x coord that divides the input data into two slopes.
 *
 * The operator performance measurements tend to produce results where
 * we get small operation counts "for free" (e.g. because the operations
 * are performed during some memory transfer overhead or something),
 * resulting in a curve with two parts: an initial horizontal line segment,
 * and a rising line.
 *
 * This function finds the x coordinate that divides the input data into
 * two parts such that the sum of the mean square errors for the
 * least-squares estimated lines for the two parts is minimized, under the
 * additional condition that the left line is horizontal.
 *
 * This function returns a number X s.t. { pt | pt is in data, pt.x >= X }
 * is the right line, and the rest of data is the left line.
 *//*--------------------------------------------------------------------*/
static float findSlopePivotX(const vector<Vec2> &data)
{
    std::set<float> xCoords;
    for (int i = 0; i < (int)data.size(); i++)
        xCoords.insert(data[i].x());

    float lowestError = std::numeric_limits<float>::infinity();
    float bestPivotX  = -std::numeric_limits<float>::infinity();

    for (std::set<float>::const_iterator pivotX = xCoords.begin(); pivotX != xCoords.end(); ++pivotX)
    {
        vector<Vec2> leftData;
        vector<Vec2> rightData;
        for (int i = 0; i < (int)data.size(); i++)
        {
            if (data[i].x() < *pivotX)
                leftData.push_back(data[i]);
            else
                rightData.push_back(data[i]);
        }

        if (numDistinctX(rightData) < 3) // We don't trust the right data if there's too little of it.
            break;

        {
            const float totalError = verticalVariance(leftData) + simpleLinearRegressionError(rightData);

            if (totalError < lowestError)
            {
                lowestError = totalError;
                bestPivotX  = *pivotX;
            }
        }
    }

    DE_ASSERT(lowestError < std::numeric_limits<float>::infinity());

    return bestPivotX;
}

struct SegmentedEstimator
{
    float pivotX; //!< Value returned by findSlopePivotX, or -infinity if only single line.
    gls::LineParameters left;
    gls::LineParameters right;
    SegmentedEstimator(const gls::LineParameters &l, const gls::LineParameters &r, float pivotX_)
        : pivotX(pivotX_)
        , left(l)
        , right(r)
    {
    }
};

/*--------------------------------------------------------------------*//*!
 * \brief Compute line estimators for (potentially) two-segment data.
 *
 * Splits the given data into left and right parts (using findSlopePivotX)
 * and returns the line estimates for them.
 *
 * Sometimes, however (especially in fragment shader cases) the data is
 * in fact not segmented, but a straight line. This function attempts to
 * detect if this the case, and if so, sets left.offset = right.offset and
 * left.slope = 0, meaning essentially that the initial "flat" part of the
 * data has zero width.
 *//*--------------------------------------------------------------------*/
static SegmentedEstimator computeSegmentedEstimator(const vector<Vec2> &data)
{
    const float pivotX = findSlopePivotX(data);
    vector<Vec2> leftData;
    vector<Vec2> rightData;

    for (int i = 0; i < (int)data.size(); i++)
    {
        if (data[i].x() < pivotX)
            leftData.push_back(data[i]);
        else
            rightData.push_back(data[i]);
    }

    {
        const gls::LineParameters leftLine  = gls::theilSenLinearRegression(leftData);
        const gls::LineParameters rightLine = gls::theilSenLinearRegression(rightData);

        if (numDistinctX(leftData) < 2 || leftLine.coefficient > rightLine.coefficient * 0.5f)
        {
            // Left data doesn't seem credible; assume the data is just a single line.
            const gls::LineParameters entireLine = gls::theilSenLinearRegression(data);
            return SegmentedEstimator(gls::LineParameters(entireLine.offset, 0.0f), entireLine,
                                      -std::numeric_limits<float>::infinity());
        }
        else
            return SegmentedEstimator(leftLine, rightLine, pivotX);
    }
}

OperatorPerformanceCase::OperatorPerformanceCase(tcu::TestContext &testCtx, glu::RenderContext &renderCtx,
                                                 const char *name, const char *description, CaseType caseType,
                                                 int numWorkloads,
                                                 const InitialCalibrationStorage &initialCalibrationStorage)
    : tcu::TestCase(testCtx, tcu::NODETYPE_PERFORMANCE, name, description)
    , m_renderCtx(renderCtx)
    , m_caseType(caseType)
    , m_numMeasurementsPerWorkload(
          getIterationCountOrDefault(m_testCtx.getCommandLine(), DEFAULT_NUM_MEASUREMENTS_PER_WORKLOAD))
    , m_numWorkloads(numWorkloads)
    , m_workloadNdx(-1)
    , m_workloadMeasurementNdx(-1)
    , m_state(STATE_LAST)
    , m_measureProgramNdx(-1)
    , m_initialCalibrationStorage(initialCalibrationStorage)
    , m_viewportWidth(caseType == CASETYPE_VERTEX ? 32 : renderCtx.getRenderTarget().getWidth())
    , m_viewportHeight(caseType == CASETYPE_VERTEX ? 32 : renderCtx.getRenderTarget().getHeight())
    , m_gridSizeX(caseType == CASETYPE_FRAGMENT ? 1 : 100)
    , m_gridSizeY(caseType == CASETYPE_FRAGMENT ? 1 : 100)
{
    DE_ASSERT(m_numWorkloads > 0);
}

OperatorPerformanceCase::~OperatorPerformanceCase(void)
{
    if (!m_attribBuffers.empty())
    {
        m_renderCtx.getFunctions().deleteBuffers((glw::GLsizei)m_attribBuffers.size(), &m_attribBuffers[0]);
        m_attribBuffers.clear();
    }
}

static void logRenderTargetInfo(TestLog &log, const tcu::RenderTarget &renderTarget)
{
    log << TestLog::Section("RenderTarget", "Render target") << TestLog::Message << "size: " << renderTarget.getWidth()
        << "x" << renderTarget.getHeight() << TestLog::EndMessage << TestLog::Message << "bits:"
        << " R" << renderTarget.getPixelFormat().redBits << " G" << renderTarget.getPixelFormat().greenBits << " B"
        << renderTarget.getPixelFormat().blueBits << " A" << renderTarget.getPixelFormat().alphaBits << " D"
        << renderTarget.getDepthBits() << " S" << renderTarget.getStencilBits() << TestLog::EndMessage;

    if (renderTarget.getNumSamples() != 0)
        log << TestLog::Message << renderTarget.getNumSamples() << "x MSAA" << TestLog::EndMessage;
    else
        log << TestLog::Message << "No MSAA" << TestLog::EndMessage;

    log << TestLog::EndSection;
}

vector<Vec2> OperatorPerformanceCase::getWorkloadMedianDataPoints(int progNdx) const
{
    const vector<WorkloadRecord> &records = m_workloadRecords[progNdx];
    vector<Vec2> result;

    for (int i = 0; i < (int)records.size(); i++)
        result.push_back(Vec2((float)records[i].workloadSize, records[i].getMedianTime()));

    return result;
}

void OperatorPerformanceCase::prepareProgram(int progNdx)
{
    DE_ASSERT(progNdx < (int)m_programs.size());
    DE_ASSERT(m_programData.size() == m_programs.size());

    const glw::Functions &gl     = m_renderCtx.getFunctions();
    const ShaderProgram &program = *m_programs[progNdx];

    vector<AttribSpec> attributes = m_programData[progNdx].attributes;

    attributes.push_back(AttribSpec("a_position", Vec4(-1.0f, -1.0f, 0.0f, 1.0f), Vec4(1.0f, -1.0f, 0.0f, 1.0f),
                                    Vec4(-1.0f, 1.0f, 0.0f, 1.0f), Vec4(1.0f, 1.0f, 0.0f, 1.0f)));

    DE_ASSERT(program.isOk());

    // Generate vertices.
    if (!m_attribBuffers.empty())
        gl.deleteBuffers((glw::GLsizei)m_attribBuffers.size(), &m_attribBuffers[0]);
    m_attribBuffers.resize(attributes.size(), 0);
    gl.genBuffers((glw::GLsizei)m_attribBuffers.size(), &m_attribBuffers[0]);
    GLU_EXPECT_NO_ERROR(gl.getError(), "glGenBuffers()");

    for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
    {
        std::vector<float> vertices;
        generateVertices(vertices, m_gridSizeX, m_gridSizeY, attributes[attribNdx]);

        gl.bindBuffer(GL_ARRAY_BUFFER, m_attribBuffers[attribNdx]);
        gl.bufferData(GL_ARRAY_BUFFER, (glw::GLsizeiptr)(vertices.size() * sizeof(float)), &vertices[0],
                      GL_STATIC_DRAW);
        GLU_EXPECT_NO_ERROR(gl.getError(), "Upload buffer data");
    }

    // Setup attribute bindings.
    for (int attribNdx = 0; attribNdx < (int)attributes.size(); attribNdx++)
    {
        int location = gl.getAttribLocation(program.getProgram(), attributes[attribNdx].name.c_str());

        if (location >= 0)
        {
            gl.enableVertexAttribArray(location);
            gl.bindBuffer(GL_ARRAY_BUFFER, m_attribBuffers[attribNdx]);
            gl.vertexAttribPointer(location, 4, GL_FLOAT, GL_FALSE, 0, nullptr);
        }
    }
    GLU_EXPECT_NO_ERROR(gl.getError(), "Setup vertex input state");

    gl.useProgram(program.getProgram());
    setGeneralUniforms(program.getProgram());
    gl.viewport(0, 0, m_viewportWidth, m_viewportHeight);
}

void OperatorPerformanceCase::prepareWorkload(int progNdx, int workload)
{
    setWorkloadSizeUniform(m_programs[progNdx]->getProgram(), workload);
    render(m_calibrator.getCallCount());
}

void OperatorPerformanceCase::prepareNextRound(void)
{
    DE_ASSERT(m_state == STATE_CALIBRATING || m_state == STATE_FIND_HIGH_WORKLOAD || m_state == STATE_MEASURING);

    TestLog &log = m_testCtx.getLog();

    if (m_state == STATE_CALIBRATING && m_calibrator.getState() == TheilSenCalibrator::STATE_FINISHED)
    {
        m_measureProgramNdx = 0;
        m_state             = STATE_FIND_HIGH_WORKLOAD;
    }

    if (m_state == STATE_CALIBRATING)
        prepareWorkload(0, 1);
    else if (m_state == STATE_FIND_HIGH_WORKLOAD)
    {
        vector<WorkloadRecord> &records = m_workloadRecordsFindHigh[m_measureProgramNdx];

        if (records.empty() || records.back().getMedianTime() < 2.0f * records[0].getMedianTime())
        {
            int workloadSize;

            if (records.empty())
                workloadSize = 1;
            else
            {
                workloadSize = records.back().workloadSize * 2;

                if (workloadSize > MAX_WORKLOAD_SIZE)
                {
                    log << TestLog::Message << "Even workload size " << records.back().workloadSize
                        << " doesn't give high enough frame time for program " << m_measureProgramNdx
                        << ". Can't get sensible result." << TestLog::EndMessage;
                    MEASUREMENT_FAIL();
                }
            }

            records.push_back(WorkloadRecord(workloadSize));
            prepareWorkload(0, workloadSize);
            m_workloadMeasurementNdx = 0;
        }
        else
        {
            m_highWorkloadSizes[m_measureProgramNdx] = records.back().workloadSize;
            m_measureProgramNdx++;

            if (m_measureProgramNdx >= (int)m_programs.size())
            {
                m_state             = STATE_MEASURING;
                m_workloadNdx       = -1;
                m_measureProgramNdx = 0;
            }

            prepareProgram(m_measureProgramNdx);
            prepareNextRound();
        }
    }
    else
    {
        m_workloadNdx++;

        if (m_workloadNdx < m_numWorkloads)
        {
            DE_ASSERT(m_numWorkloads > 1);
            const int highWorkload = m_highWorkloadSizes[m_measureProgramNdx];
            const int workload     = highWorkload > m_numWorkloads ?
                                         1 + m_workloadNdx * (highWorkload - 1) / (m_numWorkloads - 1) :
                                         1 + m_workloadNdx;

            prepareWorkload(m_measureProgramNdx, workload);

            m_workloadMeasurementNdx = 0;

            m_workloadRecords[m_measureProgramNdx].push_back(WorkloadRecord(workload));
        }
        else
        {
            m_measureProgramNdx++;

            if (m_measureProgramNdx < (int)m_programs.size())
            {
                m_workloadNdx            = -1;
                m_workloadMeasurementNdx = 0;
                prepareProgram(m_measureProgramNdx);
                prepareNextRound();
            }
            else
                m_state = STATE_REPORTING;
        }
    }
}

void OperatorPerformanceCase::init(void)
{
    TestLog &log             = m_testCtx.getLog();
    const glw::Functions &gl = m_renderCtx.getFunctions();

    // Validate that we have sane grid and viewport setup.
    DE_ASSERT(de::inBounds(m_gridSizeX, 1, 256) && de::inBounds(m_gridSizeY, 1, 256));
    TCU_CHECK(de::inRange(m_viewportWidth, 1, m_renderCtx.getRenderTarget().getWidth()) &&
              de::inRange(m_viewportHeight, 1, m_renderCtx.getRenderTarget().getHeight()));

    logRenderTargetInfo(log, m_renderCtx.getRenderTarget());

    log << TestLog::Message << "Using additive blending." << TestLog::EndMessage;
    gl.enable(GL_BLEND);
    gl.blendEquation(GL_FUNC_ADD);
    gl.blendFunc(GL_ONE, GL_ONE);

    // Generate programs.
    DE_ASSERT(m_programs.empty());
    m_programData = generateProgramData();
    DE_ASSERT(!m_programData.empty());

    for (int progNdx = 0; progNdx < (int)m_programData.size(); progNdx++)
    {
        const string &vert = m_programData[progNdx].vertShaderSource;
        const string &frag = m_programData[progNdx].fragShaderSource;

        m_programs.push_back(
            SharedPtr<ShaderProgram>(new ShaderProgram(m_renderCtx, glu::makeVtxFragSources(vert, frag))));

        if (!m_programs.back()->isOk())
        {
            log << *m_programs.back();
            TCU_FAIL("Compile failed");
        }
    }

    // Log all programs.
    for (int progNdx = 0; progNdx < (int)m_programs.size(); progNdx++)
        log << TestLog::Section("Program" + de::toString(progNdx), "Program " + de::toString(progNdx))
            << TestLog::Message << m_programData[progNdx].description << TestLog::EndMessage << *m_programs[progNdx]
            << TestLog::EndSection;

    m_highWorkloadSizes.resize(m_programData.size());
    m_workloadRecordsFindHigh.resize(m_programData.size());
    m_workloadRecords.resize(m_programData.size());

    m_calibrator.clear(
        CalibratorParameters(m_initialCalibrationStorage->initialNumCalls, 10 /* calibrate iteration frames */,
                             2000.0f /* calibrate iteration shortcut threshold (ms) */,
                             16 /* max calibrate iterations */, 1000.0f / 30.0f /* frame time (ms) */,
                             1000.0f / 60.0f /* frame time cap (ms) */, 1000.0f /* target measure duration (ms) */));
    m_state = STATE_CALIBRATING;

    prepareProgram(0);
    prepareNextRound();
}

void OperatorPerformanceCase::deinit(void)
{
    if (!m_attribBuffers.empty())
    {
        m_renderCtx.getFunctions().deleteBuffers((glw::GLsizei)m_attribBuffers.size(), &m_attribBuffers[0]);
        m_attribBuffers.clear();
    }

    m_programs.clear();
}

void OperatorPerformanceCase::render(int numDrawCalls)
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const int numVertices    = getNumVertices(m_gridSizeX, m_gridSizeY);

    for (int callNdx = 0; callNdx < numDrawCalls; callNdx++)
        gl.drawArrays(GL_TRIANGLES, 0, numVertices);

    glu::readPixels(m_renderCtx, 0, 0,
                    tcu::Surface(1, 1).getAccess()); // \note Serves as a more reliable replacement for glFinish().
}

uint64_t OperatorPerformanceCase::renderAndMeasure(int numDrawCalls)
{
    const uint64_t startTime = deGetMicroseconds();
    render(numDrawCalls);
    return deGetMicroseconds() - startTime;
}

void OperatorPerformanceCase::adjustAndLogGridAndViewport(void)
{
    TestLog &log = m_testCtx.getLog();

    // If call count is just 1, and the target frame time still wasn't reached, reduce grid or viewport size.
    if (m_calibrator.getCallCount() == 1)
    {
        const gls::MeasureState &calibratorMeasure = m_calibrator.getMeasureState();
        const float drawCallTime = (float)calibratorMeasure.getTotalTime() / (float)calibratorMeasure.frameTimes.size();
        const float targetDrawCallTime = m_calibrator.getParameters().targetFrameTimeUs;
        const float targetRatio        = targetDrawCallTime / drawCallTime;

        if (targetRatio < 0.95f)
        {
            // Reduce grid or viewport size assuming draw call time scales proportionally.
            if (m_caseType == CASETYPE_VERTEX)
            {
                const float targetRatioSqrt = deFloatSqrt(targetRatio);
                m_gridSizeX                 = (int)(targetRatioSqrt * (float)m_gridSizeX);
                m_gridSizeY                 = (int)(targetRatioSqrt * (float)m_gridSizeY);
                TCU_CHECK_MSG(m_gridSizeX >= 1 && m_gridSizeY >= 1,
                              "Can't decrease grid size enough to achieve low-enough draw times");
                log << TestLog::Message
                    << "Note: triangle grid size reduced from original; it's now smaller than during calibration."
                    << TestLog::EndMessage;
            }
            else
            {
                const float targetRatioSqrt = deFloatSqrt(targetRatio);
                m_viewportWidth             = (int)(targetRatioSqrt * (float)m_viewportWidth);
                m_viewportHeight            = (int)(targetRatioSqrt * (float)m_viewportHeight);
                TCU_CHECK_MSG(m_viewportWidth >= 1 && m_viewportHeight >= 1,
                              "Can't decrease viewport size enough to achieve low-enough draw times");
                log << TestLog::Message
                    << "Note: viewport size reduced from original; it's now smaller than during calibration."
                    << TestLog::EndMessage;
            }
        }
    }

    prepareProgram(0);

    // Log grid and viewport sizes.
    log << TestLog::Message << "Grid size: " << m_gridSizeX << "x" << m_gridSizeY << TestLog::EndMessage;
    log << TestLog::Message << "Viewport: " << m_viewportWidth << "x" << m_viewportHeight << TestLog::EndMessage;
}

OperatorPerformanceCase::IterateResult OperatorPerformanceCase::iterate(void)
{
    const TheilSenCalibrator::State calibratorState = m_calibrator.getState();

    if (calibratorState != TheilSenCalibrator::STATE_FINISHED)
    {
        if (calibratorState == TheilSenCalibrator::STATE_RECOMPUTE_PARAMS)
            m_calibrator.recomputeParameters();
        else if (calibratorState == TheilSenCalibrator::STATE_MEASURE)
            m_calibrator.recordIteration(renderAndMeasure(m_calibrator.getCallCount()));
        else
            DE_ASSERT(false);

        if (m_calibrator.getState() == TheilSenCalibrator::STATE_FINISHED)
        {
            logCalibrationInfo(m_testCtx.getLog(), m_calibrator);
            adjustAndLogGridAndViewport();
            prepareNextRound();
            m_initialCalibrationStorage->initialNumCalls = m_calibrator.getCallCount();
        }
    }
    else if (m_state == STATE_FIND_HIGH_WORKLOAD || m_state == STATE_MEASURING)
    {
        if (m_workloadMeasurementNdx < m_numMeasurementsPerWorkload)
        {
            vector<WorkloadRecord> &records = m_state == STATE_FIND_HIGH_WORKLOAD ?
                                                  m_workloadRecordsFindHigh[m_measureProgramNdx] :
                                                  m_workloadRecords[m_measureProgramNdx];
            records.back().addFrameTime((float)renderAndMeasure(m_calibrator.getCallCount()));
            m_workloadMeasurementNdx++;
        }
        else
            prepareNextRound();
    }
    else
    {
        DE_ASSERT(m_state == STATE_REPORTING);

        TestLog &log            = m_testCtx.getLog();
        const int drawCallCount = m_calibrator.getCallCount();

        {
            // Compute per-program estimators for measurements.
            vector<SegmentedEstimator> estimators;
            for (int progNdx = 0; progNdx < (int)m_programs.size(); progNdx++)
                estimators.push_back(computeSegmentedEstimator(getWorkloadMedianDataPoints(progNdx)));

            // Log measurements and their estimators for all programs.
            for (int progNdx = 0; progNdx < (int)m_programs.size(); progNdx++)
            {
                const SegmentedEstimator &estimator = estimators[progNdx];
                const string progNdxStr             = de::toString(progNdx);
                vector<WorkloadRecord> records      = m_workloadRecords[progNdx];
                std::sort(records.begin(), records.end());

                {
                    const tcu::ScopedLogSection section(log, "Program" + progNdxStr + "Measurements",
                                                        "Measurements for program " + progNdxStr);

                    // Sample list of individual frame times.

                    log << TestLog::SampleList("Program" + progNdxStr + "IndividualFrameTimes",
                                               "Individual frame times")
                        << TestLog::SampleInfo
                        << TestLog::ValueInfo("Workload", "Workload", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
                        << TestLog::ValueInfo("FrameTime", "Frame time", "us", QP_SAMPLE_VALUE_TAG_RESPONSE)
                        << TestLog::EndSampleInfo;

                    for (int i = 0; i < (int)records.size(); i++)
                        for (int j = 0; j < (int)records[i].frameTimes.size(); j++)
                            log << TestLog::Sample << records[i].workloadSize << records[i].frameTimes[j]
                                << TestLog::EndSample;

                    log << TestLog::EndSampleList;

                    // Sample list of median frame times.

                    log << TestLog::SampleList("Program" + progNdxStr + "MedianFrameTimes", "Median frame times")
                        << TestLog::SampleInfo
                        << TestLog::ValueInfo("Workload", "Workload", "", QP_SAMPLE_VALUE_TAG_PREDICTOR)
                        << TestLog::ValueInfo("MedianFrameTime", "Median frame time", "us",
                                              QP_SAMPLE_VALUE_TAG_RESPONSE)
                        << TestLog::EndSampleInfo;

                    for (int i = 0; i < (int)records.size(); i++)
                        log << TestLog::Sample << records[i].workloadSize << records[i].getMedianTime()
                            << TestLog::EndSample;

                    log << TestLog::EndSampleList;

                    log << TestLog::Float("Program" + progNdxStr + "WorkloadCostEstimate", "Workload cost estimate",
                                          "us / workload", QP_KEY_TAG_TIME, estimator.right.coefficient);

                    if (estimator.pivotX > -std::numeric_limits<float>::infinity())
                        log << TestLog::Message << "Note: the data points with x coordinate greater than or equal to "
                            << estimator.pivotX
                            << " seem to form a rising line, and the rest of data points seem to form a "
                               "near-horizontal line"
                            << TestLog::EndMessage << TestLog::Message << "Note: the left line is estimated to be "
                            << lineParamsString(estimator.left) << " and the right line "
                            << lineParamsString(estimator.right) << TestLog::EndMessage;
                    else
                        log << TestLog::Message
                            << "Note: the data seem to form a single line: " << lineParamsString(estimator.right)
                            << TestLog::EndMessage;
                }
            }

            for (int progNdx = 0; progNdx < (int)m_programs.size(); progNdx++)
            {
                if (estimators[progNdx].right.coefficient <= 0.0f)
                {
                    log << TestLog::Message << "Slope of measurements for program " << progNdx
                        << " isn't positive. Can't get sensible result." << TestLog::EndMessage;
                    MEASUREMENT_FAIL();
                }
            }

            // \note For each estimator, .right.coefficient is the increase in draw time (in microseconds) when
            // incrementing shader workload size by 1, when D draw calls are done, with a vertex/fragment count
            // of R.
            //
            // The measurements of any single program can't tell us the final result (time of single operation),
            // so we use computeSingleOperationTime to compute it from multiple programs' measurements in a
            // subclass-defined manner.
            //
            // After that, microseconds per operation can be calculated as singleOperationTime / (D * R).

            {
                vector<float> perProgramSlopes;
                for (int i = 0; i < (int)m_programs.size(); i++)
                    perProgramSlopes.push_back(estimators[i].right.coefficient);

                logSingleOperationCalculationInfo();

                const float maxSlope            = *std::max_element(perProgramSlopes.begin(), perProgramSlopes.end());
                const float usecsPerFramePerOp  = computeSingleOperationTime(perProgramSlopes);
                const int vertexOrFragmentCount = m_caseType == CASETYPE_VERTEX ?
                                                      getNumVertices(m_gridSizeX, m_gridSizeY) :
                                                      m_viewportWidth * m_viewportHeight;
                const double usecsPerDrawCallPerOp = usecsPerFramePerOp / (double)drawCallCount;
                const double usecsPerSingleOp      = usecsPerDrawCallPerOp / (double)vertexOrFragmentCount;
                const double megaOpsPerSecond = (double)(drawCallCount * vertexOrFragmentCount) / usecsPerFramePerOp;
                const int numFreeOps          = de::max(
                    0, (int)deFloatFloor(intersectionX(
                           estimators[0].left, LineParameters(estimators[0].right.offset, usecsPerFramePerOp))));

                log << TestLog::Integer("VertexOrFragmentCount",
                                        "R = " + string(m_caseType == CASETYPE_VERTEX ? "Vertex" : "Fragment") +
                                            " count",
                                        "", QP_KEY_TAG_NONE, vertexOrFragmentCount)

                    << TestLog::Integer("DrawCallsPerFrame", "D = Draw calls per frame", "", QP_KEY_TAG_NONE,
                                        drawCallCount)

                    << TestLog::Integer("VerticesOrFragmentsPerFrame",
                                        "R*D = " + string(m_caseType == CASETYPE_VERTEX ? "Vertices" : "Fragments") +
                                            " per frame",
                                        "", QP_KEY_TAG_NONE, vertexOrFragmentCount * drawCallCount)

                    << TestLog::Float("TimePerFramePerOp",
                                      "Estimated cost of R*D " +
                                          string(m_caseType == CASETYPE_VERTEX ? "vertices" : "fragments") +
                                          " (i.e. one frame) with one shader operation",
                                      "us", QP_KEY_TAG_TIME, (float)usecsPerFramePerOp)

                    << TestLog::Float("TimePerDrawcallPerOp",
                                      "Estimated cost of one draw call with one shader operation", "us",
                                      QP_KEY_TAG_TIME, (float)usecsPerDrawCallPerOp)

                    << TestLog::Float("TimePerSingleOp", "Estimated cost of a single shader operation", "us",
                                      QP_KEY_TAG_TIME, (float)usecsPerSingleOp);

                // \note Sometimes, when the operation is free or very cheap, it can happen that the shader with the operation runs,
                //         for some reason, a bit faster than the shader without the operation, and thus we get a negative result. The
                //         following threshold values for accepting a negative or almost-zero result are rather quick and dirty.
                if (usecsPerFramePerOp <= -0.1f * maxSlope)
                {
                    log << TestLog::Message << "Got strongly negative result." << TestLog::EndMessage;
                    MEASUREMENT_FAIL();
                }
                else if (usecsPerFramePerOp <= 0.001 * maxSlope)
                {
                    log << TestLog::Message << "Cost of operation seems to be approximately zero."
                        << TestLog::EndMessage;
                    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, "Pass");
                }
                else
                {
                    log << TestLog::Float("OpsPerSecond", "Operations per second", "Million/s", QP_KEY_TAG_PERFORMANCE,
                                          (float)megaOpsPerSecond)

                        << TestLog::Integer("NumFreeOps", "Estimated number of \"free\" operations", "",
                                            QP_KEY_TAG_PERFORMANCE, numFreeOps);

                    m_testCtx.setTestResult(QP_TEST_RESULT_PASS, de::floatToString((float)megaOpsPerSecond, 2).c_str());
                }

                m_state = STATE_FINISHED;
            }
        }

        return STOP;
    }

    return CONTINUE;
}

// Binary operator case.
class BinaryOpCase : public OperatorPerformanceCase
{
public:
    BinaryOpCase(Context &context, const char *name, const char *description, const char *op, glu::DataType type,
                 glu::Precision precision, bool useSwizzle, bool isVertex,
                 const InitialCalibrationStorage &initialCalibration);

protected:
    vector<ProgramContext> generateProgramData(void) const;
    void setGeneralUniforms(uint32_t program) const;
    void setWorkloadSizeUniform(uint32_t program, int numOperations) const;
    float computeSingleOperationTime(const vector<float> &perProgramOperationCosts) const;
    void logSingleOperationCalculationInfo(void) const;

private:
    enum ProgramID
    {
        // \note 0-based sequential numbering is relevant, because these are also used as vector indices.
        // \note The first program should be the heaviest, because OperatorPerformanceCase uses it to reduce grid/viewport size when going too slow.
        PROGRAM_WITH_BIGGER_LOOP = 0,
        PROGRAM_WITH_SMALLER_LOOP,

        PROGRAM_LAST
    };

    ProgramContext generateSingleProgramData(ProgramID) const;

    const string m_op;
    const glu::DataType m_type;
    const glu::Precision m_precision;
    const bool m_useSwizzle;
};

BinaryOpCase::BinaryOpCase(Context &context, const char *name, const char *description, const char *op,
                           glu::DataType type, glu::Precision precision, bool useSwizzle, bool isVertex,
                           const InitialCalibrationStorage &initialCalibration)
    : OperatorPerformanceCase(context.getTestContext(), context.getRenderContext(), name, description,
                              isVertex ? CASETYPE_VERTEX : CASETYPE_FRAGMENT, NUM_WORKLOADS, initialCalibration)
    , m_op(op)
    , m_type(type)
    , m_precision(precision)
    , m_useSwizzle(useSwizzle)
{
}

BinaryOpCase::ProgramContext BinaryOpCase::generateSingleProgramData(ProgramID programID) const
{
    DE_ASSERT(glu::isDataTypeFloatOrVec(m_type) || glu::isDataTypeIntOrIVec(m_type));

    const bool isVertexCase     = m_caseType == CASETYPE_VERTEX;
    const char *const precision = glu::getPrecisionName(m_precision);
    const char *const inputPrecision =
        glu::isDataTypeIntOrIVec(m_type) && m_precision == glu::PRECISION_LOWP ? "mediump" : precision;
    const char *const typeName = getDataTypeName(m_type);

    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n";
    frag << "#version 300 es\n"
         << "layout (location = 0) out mediump vec4 o_color;\n";

    // Attributes.
    vtx << "in highp vec4 a_position;\n";
    for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS + 1; i++)
        vtx << "in " << inputPrecision << " vec4 a_in" << i << ";\n";

    if (isVertexCase)
    {
        vtx << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }
    else
    {
        for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS + 1; i++)
        {
            vtx << "out " << inputPrecision << " vec4 v_in" << i << ";\n";
            frag << "in " << inputPrecision << " vec4 v_in" << i << ";\n";
        }
    }

    op << "uniform mediump int u_numLoopIterations;\n";
    if (isVertexCase)
        op << "uniform mediump float u_zero;\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";

    if (!isVertexCase)
        vtx << "\tgl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Expression inputs.
    const char *const prefix = isVertexCase ? "a_" : "v_";
    for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS + 1; i++)
    {
        const int inSize = getDataTypeScalarSize(m_type);
        const bool isInt = de::inRange<int>(m_type, TYPE_INT, TYPE_INT_VEC4);
        const bool cast  = isInt || (!m_useSwizzle && m_type != TYPE_FLOAT_VEC4);

        op << "\t" << precision << " " << typeName << " in" << i << " = ";

        if (cast)
            op << typeName << "(";

        op << prefix << "in" << i;

        if (m_useSwizzle)
            op << "." << s_swizzles[i % DE_LENGTH_OF_ARRAY(s_swizzles)][inSize - 1];

        if (cast)
            op << ")";

        op << ";\n";
    }

    // Operation accumulation variables.
    for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS; i++)
    {
        op << "\t" << precision << " " << typeName << " acc" << i << "a"
           << " = in" << i + 0 << ";\n";
        op << "\t" << precision << " " << typeName << " acc" << i << "b"
           << " = in" << i + 1 << ";\n";
    }

    // Loop, with expressions in it.
    op << "\tfor (int i = 0; i < u_numLoopIterations; i++)\n";
    op << "\t{\n";
    {
        const int unrollAmount = programID == PROGRAM_WITH_SMALLER_LOOP ?
                                     BINARY_OPERATOR_CASE_SMALL_PROGRAM_UNROLL_AMOUNT :
                                     BINARY_OPERATOR_CASE_BIG_PROGRAM_UNROLL_AMOUNT;
        for (int unrollNdx = 0; unrollNdx < unrollAmount; unrollNdx++)
        {
            for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS; i++)
            {
                if (i > 0 || unrollNdx > 0)
                    op << "\n";
                op << "\t\tacc" << i << "a = acc" << i << "b " << m_op << " acc" << i << "a"
                   << ";\n";
                op << "\t\tacc" << i << "b = acc" << i << "a " << m_op << " acc" << i << "b"
                   << ";\n";
            }
        }
    }
    op << "\t}\n";
    op << "\n";

    // Result variable (sum of accumulation variables).
    op << "\t" << precision << " " << typeName << " res =";
    for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS; i++)
        op << (i > 0 ? " " + m_op : "") << " acc" << i << "b";
    op << ";\n";

    // Convert to color.
    op << "\tmediump vec4 color = ";
    if (m_type == TYPE_FLOAT_VEC4)
        op << "res";
    else
    {
        int size = getDataTypeScalarSize(m_type);
        op << "vec4(res";

        for (int i = size; i < 4; i++)
            op << ", " << (i == 3 ? "1.0" : "0.0");

        op << ")";
    }
    op << ";\n";
    op << "\t" << (isVertexCase ? "v_color" : "o_color") << " = color;\n";

    if (isVertexCase)
    {
        vtx << "    gl_Position = a_position + u_zero*color;\n";
        frag << "    o_color = v_color;\n";
    }
    else
    {
        for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS + 1; i++)
            vtx << "    v_in" << i << " = a_in" << i << ";\n";
    }

    vtx << "}\n";
    frag << "}\n";

    {
        vector<AttribSpec> attributes;
        for (int i = 0; i < BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS + 1; i++)
            attributes.push_back(
                AttribSpec(("a_in" + de::toString(i)).c_str(),
                           Vec4(2.0f, 2.0f, 2.0f, 1.0f).swizzle((i + 0) % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4),
                           Vec4(1.0f, 2.0f, 1.0f, 2.0f).swizzle((i + 0) % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4),
                           Vec4(2.0f, 1.0f, 2.0f, 2.0f).swizzle((i + 0) % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4),
                           Vec4(1.0f, 1.0f, 2.0f, 1.0f).swizzle((i + 0) % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4)));

        {
            string description = "This is the program with the ";

            description += programID == PROGRAM_WITH_SMALLER_LOOP ? "smaller" :
                           programID == PROGRAM_WITH_BIGGER_LOOP  ? "bigger" :
                                                                    nullptr;

            description += " loop.\n"
                           "Note: workload size for this program means the number of loop iterations.";

            return ProgramContext(vtx.str(), frag.str(), attributes, description);
        }
    }
}

vector<BinaryOpCase::ProgramContext> BinaryOpCase::generateProgramData(void) const
{
    vector<ProgramContext> progData;
    for (int i = 0; i < PROGRAM_LAST; i++)
        progData.push_back(generateSingleProgramData((ProgramID)i));
    return progData;
}

void BinaryOpCase::setGeneralUniforms(uint32_t program) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    gl.uniform1f(gl.getUniformLocation(program, "u_zero"), 0.0f);
}

void BinaryOpCase::setWorkloadSizeUniform(uint32_t program, int numLoopIterations) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    gl.uniform1i(gl.getUniformLocation(program, "u_numLoopIterations"), numLoopIterations);
}

float BinaryOpCase::computeSingleOperationTime(const vector<float> &perProgramOperationCosts) const
{
    DE_ASSERT(perProgramOperationCosts.size() == PROGRAM_LAST);

    const int baseNumOpsInsideLoop           = 2 * BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS;
    const int numOpsInsideLoopInSmallProgram = baseNumOpsInsideLoop * BINARY_OPERATOR_CASE_SMALL_PROGRAM_UNROLL_AMOUNT;
    const int numOpsInsideLoopInBigProgram   = baseNumOpsInsideLoop * BINARY_OPERATOR_CASE_BIG_PROGRAM_UNROLL_AMOUNT;
    DE_STATIC_ASSERT(numOpsInsideLoopInBigProgram > numOpsInsideLoopInSmallProgram);
    const int opDiff = numOpsInsideLoopInBigProgram - numOpsInsideLoopInSmallProgram;
    const float programOperationCostDiff =
        perProgramOperationCosts[PROGRAM_WITH_BIGGER_LOOP] - perProgramOperationCosts[PROGRAM_WITH_SMALLER_LOOP];

    return programOperationCostDiff / (float)opDiff;
}

void BinaryOpCase::logSingleOperationCalculationInfo(void) const
{
    const int baseNumOpsInsideLoop           = 2 * BINARY_OPERATOR_CASE_NUM_INDEPENDENT_CALCULATIONS;
    const int numOpsInsideLoopInSmallProgram = baseNumOpsInsideLoop * BINARY_OPERATOR_CASE_SMALL_PROGRAM_UNROLL_AMOUNT;
    const int numOpsInsideLoopInBigProgram   = baseNumOpsInsideLoop * BINARY_OPERATOR_CASE_BIG_PROGRAM_UNROLL_AMOUNT;
    const int opDiff                         = numOpsInsideLoopInBigProgram - numOpsInsideLoopInSmallProgram;
    const char *const opName                 = m_op == "+" ? "addition" :
                                               m_op == "-" ? "subtraction" :
                                               m_op == "*" ? "multiplication" :
                                               m_op == "/" ? "division" :
                                                             nullptr;
    DE_ASSERT(opName != nullptr);

    m_testCtx.getLog()
        << TestLog::Message << "Note: the bigger program contains " << opDiff << " more " << opName
        << " operations in one loop iteration than the small program; "
        << "cost of one operation is calculated as (cost_of_bigger_workload - cost_of_smaller_workload) / " << opDiff
        << TestLog::EndMessage;
}

// Built-in function case.
class FunctionCase : public OperatorPerformanceCase
{
public:
    enum
    {
        MAX_PARAMS = 3
    };

    FunctionCase(
        Context &context, const char *name, const char *description, const char *func, glu::DataType returnType,
        const glu::DataType paramTypes[MAX_PARAMS], const Vec4 &attribute,
        int modifyParamNdx, //!< Add a compile-time constant (2.0) to the parameter at this index. This is ignored if negative.
        bool useNearlyConstantINputs, //!< Function inputs shouldn't be much bigger than 'attribute'.
        glu::Precision precision, bool isVertex, const InitialCalibrationStorage &initialCalibration);

protected:
    vector<ProgramContext> generateProgramData(void) const;
    void setGeneralUniforms(uint32_t program) const;
    void setWorkloadSizeUniform(uint32_t program, int numOperations) const;
    float computeSingleOperationTime(const vector<float> &perProgramOperationCosts) const;
    void logSingleOperationCalculationInfo(void) const;

private:
    enum ProgramID
    {
        // \note 0-based sequential numbering is relevant, because these are also used as vector indices.
        // \note The first program should be the heaviest, because OperatorPerformanceCase uses it to reduce grid/viewport size when going too slow.
        PROGRAM_WITH_FUNCTION_CALLS = 0,
        PROGRAM_WITHOUT_FUNCTION_CALLS,

        PROGRAM_LAST
    };

    //! Forms a "sum" expression from aExpr and bExpr; for booleans, this is "equal(a,b)", otherwise actual sum.
    static string sumExpr(const string &aExpr, const string &bExpr, glu::DataType type);
    //! Forms an expression used to increment an input value in the shader. If type is boolean, this is just
    //! baseExpr; otherwise, baseExpr is modified by multiplication or division by a loop index,
    //! to prevent simple compiler optimizations. See m_useNearlyConstantInputs for more explanation.
    static string incrementExpr(const string &baseExpr, glu::DataType type, bool divide);

    ProgramContext generateSingleProgramData(ProgramID) const;

    const string m_func;
    const glu::DataType m_returnType;
    glu::DataType m_paramTypes[MAX_PARAMS];
    // \note m_modifyParamNdx, if not negative, specifies the index of the parameter to which a
    //         compile-time constant (2.0) is added. This is a quick and dirty way to deal with
    //         functions like clamp or smoothstep that require that a certain parameter is
    //         greater than a certain other parameter.
    const int m_modifyParamNdx;
    // \note m_useNearlyConstantInputs determines whether the inputs given to the function
    //         should increase (w.r.t m_attribute) only by very small amounts. This is relevant
    //         for functions like asin, which requires its inputs to be in a specific range.
    //         In practice, this affects whether expressions used to increment the input
    //         variables use division instead of multiplication; normally, multiplication is used,
    //         but it's hard to keep the increments very small that way, and division shouldn't
    //         be the default, since for many functions (probably not asin, luckily), division
    //         is too heavy and dominates time-wise.
    const bool m_useNearlyConstantInputs;
    const Vec4 m_attribute;
    const glu::Precision m_precision;
};

FunctionCase::FunctionCase(Context &context, const char *name, const char *description, const char *func,
                           glu::DataType returnType, const glu::DataType paramTypes[MAX_PARAMS], const Vec4 &attribute,
                           int modifyParamNdx, bool useNearlyConstantInputs, glu::Precision precision, bool isVertex,
                           const InitialCalibrationStorage &initialCalibration)
    : OperatorPerformanceCase(context.getTestContext(), context.getRenderContext(), name, description,
                              isVertex ? CASETYPE_VERTEX : CASETYPE_FRAGMENT, NUM_WORKLOADS, initialCalibration)
    , m_func(func)
    , m_returnType(returnType)
    , m_modifyParamNdx(modifyParamNdx)
    , m_useNearlyConstantInputs(useNearlyConstantInputs)
    , m_attribute(attribute)
    , m_precision(precision)
{
    for (int i = 0; i < MAX_PARAMS; i++)
        m_paramTypes[i] = paramTypes[i];
}

string FunctionCase::sumExpr(const string &aExpr, const string &bExpr, glu::DataType type)
{
    if (glu::isDataTypeBoolOrBVec(type))
    {
        if (type == glu::TYPE_BOOL)
            return "(" + aExpr + " == " + bExpr + ")";
        else
            return "equal(" + aExpr + ", " + bExpr + ")";
    }
    else
        return "(" + aExpr + " + " + bExpr + ")";
}

string FunctionCase::incrementExpr(const string &baseExpr, glu::DataType type, bool divide)
{
    const string mulOrDiv = divide ? "/" : "*";

    return glu::isDataTypeBoolOrBVec(type) ? baseExpr :
           glu::isDataTypeIntOrIVec(type)  ? "(" + baseExpr + mulOrDiv + "(i+1))" :
                                             "(" + baseExpr + mulOrDiv + "float(i+1))";
}

FunctionCase::ProgramContext FunctionCase::generateSingleProgramData(ProgramID programID) const
{
    const bool isVertexCase           = m_caseType == CASETYPE_VERTEX;
    const char *const precision       = glu::getPrecisionName(m_precision);
    const char *const returnTypeName  = getDataTypeName(m_returnType);
    const string returnPrecisionMaybe = glu::isDataTypeBoolOrBVec(m_returnType) ? "" : string() + precision + " ";
    const char *inputPrecision        = nullptr;
    const bool isMatrixReturn         = isDataTypeMatrix(m_returnType);
    int numParams                     = 0;
    const char *paramTypeNames[MAX_PARAMS];
    string paramPrecisionsMaybe[MAX_PARAMS];

    for (int i = 0; i < MAX_PARAMS; i++)
    {
        paramTypeNames[i]       = getDataTypeName(m_paramTypes[i]);
        paramPrecisionsMaybe[i] = glu::isDataTypeBoolOrBVec(m_paramTypes[i]) ? "" : string() + precision + " ";

        if (inputPrecision == nullptr && isDataTypeIntOrIVec(m_paramTypes[i]) && m_precision == glu::PRECISION_LOWP)
            inputPrecision = "mediump";

        if (m_paramTypes[i] != TYPE_INVALID)
            numParams = i + 1;
    }

    DE_ASSERT(numParams > 0);

    if (inputPrecision == nullptr)
        inputPrecision = precision;

    int numAttributes = FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS + numParams - 1;
    std::ostringstream vtx;
    std::ostringstream frag;
    std::ostringstream &op = isVertexCase ? vtx : frag;

    vtx << "#version 300 es\n";
    frag << "#version 300 es\n"
         << "layout (location = 0) out mediump vec4 o_color;\n";

    // Attributes.
    vtx << "in highp vec4 a_position;\n";
    for (int i = 0; i < numAttributes; i++)
        vtx << "in " << inputPrecision << " vec4 a_in" << i << ";\n";

    if (isVertexCase)
    {
        vtx << "out mediump vec4 v_color;\n";
        frag << "in mediump vec4 v_color;\n";
    }
    else
    {
        for (int i = 0; i < numAttributes; i++)
        {
            vtx << "out " << inputPrecision << " vec4 v_in" << i << ";\n";
            frag << "in " << inputPrecision << " vec4 v_in" << i << ";\n";
        }
    }

    op << "uniform mediump int u_numLoopIterations;\n";
    if (isVertexCase)
        op << "uniform mediump float u_zero;\n";

    for (int paramNdx = 0; paramNdx < numParams; paramNdx++)
        op << "uniform " << paramPrecisionsMaybe[paramNdx] << paramTypeNames[paramNdx] << " u_inc"
           << (char)('A' + paramNdx) << ";\n";

    vtx << "\n";
    vtx << "void main()\n";
    vtx << "{\n";

    if (!isVertexCase)
        vtx << "\tgl_Position = a_position;\n";

    frag << "\n";
    frag << "void main()\n";
    frag << "{\n";

    // Function call input and return value accumulation variables.
    {
        const char *const inPrefix = isVertexCase ? "a_" : "v_";

        for (int calcNdx = 0; calcNdx < FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS; calcNdx++)
        {
            for (int paramNdx = 0; paramNdx < numParams; paramNdx++)
            {
                const glu::DataType paramType = m_paramTypes[paramNdx];
                const bool mustCast           = paramType != glu::TYPE_FLOAT_VEC4;

                op << "\t" << paramPrecisionsMaybe[paramNdx] << paramTypeNames[paramNdx] << " in" << calcNdx
                   << (char)('a' + paramNdx) << " = ";

                if (mustCast)
                    op << paramTypeNames[paramNdx] << "(";

                if (glu::isDataTypeMatrix(paramType))
                {
                    static const char *const swizzles[3] = {"x", "xy", "xyz"};
                    const int numRows                    = glu::getDataTypeMatrixNumRows(paramType);
                    const int numCols                    = glu::getDataTypeMatrixNumColumns(paramType);
                    const string swizzle                 = numRows < 4 ? string() + "." + swizzles[numRows - 1] : "";

                    for (int i = 0; i < numCols; i++)
                        op << (i > 0 ? ", " : "") << inPrefix << "in" << calcNdx + paramNdx << swizzle;
                }
                else
                {
                    op << inPrefix << "in" << calcNdx + paramNdx;

                    if (paramNdx == m_modifyParamNdx)
                    {
                        DE_ASSERT(glu::isDataTypeFloatOrVec(paramType));
                        op << " + 2.0";
                    }
                }

                if (mustCast)
                    op << ")";

                op << ";\n";
            }

            op << "\t" << returnPrecisionMaybe << returnTypeName << " res" << calcNdx << " = " << returnTypeName
               << "(0);\n";
        }
    }

    // Loop with expressions in it.
    op << "\tfor (int i = 0; i < u_numLoopIterations; i++)\n";
    op << "\t{\n";
    for (int calcNdx = 0; calcNdx < FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS; calcNdx++)
    {
        if (calcNdx > 0)
            op << "\n";

        op << "\t\t{\n";

        for (int inputNdx = 0; inputNdx < numParams; inputNdx++)
        {
            const string inputName = "in" + de::toString(calcNdx) + (char)('a' + inputNdx);
            const string incName   = string() + "u_inc" + (char)('A' + inputNdx);
            const string incExpr   = incrementExpr(incName, m_paramTypes[inputNdx], m_useNearlyConstantInputs);

            op << "\t\t\t" << inputName << " = " << sumExpr(inputName, incExpr, m_paramTypes[inputNdx]) << ";\n";
        }

        op << "\t\t\t" << returnPrecisionMaybe << returnTypeName << " eval" << calcNdx << " = ";

        if (programID == PROGRAM_WITH_FUNCTION_CALLS)
        {
            op << m_func << "(";

            for (int paramNdx = 0; paramNdx < numParams; paramNdx++)
            {
                if (paramNdx > 0)
                    op << ", ";

                op << "in" << calcNdx << (char)('a' + paramNdx);
            }

            op << ")";
        }
        else
        {
            DE_ASSERT(programID == PROGRAM_WITHOUT_FUNCTION_CALLS);
            op << returnTypeName << "(1)";
        }

        op << ";\n";

        {
            const string resName  = "res" + de::toString(calcNdx);
            const string evalName = "eval" + de::toString(calcNdx);
            const string incExpr  = incrementExpr(evalName, m_returnType, m_useNearlyConstantInputs);

            op << "\t\t\tres" << calcNdx << " = " << sumExpr(resName, incExpr, m_returnType) << ";\n";
        }

        op << "\t\t}\n";
    }
    op << "\t}\n";
    op << "\n";

    // Result variables.
    for (int inputNdx = 0; inputNdx < numParams; inputNdx++)
    {
        op << "\t" << paramPrecisionsMaybe[inputNdx] << paramTypeNames[inputNdx] << " sumIn" << (char)('A' + inputNdx)
           << " = ";
        {
            string expr = string() + "in0" + (char)('a' + inputNdx);
            for (int i = 1; i < FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS; i++)
                expr =
                    sumExpr(expr, string() + "in" + de::toString(i) + (char)('a' + inputNdx), m_paramTypes[inputNdx]);
            op << expr;
        }
        op << ";\n";
    }

    op << "\t" << returnPrecisionMaybe << returnTypeName << " sumRes = ";
    {
        string expr = "res0";
        for (int i = 1; i < FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS; i++)
            expr = sumExpr(expr, "res" + de::toString(i), m_returnType);
        op << expr;
    }
    op << ";\n";

    {
        glu::DataType finalResultDataType = glu::TYPE_LAST;

        if (glu::isDataTypeMatrix(m_returnType))
        {
            finalResultDataType = m_returnType;

            op << "\t" << precision << " " << returnTypeName << " finalRes = ";

            for (int inputNdx = 0; inputNdx < numParams; inputNdx++)
            {
                DE_ASSERT(m_paramTypes[inputNdx] == m_returnType);
                op << "sumIn" << (char)('A' + inputNdx) << " + ";
            }
            op << "sumRes;\n";
        }
        else
        {
            int numFinalResComponents = glu::getDataTypeScalarSize(m_returnType);
            for (int inputNdx = 0; inputNdx < numParams; inputNdx++)
                numFinalResComponents =
                    de::max(numFinalResComponents, glu::getDataTypeScalarSize(m_paramTypes[inputNdx]));

            finalResultDataType = getDataTypeFloatOrVec(numFinalResComponents);

            {
                const string finalResType = glu::getDataTypeName(finalResultDataType);
                op << "\t" << precision << " " << finalResType << " finalRes = ";
                for (int inputNdx = 0; inputNdx < numParams; inputNdx++)
                    op << finalResType << "(sumIn" << (char)('A' + inputNdx) << ") + ";
                op << finalResType << "(sumRes);\n";
            }
        }

        // Convert to color.
        op << "\tmediump vec4 color = ";
        if (finalResultDataType == TYPE_FLOAT_VEC4)
            op << "finalRes";
        else
        {
            int size = isMatrixReturn ? getDataTypeMatrixNumRows(finalResultDataType) :
                                        getDataTypeScalarSize(finalResultDataType);

            op << "vec4(";

            if (isMatrixReturn)
            {
                for (int i = 0; i < getDataTypeMatrixNumColumns(finalResultDataType); i++)
                {
                    if (i > 0)
                        op << " + ";
                    op << "finalRes[" << i << "]";
                }
            }
            else
                op << "finalRes";

            for (int i = size; i < 4; i++)
                op << ", " << (i == 3 ? "1.0" : "0.0");

            op << ")";
        }
        op << ";\n";
        op << "\t" << (isVertexCase ? "v_color" : "o_color") << " = color;\n";

        if (isVertexCase)
        {
            vtx << "    gl_Position = a_position + u_zero*color;\n";
            frag << "    o_color = v_color;\n";
        }
        else
        {
            for (int i = 0; i < numAttributes; i++)
                vtx << "    v_in" << i << " = a_in" << i << ";\n";
        }

        vtx << "}\n";
        frag << "}\n";
    }

    {
        vector<AttribSpec> attributes;
        for (int i = 0; i < numAttributes; i++)
            attributes.push_back(AttribSpec(("a_in" + de::toString(i)).c_str(),
                                            m_attribute.swizzle((i + 0) % 4, (i + 1) % 4, (i + 2) % 4, (i + 3) % 4),
                                            m_attribute.swizzle((i + 1) % 4, (i + 2) % 4, (i + 3) % 4, (i + 0) % 4),
                                            m_attribute.swizzle((i + 2) % 4, (i + 3) % 4, (i + 0) % 4, (i + 1) % 4),
                                            m_attribute.swizzle((i + 3) % 4, (i + 0) % 4, (i + 1) % 4, (i + 2) % 4)));

        {
            string description = "This is the program ";

            description += programID == PROGRAM_WITHOUT_FUNCTION_CALLS ? "without" :
                           programID == PROGRAM_WITH_FUNCTION_CALLS    ? "with" :
                                                                         nullptr;

            description += " '" + m_func +
                           "' function calls.\n"
                           "Note: workload size for this program means the number of loop iterations.";

            return ProgramContext(vtx.str(), frag.str(), attributes, description);
        }
    }
}

vector<FunctionCase::ProgramContext> FunctionCase::generateProgramData(void) const
{
    vector<ProgramContext> progData;
    for (int i = 0; i < PROGRAM_LAST; i++)
        progData.push_back(generateSingleProgramData((ProgramID)i));
    return progData;
}

void FunctionCase::setGeneralUniforms(uint32_t program) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();

    gl.uniform1f(gl.getUniformLocation(program, "u_zero"), 0.0f);

    for (int paramNdx = 0; paramNdx < MAX_PARAMS; paramNdx++)
    {
        if (m_paramTypes[paramNdx] != glu::TYPE_INVALID)
        {
            const glu::DataType paramType = m_paramTypes[paramNdx];
            const int scalarSize          = glu::getDataTypeScalarSize(paramType);
            const int location = gl.getUniformLocation(program, (string() + "u_inc" + (char)('A' + paramNdx)).c_str());

            if (glu::isDataTypeFloatOrVec(paramType))
            {
                float values[4];
                for (int i = 0; i < DE_LENGTH_OF_ARRAY(values); i++)
                    values[i] = (float)paramNdx * 0.01f + (float)i * 0.001f; // Arbitrary small values.
                uniformNfv(gl, scalarSize, location, 1, &values[0]);
            }
            else if (glu::isDataTypeIntOrIVec(paramType))
            {
                int values[4];
                for (int i = 0; i < DE_LENGTH_OF_ARRAY(values); i++)
                    values[i] = paramNdx * 100 + i; // Arbitrary values.
                uniformNiv(gl, scalarSize, location, 1, &values[0]);
            }
            else if (glu::isDataTypeBoolOrBVec(paramType))
            {
                int values[4];
                for (int i = 0; i < DE_LENGTH_OF_ARRAY(values); i++)
                    values[i] = (paramNdx >> i) & 1; // Arbitrary values.
                uniformNiv(gl, scalarSize, location, 1, &values[0]);
            }
            else if (glu::isDataTypeMatrix(paramType))
            {
                const int size = glu::getDataTypeMatrixNumRows(paramType);
                DE_ASSERT(size == glu::getDataTypeMatrixNumColumns(paramType));
                float values[4 * 4];
                for (int i = 0; i < DE_LENGTH_OF_ARRAY(values); i++)
                    values[i] = (float)paramNdx * 0.01f + (float)i * 0.001f; // Arbitrary values.
                uniformMatrixNfv(gl, size, location, 1, &values[0]);
            }
            else
                DE_ASSERT(false);
        }
    }
}

void FunctionCase::setWorkloadSizeUniform(uint32_t program, int numLoopIterations) const
{
    const glw::Functions &gl = m_renderCtx.getFunctions();
    const int loc            = gl.getUniformLocation(program, "u_numLoopIterations");

    gl.uniform1i(loc, numLoopIterations);
}

float FunctionCase::computeSingleOperationTime(const vector<float> &perProgramOperationCosts) const
{
    DE_ASSERT(perProgramOperationCosts.size() == PROGRAM_LAST);
    const int numFunctionCalls           = FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS;
    const float programOperationCostDiff = perProgramOperationCosts[PROGRAM_WITH_FUNCTION_CALLS] -
                                           perProgramOperationCosts[PROGRAM_WITHOUT_FUNCTION_CALLS];

    return programOperationCostDiff / (float)numFunctionCalls;
}

void FunctionCase::logSingleOperationCalculationInfo(void) const
{
    const int numFunctionCalls = FUNCTION_CASE_NUM_INDEPENDENT_CALCULATIONS;

    m_testCtx.getLog() << TestLog::Message << "Note: program " << (int)PROGRAM_WITH_FUNCTION_CALLS << " contains "
                       << numFunctionCalls << " calls to '" << m_func << "' in one loop iteration; "
                       << "cost of one operation is calculated as "
                       << "(cost_of_workload_with_calls - cost_of_workload_without_calls) / " << numFunctionCalls
                       << TestLog::EndMessage;
}

} // namespace

ShaderOperatorTests::ShaderOperatorTests(Context &context)
    : TestCaseGroup(context, "operator", "Operator Performance Tests")
{
}

ShaderOperatorTests::~ShaderOperatorTests(void)
{
}

void ShaderOperatorTests::init(void)
{
    // Binary operator cases

    static const DataType binaryOpTypes[] = {
        TYPE_FLOAT, TYPE_FLOAT_VEC2, TYPE_FLOAT_VEC3, TYPE_FLOAT_VEC4,
        TYPE_INT,   TYPE_INT_VEC2,   TYPE_INT_VEC3,   TYPE_INT_VEC4,
    };
    static const Precision precisions[] = {PRECISION_LOWP, PRECISION_MEDIUMP, PRECISION_HIGHP};
    static const struct
    {
        const char *name;
        const char *op;
        bool swizzle;
    } binaryOps[] = {{"add", "+", false}, {"sub", "-", true}, {"mul", "*", false}, {"div", "/", true}};

    tcu::TestCaseGroup *const binaryOpsGroup =
        new tcu::TestCaseGroup(m_testCtx, "binary_operator", "Binary Operator Performance Tests");
    addChild(binaryOpsGroup);

    for (int opNdx = 0; opNdx < DE_LENGTH_OF_ARRAY(binaryOps); opNdx++)
    {
        tcu::TestCaseGroup *const opGroup = new tcu::TestCaseGroup(m_testCtx, binaryOps[opNdx].name, "");
        binaryOpsGroup->addChild(opGroup);

        for (int isFrag = 0; isFrag <= 1; isFrag++)
        {
            const BinaryOpCase::InitialCalibrationStorage shaderGroupCalibrationStorage(
                new BinaryOpCase::InitialCalibration);
            const bool isVertex = isFrag == 0;
            tcu::TestCaseGroup *const shaderGroup =
                new tcu::TestCaseGroup(m_testCtx, isVertex ? "vertex" : "fragment", "");
            opGroup->addChild(shaderGroup);

            for (int typeNdx = 0; typeNdx < DE_LENGTH_OF_ARRAY(binaryOpTypes); typeNdx++)
            {
                for (int precNdx = 0; precNdx < DE_LENGTH_OF_ARRAY(precisions); precNdx++)
                {
                    const DataType type       = binaryOpTypes[typeNdx];
                    const Precision precision = precisions[precNdx];
                    const char *const op      = binaryOps[opNdx].op;
                    const bool useSwizzle     = binaryOps[opNdx].swizzle;
                    std::ostringstream name;

                    name << getPrecisionName(precision) << "_" << getDataTypeName(type);

                    shaderGroup->addChild(new BinaryOpCase(m_context, name.str().c_str(), "", op, type, precision,
                                                           useSwizzle, isVertex, shaderGroupCalibrationStorage));
                }
            }
        }
    }

    // Built-in function cases.

    // Non-specific (i.e. includes gentypes) parameter types for the functions.
    enum ValueType
    {
        VALUE_NONE          = 0,
        VALUE_FLOAT         = (1 << 0),  // float scalar
        VALUE_FLOAT_VEC     = (1 << 1),  // float vector
        VALUE_FLOAT_VEC34   = (1 << 2),  // float vector of size 3 or 4
        VALUE_FLOAT_GENTYPE = (1 << 3),  // float scalar/vector
        VALUE_VEC3          = (1 << 4),  // vec3 only
        VALUE_VEC4          = (1 << 5),  // vec4 only
        VALUE_MATRIX        = (1 << 6),  // matrix
        VALUE_BOOL          = (1 << 7),  // boolean scalar
        VALUE_BOOL_VEC      = (1 << 8),  // boolean vector
        VALUE_BOOL_VEC4     = (1 << 9),  // bvec4 only
        VALUE_BOOL_GENTYPE  = (1 << 10), // boolean scalar/vector
        VALUE_INT           = (1 << 11), // int scalar
        VALUE_INT_VEC       = (1 << 12), // int vector
        VALUE_INT_VEC4      = (1 << 13), // ivec4 only
        VALUE_INT_GENTYPE   = (1 << 14), // int scalar/vector

        // Shorthands.
        N   = VALUE_NONE,
        F   = VALUE_FLOAT,
        FV  = VALUE_FLOAT_VEC,
        VL  = VALUE_FLOAT_VEC34, // L for "large"
        GT  = VALUE_FLOAT_GENTYPE,
        V3  = VALUE_VEC3,
        V4  = VALUE_VEC4,
        M   = VALUE_MATRIX,
        B   = VALUE_BOOL,
        BV  = VALUE_BOOL_VEC,
        B4  = VALUE_BOOL_VEC4,
        BGT = VALUE_BOOL_GENTYPE,
        I   = VALUE_INT,
        IV  = VALUE_INT_VEC,
        I4  = VALUE_INT_VEC4,
        IGT = VALUE_INT_GENTYPE,

        VALUE_ANY_FLOAT =
            VALUE_FLOAT | VALUE_FLOAT_VEC | VALUE_FLOAT_GENTYPE | VALUE_VEC3 | VALUE_VEC4 | VALUE_FLOAT_VEC34,
        VALUE_ANY_INT  = VALUE_INT | VALUE_INT_VEC | VALUE_INT_GENTYPE | VALUE_INT_VEC4,
        VALUE_ANY_BOOL = VALUE_BOOL | VALUE_BOOL_VEC | VALUE_BOOL_GENTYPE | VALUE_BOOL_VEC4,

        VALUE_ANY_GENTYPE = VALUE_FLOAT_VEC | VALUE_FLOAT_GENTYPE | VALUE_FLOAT_VEC34 | VALUE_BOOL_VEC |
                            VALUE_BOOL_GENTYPE | VALUE_INT_VEC | VALUE_INT_GENTYPE | VALUE_MATRIX
    };
    enum PrecisionMask
    {
        PRECMASK_NA      = 0, //!< Precision not applicable (booleans)
        PRECMASK_LOWP    = (1 << PRECISION_LOWP),
        PRECMASK_MEDIUMP = (1 << PRECISION_MEDIUMP),
        PRECMASK_HIGHP   = (1 << PRECISION_HIGHP),

        PRECMASK_MEDIUMP_HIGHP = (1 << PRECISION_MEDIUMP) | (1 << PRECISION_HIGHP),
        PRECMASK_ALL           = (1 << PRECISION_LOWP) | (1 << PRECISION_MEDIUMP) | (1 << PRECISION_HIGHP)
    };

    static const DataType floatTypes[]  = {TYPE_FLOAT, TYPE_FLOAT_VEC2, TYPE_FLOAT_VEC3, TYPE_FLOAT_VEC4};
    static const DataType intTypes[]    = {TYPE_INT, TYPE_INT_VEC2, TYPE_INT_VEC3, TYPE_INT_VEC4};
    static const DataType boolTypes[]   = {TYPE_BOOL, TYPE_BOOL_VEC2, TYPE_BOOL_VEC3, TYPE_BOOL_VEC4};
    static const DataType matrixTypes[] = {TYPE_FLOAT_MAT2, TYPE_FLOAT_MAT3, TYPE_FLOAT_MAT4};

    tcu::TestCaseGroup *const angleAndTrigonometryGroup = new tcu::TestCaseGroup(
        m_testCtx, "angle_and_trigonometry", "Built-In Angle and Trigonometry Function Performance Tests");
    tcu::TestCaseGroup *const exponentialGroup =
        new tcu::TestCaseGroup(m_testCtx, "exponential", "Built-In Exponential Function Performance Tests");
    tcu::TestCaseGroup *const commonFunctionsGroup =
        new tcu::TestCaseGroup(m_testCtx, "common_functions", "Built-In Common Function Performance Tests");
    tcu::TestCaseGroup *const geometricFunctionsGroup =
        new tcu::TestCaseGroup(m_testCtx, "geometric", "Built-In Geometric Function Performance Tests");
    tcu::TestCaseGroup *const matrixFunctionsGroup =
        new tcu::TestCaseGroup(m_testCtx, "matrix", "Built-In Matrix Function Performance Tests");
    tcu::TestCaseGroup *const floatCompareGroup = new tcu::TestCaseGroup(
        m_testCtx, "float_compare", "Built-In Floating Point Comparison Function Performance Tests");
    tcu::TestCaseGroup *const intCompareGroup =
        new tcu::TestCaseGroup(m_testCtx, "int_compare", "Built-In Integer Comparison Function Performance Tests");
    tcu::TestCaseGroup *const boolCompareGroup =
        new tcu::TestCaseGroup(m_testCtx, "bool_compare", "Built-In Boolean Comparison Function Performance Tests");

    addChild(angleAndTrigonometryGroup);
    addChild(exponentialGroup);
    addChild(commonFunctionsGroup);
    addChild(geometricFunctionsGroup);
    addChild(matrixFunctionsGroup);
    addChild(floatCompareGroup);
    addChild(intCompareGroup);
    addChild(boolCompareGroup);

    // Some attributes to be used as parameters for the functions.
    const Vec4 attrPos    = Vec4(2.3f, 1.9f, 0.8f, 0.7f);
    const Vec4 attrNegPos = Vec4(-1.3f, 2.5f, -3.5f, 4.3f);
    const Vec4 attrSmall  = Vec4(-0.9f, 0.8f, -0.4f, 0.2f);
    const Vec4 attrBig    = Vec4(1.3f, 2.4f, 3.0f, 4.0f);

    // \todo The following functions and variants are missing, and should be added in the future:
    //         - modf (has an output parameter, not currently handled by test code)
    //         - functions with uint/uvec* return or parameter types
    //         - non-matrix <-> matrix functions (outerProduct etc.)
    // \note Remember to update test spec when these are added.

    // Function name, return type and parameter type information; also, what attribute should be used in the test.
    // \note Different versions of the same function (i.e. with the same group name) can be defined by putting them successively in this array.
    // \note In order to reduce case count and thus total execution time, we don't test all input type combinations for every function.
    static const struct
    {
        tcu::TestCaseGroup *parentGroup;
        const char *groupName;
        const char *func;
        const ValueType types[FunctionCase::MAX_PARAMS + 1]; // Return type and parameter types, in that order.
        const Vec4 &attribute;
        int modifyParamNdx;
        bool useNearlyConstantInputs;
        bool booleanCase;
        PrecisionMask precMask;
    } functionCaseGroups[] = {
        {angleAndTrigonometryGroup, "radians", "radians", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "degrees", "degrees", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "sin", "sin", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "cos", "cos", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "tan", "tan", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "asin", "asin", {F, F, N, N}, attrSmall, -1, true, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "acos", "acos", {F, F, N, N}, attrSmall, -1, true, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "atan2", "atan", {F, F, F, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "atan", "atan", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "sinh", "sinh", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "cosh", "cosh", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "tanh", "tanh", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "asinh", "asinh", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "acosh", "acosh", {F, F, N, N}, attrBig, -1, false, false, PRECMASK_ALL},
        {angleAndTrigonometryGroup, "atanh", "atanh", {F, F, N, N}, attrSmall, -1, true, false, PRECMASK_ALL},

        {exponentialGroup, "pow", "pow", {F, F, F, N}, attrPos, -1, false, false, PRECMASK_ALL},
        {exponentialGroup, "exp", "exp", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {exponentialGroup, "log", "log", {F, F, N, N}, attrPos, -1, false, false, PRECMASK_ALL},
        {exponentialGroup, "exp2", "exp2", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {exponentialGroup, "log2", "log2", {F, F, N, N}, attrPos, -1, false, false, PRECMASK_ALL},
        {exponentialGroup, "sqrt", "sqrt", {F, F, N, N}, attrPos, -1, false, false, PRECMASK_ALL},
        {exponentialGroup, "inversesqrt", "inversesqrt", {F, F, N, N}, attrPos, -1, false, false, PRECMASK_ALL},

        {commonFunctionsGroup, "abs", "abs", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "abs", "abs", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "sign", "sign", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "sign", "sign", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "floor", "floor", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "floor", "floor", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "trunc", "trunc", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "trunc", "trunc", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "round", "round", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "round", "round", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup,
         "roundEven",
         "roundEven",
         {F, F, N, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "roundEven", "roundEven", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "ceil", "ceil", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "ceil", "ceil", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "fract", "fract", {F, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "fract", "fract", {V4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "mod", "mod", {GT, GT, GT, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "min", "min", {F, F, F, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "min", "min", {V4, V4, V4, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "max", "max", {F, F, F, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "max", "max", {V4, V4, V4, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "clamp", "clamp", {F, F, F, F}, attrSmall, 2, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "clamp", "clamp", {V4, V4, V4, V4}, attrSmall, 2, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "mix", "mix", {F, F, F, F}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "mix", "mix", {V4, V4, V4, V4}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "mix", "mix", {F, F, F, B}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "mix", "mix", {V4, V4, V4, B4}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "step", "step", {F, F, F, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "step", "step", {V4, V4, V4, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup,
         "smoothstep",
         "smoothstep",
         {F, F, F, F},
         attrSmall,
         1,
         false,
         false,
         PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "smoothstep", "smoothstep", {V4, V4, V4, V4}, attrSmall, 1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "isnan", "isnan", {B, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "isnan", "isnan", {B4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup, "isinf", "isinf", {B, F, N, N}, attrNegPos, -1, false, false, PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup, "isinf", "isinf", {B4, V4, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {commonFunctionsGroup,
         "floatBitsToInt",
         "floatBitsToInt",
         {I, F, N, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup,
         "floatBitsToInt",
         "floatBitsToInt",
         {I4, V4, N, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {commonFunctionsGroup,
         "intBitsToFloat",
         "intBitsToFloat",
         {F, I, N, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_MEDIUMP_HIGHP},
        {commonFunctionsGroup,
         "intBitsToFloat",
         "intBitsToFloat",
         {V4, I4, N, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},

        {geometricFunctionsGroup, "length", "length", {F, VL, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {geometricFunctionsGroup, "distance", "distance", {F, VL, VL, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {geometricFunctionsGroup, "dot", "dot", {F, VL, VL, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {geometricFunctionsGroup, "cross", "cross", {V3, V3, V3, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {geometricFunctionsGroup, "normalize", "normalize", {VL, VL, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {geometricFunctionsGroup,
         "faceforward",
         "faceforward",
         {VL, VL, VL, VL},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {geometricFunctionsGroup, "reflect", "reflect", {VL, VL, VL, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {geometricFunctionsGroup, "refract", "refract", {VL, VL, VL, F}, attrNegPos, -1, false, false, PRECMASK_ALL},

        {matrixFunctionsGroup,
         "matrixCompMult",
         "matrixCompMult",
         {M, M, M, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {matrixFunctionsGroup, "transpose", "transpose", {M, M, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {matrixFunctionsGroup, "inverse", "inverse", {M, M, N, N}, attrNegPos, -1, false, false, PRECMASK_ALL},

        {floatCompareGroup, "lessThan", "lessThan", {BV, FV, FV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {floatCompareGroup,
         "lessThanEqual",
         "lessThanEqual",
         {BV, FV, FV, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {floatCompareGroup, "greaterThan", "greaterThan", {BV, FV, FV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {floatCompareGroup,
         "greaterThanEqual",
         "greaterThanEqual",
         {BV, FV, FV, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {floatCompareGroup, "equal", "equal", {BV, FV, FV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {floatCompareGroup, "notEqual", "notEqual", {BV, FV, FV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},

        {intCompareGroup, "lessThan", "lessThan", {BV, IV, IV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {intCompareGroup,
         "lessThanEqual",
         "lessThanEqual",
         {BV, IV, IV, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {intCompareGroup, "greaterThan", "greaterThan", {BV, IV, IV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {intCompareGroup,
         "greaterThanEqual",
         "greaterThanEqual",
         {BV, IV, IV, N},
         attrNegPos,
         -1,
         false,
         false,
         PRECMASK_ALL},
        {intCompareGroup, "equal", "equal", {BV, IV, IV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},
        {intCompareGroup, "notEqual", "notEqual", {BV, IV, IV, N}, attrNegPos, -1, false, false, PRECMASK_ALL},

        {boolCompareGroup, "equal", "equal", {BV, BV, BV, N}, attrNegPos, -1, false, true, PRECMASK_MEDIUMP},
        {boolCompareGroup, "notEqual", "notEqual", {BV, BV, BV, N}, attrNegPos, -1, false, true, PRECMASK_MEDIUMP},
        {boolCompareGroup, "any", "any", {B, BV, N, N}, attrNegPos, -1, false, true, PRECMASK_MEDIUMP},
        {boolCompareGroup, "all", "all", {B, BV, N, N}, attrNegPos, -1, false, true, PRECMASK_MEDIUMP},
        {boolCompareGroup, "not", "not", {BV, BV, N, N}, attrNegPos, -1, false, true, PRECMASK_MEDIUMP}};

    // vertexSubGroup and fragmentSubGroup are the groups where the various vertex/fragment cases of a single function are added.
    // \note These are defined here so that different versions (different entries in the functionCaseGroups array) of the same function can be put in the same group.
    tcu::TestCaseGroup *vertexSubGroup   = nullptr;
    tcu::TestCaseGroup *fragmentSubGroup = nullptr;
    FunctionCase::InitialCalibrationStorage vertexSubGroupCalibrationStorage;
    FunctionCase::InitialCalibrationStorage fragmentSubGroupCalibrationStorage;
    for (int funcNdx = 0; funcNdx < DE_LENGTH_OF_ARRAY(functionCaseGroups); funcNdx++)
    {
        tcu::TestCaseGroup *const parentGroup = functionCaseGroups[funcNdx].parentGroup;
        const char *const groupName           = functionCaseGroups[funcNdx].groupName;
        const char *const groupFunc           = functionCaseGroups[funcNdx].func;
        const ValueType *const funcTypes      = functionCaseGroups[funcNdx].types;
        const Vec4 &groupAttribute            = functionCaseGroups[funcNdx].attribute;
        const int modifyParamNdx              = functionCaseGroups[funcNdx].modifyParamNdx;
        const bool useNearlyConstantInputs    = functionCaseGroups[funcNdx].useNearlyConstantInputs;
        const bool booleanCase                = functionCaseGroups[funcNdx].booleanCase;
        const PrecisionMask precMask          = functionCaseGroups[funcNdx].precMask;

        // If this is a new function and not just a different version of the previously defined function, create a new group.
        if (funcNdx == 0 || parentGroup != functionCaseGroups[funcNdx - 1].parentGroup ||
            string(groupName) != functionCaseGroups[funcNdx - 1].groupName)
        {
            tcu::TestCaseGroup *const funcGroup = new tcu::TestCaseGroup(m_testCtx, groupName, "");
            functionCaseGroups[funcNdx].parentGroup->addChild(funcGroup);

            vertexSubGroup   = new tcu::TestCaseGroup(m_testCtx, "vertex", "");
            fragmentSubGroup = new tcu::TestCaseGroup(m_testCtx, "fragment", "");

            funcGroup->addChild(vertexSubGroup);
            funcGroup->addChild(fragmentSubGroup);

            vertexSubGroupCalibrationStorage =
                FunctionCase::InitialCalibrationStorage(new FunctionCase::InitialCalibration);
            fragmentSubGroupCalibrationStorage =
                FunctionCase::InitialCalibrationStorage(new FunctionCase::InitialCalibration);
        }

        DE_ASSERT(vertexSubGroup != nullptr);
        DE_ASSERT(fragmentSubGroup != nullptr);

        // Find the type size range of parameters (e.g. from 2 to 4 in case of vectors).
        int genTypeFirstSize = 1;
        int genTypeLastSize  = 1;

        // Find the first return value or parameter with a gentype (if any) and set sizes accordingly.
        // \note Assumes only matching sizes gentypes are to be found, e.g. no "genType func (vec param)"
        for (int i = 0; i < FunctionCase::MAX_PARAMS + 1 && genTypeLastSize == 1; i++)
        {
            switch (funcTypes[i])
            {
            case VALUE_FLOAT_VEC:
            case VALUE_BOOL_VEC:
            case VALUE_INT_VEC: // \note Fall-through.
                genTypeFirstSize = 2;
                genTypeLastSize  = 4;
                break;
            case VALUE_FLOAT_VEC34:
                genTypeFirstSize = 3;
                genTypeLastSize  = 4;
                break;
            case VALUE_FLOAT_GENTYPE:
            case VALUE_BOOL_GENTYPE:
            case VALUE_INT_GENTYPE: // \note Fall-through.
                genTypeFirstSize = 1;
                genTypeLastSize  = 4;
                break;
            case VALUE_MATRIX:
                genTypeFirstSize = 2;
                genTypeLastSize  = 4;
                break;
            // If none of the above, keep looping.
            default:
                break;
            }
        }

        // Create a case for each possible size of the gentype.
        for (int curSize = genTypeFirstSize; curSize <= genTypeLastSize; curSize++)
        {
            // Determine specific types for return value and the parameters, according to curSize. Non-gentypes not affected by curSize.
            DataType types[FunctionCase::MAX_PARAMS + 1];
            for (int i = 0; i < FunctionCase::MAX_PARAMS + 1; i++)
            {
                if (funcTypes[i] == VALUE_NONE)
                    types[i] = TYPE_INVALID;
                else
                {
                    int isFloat      = funcTypes[i] & VALUE_ANY_FLOAT;
                    int isBool       = funcTypes[i] & VALUE_ANY_BOOL;
                    int isInt        = funcTypes[i] & VALUE_ANY_INT;
                    int isMat        = funcTypes[i] == VALUE_MATRIX;
                    int inSize       = (funcTypes[i] & VALUE_ANY_GENTYPE) ? curSize :
                                       funcTypes[i] == VALUE_VEC3         ? 3 :
                                       funcTypes[i] == VALUE_VEC4         ? 4 :
                                       funcTypes[i] == VALUE_BOOL_VEC4    ? 4 :
                                       funcTypes[i] == VALUE_INT_VEC4     ? 4 :
                                                                            1;
                    int typeArrayNdx = isMat ? inSize - 2 : inSize - 1; // \note No matrices of size 1.

                    types[i] = isFloat ? floatTypes[typeArrayNdx] :
                               isBool  ? boolTypes[typeArrayNdx] :
                               isInt   ? intTypes[typeArrayNdx] :
                               isMat   ? matrixTypes[typeArrayNdx] :
                                         TYPE_LAST;
                }

                DE_ASSERT(types[i] != TYPE_LAST);
            }

            // Array for just the parameter types.
            DataType paramTypes[FunctionCase::MAX_PARAMS];
            for (int i = 0; i < FunctionCase::MAX_PARAMS; i++)
                paramTypes[i] = types[i + 1];

            for (int prec = (int)PRECISION_LOWP; prec < (int)PRECISION_LAST; prec++)
            {
                if ((precMask & (1 << prec)) == 0)
                    continue;

                const string precisionPrefix = booleanCase ? "" : (string(getPrecisionName((Precision)prec)) + "_");
                std::ostringstream caseName;

                caseName << precisionPrefix;

                // Write the name of each distinct parameter data type into the test case name.
                for (int i = 1; i < FunctionCase::MAX_PARAMS + 1 && types[i] != TYPE_INVALID; i++)
                {
                    if (i == 1 || types[i] != types[i - 1])
                    {
                        if (i > 1)
                            caseName << "_";

                        caseName << getDataTypeName(types[i]);
                    }
                }

                for (int fragI = 0; fragI <= 1; fragI++)
                {
                    const bool vert                 = fragI == 0;
                    tcu::TestCaseGroup *const group = vert ? vertexSubGroup : fragmentSubGroup;
                    group->addChild(
                        new FunctionCase(m_context, caseName.str().c_str(), "", groupFunc, types[0], paramTypes,
                                         groupAttribute, modifyParamNdx, useNearlyConstantInputs, (Precision)prec, vert,
                                         vert ? vertexSubGroupCalibrationStorage : fragmentSubGroupCalibrationStorage));
                }
            }
        }
    }
}

} // namespace Performance
} // namespace gles3
} // namespace deqp
