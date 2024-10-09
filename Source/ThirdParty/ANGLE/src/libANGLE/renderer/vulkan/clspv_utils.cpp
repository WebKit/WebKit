//
// Copyright 2024 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Utilities to map clspv interface variables to OpenCL and Vulkan mappings.
//

#include "libANGLE/renderer/vulkan/clspv_utils.h"
#include "libANGLE/renderer/vulkan/CLDeviceVk.h"

#include <string>

#include "common/string_utils.h"

#include "CL/cl_half.h"

namespace rx
{
constexpr std::string_view kPrintfConversionSpecifiers = "diouxXfFeEgGaAcsp";
constexpr std::string_view kPrintfFlagsSpecifiers      = "-+ #0";
constexpr std::string_view kPrintfPrecisionSpecifiers  = "123456789.";
constexpr std::string_view kPrintfVectorSizeSpecifiers = "23468";

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

char getPrintfConversionSpecifier(std::string_view formatString)
{
    return formatString.at(formatString.find_first_of(kPrintfConversionSpecifiers));
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

    char conversion = std::tolower(getPrintfConversionSpecifier(formatString));
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
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            cl_half_to_float(ReadPtrAs<cl_half>(data)));
                else if (size == 4)
                    bytesWritten =
                        snprintf(out.data(), outSize, formatString.c_str(), ReadPtrAs<float>(data));
                else
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<double>(data));
                break;
            }
            default:
            {
                if (size == 1)
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint8_t>(data));
                else if (size == 2)
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint16_t>(data));
                else if (size == 4)
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint32_t>(data));
                else
                    bytesWritten = snprintf(out.data(), outSize, formatString.c_str(),
                                            ReadPtrAs<uint64_t>(data));
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

    size_t conversionPos = formatString.find_first_of(kPrintfConversionSpecifiers);
    // keep everything after conversion specifier in remainingFormat
    std::string remainingFormat = formatString.substr(conversionPos + 1);
    formatString                = formatString.substr(0, conversionPos + 1);

    size_t vectorPos       = formatString.find_first_of('v');
    size_t vectorLengthPos = ++vectorPos;
    size_t vectorLengthPosEnd =
        formatString.find_first_not_of(kPrintfVectorSizeSpecifiers, vectorLengthPos);

    std::string preVectorString  = formatString.substr(0, vectorPos - 1);
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
    uint32_t printfID               = ReadPtrAsAndIncrement<uint32_t>(data);
    const std::string &formatString = descs->at(printfID).formatSpecifier;

    std::string printfOutput = "";

    // formatString could be "<string literal> <% format specifiers ...> <string literal>"
    // print the literal part if any first
    size_t nextFormatSpecPos = formatString.find_first_of('%');
    printfOutput += formatString.substr(0, nextFormatSpecPos);

    // print each <% format specifier> + any string literal separately using snprintf
    size_t idx = 0;
    while (nextFormatSpecPos < formatString.size() - 1)
    {
        // Get the part of the format string before the next format specifier
        size_t partStart             = nextFormatSpecPos;
        size_t partEnd               = formatString.find_first_of('%', partStart + 1);
        std::string partFormatString = formatString.substr(partStart, partEnd - partStart);

        // Handle special cases
        if (partEnd == partStart + 1)
        {
            printfOutput += "%";
            nextFormatSpecPos = partEnd + 1;
            continue;
        }
        else if (partEnd == std::string::npos && idx >= descs->at(printfID).argSizes.size())
        {
            // If there are no remaining arguments, the rest of the format
            // should be printed verbatim
            printfOutput += partFormatString;
            break;
        }

        // The size of the argument that this format part will consume
        const uint32_t &size = descs->at(printfID).argSizes[idx];

        if (data + size > dataEnd)
        {
            data += size;
            return;
        }

        // vector format need special care for snprintf
        if (!IsVectorFormat(partFormatString))
        {
            // not a vector format can be printed through snprintf
            // except for %s
            if (getPrintfConversionSpecifier(partFormatString) == 's')
            {
                uint32_t stringID = ReadPtrAs<uint32_t>(data);
                printfOutput +=
                    PrintFormattedString(partFormatString,
                                         reinterpret_cast<const unsigned char *>(
                                             descs->at(stringID).formatSpecifier.c_str()),
                                         size);
            }
            else
            {
                printfOutput += PrintFormattedString(partFormatString, data, size);
            }
            data += size;
        }
        else
        {
            printfOutput += PrintVectorFormatIntoString(partFormatString, data, size);
            data += size;
        }

        // Move to the next format part and prepare to handle the next arg
        nextFormatSpecPos = partEnd;
        idx++;
    }

    std::printf("%s", printfOutput.c_str());
}

}  // namespace

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

    cl_uint addressBits;
    if (IsError(device->getInfoUInt(cl::DeviceInfo::AddressBits, &addressBits)))
    {
        // This should'nt fail here
        ASSERT(false);
    }
    options += addressBits == 64 ? " -arch=spir64" : " -arch=spir";

    // Other internal Clspv compiler flags that are needed/required
    options += " --long-vector";
    options += " --global-offset";
    options += " --enable-printf";

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

    return options;
}

}  // namespace rx
