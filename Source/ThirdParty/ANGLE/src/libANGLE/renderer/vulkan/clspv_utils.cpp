//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to map clspv interface variables to OpenCL and Vulkan mappings.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"

#include "libANGLE/CLDevice.h"

#include "clspv/Compiler.h"

#include "spirv-tools/libspirv.h"
#include "spirv-tools/libspirv.hpp"
#include "spirv-tools/optimizer.hpp"
#include "spirv/unified1/NonSemanticClspvReflection.h"
#include "spirv/unified1/spirv.hpp"

#include <mutex>
#include <string>
#include <string_view>

#if defined(ANGLE_ENABLE_ASSERTS)
constexpr bool kAngleDebug = true;
#else
constexpr bool kAngleDebug = false;
#endif

namespace rx
{
constexpr std::string_view kPrintfConversionSpecifiers = "diouxXfFeEgGaAcsp%";
constexpr std::string_view kPrintfFlagsSpecifiers      = "-+ #0";
constexpr std::string_view kPrintfPrecisionSpecifiers  = "123456789.";
constexpr std::string_view kPrintfVectorSizeSpecifiers = "2346816";

namespace
{

template <typename T>
T ReadPtrAs(const unsigned char *data)
{
    return *(reinterpret_cast<const T *>(data));
}

template <typename T>
T ReadPtrAsAndIncrement(unsigned char *&data)
{
    T out = *(reinterpret_cast<T *>(data));
    data += sizeof(T);
    return out;
}

bool IsVectorFormat(std::string_view formatString)
{
    ASSERT(formatString.at(0) == '%');

    // go past the flags, field width and precision
    size_t pos = formatString.find_first_not_of(kPrintfFlagsSpecifiers, 1ul);
    pos        = formatString.find_first_not_of(kPrintfPrecisionSpecifiers, pos);

    return (formatString.at(pos) == 'v');
}

// Printing an individual formatted string into a std::string
// snprintf is used for parsing as OpenCL C printf is similar to printf
std::string PrintFormattedString(const std::string &formatString,
                                 const unsigned char *data,
                                 size_t size)
{
    ASSERT(std::count(formatString.begin(), formatString.end(), '%') == 1);

    size_t outSize = 1024;
    std::vector<char> out(outSize);
    out[0] = '\0';

    char conversion = std::tolower(formatString.back());
    bool finished   = false;
    while (!finished)
    {
        int bytesWritten = 0;
        switch (conversion)
        {
            case 's':
            {
                bytesWritten = snprintf(out.data(), outSize, formatString.c_str(), data);
                break;
            }
            case 'f':
            case 'e':
            case 'g':
            case 'a':
            {
                // all floats with same convention as snprintf
                if (size == 2)
                {
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            cl_half_to_float(ReadPtrAs<cl_half>(data)));
                }
                else if (size == 4)
                {
                    bytesWritten =
                        snprintf(out.data(), outSize, formatString.c_str(), ReadPtrAs<float>(data));
                }
                else
                {
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<double>(data));
                }
                break;
            }
            default:
            {
                if (size == 1)
                {
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint8_t>(data));
                }
                else if (size == 2)
                {
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint16_t>(data));
                }
                else if (size == 4)
                {
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint32_t>(data));
                }
                else
                {
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint64_t>(data));
                }
                break;
            }
        }
        if (bytesWritten < 0)
        {
            out[0]   = '\0';
            finished = true;
        }
        else if (bytesWritten < static_cast<long>(outSize))
        {
            finished = true;
        }
        else
        {
            // insufficient size redo above post increment of size
            outSize *= 2;
            out.resize(outSize);
        }
    }

    return std::string(out.data());
}

// Spec mention vn modifier to be printed in the form v1,v2...vn
std::string PrintVectorFormatIntoString(std::string formatString,
                                        const unsigned char *data,
                                        const uint32_t size)
{
    ASSERT(IsVectorFormat(formatString));

    size_t conversionPos = formatString.length() - 1;
    // keep everything after conversion specifier in remainingFormat
    std::string remainingFormat = formatString.substr(conversionPos + 1);
    formatString                = formatString.substr(0, conversionPos + 1);

    size_t vectorPos       = formatString.find_first_of('v');
    size_t vectorLengthPos = vectorPos + 1;
    size_t vectorLengthPosEnd =
        formatString.find_first_not_of(kPrintfVectorSizeSpecifiers, vectorLengthPos);

    std::string preVectorString  = formatString.substr(0, vectorPos);
    std::string postVectorString = formatString.substr(vectorLengthPosEnd, formatString.size());
    std::string vectorLengthStr  = formatString.substr(vectorLengthPos, vectorLengthPosEnd);
    int vectorLength             = std::atoi(vectorLengthStr.c_str());

    // skip the vector specifier
    formatString = preVectorString + postVectorString;

    // Get the length modifier
    int elementSize = 0;
    if (postVectorString.find("hh") != std::string::npos)
    {
        elementSize = 1;
    }
    else if (postVectorString.find("hl") != std::string::npos)
    {
        elementSize = 4;
        // snprintf doesn't recognize the hl modifier so strip it
        size_t hl = formatString.find("hl");
        formatString.erase(hl, 2);
    }
    else if (postVectorString.find("h") != std::string::npos)
    {
        elementSize = 2;
    }
    else if (postVectorString.find("l") != std::string::npos)
    {
        elementSize = 8;
    }
    else
    {
        WARN() << "Vector specifier is used without a length modifier. Guessing it from "
                  "vector length and argument sizes in PrintInfo. Kernel modification is "
                  "recommended.";
        elementSize = size / vectorLength;
    }

    std::string out{""};
    for (int i = 0; i < vectorLength - 1; i++)
    {
        out += PrintFormattedString(formatString, data, size / vectorLength) + ",";
        data += elementSize;
    }
    out += PrintFormattedString(formatString, data, size / vectorLength) + remainingFormat;

    return out;
}

