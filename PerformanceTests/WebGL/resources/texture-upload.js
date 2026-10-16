// Benchmarks of uploading a whole texture level of each texture format of WebGL 2 and its
// extensions, from an ArrayBufferView, at several sizes and in several states of the GPU work around
// the upload. There are three ways to upload, one per suite:
//
//  - TextureUpload, runTextureUpload(): each run redefines the level of a texture with
//    texImage2D(), or compressedTexImage2D(), and the data.
//  - TextureSubUpload, runTextureSubUpload(): setup() allocates the level of a texture with
//    texImage2D() and no data, or compressedTexImage2D() and zeros. Each run replaces the whole
//    level with texSubImage2D() or compressedTexSubImage2D().
//  - TextureStorageUpload, runTextureStorageUpload(): each run creates a new texture with
//    immutable storage, texStorage2D() with one level, and fills the level with texSubImage2D() or
//    compressedTexSubImage2D(). So a run measures allocating immutable storage and uploading to
//    it, not just the upload. It deletes the texture of the previous run first, so that probe()
//    can read the texture of the current run.
//
// The sizes and states are there to find where one way of uploading in the implementation starts
// to beat another, for example copying on the CPU against copying through a staging buffer on the
// GPU. The state is one of:
//
//  - "idle": nothing before an upload. Each run starts after probe() has read back the previous
//    run, so no GPU work is pending, no render pass is active, and the GPU is not using the
//    textures.
//  - "pass": before each upload, a draw to a separate 1024x1024 RGBA8 framebuffer, the pass target,
//    that does not sample the texture. So a render pass is active during the upload, and the GPU
//    is not using the texture. An implementation that ends the render pass to upload has to start
//    another one for the next draw.
//  - "used": before each upload, a draw to the pass target that samples one texel of the texture.
//    So the active render pass uses the texture, which the upload then overwrites.
//  - "used-flushed": as "used", then flush(). So a submitted command buffer that the GPU may still
//    be running uses the texture, and no render pass is active.
//
// With a state other than "idle", a run ends with one more draw to the pass target after the
// uploads. The draws write 8x8 pixels each, so their cost is small and does not depend on the size
// of the textures. runTextureStorageUpload() creates new textures, which no draw can have used
// before the upload, so it runs only in "idle" and "pass".
//
// A run uploads to 1024 * 1024 / (width * height) textures, rounded down, at least 1 and at most
// 64: one 2048x2048 texture, but 64 textures of 64x64, since such an upload takes far less than a
// millisecond and the readback of probe() should be a small part of the run.
//
// After the uploads, probe() samples one texel of each texture in a draw, at a position that
// depends on n, and reads them back with one readPixels(). That is where the GPU has to finish the
// uploads, and the texels are checked. verify() samples every texel of each texture. For a texture
// of more than 1024 * 1024 texels, it checks every 8th row, the last row and the last column, so
// that the check stays fast in Debug builds. The texture is sampled with texelFetch() into an
// RGBA32F, RGBA32I or RGBA32UI render target, depending on whether the format is sampled as float,
// int or uint, so that formats that are not renderable can be checked, and the decoded values are
// read back exactly. The float target needs EXT_color_buffer_float. Depth formats are sampled as
// their depth, and DEPTH_STENCIL formats also as their stencil value, with WEBGL_stencil_texturing.
//
// The uploaded data is one of a few patterns, which setup() generates, so that the measured time
// is the upload, not JavaScript filling arrays: 4 patterns, or 2 for a texture of more than
// 1024 * 1024 texels, where a pattern of 2048x2048 RGBA32F data is 64 MB. Iteration n uploads
// pattern (n + t) % patternCount to texture t. A pattern is a sequence of 13 texels, or blocks of
// compressed formats, that repeats through the level, and the checks compare each texel with the
// decoded value of its entry in the sequence. 13 is prime, so the sequence does not line up with
// rows. Each value of every component differs from one pattern to the next. For each format, the
// values are chosen to be exact in its storage, so the expected decoded value can be computed
// exactly, and the tolerances only allow for rounding by the GPU.
//
// The sizes are multiples of 4, the block size of most compressed formats. ASTC blocks that are 5,
// 6, 8, 10 or 12 texels wide or high, and blocks that don't divide a size, leave partial blocks at
// the right and top edges, which the formats allow. PVRTC textures must be square with sides that
// are powers of two, so generate-tests.py gives PVRTC formats only such sizes. A row of most
// uncompressed sizes is a multiple of 4 bytes, the default UNPACK_ALIGNMENT; setup() sets it to 1
// for the others.
//
// The *.html files, which generate-tests.py writes, give the format, the size and the state in
// options: { internalFormat, type, width, height, state } for uncompressed formats and
// { internalFormat, width, height, state } for compressed formats, where state is "idle" if it is
// omitted. The properties of the format come from WebGLFormats: format, the channels that say how
// the texture stores each component, and extension for uncompressed formats, and compression,
// which says how blocks are encoded, see blockEncoders, the block size, srgb and extension for
// compressed formats. The enums of extensions come from the extension objects.
(function () {
"use strict";

// The width and height of the textures, and the number of patterns. runTextureUpload() sets them,
// since a page runs one benchmark.
let width;
let height;
let patternCount;
const sequenceLength = 13;
// A run uploads to at most this many textures, and to fewer of more than this many texels each.
const maxTextureCount = 64;
const textureCountTexels = 1024 * 1024;

// The array type of each type, as WebGL requires, and the bit widths and positions of the
// components of packed types.
const types = {
    UNSIGNED_BYTE: { array: Uint8Array, bits: 8 },
    BYTE: { array: Int8Array, bits: 8 },
    UNSIGNED_SHORT: { array: Uint16Array, bits: 16 },
    SHORT: { array: Int16Array, bits: 16 },
    UNSIGNED_INT: { array: Uint32Array, bits: 32 },
    INT: { array: Int32Array, bits: 32 },
    HALF_FLOAT: { array: Uint16Array },
    FLOAT: { array: Float32Array },
    UNSIGNED_SHORT_5_6_5: { array: Uint16Array, fields: [[5, 11], [6, 5], [5, 0]] },
    UNSIGNED_SHORT_4_4_4_4: { array: Uint16Array, fields: [[4, 12], [4, 8], [4, 4], [4, 0]] },
    UNSIGNED_SHORT_5_5_5_1: { array: Uint16Array, fields: [[5, 11], [5, 6], [5, 1], [1, 0]] },
    UNSIGNED_INT_2_10_10_10_REV: { array: Uint32Array, fields: [[10, 0], [10, 10], [10, 20], [2, 30]] },
    UNSIGNED_INT_24_8: { array: Uint32Array, fields: [[24, 8], [8, 0]] },
    UNSIGNED_INT_10F_11F_11F_REV: { array: Uint32Array, packedFloat: true },
    UNSIGNED_INT_5_9_9_9_REV: { array: Uint32Array, packedFloat: true },
};

// What each kind of component of the channels option stores. kind is "unorm", "snorm", "srgb",
// "uint", "int", "float", "ufloat" or "depthfloat", and bits its size where that matters.
const componentKinds = {
    unorm1: { kind: "unorm", bits: 1 },
    unorm2: { kind: "unorm", bits: 2 },
    unorm4: { kind: "unorm", bits: 4 },
    unorm5: { kind: "unorm", bits: 5 },
    unorm6: { kind: "unorm", bits: 6 },
    unorm8: { kind: "unorm", bits: 8 },
    unorm10: { kind: "unorm", bits: 10 },
    unorm16: { kind: "unorm", bits: 16 },
    snorm8: { kind: "snorm", bits: 8 },
    snorm16: { kind: "snorm", bits: 16 },
    srgb8: { kind: "srgb", bits: 8 },
    uint2: { kind: "uint", bits: 2 },
    uint8: { kind: "uint", bits: 8 },
    uint10: { kind: "uint", bits: 10 },
    uint16: { kind: "uint", bits: 16 },
    uint32: { kind: "uint", bits: 32 },
    int8: { kind: "int", bits: 8 },
    int16: { kind: "int", bits: 16 },
    int32: { kind: "int", bits: 32 },
    half: { kind: "float" },
    float: { kind: "float" },
    uf11: { kind: "ufloat" },
    uf10: { kind: "ufloat" },
    e5: { kind: "ufloat" },
    depth16: { kind: "unorm", bits: 16 },
    depth24: { kind: "unorm", bits: 24 },
    depth32: { kind: "unorm", bits: 32 },
    depth32f: { kind: "depthfloat" },
    stencil8: { kind: "uint", bits: 8 },
};

// A fraction from 0 to 1 for component c of entry i of the sequence of pattern p. It differs by a
// quarter from one pattern to the next.
function fraction(p, i, c)
{
    return ((i * 7 + c * 3) % sequenceLength / sequenceLength + p / patternCount) % 1;
}

// The value that a component of a kind stores for component c of entry i of pattern p: an integer
// for normalized and integer kinds, a number for float kinds. All are exact in their storage:
// multiples of 1/8 from -2 to 6 for half and float, multiples of 1/16 from 0 to 4 for unsigned
// floats, multiples of 1/64 from 0 to 1 for float depth.
function storedValue(component, p, i, c)
{
    const t = fraction(p, i, c);
    const bits = component.bits;
    switch (component.kind) {
    case "unorm":
    case "uint":
        // A quarter of 1 or 2 bits is not a step, so these count up instead.
        if (bits <= 2)
            return (i + c + p) % (1 << bits);
        return Math.floor(t * 2 ** bits);
    case "srgb":
        return Math.floor(t * 256);
    case "snorm":
        return Math.floor(t * (2 ** bits - 1)) - (2 ** (bits - 1) - 1);
    case "int":
        return Math.floor(t * 2 ** bits) - 2 ** (bits - 1);
    case "float":
        return (Math.floor(t * 64) - 16) / 8;
    case "ufloat":
        return Math.floor(t * 64) / 16;
    case "depthfloat":
        return Math.floor(t * 64) / 64;
    }
    throw new Error("Unknown component kind " + component.kind);
}

function srgbToLinear(value)
{
    return value <= 0.04045 ? value / 12.92 : ((value + 0.055) / 1.055) ** 2.4;
}

// What a stored value reads back as when sampled.
function decodedValue(component, value)
{
    switch (component.kind) {
    case "unorm":
        return value / (2 ** component.bits - 1);
    case "srgb":
        return srgbToLinear(value / 255);
    case "snorm":
        return Math.max(value / (2 ** (component.bits - 1) - 1), -1);
    }
    return value;
}

// How much the sampled value may differ from decodedValue().
function tolerance(component)
{
    switch (component.kind) {
    case "unorm":
        // At least 1e-6, for depth formats stored as 32-bit floats.
        return Math.max(0.25 / (2 ** component.bits - 1), 1e-6);
    case "srgb":
        return 0.001;
    case "snorm":
        return 0.25 / (2 ** (component.bits - 1) - 1);
    case "uint":
    case "int":
        return 0;
    }
    return 1e-6;
}

// The value with `from` bits repeated to fill `to` bits, which is how a normalized value converts
// exactly to more bits. For example 5 bits abcde become the 8 bits abcdeabc.
function replicateBits(value, from, to)
{
    let result = 0;
    let filled = 0;
    while (filled < to) {
        result = result * 2 ** from + value;
        filled += from;
    }
    return Math.floor(result / 2 ** (filled - to));
}

const float32 = new Float32Array(1);
const float32Bits = new Uint32Array(float32.buffer);

function bitsOfFloat(value)
{
    float32[0] = value;
    return float32Bits[0];
}

// The bits of a float with mantissaBits bits of mantissa and a 5-bit exponent, and a sign bit if
// signed, for a value that it represents exactly.
function smallFloatBits(value, mantissaBits, signed)
{
    if (!value)
        return 0;
    const bits = bitsOfFloat(value);
    const exponent = ((bits >>> 23) & 0xff) - 127 + 15;
    const mantissa = (bits >>> (23 - mantissaBits)) & ((1 << mantissaBits) - 1);
    const sign = signed ? (bits >>> 31) << (5 + mantissaBits) : 0;
    return sign | (exponent << mantissaBits) | mantissa;
}

function halfFloatBits(value)
{
    return smallFloatBits(value, 10, true);
}

function halfFloatValue(bits)
{
    const sign = bits & 0x8000 ? -1 : 1;
    const exponent = (bits >> 10) & 0x1f;
    const mantissa = bits & 0x3ff;
    if (!exponent)
        return sign * mantissa * 2 ** -24;
    return sign * (1 + mantissa / 1024) * 2 ** (exponent - 15);
}

// UNSIGNED_INT_5_9_9_9_REV, as in the ES 3.0 specification, section 3.8.3.2.
function rgb9e5Bits(red, green, blue)
{
    const max = Math.max(red, green, blue);
    if (!max)
        return 0;
    let exponent = Math.max(-16, Math.floor(Math.log2(max))) + 1 + 15;
    let scale = 2 ** (exponent - 15 - 9);
    if (Math.floor(max / scale + 0.5) == 512) {
        ++exponent;
        scale *= 2;
    }
    const mantissa = value => Math.floor(value / scale + 0.5);
    return (mantissa(red) | (mantissa(green) << 9) | (mantissa(blue) << 18) | (exponent << 27)) >>> 0;
}

// Writes texel i of pattern p in format and type at element offset of array, and returns the
// stored values of its components.
function writeTexel(options, components, type, p, i, array, offset)
{
    const values = components.map((component, c) => storedValue(component, p, i, c));
    if (type.fields) {
        let packed = 0;
        type.fields.forEach(([bits, shift], c) => {
            packed += replicateBits(values[c], components[c].bits, bits) * 2 ** shift;
        });
        array[offset] = packed;
    } else if (options.type == "UNSIGNED_INT_10F_11F_11F_REV")
        array[offset] = (smallFloatBits(values[0], 6) | (smallFloatBits(values[1], 6) << 11) | (smallFloatBits(values[2], 5) << 22)) >>> 0;
    else if (options.type == "UNSIGNED_INT_5_9_9_9_REV")
        array[offset] = rgb9e5Bits(values[0], values[1], values[2]);
    else {
        values.forEach((value, c) => {
            const component = components[c];
            if (options.type == "HALF_FLOAT")
                array[offset + c] = halfFloatBits(value);
            else if (component.kind == "unorm" || component.kind == "srgb")
                array[offset + c] = replicateBits(value, component.bits, type.bits);
            else
                array[offset + c] = value;
        });
    }
    return values;
}

// The RGBA value that sampling a texel of format with decoded components gives, and the allowed
// difference of each channel. Depth formats give the depth in red, and the checks of
// DEPTH_STENCIL formats write the stencil value to green. A missing alpha channel is 1.
function sampledTexel(format, decoded, tolerances, isFloat)
{
    const [a, b, c, d] = decoded;
    const [ta, tb, tc, td] = tolerances;
    const exact = isFloat ? 1e-6 : 0;
    switch (format) {
    case "RED":
    case "RED_INTEGER":
        return [[a, 0, 0, 1], [ta, exact, exact, exact]];
    case "RG":
    case "RG_INTEGER":
        return [[a, b, 0, 1], [ta, tb, exact, exact]];
    case "RGB":
    case "RGB_INTEGER":
        return [[a, b, c, 1], [ta, tb, tc, exact]];
    case "RGBA":
    case "RGBA_INTEGER":
        return [[a, b, c, d], [ta, tb, tc, td]];
    case "LUMINANCE":
        return [[a, a, a, 1], [ta, ta, ta, exact]];
    case "LUMINANCE_ALPHA":
        return [[a, a, a, b], [ta, ta, ta, tb]];
    case "ALPHA":
        return [[0, 0, 0, a], [exact, exact, exact, ta]];
    case "DEPTH_COMPONENT":
        return [[a, -1, 0, 0], [ta, 0, 0, 0]];
    case "DEPTH_STENCIL":
        return [[a, b, 0, 0], [ta, 0, 0, 0]];
    }
    throw new Error("Unknown format " + format);
}

// Fills the rest of bytes by repeating its first `filled` bytes.
function repeatPrefix(bytes, filled)
{
    while (filled < bytes.length) {
        const count = Math.min(filled, bytes.length - filled);
        bytes.copyWithin(filled, 0, count);
        filled += count;
    }
}

// Generates the patterns of an uncompressed format: for each, the data to upload and the expected
// sampled RGBA value of each entry of the sequence.
function uncompressedPatterns(options)
{
    const type = types[options.type];
    const components = options.channels.map(name => componentKinds[name]);
    const isFloat = !components.some(component => component.kind == "uint" || component.kind == "int") || options.format.startsWith("DEPTH");
    const elementsPerTexel = type.fields || type.packedFloat ? 1 : components.length;
    const patterns = [];
    for (let p = 0; p < patternCount; ++p) {
        const data = new type.array(width * height * elementsPerTexel);
        const expected = new Float64Array(sequenceLength * 4);
        let tolerances;
        for (let i = 0; i < sequenceLength; ++i) {
            const values = writeTexel(options, components, type, p, i, data, i * elementsPerTexel);
            const decoded = values.map((value, c) => decodedValue(components[c], value));
            const [rgba, allowed] = sampledTexel(options.format, decoded, components.map(tolerance), isFloat);
            expected.set(rgba, i * 4);
            tolerances = allowed;
        }
        repeatPrefix(new Uint8Array(data.buffer), sequenceLength * elementsPerTexel * data.BYTES_PER_ELEMENT);
        patterns.push({ data, expected, tolerances });
    }
    return patterns;
}

// A tiny bit writer for compressed blocks, least significant bit of byte 0 first.
function writeBits(block, position, count, value)
{
    for (let bit = 0; bit < count; ++bit, ++position) {
        if (value & (1 << bit))
            block[position >> 3] |= 1 << (position & 7);
    }
    return position;
}

function expand(value, bits)
{
    return replicateBits(value, bits, 8) / 255;
}

// BC1 color block, which BC2 and BC3 also use: both endpoints are the same 5:6:5 color and every
// index is 0, which selects the first endpoint in both modes of BC1.
function bc1Color(block, offset, p, i)
{
    const red = Math.floor(fraction(p, i, 0) * 32);
    const green = Math.floor(fraction(p, i, 1) * 64);
    const blue = Math.floor(fraction(p, i, 2) * 32);
    const color = (red << 11) | (green << 5) | blue;
    block[offset] = block[offset + 2] = color & 0xff;
    block[offset + 1] = block[offset + 3] = color >> 8;
    return [expand(red, 5), expand(green, 6), expand(blue, 5)];
}

// BC4 block, one channel, which BC5 uses twice: both endpoints are the same and every index is 0,
// which selects the first endpoint.
function bc4Channel(block, offset, p, i, c, signed)
{
    if (signed) {
        const value = Math.floor(fraction(p, i, c) * 255) - 127;
        block[offset] = block[offset + 1] = value & 0xff;
        return value / 127;
    }
    const value = Math.floor(fraction(p, i, c) * 256);
    block[offset] = block[offset + 1] = value;
    return value / 255;
}

// BC6H mode 11: one region, 10-bit endpoints without deltas, both endpoints the same and every
// index 0. The decoded value follows the unquantization of the BPTC specification.
function bc6hBlock(block, p, i, signed)
{
    const endpoints = [0, 1, 2].map(c => signed ? Math.floor(fraction(p, i, c) * 1000) - 500 : 100 + Math.floor(fraction(p, i, c) * 800));
    let position = writeBits(block, 0, 5, 0x03);
    for (let endpoint = 0; endpoint < 2; ++endpoint) {
        for (const value of endpoints)
            position = writeBits(block, position, 10, value & 0x3ff);
    }
    return endpoints.map(value => {
        let half;
        if (signed) {
            let magnitude = Math.abs(value);
            if (magnitude >= 511)
                magnitude = 0x7fff;
            else if (magnitude)
                magnitude = ((magnitude << 15) + 0x4000) >> 9;
            magnitude = (magnitude * 31) >> 5;
            half = value < 0 ? 0x8000 | magnitude : magnitude;
        } else {
            let unquantized = value;
            if (value == 1023)
                unquantized = 0xffff;
            else if (value)
                unquantized = ((value << 16) + 0x8000) >> 10;
            half = (unquantized * 31) >> 6;
        }
        return halfFloatValue(half);
    });
}

// BC7 mode 6: 7-bit RGBA endpoints with a p-bit of 0, both endpoints the same and every index 0.
function bc7Block(block, p, i)
{
    const endpoints = [0, 1, 2, 3].map(c => Math.floor(fraction(p, i, c) * 128));
    let position = writeBits(block, 0, 7, 0x40);
    for (const value of endpoints) {
        position = writeBits(block, position, 7, value);
        position = writeBits(block, position, 7, value);
    }
    return endpoints.map(value => (value << 1) / 255);
}

// ETC1 block in individual mode, which ETC2 decodes the same way: both subblocks have the same
// 4-bit color and codeword table 0, and every pixel index is 0, which adds 2.
function etc1Block(block, offset, p, i)
{
    const color = [0, 1, 2].map(c => Math.floor(fraction(p, i, c) * 15));
    for (let c = 0; c < 3; ++c)
        block[offset + c] = (color[c] << 4) | color[c];
    return color.map(value => (value * 17 + 2) / 255);
}

// ETC2 block with punchthrough alpha, in differential mode with the opaque bit set: a 5-bit color
// with zero deltas, codeword table 0, and every pixel index 0, which adds 2.
function etc2PunchthroughBlock(block, p, i)
{
    const color = [0, 1, 2].map(c => Math.floor(fraction(p, i, c) * 30));
    for (let c = 0; c < 3; ++c)
        block[c] = color[c] << 3;
    block[3] = 0x02;
    return color.map(value => (replicateBits(value, 5, 8) + 2) / 255);
}

// EAC block: multiplier 1, modifier table 0 and every index 0, which selects the modifier -3.
// Unsigned R11 decodes to base * 8 + 4 - 3 * 8, signed R11 to base * 8 - 3 * 8, and the alpha of
// RGBA8 ETC2 EAC to base - 3.
function eacBlock(block, offset, p, i, c, mode)
{
    block[offset + 1] = 0x10;
    if (mode == "signed") {
        const base = Math.floor(fraction(p, i, c) * 250) - 123;
        block[offset] = base & 0xff;
        return (base * 8 - 24) / 1023;
    }
    const base = 3 + Math.floor(fraction(p, i, c) * 253);
    block[offset] = base;
    return mode == "alpha" ? (base - 3) / 255 : (base * 8 - 20) / 2047;
}

// ASTC void-extent block, a constant LDR color with no extent, for any block size. Each 16-bit
// component is an 8-bit value repeated, so it decodes to the same value with 8-bit and 16-bit
// precision, and sRGB formats use its top 8 bits.
function astcBlock(block, p, i)
{
    block.set([0xfc, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff]);
    return [0, 1, 2, 3].map(c => {
        const value = Math.floor(fraction(p, i, c) * 256);
        block[8 + c * 2] = block[9 + c * 2] = value;
        return value / 255;
    });
}

// PVRTC block, opaque, with colors A and B the same and modulation 0. PVRTC interpolates the colors
// of neighboring blocks, so every block of a pattern is the same, and the channels are 0 or 1,
// which are exact however the decoder expands the 5-bit and 4-bit components.
function pvrtcBlock(block, p)
{
    const [red, green, blue] = p % 2 ? [1, 0, 1] : [0, 1, 0];
    // Color A in bits 1 to 15 and color B in bits 16 to 31 of the second 32-bit word.
    const colorA = 0x8000 | (red * 0x1f) << 10 | (green * 0x1f) << 5 | (blue * 0xf) << 1;
    const colorB = 0x8000 | (red * 0x1f) << 10 | (green * 0x1f) << 5 | (blue * 0x1f);
    block[4] = colorA & 0xff;
    block[5] = colorA >> 8;
    block[6] = colorB & 0xff;
    block[7] = colorB >> 8;
    return [red, green, blue];
}

// For each compression of the options: writes block i of pattern p into block, which is zeroed,
// and returns its decoded RGBA color, with alpha omitted when it is 1, the allowed difference of
// each channel, and the length of the sequence of blocks.
const blockEncoders = {
    bc1: { tolerance: 0.004, encode: (block, p, i) => bc1Color(block, 0, p, i) },
    bc2: { tolerance: 0.004, encode: (block, p, i) => {
        const alpha = Math.floor(fraction(p, i, 3) * 16);
        block.fill(alpha * 17, 0, 8);
        return bc1Color(block, 8, p, i).concat([alpha / 15]);
    } },
    bc3: { tolerance: 0.004, encode: (block, p, i) => {
        const alpha = bc4Channel(block, 0, p, i, 3, false);
        return bc1Color(block, 8, p, i).concat([alpha]);
    } },
    bc4: { tolerance: 0.002, encode: (block, p, i) => [bc4Channel(block, 0, p, i, 0, false), 0, 0] },
    bc4_snorm: { tolerance: 0.004, encode: (block, p, i) => [bc4Channel(block, 0, p, i, 0, true), 0, 0] },
    bc5: { tolerance: 0.002, encode: (block, p, i) => [bc4Channel(block, 0, p, i, 0, false), bc4Channel(block, 8, p, i, 1, false), 0] },
    bc5_snorm: { tolerance: 0.004, encode: (block, p, i) => [bc4Channel(block, 0, p, i, 0, true), bc4Channel(block, 8, p, i, 1, true), 0] },
    bc6h_ufloat: { tolerance: value => Math.abs(value) / 1024 + 1e-6, encode: (block, p, i) => bc6hBlock(block, p, i, false) },
    bc6h_sfloat: { tolerance: value => Math.abs(value) / 1024 + 1e-6, encode: (block, p, i) => bc6hBlock(block, p, i, true) },
    bc7: { tolerance: 0.002, encode: bc7Block },
    etc1: { tolerance: 0.002, encode: (block, p, i) => etc1Block(block, 0, p, i) },
    etc2_punchthrough: { tolerance: 0.002, encode: etc2PunchthroughBlock },
    etc2_eac: { tolerance: 0.002, encode: (block, p, i) => {
        const alpha = eacBlock(block, 0, p, i, 3, "alpha");
        return etc1Block(block, 8, p, i).concat([alpha]);
    } },
    eac_r11: { tolerance: 0.001, encode: (block, p, i) => [eacBlock(block, 0, p, i, 0, "unsigned"), 0, 0] },
    eac_r11_snorm: { tolerance: 0.002, encode: (block, p, i) => [eacBlock(block, 0, p, i, 0, "signed"), 0, 0] },
    eac_rg11: { tolerance: 0.001, encode: (block, p, i) => [eacBlock(block, 0, p, i, 0, "unsigned"), eacBlock(block, 8, p, i, 1, "unsigned"), 0] },
    eac_rg11_snorm: { tolerance: 0.002, encode: (block, p, i) => [eacBlock(block, 0, p, i, 0, "signed"), eacBlock(block, 8, p, i, 1, "signed"), 0] },
    astc: { tolerance: 0.002, encode: astcBlock },
    pvrtc: { tolerance: 0.004, sequenceLength: 1, encode: pvrtcBlock },
};

function compressedPatterns(options)
{
    const encoder = blockEncoders[options.compression];
    if (!encoder)
        throw new Error("Unknown compression " + options.compression);
    const blocksPerRow = Math.ceil(width / options.blockWidth);
    const blockCount = blocksPerRow * Math.ceil(height / options.blockHeight);
    const length = encoder.sequenceLength || sequenceLength;
    const patterns = [];
    for (let p = 0; p < patternCount; ++p) {
        const data = new Uint8Array(blockCount * options.blockBytes);
        const expected = new Float64Array(length * 4);
        for (let i = 0; i < length; ++i) {
            const color = encoder.encode(data.subarray(i * options.blockBytes, (i + 1) * options.blockBytes), p, i);
            if (color.length == 3)
                color.push(1);
            for (let c = 0; c < 4; ++c)
                expected[i * 4 + c] = options.srgb && c < 3 ? srgbToLinear(color[c]) : color[c];
        }
        repeatPrefix(data, length * options.blockBytes);
        // sRGB decoding steepens the curve up to 2.4 times, so its tolerance is 3 times larger.
        const colorTolerance = options.srgb ? 3 * encoder.tolerance : encoder.tolerance;
        patterns.push({ data, expected, tolerances: [colorTolerance, colorTolerance, colorTolerance, encoder.tolerance], blocksPerRow, length });
    }
    return patterns;
}

const vertexShader = `#version 300 es
in vec2 aPosition;
void main() {
    gl_Position = vec4(aPosition, 0.0, 1.0);
}`;

// Copies the texel at the position of each pixel plus uOffset to the pixel. Depth formats write the
// depth to red and -1 to green, which stencilShader overwrites for DEPTH_STENCIL formats.
function samplingShader(sampler)
{
    const prefix = { float: "", depth: "", int: "i", uint: "u" }[sampler];
    const value = sampler == "depth" ? "vec4(texelFetch(uTexture, position, 0).r, -1.0, 0.0, 0.0)" : "texelFetch(uTexture, position, 0)";
    return `#version 300 es
precision highp float;
precision highp int;
uniform highp ${prefix}sampler2D uTexture;
uniform ivec2 uOffset;
out highp ${prefix}vec4 outColor;
void main() {
    ivec2 position = ivec2(gl_FragCoord.xy) + uOffset;
    outColor = ${value};
}`;
}

// Writes the stencil value of each texel of a DEPTH_STENCIL texture, sampled with
// DEPTH_STENCIL_TEXTURE_MODE_WEBGL set to STENCIL_INDEX_WEBGL, to green.
const stencilShader = `#version 300 es
precision highp float;
precision highp int;
uniform highp usampler2D uTexture;
uniform ivec2 uOffset;
out highp vec4 outColor;
void main() {
    outColor = vec4(0.0, float(texelFetch(uTexture, ivec2(gl_FragCoord.xy) + uOffset, 0).r), 0.0, 0.0);
}`;

// The shader of the draws to the pass target, see the comment at the top. With a sampler, it reads
// one texel of the bound texture. uScale is 0, so the color does not depend on the texel, but the
// compiler cannot remove the read.
function passShader(sampler)
{
    if (!sampler) {
        return `#version 300 es
precision mediump float;
out vec4 outColor;
void main() {
    outColor = vec4(0.25);
}`;
    }
    const prefix = { float: "", depth: "", int: "i", uint: "u" }[sampler];
    return `#version 300 es
precision highp float;
precision highp int;
uniform highp ${prefix}sampler2D uTexture;
uniform float uScale;
out vec4 outColor;
void main() {
    outColor = vec4(texelFetch(uTexture, ivec2(0), 0)) * uScale + vec4(0.25);
}`;
}

const passTargetSize = 1024;
const states = ["idle", "pass", "used", "used-flushed"];

// The render target format and readPixels() format of each way of sampling.
const targets = {
    float: { internalFormat: "RGBA32F", format: "RGBA", type: "FLOAT", array: Float32Array },
    depth: { internalFormat: "RGBA32F", format: "RGBA", type: "FLOAT", array: Float32Array },
    int: { internalFormat: "RGBA32I", format: "RGBA_INTEGER", type: "INT", array: Int32Array },
    uint: { internalFormat: "RGBA32UI", format: "RGBA_INTEGER", type: "UNSIGNED_INT", array: Uint32Array },
};

function samplerOf(options)
{
    if (options.compression)
        return "float";
    if (options.format.startsWith("DEPTH"))
        return "depth";
    if (options.format.endsWith("_INTEGER"))
        return options.channels[0].startsWith("uint") ? "uint" : "int";
    return "float";
}

// mode is "texImage2D", "texSubImage2D" or "texStorage2D", see the comment at the top.
function runTextureUpload(mode, permutation)
{
    const options = permutation.type ? WebGLBench.textureFormat(permutation.internalFormat, permutation.type) : WebGLBench.compressedTextureFormat(permutation.internalFormat);
    width = permutation.width;
    height = permutation.height;
    const texels = width * height;
    const state = permutation.state || "idle";
    if (!states.includes(state))
        throw new Error("Unknown state " + state);
    const usesTexture = state == "used" || state == "used-flushed";
    if (mode == "texStorage2D" && usesTexture)
        throw new Error("texStorage2D creates new textures, which no draw can have used");
    patternCount = texels > textureCountTexels ? 2 : 4;
    const textureCount = Math.min(maxTextureCount, Math.max(1, Math.floor(textureCountTexels / texels)));
    // probe() samples a texel of each texture to a pixel of the first row of the target.
    const targetWidth = Math.max(width, textureCount);
    const compressed = !!options.compression;
    const sampler = samplerOf(options);
    const target = targets[sampler];
    const hasStencil = options.format == "DEPTH_STENCIL";
    const extensions = options.extension ? [options.extension] : [];
    if (target.type == "FLOAT" && !extensions.includes("EXT_color_buffer_float"))
        extensions.push("EXT_color_buffer_float");
    if (hasStencil)
        extensions.push("WEBGL_stencil_texturing");

    let gl;
    let internalFormat;
    let format;
    let type;
    let patterns;
    const textures = [];
    let targetFramebuffer;
    let samplingProgram;
    let samplingOffsetLocation;
    let stencilProgram;
    let stencilOffsetLocation;
    let stencilTexturing;
    let passFramebuffer;
    let passColorProgram;
    let passSamplingProgram;
    // The texels that probe() reads back, one per texture, and their expected values.
    const probePixels = new target.array(textureCount * 4);
    const probeExpected = new Float64Array(textureCount * 4);

    function patternFor(n, texture)
    {
        return patterns[(n + texture) % patternCount];
    }

    // The entry of the sequence of the texel at (x, y).
    function entryOf(pattern, x, y)
    {
        if (compressed)
            return ((y / options.blockHeight | 0) * pattern.blocksPerRow + (x / options.blockWidth | 0)) % pattern.length;
        return (y * width + x) % sequenceLength;
    }

    function upload(data)
    {
        if (mode == "texImage2D") {
            if (compressed)
                gl.compressedTexImage2D(gl.TEXTURE_2D, 0, internalFormat, width, height, 0, data);
            else
                gl.texImage2D(gl.TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, data);
        } else {
            if (compressed)
                gl.compressedTexSubImage2D(gl.TEXTURE_2D, 0, 0, 0, width, height, internalFormat, data);
            else
                gl.texSubImage2D(gl.TEXTURE_2D, 0, 0, 0, width, height, format, type, data);
        }
    }

    // Samples the bound texture into a rectangle of the target: the texel at the position of each
    // pixel plus (offsetX, offsetY).
    function sample(offsetX, offsetY, x, y, width, height)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, targetFramebuffer);
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor(x, y, width, height);
        gl.useProgram(samplingProgram);
        gl.uniform2i(samplingOffsetLocation, offsetX, offsetY);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        if (hasStencil) {
            gl.texParameteri(gl.TEXTURE_2D, stencilTexturing.DEPTH_STENCIL_TEXTURE_MODE_WEBGL, stencilTexturing.STENCIL_INDEX_WEBGL);
            gl.useProgram(stencilProgram);
            gl.uniform2i(stencilOffsetLocation, offsetX, offsetY);
            gl.colorMask(false, true, false, false);
            gl.drawArrays(gl.TRIANGLES, 0, 6);
            gl.colorMask(true, true, true, true);
            gl.texParameteri(gl.TEXTURE_2D, stencilTexturing.DEPTH_STENCIL_TEXTURE_MODE_WEBGL, gl.DEPTH_COMPONENT);
        }
        gl.disable(gl.SCISSOR_TEST);
    }

    // Draws 8x8 pixels to the pass target, at a place that depends on index. With useTexture, the
    // draw samples the bound texture.
    function drawToPassTarget(index, useTexture)
    {
        gl.bindFramebuffer(gl.FRAMEBUFFER, passFramebuffer);
        gl.viewport(0, 0, passTargetSize, passTargetSize);
        gl.enable(gl.SCISSOR_TEST);
        gl.scissor((index % 128) * 8, (index / 128 | 0) * 8, 8, 8);
        gl.useProgram(useTexture ? passSamplingProgram : passColorProgram);
        gl.drawArrays(gl.TRIANGLES, 0, 6);
        gl.disable(gl.SCISSOR_TEST);
    }

    function readTarget(x, y, width, height, pixels)
    {
        pixels = pixels || new target.array(width * height * 4);
        gl.readPixels(x, y, width, height, gl[target.format], gl[target.type], pixels);
        return pixels;
    }

    // Checks a rectangle of the target, which holds the texels of texture at the same positions.
    function check(label, n, texture, x, y, width, height)
    {
        const pattern = patternFor(n, texture);
        WebGLBench.verifyPixelsFromTable(label, readTarget(x, y, width, height), x, y, width, height,
            pattern.expected, (column, row) => entryOf(pattern, column, row), pattern.tolerances);
    }

    const what = width + "x" + height + " " + options.internalFormat + (compressed ? "" : " data with format " + options.format + " and type " + options.type);
    const into = textureCount > 1 ? "each of " + textureCount + " textures" : "a texture";
    let description;
    if (mode == "texImage2D")
        description = "Measures " + (compressed ? "compressedTexImage2D" : "texImage2D") + " of " + what + " into " + into + ", which redefines it";
    else if (mode == "texSubImage2D")
        description = "Measures " + (compressed ? "compressedTexSubImage2D" : "texSubImage2D") + " of " + what + " into the whole level of " + (textureCount > 1 ? "each of " + textureCount + " existing textures" : "an existing texture");
    else {
        description = "Measures creating " + (textureCount > 1 ? textureCount + " textures" : "a texture") + " with texStorage2D of " + width + "x" + height + " " + options.internalFormat + " and filling "
            + (textureCount > 1 ? "each level" : "its level") + " with " + (compressed ? "compressedTexSubImage2D" : "texSubImage2D of data with format " + options.format + " and type " + options.type)
            + ", and deleting the " + (textureCount > 1 ? "textures" : "texture") + " of the previous run";
    }
    const eachUpload = textureCount > 1 ? "each upload" : "the upload";
    if (state == "pass")
        description += ", " + eachUpload + " right after a draw to another framebuffer, so that a render pass is active, and with a draw to that framebuffer after the uploads";
    else if (state == "used")
        description += ", " + eachUpload + " right after a draw to another framebuffer that samples the texture, so that the active render pass uses it, and with a draw to that framebuffer after the uploads";
    else if (state == "used-flushed")
        description += ", " + eachUpload + " right after a draw to another framebuffer that samples the texture and a flush(), so that a submitted command buffer uses it, and with a draw to that framebuffer after the uploads";
    description += textureCount > 1 ? ", then sampling 1 texel of each texture in a draw and reading them back." : ", then sampling 1 texel in a draw and reading it back.";

    WebGLBench.run({
        description,
        contextType: "webgl2",
        extensions,
        setup(context, extensionObjects) {
            gl = context;
            internalFormat = WebGLBench.enumValue(gl, extensionObjects, options.internalFormat);
            if (!compressed) {
                format = gl[options.format];
                type = gl[options.type];
            }
            patterns = compressed ? compressedPatterns(options) : uncompressedPatterns(options);
            // The patterns are tightly packed.
            if (!compressed && (patterns[0].data.byteLength / height) % 4)
                gl.pixelStorei(gl.UNPACK_ALIGNMENT, 1);

            const targetTexture = gl.createTexture();
            gl.bindTexture(gl.TEXTURE_2D, targetTexture);
            gl.texStorage2D(gl.TEXTURE_2D, 1, gl[target.internalFormat], targetWidth, height);
            targetFramebuffer = gl.createFramebuffer();
            gl.bindFramebuffer(gl.FRAMEBUFFER, targetFramebuffer);
            gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, targetTexture, 0);
            if (gl.checkFramebufferStatus(gl.FRAMEBUFFER) != gl.FRAMEBUFFER_COMPLETE)
                throw new Error(target.internalFormat + " render target is not complete");

            // A sampler object, so that each new texture samples with NEAREST filtering, which
            // makes textures without mipmaps complete.
            const samplerObject = gl.createSampler();
            gl.samplerParameteri(samplerObject, gl.TEXTURE_MIN_FILTER, gl.NEAREST);
            gl.samplerParameteri(samplerObject, gl.TEXTURE_MAG_FILTER, gl.NEAREST);
            gl.bindSampler(0, samplerObject);

            const zeros = compressed && mode == "texSubImage2D" ? new Uint8Array(patterns[0].data.length) : null;
            for (let texture = 0; texture < textureCount; ++texture) {
                textures.push(gl.createTexture());
                gl.bindTexture(gl.TEXTURE_2D, textures[texture]);
                if (mode == "texSubImage2D") {
                    if (compressed)
                        gl.compressedTexImage2D(gl.TEXTURE_2D, 0, internalFormat, width, height, 0, zeros);
                    else
                        gl.texImage2D(gl.TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, null);
                }
            }

            if (hasStencil) {
                stencilTexturing = extensionObjects.WEBGL_stencil_texturing;
                stencilProgram = WebGLBench.createProgram(gl, vertexShader, stencilShader, ["aPosition"]);
                stencilOffsetLocation = gl.getUniformLocation(stencilProgram, "uOffset");
            }
            if (state != "idle") {
                const passTexture = gl.createTexture();
                gl.bindTexture(gl.TEXTURE_2D, passTexture);
                gl.texStorage2D(gl.TEXTURE_2D, 1, gl.RGBA8, passTargetSize, passTargetSize);
                passFramebuffer = gl.createFramebuffer();
                gl.bindFramebuffer(gl.FRAMEBUFFER, passFramebuffer);
                gl.framebufferTexture2D(gl.FRAMEBUFFER, gl.COLOR_ATTACHMENT0, gl.TEXTURE_2D, passTexture, 0);
                gl.clearColor(0, 0, 0, 0);
                gl.clear(gl.COLOR_BUFFER_BIT);
                passColorProgram = WebGLBench.createProgram(gl, vertexShader, passShader(null), ["aPosition"]);
                if (usesTexture) {
                    passSamplingProgram = WebGLBench.createProgram(gl, vertexShader, passShader(sampler), ["aPosition"]);
                    gl.useProgram(passSamplingProgram);
                    gl.uniform1f(gl.getUniformLocation(passSamplingProgram, "uScale"), 0);
                }
            }
            samplingProgram = WebGLBench.createProgram(gl, vertexShader, samplingShader(sampler), ["aPosition"]);
            samplingOffsetLocation = gl.getUniformLocation(samplingProgram, "uOffset");
            WebGLBench.bindFullscreenQuad(gl);
            gl.viewport(0, 0, targetWidth, height);
        },
        iteration(context, n) {
            for (let texture = 0; texture < textureCount; ++texture) {
                if (mode == "texStorage2D") {
                    gl.deleteTexture(textures[texture]);
                    textures[texture] = gl.createTexture();
                    gl.bindTexture(gl.TEXTURE_2D, textures[texture]);
                    gl.texStorage2D(gl.TEXTURE_2D, 1, internalFormat, width, height);
                } else
                    gl.bindTexture(gl.TEXTURE_2D, textures[texture]);
                if (state != "idle")
                    drawToPassTarget(texture, usesTexture);
                if (state == "used-flushed")
                    gl.flush();
                upload(patternFor(n, texture).data);
            }
            if (state != "idle") {
                drawToPassTarget(textureCount, false);
                gl.viewport(0, 0, targetWidth, height);
            }
        },
        probe(context, n) {
            // Samples texel (x, y) of texture t to pixel (t, 0) of the target.
            let x;
            let y;
            for (let texture = 0; texture < textureCount; ++texture) {
                x = WebGLBench.hash(2 * (n * textureCount + texture)) % width;
                y = WebGLBench.hash(2 * (n * textureCount + texture) + 1) % height;
                gl.bindTexture(gl.TEXTURE_2D, textures[texture]);
                sample(x - texture, y, texture, 0, 1, 1);
                const pattern = patternFor(n, texture);
                const entry = entryOf(pattern, x, y);
                for (let c = 0; c < 4; ++c)
                    probeExpected[texture * 4 + c] = pattern.expected[entry * 4 + c];
            }
            const label = textureCount > 1 ? "Sampled texel of each texture, at (texture, 0)" : "Sampled texel at (" + x + ", " + y + ")";
            WebGLBench.verifyPixelsFromTable(label, readTarget(0, 0, textureCount, 1, probePixels), 0, 0, textureCount, 1,
                probeExpected, column => column, patterns[0].tolerances);
        },
        verify(context, n) {
            for (let texture = 0; texture < textureCount; ++texture) {
                gl.bindTexture(gl.TEXTURE_2D, textures[texture]);
                sample(0, 0, 0, 0, width, height);
                const label = textureCount > 1 ? "Sampled texels of texture " + texture : "Sampled texels";
                if (texels <= textureCountTexels) {
                    check(label, n, texture, 0, 0, width, height);
                    continue;
                }
                for (let y = 0; y < height; y += 8)
                    check(label + ", row " + y, n, texture, 0, y, width, 1);
                check(label + ", last row", n, texture, 0, height - 1, width, 1);
                check(label + ", last column", n, texture, width - 1, 0, 1, height);
            }
        }
    });
}

window.runTextureUpload = options => runTextureUpload("texImage2D", options);
window.runTextureSubUpload = options => runTextureUpload("texSubImage2D", options);
window.runTextureStorageUpload = options => runTextureUpload("texStorage2D", options);
})();
