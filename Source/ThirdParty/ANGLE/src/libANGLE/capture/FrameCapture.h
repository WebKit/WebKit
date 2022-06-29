//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// FrameCapture.h:
//   ANGLE Frame capture interface.
//

#ifndef LIBANGLE_FRAME_CAPTURE_H_
#define LIBANGLE_FRAME_CAPTURE_H_

#include "common/PackedEnums.h"
#include "common/system_utils.h"
#include "libANGLE/Context.h"
#include "libANGLE/angletypes.h"
#include "libANGLE/capture/frame_capture_utils_autogen.h"
#include "libANGLE/entry_points_utils.h"

namespace gl
{
enum class GLenumGroup;
}

namespace angle
{

using ParamData = std::vector<std::vector<uint8_t>>;
struct ParamCapture : angle::NonCopyable
{
    ParamCapture();
    ParamCapture(const char *nameIn, ParamType typeIn);
    ~ParamCapture();

    ParamCapture(ParamCapture &&other);
    ParamCapture &operator=(ParamCapture &&other);

    std::string name;
    ParamType type;
    ParamValue value;
    gl::GLenumGroup enumGroup;  // only used for param type GLenum, GLboolean and GLbitfield
    ParamData data;
    int dataNElements           = 0;
    int arrayClientPointerIndex = -1;
    size_t readBufferSizeBytes  = 0;
};

class ParamBuffer final : angle::NonCopyable
{
  public:
    ParamBuffer();
    ~ParamBuffer();

    ParamBuffer(ParamBuffer &&other);
    ParamBuffer &operator=(ParamBuffer &&other);

    template <typename T>
    void addValueParam(const char *paramName, ParamType paramType, T paramValue);
    template <typename T>
    void setValueParamAtIndex(const char *paramName, ParamType paramType, T paramValue, int index);
    template <typename T>
    void addEnumParam(const char *paramName,
                      gl::GLenumGroup enumGroup,
                      ParamType paramType,
                      T paramValue);

    ParamCapture &getParam(const char *paramName, ParamType paramType, int index);
    const ParamCapture &getParam(const char *paramName, ParamType paramType, int index) const;
    ParamCapture &getParamFlexName(const char *paramName1,
                                   const char *paramName2,
                                   ParamType paramType,
                                   int index);
    const ParamCapture &getParamFlexName(const char *paramName1,
                                         const char *paramName2,
                                         ParamType paramType,
                                         int index) const;
    const ParamCapture &getReturnValue() const { return mReturnValueCapture; }

    void addParam(ParamCapture &&param);
    void addReturnValue(ParamCapture &&returnValue);
    bool hasClientArrayData() const { return mClientArrayDataParam != -1; }
    ParamCapture &getClientArrayPointerParameter();
    size_t getReadBufferSize() const { return mReadBufferSize; }

    const std::vector<ParamCapture> &getParamCaptures() const { return mParamCaptures; }

    // These helpers allow us to track the ID of the buffer that was active when
    // MapBufferRange was called.  We'll use it during replay to track the
    // buffer's contents, as they can be modified by the host.
    void setMappedBufferID(gl::BufferID bufferID) { mMappedBufferID = bufferID; }
    gl::BufferID getMappedBufferID() const { return mMappedBufferID; }

  private:
    std::vector<ParamCapture> mParamCaptures;
    ParamCapture mReturnValueCapture;
    int mClientArrayDataParam = -1;
    size_t mReadBufferSize    = 0;
    gl::BufferID mMappedBufferID;
};

struct CallCapture
{
    CallCapture(EntryPoint entryPointIn, ParamBuffer &&paramsIn);
    CallCapture(const std::string &customFunctionNameIn, ParamBuffer &&paramsIn);
    ~CallCapture();

    CallCapture(CallCapture &&other);
    CallCapture &operator=(CallCapture &&other);

    const char *name() const;

    EntryPoint entryPoint;
    std::string customFunctionName;
    ParamBuffer params;
    bool isActive = true;
};

class ReplayContext
{
  public:
    ReplayContext(size_t readBufferSizebytes, const gl::AttribArray<size_t> &clientArraysSizebytes);
    ~ReplayContext();

    template <typename T>
    T getReadBufferPointer(const ParamCapture &param)
    {
        ASSERT(param.readBufferSizeBytes > 0);
        ASSERT(mReadBuffer.size() >= param.readBufferSizeBytes);
        return reinterpret_cast<T>(mReadBuffer.data());
    }
    template <typename T>
    T getAsConstPointer(const ParamCapture &param)
    {
        if (param.arrayClientPointerIndex != -1)
        {
            return reinterpret_cast<T>(mClientArraysBuffer[param.arrayClientPointerIndex].data());
        }

        if (!param.data.empty())
        {
            ASSERT(param.data.size() == 1);
            return reinterpret_cast<T>(param.data[0].data());
        }

        return nullptr;
    }