// Process the printf stream by breaking them down into individual format specifier and processing
// them.
void ProcessPrintfStatement(unsigned char *&data,
                            const angle::HashMap<uint32_t, ClspvPrintfInfo> *descs,
                            const unsigned char *dataEnd)
{
    // printf storage buffer contents - | id | formatString | argSizes... |
    uint32_t printfID = ReadPtrAsAndIncrement<uint32_t>(data);

    const std::string &formatString       = descs->at(printfID).formatSpecifier;
    const std::vector<uint32_t> &argSizes = descs->at(printfID).argSizes;

    if (formatString.find_first_of('%') == std::string::npos)
    {
        data = const_cast<unsigned char *>(dataEnd);
        std::printf("%s", formatString.c_str());
        return;
    }

    // process each char from the format string
    std::string printfOutput = "";
    size_t argIdx            = 0;
    size_t formatIdx         = 0;
    const size_t formatLen   = formatString.length();
    while (formatIdx < formatLen)
    {
        if (formatString[formatIdx] == '%' && (formatIdx + 1) < formatLen)
        {
            const size_t formatSpecifierEndPos =
                formatString.find_first_of(kPrintfConversionSpecifiers, formatIdx + 1);
            if (ANGLE_UNLIKELY(formatSpecifierEndPos == std::string::npos))
            {
                WARN() << "malformed/trailing format specifier. skipping...";
                data = const_cast<unsigned char *>(dataEnd);
                return;
            }

            std::string_view formatSpecifier =
                std::string_view(&formatString[formatIdx], (formatSpecifierEndPos - formatIdx) + 1);

            if (formatSpecifier == "%%")
            {  // special case where "%%" specifier is treated as literal "%"
                printfOutput += "%";
                formatIdx += formatSpecifier.size();
                continue;
            }

            // The size of the argument that this format part will consume
            const uint32_t &size = argSizes[argIdx++];
            if (data + size > dataEnd)
            {
                data += size;
                return;
            }

            if (!IsVectorFormat(formatSpecifier))
            {
                if (formatSpecifier.back() == 's')
                {
                    uint32_t stringID = ReadPtrAs<uint32_t>(data);
                    printfOutput +=
                        PrintFormattedString(std::string{formatSpecifier},
                                             reinterpret_cast<const unsigned char *>(
                                                 descs->at(stringID).formatSpecifier.c_str()),
                                             size);
                }
                else
                {
                    printfOutput += PrintFormattedString(std::string{formatSpecifier}, data, size);
                }
                data += size;
            }
            else
            {
                printfOutput +=
                    PrintVectorFormatIntoString(std::string{formatSpecifier}, data, size);
                data += size;
            }
            formatIdx += formatSpecifier.size();
        }
        else
        {
            printfOutput += formatString[formatIdx];
            formatIdx++;
        }
    }

    std::printf("%s", printfOutput.c_str());

    if (kAngleDebug)
    {  // note/fyi: this log will break conformance testing
        INFO() << "ANGLE-CL.Kernel: " << printfOutput.c_str();
    }
}

std::string GetSpvVersionAsClspvString(spv_target_env spvVersion)
{
    switch (spvVersion)
    {
        default:
        case SPV_ENV_VULKAN_1_0:
            return "1.0";
        case SPV_ENV_VULKAN_1_1:
            return "1.3";
        case SPV_ENV_VULKAN_1_1_SPIRV_1_4:
            return "1.4";
        case SPV_ENV_VULKAN_1_2:
            return "1.5";
        case SPV_ENV_VULKAN_1_3:
            return "1.6";
    }
}

std::vector<std::string> GetNativeBuiltins(const vk::Renderer *renderer)
{
    if (renderer->getFeatures().usesNativeBuiltinClKernel.enabled)
    {
        return std::vector<std::string>({"fma", "half_exp2", "exp2"});
    }

    return {};
}
}  // anonymous namespace

