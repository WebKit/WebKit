//
// Copyright 2019 The ANGLE Project. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "common.h"

constant bool kSourceBufferAligned[[function_constant(0)]];
constant bool kSourceIndexIsU8[[function_constant(1)]];
constant bool kSourceIndexIsU16[[function_constant(2)]];
constant bool kSourceIndexIsU32[[function_constant(3)]];
constant bool kSourceBufferUnaligned = !kSourceBufferAligned;
constant bool kUseSourceBufferU8     = kSourceIndexIsU8 || kSourceBufferUnaligned;
constant bool kUseSourceBufferU16    = kSourceIndexIsU16 && kSourceBufferAligned;
constant bool kUseSourceBufferU32    = kSourceIndexIsU32 && kSourceBufferAligned;

struct IndexConversionParams
{
    uint32_t srcOffset;  // offset in bytes
    uint32_t indexCount;
};

#define ANGLE_IDX_CONVERSION_GUARD(IDX, OPTS) ANGLE_KERNEL_GUARD(IDX, OPTS.indexCount)

inline ushort getIndexAligned(constant ushort *inputAligned, uint offset, uint idx)
{
    return inputAligned[offset / 2 + idx];
}
inline uint getIndexAligned(constant uint *inputAligned, uint offset, uint idx)
{
    return inputAligned[offset / 4 + idx];
}
inline uchar getIndexAligned(constant uchar *input, uint offset, uint idx)
{
    return input[offset + idx];
}
inline ushort getIndexUnalignedU16(constant uchar *input, uint offset, uint idx)
{
    ushort inputLo = input[offset + 2 * idx];
    ushort inputHi = input[offset + 2 * idx + 1];
    // Little endian conversion:
    return inputLo | (inputHi << 8);
}
inline uint getIndexUnalignedU32(constant uchar *input, uint offset, uint idx)
{
    uint input0 = input[offset + 4 * idx];
    uint input1 = input[offset + 4 * idx + 1];
    uint input2 = input[offset + 4 * idx + 2];
    uint input3 = input[offset + 4 * idx + 3];
    // Little endian conversion:
    return input0 | (input1 << 8) | (input2 << 16) | (input3 << 24);
}

kernel void convertIndexU8ToU16(uint idx[[thread_position_in_grid]],
                                constant IndexConversionParams &options[[buffer(0)]],
                                constant uchar *input[[buffer(1)]],
                                device ushort *output[[buffer(2)]])
{
    ANGLE_IDX_CONVERSION_GUARD(idx, options);
    output[idx] = getIndexAligned(input, options.srcOffset, idx);
}

kernel void convertIndexU16(
    uint idx[[thread_position_in_grid]],
    constant IndexConversionParams &options[[buffer(0)]],
    constant uchar *input[[ buffer(1), function_constant(kSourceBufferUnaligned) ]],
    constant ushort *inputAligned[[ buffer(1), function_constant(kSourceBufferAligned) ]],
    device ushort *output[[buffer(2)]])
{
    ANGLE_IDX_CONVERSION_GUARD(idx, options);

    ushort value;
    if (kSourceBufferAligned)
    {
        value = getIndexAligned(inputAligned, options.srcOffset, idx);
    }
    else
    {
        value = getIndexUnalignedU16(input, options.srcOffset, idx);
    }
    output[idx] = value;
}

kernel void convertIndexU32(
    uint idx[[thread_position_in_grid]],
    constant IndexConversionParams &options[[buffer(0)]],
    constant uchar *input[[ buffer(1), function_constant(kSourceBufferUnaligned) ]],
    constant uint *inputAligned[[ buffer(1), function_constant(kSourceBufferAligned) ]],
    device uint *output[[buffer(2)]])
{
    ANGLE_IDX_CONVERSION_GUARD(idx, options);

    uint value;
    if (kSourceBufferAligned)
    {
        value = getIndexAligned(inputAligned, options.srcOffset, idx);
    }
    else
    {
        value = getIndexUnalignedU32(input, options.srcOffset, idx);
    }
    output[idx] = value;
}

struct TriFanArrayParams
{
    uint firstVertex;
    uint vertexCountFrom3rd;  // vertex count excluding the 1st & 2nd vertices.
};
kernel void genTriFanIndicesFromArray(uint idx[[thread_position_in_grid]],
                                      constant TriFanArrayParams &options[[buffer(0)]],
                                      device uint *output[[buffer(2)]])
{
    ANGLE_KERNEL_GUARD(idx, options.vertexCountFrom3rd);

    uint vertexIdx = options.firstVertex + 2 + idx;

    output[3 * idx]     = options.firstVertex;
    output[3 * idx + 1] = vertexIdx - 1;
    output[3 * idx + 2] = vertexIdx;
}

inline uint getIndexU32(uint offset,
                        uint idx,
                        constant uchar *inputU8[[function_constant(kUseSourceBufferU8)]],
                        constant ushort *inputU16[[function_constant(kUseSourceBufferU16)]],
                        constant uint *inputU32[[function_constant(kUseSourceBufferU32)]])
{
    if (kUseSourceBufferU8)
    {
        if (kSourceIndexIsU16)
        {
            return getIndexUnalignedU16(inputU8, offset, idx);
        }
        else if (kSourceIndexIsU32)
        {
            return getIndexUnalignedU32(inputU8, offset, idx);
        }
        return getIndexAligned(inputU8, offset, idx);
    }
    else if (kUseSourceBufferU16)
    {
        return getIndexAligned(inputU16, offset, idx);
    }
    else if (kUseSourceBufferU32)
    {
        return getIndexAligned(inputU32, offset, idx);
    }
    return 0;
}

// Generate triangle fan indices from an indices buffer. indexCount options indicates number
// of indices starting from the 3rd.
kernel void genTriFanIndicesFromElements(
    uint idx[[thread_position_in_grid]],
    constant IndexConversionParams &options[[buffer(0)]],
    constant uchar *inputU8[[ buffer(1), function_constant(kUseSourceBufferU8) ]],
    constant ushort *inputU16[[ buffer(1), function_constant(kUseSourceBufferU16) ]],
    constant uint *inputU32[[ buffer(1), function_constant(kUseSourceBufferU32) ]],
    device uint *output[[buffer(2)]])
{
    ANGLE_IDX_CONVERSION_GUARD(idx, options);

    uint elemIdx = 2 + idx;

    output[3 * idx]     = getIndexU32(options.srcOffset, 0, inputU8, inputU16, inputU32);
    output[3 * idx + 1] = getIndexU32(options.srcOffset, elemIdx - 1, inputU8, inputU16, inputU32);
    output[3 * idx + 2] = getIndexU32(options.srcOffset, elemIdx, inputU8, inputU16, inputU32);
}