    template <typename T>
    T getAsPointerConstPointer(const ParamCapture &param)
    {
        static_assert(sizeof(typename std::remove_pointer<T>::type) == sizeof(uint8_t *),
                      "pointer size not match!");

        ASSERT(!param.data.empty());
        mPointersBuffer.clear();
        mPointersBuffer.reserve(param.data.size());
        for (const std::vector<uint8_t> &data : param.data)
        {
            mPointersBuffer.emplace_back(data.data());
        }
        return reinterpret_cast<T>(mPointersBuffer.data());
    }

    gl::AttribArray<std::vector<uint8_t>> &getClientArraysBuffer() { return mClientArraysBuffer; }

  private:
    std::vector<uint8_t> mReadBuffer;
    std::vector<const uint8_t *> mPointersBuffer;
    gl::AttribArray<std::vector<uint8_t>> mClientArraysBuffer;
};

// Helper to use unique IDs for each local data variable.
class DataCounters final : angle::NonCopyable
{
  public:
    DataCounters();
    ~DataCounters();

    int getAndIncrement(EntryPoint entryPoint, const std::string &paramName);

  private:
    // <CallName, ParamName>
    using Counter = std::pair<EntryPoint, std::string>;
    std::map<Counter, int> mData;
};

constexpr int kStringsNotFound = -1;
class StringCounters final : angle::NonCopyable
{
  public:
    StringCounters();
    ~StringCounters();

    int getStringCounter(const std::vector<std::string> &str);
    void setStringCounter(const std::vector<std::string> &str, int &counter);

  private:
    std::map<std::vector<std::string>, int> mStringCounterMap;
};

class DataTracker final : angle::NonCopyable
{
  public:
    DataTracker();
    ~DataTracker();

    DataCounters &getCounters() { return mCounters; }
    StringCounters &getStringCounters() { return mStringCounters; }

  private:
    DataCounters mCounters;
    StringCounters mStringCounters;
};

class ReplayWriter final : angle::NonCopyable
{
  public:
    ReplayWriter();
    ~ReplayWriter();

    void setSourceFileSizeThreshold(size_t sourceFileSizeThreshold);
    void setFilenamePattern(const std::string &pattern);
    void setCaptureLabel(const std::string &label);
    void setSourcePrologue(const std::string &prologue);
    void setHeaderPrologue(const std::string &prologue);

    void addPublicFunction(const std::string &functionProto,
                           const std::stringstream &headerStream,
                           const std::stringstream &bodyStream);
    void addPrivateFunction(const std::string &functionProto,
                            const std::stringstream &headerStream,
                            const std::stringstream &bodyStream);
    std::string getInlineVariableName(EntryPoint entryPoint, const std::string &paramName);

    std::string getInlineStringSetVariableName(EntryPoint entryPoint,
                                               const std::string &paramName,
                                               const std::vector<std::string> &strings,
                                               bool *isNewEntryOut);

    void saveFrame();
    void saveFrameIfFull();
    void saveIndexFilesAndHeader();
    void saveSetupFile();

    std::vector<std::string> getAndResetWrittenFiles();

  private:
    static std::string GetVarName(EntryPoint entryPoint, const std::string &paramName, int counter);

    void saveHeader();
    void writeReplaySource(const std::string &filename);
    void addWrittenFile(const std::string &filename);
    size_t getStoredReplaySourceSize() const;

    size_t mSourceFileSizeThreshold;
    size_t mFrameIndex;

    DataTracker mDataTracker;
    std::string mFilenamePattern;
    std::string mCaptureLabel;
    std::string mSourcePrologue;
    std::string mHeaderPrologue;

    std::vector<std::string> mReplayHeaders;
    std::vector<std::string> mGlobalVariableDeclarations;

    std::vector<std::string> mPublicFunctionPrototypes;
    std::vector<std::string> mPublicFunctions;

    std::vector<std::string> mPrivateFunctionPrototypes;
    std::vector<std::string> mPrivateFunctions;

    std::vector<std::string> mWrittenFiles;
};

using BufferCalls = std::map<GLuint, std::vector<CallCapture>>;

// true means mapped, false means unmapped
using BufferMapStatusMap = std::map<GLuint, bool>;

using FenceSyncSet   = std::set<GLsync>;
using FenceSyncCalls = std::map<GLsync, std::vector<CallCapture>>;

// For default uniforms, we need to track which ones are dirty, and the series of calls to reset.
// Each program has unique default uniforms, and each uniform has one or more locations in the
// default buffer. For reset efficiency, we track only the uniforms dirty by location, per program.

// A set of all default uniforms (per program) that were modified during the run
using DefaultUniformLocationsSet = std::set<gl::UniformLocation>;
using DefaultUniformLocationsPerProgramMap =
    std::map<gl::ShaderProgramID, DefaultUniformLocationsSet>;

// A map of programs which maps to locations and their reset calls
using DefaultUniformCallsPerLocationMap = std::map<gl::UniformLocation, std::vector<CallCapture>>;
using DefaultUniformCallsPerProgramMap =
    std::map<gl::ShaderProgramID, DefaultUniformCallsPerLocationMap>;

using ResourceSet   = std::set<GLuint>;
using ResourceCalls = std::map<GLuint, std::vector<CallCapture>>;

class TrackedResource final : angle::NonCopyable
{
  public:
    TrackedResource();
    ~TrackedResource();