namespace clspv_cl
{

cl::AddressingMode GetAddressingMode(uint32_t mask)
{
    cl::AddressingMode addressingMode = cl::AddressingMode::Clamp;

    switch (mask & clspv::kSamplerAddressMask)
    {
        case clspv::CLK_ADDRESS_NONE:
        default:
            addressingMode =
                cl::FromCLenum<cl::AddressingMode>(static_cast<CLenum>(CL_ADDRESS_NONE));
            break;
        case clspv::CLK_ADDRESS_CLAMP_TO_EDGE:
            addressingMode =
                cl::FromCLenum<cl::AddressingMode>(static_cast<CLenum>(CL_ADDRESS_CLAMP_TO_EDGE));
            break;
        case clspv::CLK_ADDRESS_CLAMP:
            addressingMode =
                cl::FromCLenum<cl::AddressingMode>(static_cast<CLenum>(CL_ADDRESS_CLAMP));
            break;
        case clspv::CLK_ADDRESS_MIRRORED_REPEAT:
            addressingMode =
                cl::FromCLenum<cl::AddressingMode>(static_cast<CLenum>(CL_ADDRESS_MIRRORED_REPEAT));
            break;
        case clspv::CLK_ADDRESS_REPEAT:
            addressingMode =
                cl::FromCLenum<cl::AddressingMode>(static_cast<CLenum>(CL_ADDRESS_REPEAT));
            break;
    }

    return addressingMode;
}

cl::FilterMode GetFilterMode(uint32_t mask)
{
    cl::FilterMode filterMode = cl::FilterMode::Nearest;

    switch (mask & clspv::kSamplerFilterMask)
    {
        case clspv::CLK_FILTER_NEAREST:
        default:
            filterMode = cl::FromCLenum<cl::FilterMode>(static_cast<CLenum>(CL_FILTER_NEAREST));
            break;
        case clspv::CLK_FILTER_LINEAR:
            filterMode = cl::FromCLenum<cl::FilterMode>(static_cast<CLenum>(CL_FILTER_LINEAR));
            break;
    }

    return filterMode;
}

}  // namespace clspv_cl

// Process the data recorded into printf storage buffer along with the info in printfino descriptor
// and write it to stdout.
angle::Result ClspvProcessPrintfBuffer(unsigned char *buffer,
                                       const size_t bufferSize,
                                       const angle::HashMap<uint32_t, ClspvPrintfInfo> *infoMap)
{
    // printf storage buffer contains a series of uint32_t values
    // the first integer is offset from second to next available free memory -- this is the amount
    // of data written by kernel.
    const size_t bytesWritten = ReadPtrAsAndIncrement<uint32_t>(buffer) * sizeof(uint32_t);
    const size_t dataSize     = bufferSize - sizeof(uint32_t);
    const size_t limit        = std::min(bytesWritten, dataSize);

    const unsigned char *dataEnd = buffer + limit;
    while (buffer < dataEnd)
    {
        ProcessPrintfStatement(buffer, infoMap, dataEnd);
    }

    if (bufferSize < bytesWritten)
    {
        WARN() << "Printf storage buffer was not sufficient for all printfs. Around "
               << 100.0 * (float)(bytesWritten - bufferSize) / bytesWritten
               << "% of them have been skipped.";
    }

    return angle::Result::Continue;
}