    const ResourceSet &getStartingResources() const { return mStartingResources; }
    ResourceSet &getStartingResources() { return mStartingResources; }
    const ResourceSet &getNewResources() const { return mNewResources; }
    ResourceSet &getNewResources() { return mNewResources; }
    const ResourceSet &getResourcesToRegen() const { return mResourcesToRegen; }
    ResourceSet &getResourcesToRegen() { return mResourcesToRegen; }
    const ResourceSet &getResourcesToRestore() const { return mResourcesToRestore; }
    ResourceSet &getResourcesToRestore() { return mResourcesToRestore; }

    void setGennedResource(GLuint id);
    void setDeletedResource(GLuint id);
    void setModifiedResource(GLuint id);
    bool resourceIsGenerated(GLuint id);

    ResourceCalls &getResourceRegenCalls() { return mResourceRegenCalls; }
    ResourceCalls &getResourceRestoreCalls() { return mResourceRestoreCalls; }

  private:
    // Resource regen calls will gen a resource
    ResourceCalls mResourceRegenCalls;
    // Resource restore calls will restore the contents of a resource
    ResourceCalls mResourceRestoreCalls;

    // Resources created during startup
    ResourceSet mStartingResources;

    // Resources created during the run that need to be deleted
    ResourceSet mNewResources;
    // Resources deleted during the run that need to be recreated
    ResourceSet mResourcesToRegen;
    // Resources modified during the run that need to be restored
    ResourceSet mResourcesToRestore;
};

using TrackedResourceArray =
    std::array<TrackedResource, static_cast<uint32_t>(ResourceIDType::EnumCount)>;

// Helper to track resource changes during the capture
class ResourceTracker final : angle::NonCopyable
{
  public:
    ResourceTracker();
    ~ResourceTracker();

    BufferCalls &getBufferMapCalls() { return mBufferMapCalls; }
    BufferCalls &getBufferUnmapCalls() { return mBufferUnmapCalls; }

    std::vector<CallCapture> &getBufferBindingCalls() { return mBufferBindingCalls; }

    void setBufferMapped(GLuint id);
    void setBufferUnmapped(GLuint id);

    bool getStartingBuffersMappedCurrent(GLuint id) const;
    bool getStartingBuffersMappedInitial(GLuint id) const;

    void setStartingBufferMapped(GLuint id, bool mapped)
    {
        // Track the current state (which will change throughout the trace)
        mStartingBuffersMappedCurrent[id] = mapped;

        // And the initial state, to compare during frame loop reset
        mStartingBuffersMappedInitial[id] = mapped;
    }

    void onShaderProgramAccess(gl::ShaderProgramID shaderProgramID);
    uint32_t getMaxShaderPrograms() const { return mMaxShaderPrograms; }

    FenceSyncSet &getStartingFenceSyncs() { return mStartingFenceSyncs; }
    FenceSyncCalls &getFenceSyncRegenCalls() { return mFenceSyncRegenCalls; }
    FenceSyncSet &getFenceSyncsToRegen() { return mFenceSyncsToRegen; }
    void setDeletedFenceSync(GLsync sync);

    DefaultUniformLocationsPerProgramMap &getDefaultUniformsToReset()
    {
        return mDefaultUniformsToReset;
    }
    DefaultUniformCallsPerLocationMap &getDefaultUniformResetCalls(gl::ShaderProgramID id)
    {
        return mDefaultUniformResetCalls[id];
    }
    void setModifiedDefaultUniform(gl::ShaderProgramID programID, gl::UniformLocation location);

    TrackedResource &getTrackedResource(ResourceIDType type)
    {
        return mTrackedResources[static_cast<uint32_t>(type)];
    }

  private:
    // Buffer map calls will map a buffer with correct offset, length, and access flags
    BufferCalls mBufferMapCalls;
    // Buffer unmap calls will bind and unmap a given buffer
    BufferCalls mBufferUnmapCalls;

    // Buffer binding calls to restore bindings recorded during MEC
    std::vector<CallCapture> mBufferBindingCalls;

    // Whether a given buffer was mapped at the start of the trace
    BufferMapStatusMap mStartingBuffersMappedInitial;
    // The status of buffer mapping throughout the trace, modified with each Map/Unmap call
    BufferMapStatusMap mStartingBuffersMappedCurrent;

    // Maximum accessed shader program ID.
    uint32_t mMaxShaderPrograms = 0;

    // Fence sync objects created during MEC setup
    FenceSyncSet mStartingFenceSyncs;
    // Fence sync regen calls will create a fence sync objects
    FenceSyncCalls mFenceSyncRegenCalls;
    // Fence syncs to regen are a list of starting fence sync objects that were deleted and need to
    // be regen'ed.
    FenceSyncSet mFenceSyncsToRegen;

    // Default uniforms that were modified during the run
    DefaultUniformLocationsPerProgramMap mDefaultUniformsToReset;
    // Calls per default uniform to return to original state
    DefaultUniformCallsPerProgramMap mDefaultUniformResetCalls;

    TrackedResourceArray mTrackedResources;
};

// Used by the CPP replay to filter out unnecessary code.
using HasResourceTypeMap = angle::PackedEnumBitSet<ResourceIDType>;

// Map of ResourceType to IDs and range of setup calls
using ResourceIDToSetupCallsMap =
    PackedEnumMap<ResourceIDType, std::map<GLuint, gl::Range<size_t>>>;

// Map of buffer ID to offset and size used when mapped
using BufferDataMap = std::map<gl::BufferID, std::pair<GLintptr, GLsizeiptr>>;

// A dictionary of sources indexed by shader type.
using ProgramSources = gl::ShaderMap<std::string>;

// Maps from IDs to sources.
using ShaderSourceMap  = std::map<gl::ShaderProgramID, std::string>;
using ProgramSourceMap = std::map<gl::ShaderProgramID, ProgramSources>;

// Map from textureID to level and data
using TextureLevels       = std::map<GLint, std::vector<uint8_t>>;
using TextureLevelDataMap = std::map<gl::TextureID, TextureLevels>;

struct SurfaceParams
{
    gl::Extents extents;
    egl::ColorSpace colorSpace;
};

// Map from ContextID to SurfaceParams
using SurfaceParamsMap = std::map<gl::ContextID, SurfaceParams>;

using CallVector = std::vector<std::vector<CallCapture> *>;

// A map from API entry point to calls
using CallResetMap = std::map<angle::EntryPoint, std::vector<CallCapture>>;

// StateResetHelper provides a simple way to track whether an entry point has been called during the
// trace, along with the reset calls to get it back to starting state.  This is useful for things
// that are one dimensional, like context bindings or context state.
class StateResetHelper final : angle::NonCopyable
{
  public:
    StateResetHelper();
    ~StateResetHelper();

    const std::set<angle::EntryPoint> &getDirtyEntryPoints() const { return mDirtyEntryPoints; }
    void setEntryPointDirty(EntryPoint entryPoint) { mDirtyEntryPoints.insert(entryPoint); }

    CallResetMap &getResetCalls() { return mResetCalls; }
    const CallResetMap &getResetCalls() const { return mResetCalls; }

  private:
    // Dirty state per entry point
    std::set<angle::EntryPoint> mDirtyEntryPoints;

    // Reset calls per API entry point
    CallResetMap mResetCalls;
};

class FrameCapture final : angle::NonCopyable
{
  public:
    FrameCapture();
    ~FrameCapture();

    std::vector<CallCapture> &getSetupCalls() { return mSetupCalls; }
    void clearSetupCalls() { mSetupCalls.clear(); }

    StateResetHelper &getStateResetHelper() { return mStateResetHelper; }
    const StateResetHelper &getStateResetHelper() const { return mStateResetHelper; }

    void reset();

  private:
    std::vector<CallCapture> mSetupCalls;

    StateResetHelper mStateResetHelper;
};

// Page range inside a coherent buffer
struct PageRange
{
    PageRange(size_t start, size_t end);
    ~PageRange();

    // Relative start page
    size_t start;

    // First page after the relative end
    size_t end;
};

// Memory address range defined by start and size
struct AddressRange
{
    AddressRange();
    AddressRange(uintptr_t start, size_t size);
    ~AddressRange();

    uintptr_t end();

    uintptr_t start;
    size_t size;
};

// Used to handle protection of buffers that overlap in pages.
enum class PageSharingType
{
    NoneShared,
    FirstShared,
    LastShared,
    FirstAndLastShared
};

class CoherentBuffer
{
  public:
    CoherentBuffer(uintptr_t start, size_t size, size_t pageSize);
    ~CoherentBuffer();

    // Sets the a range in the buffer clean and protects a selected range
    void protectPageRange(const PageRange &pageRange);

    // Sets a page dirty state and sets it's protection
    void setDirty(size_t relativePage, bool dirty);

    // Removes protection
    void removeProtection(PageSharingType sharingType);

    bool contains(size_t page, size_t *relativePage);
    bool isDirty();

    // Returns dirty page ranges
    std::vector<PageRange> getDirtyPageRanges();

    // Calculates address range from page range
    AddressRange getDirtyAddressRange(const PageRange &dirtyPageRange);
    AddressRange getRange();

  private:
    // Actual buffer start and size
    AddressRange mRange;

    // Start and size of page aligned protected area
    AddressRange mProtectionRange;

    // Start and end of protection in relative pages, calculated from mProtectionRange.
    size_t mProtectionStartPage;
    size_t mProtectionEndPage;

    size_t mPageCount;
    size_t mPageSize;

    // Clean pages are protected
    std::vector<bool> mDirtyPages;
};

class CoherentBufferTracker final : angle::NonCopyable
{
  public:
    CoherentBufferTracker();
    ~CoherentBufferTracker();

    bool isDirty(gl::BufferID id);
    void addBuffer(gl::BufferID id, uintptr_t start, size_t size);
    void removeBuffer(gl::BufferID id);
    void disable();
    void enable();
    void onEndFrame();