std::string ClspvGetCompilerOptions(const CLDeviceVk *device)
{
    ASSERT(device && device->getRenderer());
    const vk::Renderer *rendererVk = device->getRenderer();
    std::string options{""};
    std::vector<std::string> featureMacros;

    cl_uint addressBits;
    if (IsError(device->getInfoUInt(cl::DeviceInfo::AddressBits, &addressBits)))
    {
        // This shouldn't fail here
        ASSERT(false);
    }
    options += addressBits == 64 ? " -arch=spir64" : " -arch=spir";
    if (rendererVk->getFeatures().supportsBufferDeviceAddress.enabled)
    {
        ASSERT(addressBits == 64);
        options += " -physical-storage-buffers ";
    }

    // select SPIR-V version target
    options += " --spv-version=" + GetSpvVersionAsClspvString(device->getSpirvVersion());

    cl_uint nonUniformNDRangeSupport;
    if (IsError(device->getInfoUInt(cl::DeviceInfo::NonUniformWorkGroupSupport,
                                    &nonUniformNDRangeSupport)))
    {
        // This shouldn't fail here
        ASSERT(false);
    }
    // This "cl-arm-non-uniform-work-group-size" flag is needed to generate region reflection
    // instructions since clspv builtin pass is conditionally dependant on it:
    /*
        bool NonUniformNDRangeSupported() {
            return ((Language() == SourceLanguage::OpenCL_CPP) ||
                    (Language() == SourceLanguage::OpenCL_C_20) ||
                    (Language() == SourceLanguage::OpenCL_C_30) ||
                    ArmNonUniformWorkGroupSize()) &&
                    !UniformWorkgroupSize();
        }
        ...
            Value *Ret = GidBase;
            if (clspv::Option::NonUniformNDRangeSupported()) {
                auto Ptr = GetPushConstantPointer(BB, clspv::PushConstant::RegionOffset);
                auto DimPtr = Builder.CreateInBoundsGEP(VT, Ptr, Indices);
                auto Size = Builder.CreateLoad(IT, DimPtr);
                ...
    */
    options += nonUniformNDRangeSupport == CL_TRUE ? " -cl-arm-non-uniform-work-group-size" : "";

    // Other internal Clspv compiler flags that are needed/required
    options += " --long-vector";
    options += " --global-offset";
    options += " --enable-printf";
    options += " --cl-kernel-arg-info";

    // add opencl atomic feature macros
    featureMacros.push_back("__opencl_c_atomic_order_acq_rel");
    featureMacros.push_back("__opencl_c_atomic_order_seq_cst");
    featureMacros.push_back("__opencl_c_atomic_scope_device");

    // check for int8 support
    if (rendererVk->getFeatures().supportsShaderInt8.enabled)
    {
        options += " --int8 --rewrite-packed-structs";
    }

    // 8 bit storage buffer support
    if (!rendererVk->getFeatures().supports8BitStorageBuffer.enabled)
    {
        options += " --no-8bit-storage=ssbo";
    }
    if (!rendererVk->getFeatures().supports8BitUniformAndStorageBuffer.enabled)
    {
        options += " --no-8bit-storage=ubo";
    }
    if (!rendererVk->getFeatures().supports8BitPushConstant.enabled)
    {
        options += " --no-8bit-storage=pushconstant";
    }

    // 16 bit storage options
    if (!rendererVk->getFeatures().supports16BitStorageBuffer.enabled)
    {
        options += " --no-16bit-storage=ssbo";
    }
    if (!rendererVk->getFeatures().supports16BitUniformAndStorageBuffer.enabled)
    {
        options += " --no-16bit-storage=ubo";
    }
    if (!rendererVk->getFeatures().supports16BitPushConstant.enabled)
    {
        options += " --no-16bit-storage=pushconstant";
    }

    if (rendererVk->getFeatures().supportsUniformBufferStandardLayout.enabled)
    {
        options += " --std430-ubo-layout";
    }

    std::string nativeBuiltins{""};
    for (const std::string &builtin : GetNativeBuiltins(rendererVk))
    {
        nativeBuiltins += builtin + ",";
    }
    options += " --use-native-builtins=" + nativeBuiltins;
    std::vector<std::string> rteModes;
    if (rendererVk->getFeatures().supportsRoundingModeRteFp32.enabled)
    {
        rteModes.push_back("32");
    }
    if (rendererVk->getFeatures().supportsShaderFloat16.enabled)
    {
        options += " --fp16";
        if (rendererVk->getFeatures().supportsRoundingModeRteFp16.enabled)
        {
            rteModes.push_back("16");
        }
    }
    if (rendererVk->getFeatures().supportsShaderFloat64.enabled)
    {
        options += " --fp64";
        featureMacros.push_back("__opencl_c_fp64");
        if (rendererVk->getFeatures().supportsRoundingModeRteFp64.enabled)
        {
            rteModes.push_back("64");
        }
    }
    else
    {
        options += " --fp64=0";
    }

    if (device->getFrontendObject().getInfo().imageSupport)
    {
        featureMacros.push_back("__opencl_c_images");
        featureMacros.push_back("__opencl_c_3d_image_writes");
        featureMacros.push_back("__opencl_c_read_write_images");
    }

    if (rendererVk->getFeatures().supportsBufferDeviceAddress.enabled)
    {
        // It is for generating ConstantDataStorageBuffer without -physical-storage-buffers,
        // ConstantDataPointerPushConstant with -physical-storage-buffers
        // TODO: this flag is only on in case of supportsBufferDeviceAddress.enabled
        // until ConstantDataStorageBuffer will be implemented.
        // http://anglebug.com/442950569
        options += " -module-constants-in-storage-buffer";
    }

    if (rendererVk->getEnabledFeatures().features.shaderInt64)
    {
        featureMacros.push_back("__opencl_c_int64");
    }

    if (rendererVk->getFeatures().supportsShaderIntegerDotProduct.enabled)
    {
        featureMacros.push_back("__opencl_c_integer_dot_product_input_4x8bit");
        featureMacros.push_back("__opencl_c_integer_dot_product_input_4x8bit_packed");
    }

    if (device->getFrontendObject().getInfo().khrSubgroups)
    {
        featureMacros.push_back("__opencl_c_subgroups");
    }

    if (!rteModes.empty())
    {
        options += " --rounding-mode-rte=";
        options += std::reduce(std::next(rteModes.begin()), rteModes.end(), rteModes[0],
                               [](const auto a, const auto b) { return a + "," + b; });
    }
    if (!featureMacros.empty())
    {
        options += " --enable-feature-macros=";
        options +=
            std::reduce(std::next(featureMacros.begin()), featureMacros.end(), featureMacros[0],
                        [](const std::string a, const std::string b) { return a + "," + b; });
    }

    return options;
}

// A locked wrapper for clspvCompileFromSourcesString - the underneath LLVM parser is non-rentrant.
// So protecting it with mutex.
ClspvError ClspvCompileSource(const size_t programCount,
                              const size_t *programSizes,
                              const char **programs,
                              const char *options,
                              char **outputBinary,
                              size_t *outputBinarySize,
                              char **outputLog)
{
    [[clang::no_destroy]] static angle::SimpleMutex mtx;

    std::lock_guard<angle::SimpleMutex> lock(mtx);

    return clspvCompileFromSourcesString(programCount, programSizes, programs, options,
                                         outputBinary, outputBinarySize, outputLog);
}

spv_target_env ClspvGetSpirvVersion(const vk::Renderer *renderer)
{
    uint32_t vulkanApiVersion = renderer->getDeviceVersion();
    if (vulkanApiVersion < VK_API_VERSION_1_1)
    {
        // Minimum supported Vulkan version is 1.1 by Angle
        UNREACHABLE();
        return SPV_ENV_MAX;
    }
    else if (vulkanApiVersion < VK_API_VERSION_1_2)
    {
        // TODO: Might be worthwhile to make Vulkan 1.3 as minimum requirement
        // http://anglebug.com/383824579
        if (renderer->getFeatures().supportsSPIRV14.enabled)
        {
            return SPV_ENV_VULKAN_1_1_SPIRV_1_4;
        }
        return SPV_ENV_VULKAN_1_1;
    }
    else if (vulkanApiVersion < VK_API_VERSION_1_3)
    {
        return SPV_ENV_VULKAN_1_2;
    }
    else
    {
        // return the latest supported version
        return SPV_ENV_VULKAN_1_3;
    }
}