  private:
    // Detect overlapping pages when removing protection
    PageSharingType doesBufferSharePage(gl::BufferID id);

    // Returns a map to found buffers and the corresponding pages for a given address.
    // For addresses that are in a page shared by 2 buffers, 2 results are returned.
    HashMap<std::shared_ptr<CoherentBuffer>, size_t> getBufferPagesForAddress(uintptr_t address);
    PageFaultHandlerRangeType handleWrite(uintptr_t address);
    bool haveBuffer(gl::BufferID id);

  public:
    std::mutex mMutex;
    HashMap<GLuint, std::shared_ptr<CoherentBuffer>> mBuffers;

  private:
    bool mEnabled = false;
    std::unique_ptr<PageFaultHandler> mPageFaultHandler;
    size_t mPageSize;
};

// Shared class for any items that need to be tracked by FrameCapture across shared contexts
class FrameCaptureShared final : angle::NonCopyable
{
  public:
    FrameCaptureShared();
    ~FrameCaptureShared();

    void captureCall(const gl::Context *context, CallCapture &&call, bool isCallValid);
    void checkForCaptureTrigger();
    void onEndFrame(const gl::Context *context);
    void onDestroyContext(const gl::Context *context);
    void onMakeCurrent(const gl::Context *context, const egl::Surface *drawSurface);
    bool enabled() const { return mEnabled; }

    bool isCapturing() const;
    void replay(gl::Context *context);
    uint32_t getFrameCount() const;

    // Returns a frame index starting from "1" as the first frame.
    uint32_t getReplayFrameIndex() const;

    void trackBufferMapping(CallCapture *call,
                            gl::BufferID id,
                            gl::Buffer *buffer,
                            GLintptr offset,
                            GLsizeiptr length,
                            bool writable,
                            bool coherent);

    void trackTextureUpdate(const gl::Context *context, const CallCapture &call);
    void trackDefaultUniformUpdate(const gl::Context *context, const CallCapture &call);

    const std::string &getShaderSource(gl::ShaderProgramID id) const;
    void setShaderSource(gl::ShaderProgramID id, std::string sources);

    const ProgramSources &getProgramSources(gl::ShaderProgramID id) const;
    void setProgramSources(gl::ShaderProgramID id, ProgramSources sources);

    // Load data from a previously stored texture level
    const std::vector<uint8_t> &retrieveCachedTextureLevel(gl::TextureID id,
                                                           gl::TextureTarget target,
                                                           GLint level);

    // Create new texture level data and copy the source into it
    void copyCachedTextureLevel(const gl::Context *context,
                                gl::TextureID srcID,
                                GLint srcLevel,
                                gl::TextureID dstID,
                                GLint dstLevel,
                                const CallCapture &call);

    // Create the location that should be used to cache texture level data
    std::vector<uint8_t> &getCachedTextureLevelData(gl::Texture *texture,
                                                    gl::TextureTarget target,
                                                    GLint level,
                                                    EntryPoint entryPoint);

    // Capture coherent buffer storages
    void captureCoherentBufferSnapshot(const gl::Context *context, gl::BufferID bufferID);

    // Remove any cached texture levels on deletion
    void deleteCachedTextureLevelData(gl::TextureID id);

    void eraseBufferDataMapEntry(const gl::BufferID bufferId)
    {
        const auto &bufferDataInfo = mBufferDataMap.find(bufferId);
        if (bufferDataInfo != mBufferDataMap.end())
        {
            mBufferDataMap.erase(bufferDataInfo);
        }
    }

    bool hasBufferData(gl::BufferID bufferID)
    {
        const auto &bufferDataInfo = mBufferDataMap.find(bufferID);
        if (bufferDataInfo != mBufferDataMap.end())
        {
            return true;
        }
        return false;
    }

    std::pair<GLintptr, GLsizeiptr> getBufferDataOffsetAndLength(gl::BufferID bufferID)
    {
        const auto &bufferDataInfo = mBufferDataMap.find(bufferID);
        ASSERT(bufferDataInfo != mBufferDataMap.end());
        return bufferDataInfo->second;
    }

    void setCaptureActive() { mCaptureActive = true; }
    void setCaptureInactive() { mCaptureActive = false; }
    bool isCaptureActive() { return mCaptureActive; }
    bool usesMidExecutionCapture() { return mCaptureStartFrame > 1; }

    gl::ContextID getWindowSurfaceContextID() const { return mWindowSurfaceContextID; }

    void markResourceSetupCallsInactive(std::vector<CallCapture> *setupCalls,
                                        ResourceIDType type,
                                        GLuint id,
                                        gl::Range<size_t> range);

    void updateReadBufferSize(size_t readBufferSize)
    {
        mReadBufferSize = std::max(mReadBufferSize, readBufferSize);
    }

    template <typename ResourceType>
    void handleGennedResource(ResourceType resourceID)
    {
        if (isCaptureActive())
        {
            ResourceIDType idType    = GetResourceIDTypeFromType<ResourceType>::IDType;
            TrackedResource &tracker = mResourceTracker.getTrackedResource(idType);
            tracker.setGennedResource(resourceID.value);
        }
    }

    template <typename ResourceType>
    bool resourceIsGenerated(ResourceType resourceID)
    {
        ResourceIDType idType    = GetResourceIDTypeFromType<ResourceType>::IDType;
        TrackedResource &tracker = mResourceTracker.getTrackedResource(idType);
        return tracker.resourceIsGenerated(resourceID.value);
    }

    template <typename ResourceType>
    void handleDeletedResource(ResourceType resourceID)
    {
        if (isCaptureActive())
        {
            ResourceIDType idType    = GetResourceIDTypeFromType<ResourceType>::IDType;
            TrackedResource &tracker = mResourceTracker.getTrackedResource(idType);
            tracker.setDeletedResource(resourceID.value);
        }
    }

  private:
    void writeJSON(const gl::Context *context);
    void writeCppReplayIndexFiles(const gl::Context *context, bool writeResetContextCall);
    void writeMainContextCppReplay(const gl::Context *context,
                                   const std::vector<CallCapture> &setupCalls,
                                   const StateResetHelper &StateResetHelper);

    void captureClientArraySnapshot(const gl::Context *context,
                                    size_t vertexCount,
                                    size_t instanceCount);
    void captureMappedBufferSnapshot(const gl::Context *context, const CallCapture &call);

    void copyCompressedTextureData(const gl::Context *context, const CallCapture &call);
    void captureCompressedTextureData(const gl::Context *context, const CallCapture &call);

    void reset();
    void maybeOverrideEntryPoint(const gl::Context *context,
                                 CallCapture &call,
                                 std::vector<CallCapture> &newCalls);
    void maybeCapturePreCallUpdates(const gl::Context *context,
                                    CallCapture &call,
                                    std::vector<CallCapture> *shareGroupSetupCalls,
                                    ResourceIDToSetupCallsMap *resourceIDToSetupCalls);
    template <typename ParamValueType>
    void maybeGenResourceOnBind(CallCapture &call);
    void maybeCapturePostCallUpdates(const gl::Context *context);
    void maybeCaptureDrawArraysClientData(const gl::Context *context,
                                          CallCapture &call,
                                          size_t instanceCount);
    void maybeCaptureDrawElementsClientData(const gl::Context *context,
                                            CallCapture &call,
                                            size_t instanceCount);
    void maybeCaptureCoherentBuffers(const gl::Context *context);
    void updateCopyImageSubData(CallCapture &call);
    void overrideProgramBinary(const gl::Context *context,
                               CallCapture &call,
                               std::vector<CallCapture> &outCalls);
    void updateResourceCountsFromParamCapture(const ParamCapture &param, ResourceIDType idType);
    void updateResourceCountsFromCallCapture(const CallCapture &call);

    void runMidExecutionCapture(const gl::Context *context);

    void scanSetupCalls(const gl::Context *context, std::vector<CallCapture> &setupCalls);

    static void ReplayCall(gl::Context *context,
                           ReplayContext *replayContext,
                           const CallCapture &call);

    std::vector<CallCapture> mFrameCalls;
    gl::ContextID mLastContextId;

    // We save one large buffer of binary data for the whole CPP replay.
    // This simplifies a lot of file management.
    std::vector<uint8_t> mBinaryData;

    bool mEnabled;
    bool mSerializeStateEnabled;
    std::string mOutDirectory;
    std::string mCaptureLabel;
    bool mCompression;
    gl::AttribArray<int> mClientVertexArrayMap;
    uint32_t mFrameIndex;
    uint32_t mCaptureStartFrame;
    uint32_t mCaptureEndFrame;
    bool mIsFirstFrame   = true;
    bool mWroteIndexFile = false;
    SurfaceParamsMap mDrawSurfaceParams;
    gl::AttribArray<size_t> mClientArraySizes;
    size_t mReadBufferSize;
    HasResourceTypeMap mHasResourceType;
    ResourceIDToSetupCallsMap mResourceIDToSetupCalls;
    BufferDataMap mBufferDataMap;
    bool mValidateSerializedState = false;
    std::string mValidationExpression;
    bool mTrimEnabled = true;
    PackedEnumMap<ResourceIDType, uint32_t> mMaxAccessedResourceIDs;
    CoherentBufferTracker mCoherentBufferTracker;

    ResourceTracker mResourceTracker;
    ReplayWriter mReplayWriter;

    // If you don't know which frame you want to start capturing at, use the capture trigger.
    // Initialize it to the number of frames you want to capture, and then clear the value to 0 when
    // you reach the content you want to capture. Currently only available on Android.
    uint32_t mCaptureTrigger;

    bool mCaptureActive;
    std::vector<uint32_t> mActiveFrameIndices;

    bool mMidExecutionCaptureActive;

    // Cache most recently compiled and linked sources.
    ShaderSourceMap mCachedShaderSource;
    ProgramSourceMap mCachedProgramSources;

    gl::ContextID mWindowSurfaceContextID;