bool ClspvValidate(vk::Renderer *rendererVk, const angle::spirv::Blob &blob)
{
    spvtools::SpirvTools spvTool(ClspvGetSpirvVersion(rendererVk));
    spvTool.SetMessageConsumer([](spv_message_level_t level, const char *,
                                  const spv_position_t &position, const char *message) {
        switch (level)
        {
            case SPV_MSG_FATAL:
            case SPV_MSG_ERROR:
            case SPV_MSG_INTERNAL_ERROR:
                ERR() << "SPV validation error (" << position.line << "." << position.column
                      << "): " << message;
                break;
            case SPV_MSG_WARNING:
                WARN() << "SPV validation warning (" << position.line << "." << position.column
                       << "): " << message;
                break;
            case SPV_MSG_INFO:
                INFO() << "SPV validation info (" << position.line << "." << position.column
                       << "): " << message;
                break;
            case SPV_MSG_DEBUG:
                INFO() << "SPV validation debug (" << position.line << "." << position.column
                       << "): " << message;
                break;
            default:
                UNREACHABLE();
                break;
        }
    });

    spvtools::ValidatorOptions options;
    if (rendererVk->getFeatures().supportsUniformBufferStandardLayout.enabled)
    {
        // Allow UBO layouts that conform to std430 (SSBO) layout requirements
        options.SetUniformBufferStandardLayout(true);
    }

    return spvTool.Validate(blob.data(), blob.size(), options);
}