    std::vector<CallCapture> mShareGroupSetupCalls;
};

template <typename CaptureFuncT, typename... ArgsT>
void CaptureCallToFrameCapture(CaptureFuncT captureFunc,
                               bool isCallValid,
                               gl::Context *context,
                               ArgsT... captureParams)
{
    FrameCaptureShared *frameCaptureShared = context->getShareGroup()->getFrameCaptureShared();
    if (!frameCaptureShared->isCapturing())
    {
        return;
    }

    CallCapture call = captureFunc(context->getState(), isCallValid, captureParams...);

    frameCaptureShared->captureCall(context, std::move(call), isCallValid);
}

template <typename T>
void ParamBuffer::addValueParam(const char *paramName, ParamType paramType, T paramValue)
{
    ParamCapture capture(paramName, paramType);
    InitParamValue(paramType, paramValue, &capture.value);
    mParamCaptures.emplace_back(std::move(capture));
}

template <typename T>
void ParamBuffer::setValueParamAtIndex(const char *paramName,
                                       ParamType paramType,
                                       T paramValue,
                                       int index)
{
    ASSERT(mParamCaptures.size() > static_cast<size_t>(index));

    ParamCapture capture(paramName, paramType);
    InitParamValue(paramType, paramValue, &capture.value);
    mParamCaptures[index] = std::move(capture);
}

template <typename T>
void ParamBuffer::addEnumParam(const char *paramName,
                               gl::GLenumGroup enumGroup,
                               ParamType paramType,
                               T paramValue)
{
    ParamCapture capture(paramName, paramType);
    InitParamValue(paramType, paramValue, &capture.value);
    capture.enumGroup = enumGroup;
    mParamCaptures.emplace_back(std::move(capture));
}

// Pointer capture helpers.
void CaptureMemory(const void *source, size_t size, ParamCapture *paramCapture);
void CaptureString(const GLchar *str, ParamCapture *paramCapture);
void CaptureStringLimit(const GLchar *str, uint32_t limit, ParamCapture *paramCapture);
void CaptureVertexPointerGLES1(const gl::State &glState,
                               gl::ClientVertexArrayType type,
                               const void *pointer,
                               ParamCapture *paramCapture);

gl::Program *GetProgramForCapture(const gl::State &glState, gl::ShaderProgramID handle);

// For GetIntegerv, GetFloatv, etc.
void CaptureGetParameter(const gl::State &glState,
                         GLenum pname,
                         size_t typeSize,
                         ParamCapture *paramCapture);

void CaptureGetActiveUniformBlockivParameters(const gl::State &glState,
                                              gl::ShaderProgramID handle,
                                              gl::UniformBlockIndex uniformBlockIndex,
                                              GLenum pname,
                                              ParamCapture *paramCapture);

template <typename T>
void CaptureClearBufferValue(GLenum buffer, const T *value, ParamCapture *paramCapture)
{
    // Per the spec, color buffers have a vec4, the rest a single value
    uint32_t valueSize = (buffer == GL_COLOR) ? 4 : 1;
    CaptureMemory(value, valueSize * sizeof(T), paramCapture);
}

void CaptureGenHandlesImpl(GLsizei n, GLuint *handles, ParamCapture *paramCapture);

template <typename T>
void CaptureGenHandles(GLsizei n, T *handles, ParamCapture *paramCapture)
{
    paramCapture->dataNElements = n;
    CaptureGenHandlesImpl(n, reinterpret_cast<GLuint *>(handles), paramCapture);
}

template <typename T>
void CaptureArray(T *elements, GLsizei n, ParamCapture *paramCapture)
{
    paramCapture->dataNElements = n;
    CaptureMemory(elements, n * sizeof(T), paramCapture);
}

void CaptureShaderStrings(GLsizei count,
                          const GLchar *const *strings,
                          const GLint *length,
                          ParamCapture *paramCapture);

template <ParamType ParamT, typename T>
void WriteParamValueReplay(std::ostream &os, const CallCapture &call, T value);

template <>
void WriteParamValueReplay<ParamType::TGLboolean>(std::ostream &os,
                                                  const CallCapture &call,
                                                  GLboolean value);

template <>
void WriteParamValueReplay<ParamType::TGLbooleanPointer>(std::ostream &os,
                                                         const CallCapture &call,
                                                         GLboolean *value);

template <>
void WriteParamValueReplay<ParamType::TvoidConstPointer>(std::ostream &os,
                                                         const CallCapture &call,
                                                         const void *value);

template <>
void WriteParamValueReplay<ParamType::TvoidPointer>(std::ostream &os,
                                                    const CallCapture &call,
                                                    void *value);

template <>
void WriteParamValueReplay<ParamType::TGLfloatConstPointer>(std::ostream &os,
                                                            const CallCapture &call,
                                                            const GLfloat *value);

template <>
void WriteParamValueReplay<ParamType::TGLintConstPointer>(std::ostream &os,
                                                          const CallCapture &call,
                                                          const GLint *value);

template <>
void WriteParamValueReplay<ParamType::TGLsizeiPointer>(std::ostream &os,
                                                       const CallCapture &call,
                                                       GLsizei *value);

template <>
void WriteParamValueReplay<ParamType::TGLuintConstPointer>(std::ostream &os,
                                                           const CallCapture &call,
                                                           const GLuint *value);

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROCKHR>(std::ostream &os,
                                                       const CallCapture &call,
                                                       GLDEBUGPROCKHR value);

template <>
void WriteParamValueReplay<ParamType::TGLDEBUGPROC>(std::ostream &os,
                                                    const CallCapture &call,
                                                    GLDEBUGPROC value);

template <>
void WriteParamValueReplay<ParamType::TBufferID>(std::ostream &os,
                                                 const CallCapture &call,
                                                 gl::BufferID value);

template <>
void WriteParamValueReplay<ParamType::TFenceNVID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::FenceNVID value);

template <>
void WriteParamValueReplay<ParamType::TFramebufferID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::FramebufferID value);

template <>
void WriteParamValueReplay<ParamType::TMemoryObjectID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::MemoryObjectID value);

template <>
void WriteParamValueReplay<ParamType::TProgramPipelineID>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::ProgramPipelineID value);

template <>
void WriteParamValueReplay<ParamType::TQueryID>(std::ostream &os,
                                                const CallCapture &call,
                                                gl::QueryID value);

template <>
void WriteParamValueReplay<ParamType::TRenderbufferID>(std::ostream &os,
                                                       const CallCapture &call,
                                                       gl::RenderbufferID value);

template <>
void WriteParamValueReplay<ParamType::TSamplerID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::SamplerID value);

template <>
void WriteParamValueReplay<ParamType::TSemaphoreID>(std::ostream &os,
                                                    const CallCapture &call,
                                                    gl::SemaphoreID value);

template <>
void WriteParamValueReplay<ParamType::TShaderProgramID>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::ShaderProgramID value);

template <>
void WriteParamValueReplay<ParamType::TTextureID>(std::ostream &os,
                                                  const CallCapture &call,
                                                  gl::TextureID value);

template <>
void WriteParamValueReplay<ParamType::TTransformFeedbackID>(std::ostream &os,
                                                            const CallCapture &call,
                                                            gl::TransformFeedbackID value);

template <>
void WriteParamValueReplay<ParamType::TVertexArrayID>(std::ostream &os,
                                                      const CallCapture &call,
                                                      gl::VertexArrayID value);

template <>
void WriteParamValueReplay<ParamType::TUniformLocation>(std::ostream &os,
                                                        const CallCapture &call,
                                                        gl::UniformLocation value);

template <>
void WriteParamValueReplay<ParamType::TUniformBlockIndex>(std::ostream &os,
                                                          const CallCapture &call,
                                                          gl::UniformBlockIndex value);

template <>
void WriteParamValueReplay<ParamType::TGLsync>(std::ostream &os,
                                               const CallCapture &call,
                                               GLsync value);

template <>
void WriteParamValueReplay<ParamType::TGLeglImageOES>(std::ostream &os,
                                                      const CallCapture &call,
                                                      GLeglImageOES value);

template <>
void WriteParamValueReplay<ParamType::TGLubyte>(std::ostream &os,
                                                const CallCapture &call,
                                                GLubyte value);

template <>
void WriteParamValueReplay<ParamType::TEGLContext>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLContext value);

template <>
void WriteParamValueReplay<ParamType::TEGLDisplay>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLContext value);

template <>
void WriteParamValueReplay<ParamType::TEGLSurface>(std::ostream &os,
                                                   const CallCapture &call,
                                                   EGLContext value);

template <>
void WriteParamValueReplay<ParamType::TEGLDEBUGPROCKHR>(std::ostream &os,
                                                        const CallCapture &call,
                                                        EGLDEBUGPROCKHR value);

template <>
void WriteParamValueReplay<ParamType::TEGLGetBlobFuncANDROID>(std::ostream &os,
                                                              const CallCapture &call,
                                                              EGLGetBlobFuncANDROID value);

template <>
void WriteParamValueReplay<ParamType::TEGLSetBlobFuncANDROID>(std::ostream &os,
                                                              const CallCapture &call,
                                                              EGLSetBlobFuncANDROID value);
template <>
void WriteParamValueReplay<ParamType::TEGLClientBuffer>(std::ostream &os,
                                                        const CallCapture &call,
                                                        EGLClientBuffer value);

// General fallback for any unspecific type.
template <ParamType ParamT, typename T>
void WriteParamValueReplay(std::ostream &os, const CallCapture &call, T value)
{
    os << value;
}
}  // namespace angle

template <typename T>
void CaptureTextureAndSamplerParameter_params(GLenum pname,
                                              const T *param,
                                              angle::ParamCapture *paramCapture)
{
    if (pname == GL_TEXTURE_BORDER_COLOR || pname == GL_TEXTURE_CROP_RECT_OES)
    {
        CaptureMemory(param, sizeof(T) * 4, paramCapture);
    }
    else
    {
        CaptureMemory(param, sizeof(T), paramCapture);
    }
}

#endif  // LIBANGLE_FRAME_CAPTURE_H_