bool ClspvParseReflection(vk::Renderer *rendererVk,
                          const angle::spirv::Blob &blob,
                          ClspvReflectionData &reflectionDataOut)
{
    spvtools::SpirvTools spvTool(ClspvGetSpirvVersion(rendererVk));
    spvTool.SetMessageConsumer([](spv_message_level_t level, const char *,
                                  const spv_position_t &position, const char *message) {
        switch (level)
        {
            case SPV_MSG_FATAL:
            case SPV_MSG_ERROR:
            case SPV_MSG_INTERNAL_ERROR:
                ERR() << "SPV Reflection parse error: " << message;
                break;
            case SPV_MSG_WARNING:
                WARN() << "SPV Reflection parse warn: " << message;
                break;
            case SPV_MSG_INFO:
                INFO() << "SPV Reflection parse info: " << message;
                break;
            case SPV_MSG_DEBUG:
                INFO() << "SPV Reflection parse debug: " << message;
                break;
            default:
                UNREACHABLE();
                break;
        }
    });

    return spvTool.Parse(
        blob,
        [](const spv_endianness_t endianess, const spv_parsed_header_t &instruction) {
            return SPV_SUCCESS;  // nothing to do for parsing header
        },
        [&reflectionDataOut](const spv_parsed_instruction_t &instruction) {
            switch (instruction.opcode)
            {
                case spv::OpExtInst:
                {
                    if (instruction.ext_inst_type != SPV_EXT_INST_TYPE_NONSEMANTIC_CLSPVREFLECTION)
                    {
                        break;  // ignore any spv extension instructions that aren't non-semantic
                    }
                    switch (instruction.words[4])
                    {
                        case NonSemanticClspvReflectionKernel:
                        {
                            // Extract kernel name and args - add to kernel args map
                            std::string functionName =
                                reflectionDataOut.spvStrLookup[instruction.words[6]];
                            uint32_t numArgs = reflectionDataOut.spvIntLookup[instruction.words[7]];
                            reflectionDataOut.kernelArgsMap[functionName] = CLKernelArguments();
                            reflectionDataOut.kernelArgsMap[functionName].resize(numArgs);

                            // Store kernel flags and attributes
                            reflectionDataOut.kernelFlags[functionName] =
                                reflectionDataOut.spvIntLookup[instruction.words[8]];
                            reflectionDataOut.kernelAttributes[functionName] =
                                reflectionDataOut.spvStrLookup[instruction.words[9]];

                            // Save kernel name to reflection table for later use/lookup in parser
                            // routine
                            reflectionDataOut.kernelIDs.insert(instruction.words[2]);
                            reflectionDataOut.spvStrLookup[instruction.words[2]] =
                                std::string(functionName);

                            // If we already parsed some args ahead of time, populate them now
                            if (reflectionDataOut.kernelArgMap.contains(functionName))
                            {
                                for (const auto &arg : reflectionDataOut.kernelArgMap)
                                {
                                    uint32_t ordinal = arg.second.ordinal;
                                    reflectionDataOut.kernelArgsMap[functionName].at(ordinal) =
                                        std::move(arg.second);
                                }
                            }
                            break;
                        }
                        case NonSemanticClspvReflectionArgumentInfo:
                        {
                            CLKernelVk::ArgInfo kernelArgInfo;
                            kernelArgInfo.name =
                                reflectionDataOut.spvStrLookup[instruction.words[5]];
                            // If instruction has more than 5 instruction operands (minus
                            // instruction name/opcode), that means we have arg qualifiers.
                            // ArgumentInfo also counts as an operand for OpExtInst. In below
                            // example, [ %e %f %g %h ] are the arg qualifier operands.
                            //
                            // %a = OpExtInst %b %c ArgumentInfo %d [ %e %f %g %h ]
                            if (instruction.num_operands > 5)
                            {
                                kernelArgInfo.typeName =
                                    reflectionDataOut.spvStrLookup[instruction.words[6]];
                                kernelArgInfo.addressQualifier =
                                    reflectionDataOut.spvIntLookup[instruction.words[7]];
                                kernelArgInfo.accessQualifier =
                                    reflectionDataOut.spvIntLookup[instruction.words[8]];
                                kernelArgInfo.typeQualifier =
                                    reflectionDataOut.spvIntLookup[instruction.words[9]];
                            }
                            // Store kern arg for later lookup
                            reflectionDataOut.kernelArgInfos[instruction.words[2]] =
                                std::move(kernelArgInfo);
                            break;
                        }
                        case NonSemanticClspvReflectionArgumentPodUniform:
                        case NonSemanticClspvReflectionArgumentPointerUniform:
                        case NonSemanticClspvReflectionArgumentPodStorageBuffer:
                        {
                            CLKernelArgument kernelArg;
                            if (instruction.num_operands == 11)
                            {
                                const CLKernelVk::ArgInfo &kernelArgInfo =
                                    reflectionDataOut.kernelArgInfos[instruction.words[11]];
                                kernelArg.info.name             = kernelArgInfo.name;
                                kernelArg.info.typeName         = kernelArgInfo.typeName;
                                kernelArg.info.addressQualifier = kernelArgInfo.addressQualifier;
                                kernelArg.info.accessQualifier  = kernelArgInfo.accessQualifier;
                                kernelArg.info.typeQualifier    = kernelArgInfo.typeQualifier;
                            }
                            kernelArg.type = instruction.words[4];
                            kernelArg.used = true;
                            kernelArg.ordinal =
                                reflectionDataOut.spvIntLookup[instruction.words[6]];
                            kernelArg.op3 = reflectionDataOut.spvIntLookup[instruction.words[7]];
                            kernelArg.op4 = reflectionDataOut.spvIntLookup[instruction.words[8]];
                            kernelArg.op5 = reflectionDataOut.spvIntLookup[instruction.words[9]];
                            kernelArg.op6 = reflectionDataOut.spvIntLookup[instruction.words[10]];

                            if (reflectionDataOut.kernelIDs.contains(instruction.words[5]))
                            {
                                CLKernelArguments &kernelArgs =
                                    reflectionDataOut.kernelArgsMap
                                        [reflectionDataOut.spvStrLookup[instruction.words[5]]];
                                kernelArgs.at(kernelArg.ordinal) = std::move(kernelArg);
                            }
                            else
                            {
                                // Reflection kernel not yet parsed, place in temp storage for now
                                reflectionDataOut.kernelArgMap
                                    [reflectionDataOut.spvStrLookup[instruction.words[5]]] =
                                    std::move(kernelArg);
                            }

                            break;
                        }
                        case NonSemanticClspvReflectionArgumentUniform:
                        case NonSemanticClspvReflectionArgumentWorkgroup:
                        case NonSemanticClspvReflectionArgumentSampler:
                        case NonSemanticClspvReflectionArgumentStorageImage:
                        case NonSemanticClspvReflectionArgumentSampledImage:
                        case NonSemanticClspvReflectionArgumentStorageBuffer:
                        case NonSemanticClspvReflectionArgumentStorageTexelBuffer:
                        case NonSemanticClspvReflectionArgumentUniformTexelBuffer:
                        case NonSemanticClspvReflectionArgumentPodPushConstant:
                        case NonSemanticClspvReflectionArgumentPointerPushConstant:
                        {
                            CLKernelArgument kernelArg;
                            if (instruction.num_operands == 9)
                            {
                                const CLKernelVk::ArgInfo &kernelArgInfo =
                                    reflectionDataOut.kernelArgInfos[instruction.words[9]];
                                kernelArg.info.name             = kernelArgInfo.name;
                                kernelArg.info.typeName         = kernelArgInfo.typeName;
                                kernelArg.info.addressQualifier = kernelArgInfo.addressQualifier;
                                kernelArg.info.accessQualifier  = kernelArgInfo.accessQualifier;
                                kernelArg.info.typeQualifier    = kernelArgInfo.typeQualifier;
                            }

                            kernelArg.type = instruction.words[4];
                            kernelArg.used = true;
                            kernelArg.ordinal =
                                reflectionDataOut.spvIntLookup[instruction.words[6]];
                            kernelArg.op3 = reflectionDataOut.spvIntLookup[instruction.words[7]];
                            kernelArg.op4 = reflectionDataOut.spvIntLookup[instruction.words[8]];

                            if (reflectionDataOut.kernelIDs.contains(instruction.words[5]))
                            {
                                CLKernelArguments &kernelArgs =
                                    reflectionDataOut.kernelArgsMap
                                        [reflectionDataOut.spvStrLookup[instruction.words[5]]];
                                kernelArgs.at(kernelArg.ordinal) = std::move(kernelArg);
                            }
                            else
                            {
                                // Reflection kernel not yet parsed, place in temp storage for now
                                reflectionDataOut.kernelArgMap
                                    [reflectionDataOut.spvStrLookup[instruction.words[5]]] =
                                    std::move(kernelArg);
                            }
                            break;
                        }
                        case NonSemanticClspvReflectionPushConstantGlobalSize:
                        case NonSemanticClspvReflectionPushConstantGlobalOffset:
                        case NonSemanticClspvReflectionPushConstantRegionOffset:
                        case NonSemanticClspvReflectionPushConstantNumWorkgroups:
                        case NonSemanticClspvReflectionPushConstantRegionGroupOffset:
                        case NonSemanticClspvReflectionPushConstantEnqueuedLocalSize:
                        {
                            uint32_t offset = reflectionDataOut.spvIntLookup[instruction.words[5]];
                            uint32_t size   = reflectionDataOut.spvIntLookup[instruction.words[6]];
                            reflectionDataOut.pushConstants[instruction.words[4]] = {
                                .stageFlags = 0, .offset = offset, .size = size};
                            break;
                        }
                        case NonSemanticClspvReflectionSpecConstantWorkgroupSize:
                        {
                            reflectionDataOut.specConstantIDs[SpecConstantType::WorkgroupSizeX] =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            reflectionDataOut.specConstantIDs[SpecConstantType::WorkgroupSizeY] =
                                reflectionDataOut.spvIntLookup[instruction.words[6]];
                            reflectionDataOut.specConstantIDs[SpecConstantType::WorkgroupSizeZ] =
                                reflectionDataOut.spvIntLookup[instruction.words[7]];
                            reflectionDataOut.specConstantsUsed[SpecConstantType::WorkgroupSizeX] =
                                true;
                            reflectionDataOut.specConstantsUsed[SpecConstantType::WorkgroupSizeY] =
                                true;
                            reflectionDataOut.specConstantsUsed[SpecConstantType::WorkgroupSizeZ] =
                                true;
                            break;
                        }
                        case NonSemanticClspvReflectionPropertyRequiredWorkgroupSize:
                        {
                            reflectionDataOut.kernelCompileWorkgroupSize
                                [reflectionDataOut.spvStrLookup[instruction.words[5]]] = {
                                reflectionDataOut.spvIntLookup[instruction.words[6]],
                                reflectionDataOut.spvIntLookup[instruction.words[7]],
                                reflectionDataOut.spvIntLookup[instruction.words[8]]};
                            break;
                        }
                        case NonSemanticClspvReflectionSpecConstantWorkDim:
                        {
                            reflectionDataOut.specConstantIDs[SpecConstantType::WorkDimension] =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            reflectionDataOut.specConstantsUsed[SpecConstantType::WorkDimension] =
                                true;
                            break;
                        }
                        case NonSemanticClspvReflectionSpecConstantSubgroupMaxSize:
                        {
                            reflectionDataOut.specConstantIDs[SpecConstantType::SubgroupMaxSize] =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            reflectionDataOut.specConstantsUsed[SpecConstantType::SubgroupMaxSize] =
                                true;
                            break;
                        }
                        case NonSemanticClspvReflectionSpecConstantGlobalOffset:
                            reflectionDataOut.specConstantIDs[SpecConstantType::GlobalOffsetX] =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            reflectionDataOut.specConstantIDs[SpecConstantType::GlobalOffsetY] =
                                reflectionDataOut.spvIntLookup[instruction.words[6]];
                            reflectionDataOut.specConstantIDs[SpecConstantType::GlobalOffsetZ] =
                                reflectionDataOut.spvIntLookup[instruction.words[7]];
                            reflectionDataOut.specConstantsUsed[SpecConstantType::GlobalOffsetX] =
                                true;
                            reflectionDataOut.specConstantsUsed[SpecConstantType::GlobalOffsetY] =
                                true;
                            reflectionDataOut.specConstantsUsed[SpecConstantType::GlobalOffsetZ] =
                                true;
                            break;
                        case NonSemanticClspvReflectionConstantDataPointerPushConstant:
                        {
                            std::string data = reflectionDataOut.spvStrLookup[instruction.words[7]];

                            // Data must be an OpString that encodes the hexbytes of the constant
                            // data.
                            // https://github.khronos.org/SPIRV-Registry/nonsemantic/NonSemantic.ClspvReflection.html
                            // Each byte of binary data is represented by two hexadecimal digits, so
                            // the number of digits must be even
                            ASSERT(data.size() % 2 == 0);

                            reflectionDataOut.constantDataBufferInfo.bufferData =
                                angle::HexStringToUintVector(data);

                            uint32_t set      = 0;
                            uint32_t binding  = 0;
                            uint32_t pcOffset = 0;
                            pcOffset = reflectionDataOut.spvIntLookup[instruction.words[5]];

                            // Add push constant
                            reflectionDataOut.pushConstants[instruction.words[4]] = {
                                .stageFlags = 0, .offset = pcOffset, .size = CHAR_BIT};

                            // Set constant data buffer
                            reflectionDataOut.constantDataBufferInfo.set      = set;
                            reflectionDataOut.constantDataBufferInfo.binding  = binding;
                            reflectionDataOut.constantDataBufferInfo.pcOffset = pcOffset;

                            break;
                        }
                        case NonSemanticClspvReflectionPrintfInfo:
                        {
                            // Info on the format string used in the builtin printf call in kernel
                            uint32_t printfID =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            std::string formatString =
                                reflectionDataOut.spvStrLookup[instruction.words[6]];
                            reflectionDataOut.printfInfoMap[printfID].id = printfID;
                            reflectionDataOut.printfInfoMap[printfID].formatSpecifier =
                                formatString;
                            for (int i = 6; i < instruction.num_operands; i++)
                            {
                                uint16_t offset = instruction.operands[i].offset;
                                size_t size =
                                    reflectionDataOut.spvIntLookup[instruction.words[offset]];
                                reflectionDataOut.printfInfoMap[printfID].argSizes.push_back(
                                    static_cast<uint32_t>(size));
                            }

                            break;
                        }
                        case NonSemanticClspvReflectionPrintfBufferStorageBuffer:
                        {
                            // Info about the printf storage buffer that contains the formatted
                            // content
                            uint32_t set     = reflectionDataOut.spvIntLookup[instruction.words[5]];
                            uint32_t binding = reflectionDataOut.spvIntLookup[instruction.words[6]];
                            uint32_t size    = reflectionDataOut.spvIntLookup[instruction.words[7]];
                            reflectionDataOut.printfBufferStorage = {set, binding, 0, size};
                            break;
                        }
                        case NonSemanticClspvReflectionPrintfBufferPointerPushConstant:
                        {
                            uint32_t pcOffset =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            reflectionDataOut.pushConstants[instruction.words[4]] = {
                                .stageFlags = 0, .offset = pcOffset, .size = CHAR_BIT};
                            break;
                        }
                        case NonSemanticClspvReflectionNormalizedSamplerMaskPushConstant:
                        case NonSemanticClspvReflectionImageArgumentInfoChannelOrderPushConstant:
                        case NonSemanticClspvReflectionImageArgumentInfoChannelDataTypePushConstant:
                        {
                            uint32_t ordinal = reflectionDataOut.spvIntLookup[instruction.words[6]];
                            uint32_t offset  = reflectionDataOut.spvIntLookup[instruction.words[7]];
                            uint32_t size    = reflectionDataOut.spvIntLookup[instruction.words[8]];
                            VkPushConstantRange pcRange = {
                                .stageFlags = 0, .offset = offset, .size = size};
                            reflectionDataOut.imagePushConstants[instruction.words[4]].push_back(
                                {.pcRange = pcRange, .ordinal = ordinal});
                            break;
                        }
                        case NonSemanticClspvReflectionLiteralSampler:
                        {
                            uint32_t descriptorSet =
                                reflectionDataOut.spvIntLookup[instruction.words[5]];
                            ASSERT(descriptorSet <
                                   static_cast<uint32_t>(DescriptorSetIndex::EnumCount));
                            uint32_t binding = reflectionDataOut.spvIntLookup[instruction.words[6]];
                            uint32_t mask    = reflectionDataOut.spvIntLookup[instruction.words[7]];
                            cl_bool normalizedCoords          = clspv_cl::IsNormalizedCoords(mask);
                            cl::AddressingMode addressingMode = clspv_cl::GetAddressingMode(mask);
                            cl::FilterMode filterMode         = clspv_cl::GetFilterMode(mask);
                            reflectionDataOut.literalSamplers.push_back(
                                {.descriptorSet    = descriptorSet,
                                 .binding          = binding,
                                 .normalizedCoords = normalizedCoords,
                                 .addressingMode   = addressingMode,
                                 .filterMode       = filterMode});
                            break;
                        }
                        case NonSemanticClspvReflectionWorkgroupVariableSize:
                        {
                            auto size = reflectionDataOut.spvIntLookup[instruction.words[6]];
                            reflectionDataOut.workgroupVariableSize.size += size;
                            break;
                        }
                        default:
                            break;
                    }
                    break;
                }
                case spv::OpString:
                {
                    reflectionDataOut.spvStrLookup[instruction.words[1]] =
                        reinterpret_cast<const char *>(&instruction.words[2]);
                    break;
                }
                case spv::OpConstant:
                {
                    reflectionDataOut.spvIntLookup[instruction.words[2]] = instruction.words[3];
                    break;
                }
                default:
                    break;
            }
            return SPV_SUCCESS;
        });
}

bool ClspvStripReflection(vk::Renderer *rendererVk,
                          const angle::spirv::Blob &blob,
                          angle::spirv::Blob &outBlob)
{
    spvtools::OptimizerOptions optOptions;
    spvtools::Optimizer optTool(ClspvGetSpirvVersion(rendererVk));
    optTool.SetMessageConsumer([](spv_message_level_t level, const char *,
                                  const spv_position_t &position, const char *message) {
        switch (level)
        {
            case SPV_MSG_FATAL:
            case SPV_MSG_ERROR:
            case SPV_MSG_INTERNAL_ERROR:
                ERR() << "SPV Optimizer tool error: " << message;
                break;
            case SPV_MSG_WARNING:
                WARN() << "SPV Optimizer tool warn: " << message;
                break;
            case SPV_MSG_INFO:
                INFO() << "SPV Optimizer tool info: " << message;
                break;
            case SPV_MSG_DEBUG:
                INFO() << "SPV Optimizer tool debug: " << message;
                break;
            default:
                UNREACHABLE();
                break;
        }
    });
    optTool.RegisterPass(spvtools::CreateStripReflectInfoPass());
    optOptions.set_run_validator(false);

    return optTool.Run(blob.data(), blob.size(), &outBlob, optOptions);
}

}  // namespace rx
