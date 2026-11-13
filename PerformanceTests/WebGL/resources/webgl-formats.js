// The properties of the WebGL 2 texture and renderbuffer formats that the benchmarks use, in one
// table. The benchmark scripts look formats up by name, and generate-tests.py reads the table to
// decide which permutations exist. The table must stay strict JSON after "WebGLFormats = ", with
// nothing after it but the semicolon, so that generate-tests.py can parse it with json.loads().
//
// textureFormats: each combination of internal format, format and type that texImage2D() accepts
// in WebGL 2, one per row, grouped by where it comes from: ES 3.0 table 3.2 (sized internal
// formats), ES 3.0 table 3.3 (unsized internal formats), the unsized depth formats that are not in
// table 3.3 but that WebKit accepts in WebGL 2, because ANGLE supports them for OES_depth_texture,
// and EXT_texture_norm16. The columns are:
//
//   internalFormat, format, type: the combination.
//   channels: how the texture stores each component of format. unormN, snormN: N-bit normalized
//       integers. srgb8: an 8-bit sRGB-encoded component. uintN, intN: N-bit integers. half, float:
//       16-bit and 32-bit floats. uf11, uf10: unsigned 11-bit and 10-bit floats. e5: a component of
//       RGB9_E5, with an exponent shared by the three. depth16, depth24, depth32: normalized depth.
//       depth32f: float depth. stencil8: 8-bit stencil.
//   extension: the extension that the format needs as a texture, or null.
//   renderable: whether the format can be a framebuffer attachment in WebGL 2, from ES 3.0 table
//       3.13: true, false, or the extension that makes it renderable: EXT_color_buffer_float,
//       EXT_render_snorm or WEBGL_render_shared_exponent. The WebGL version of
//       EXT_color_buffer_half_float makes R16F, RG16F and RGBA16F renderable, like
//       EXT_color_buffer_float, but not RGB16F. EXT_texture_norm16 makes R16_EXT, RG16_EXT and
//       RGBA16_EXT renderable.
//   uploadable: whether texImage2D() accepts data, not only null. WebGL 2.0 section 5.14.8: data
//       of type FLOAT_32_UNSIGNED_INT_24_8_REV must be null.
//
// renderbufferOnlyFormats: the renderbuffer formats of ES 3.0 table 3.13 that are not texture
// formats, with the channels they store.
//
// compressedTextureFormats: the formats of the compressed texture extensions that WebKit exposes in
// WebGL 2 (WebGL2RenderingContext::getExtension()), grouped by extension, from each extension's
// specification. The columns are:
//
//   internalFormat: the name of the enum in the extension object.
//   compression: how texture-upload.js encodes a block, see blockEncoders there.
//   blockWidth, blockHeight, blockBytes: the size of a block in texels and in bytes.
//   srgb: whether the color is sRGB-encoded.
//   subimage: whether compressedTexSubImage2D() can replace the level. WEBGL_compressed_texture_etc1:
//       compressedTexSubImage2D() is not supported for COMPRESSED_RGB_ETC1_WEBGL.
const WebGLFormats = {
"textureFormatColumns": ["internalFormat", "format", "type", "channels", "extension", "renderable", "uploadable"],
"textureFormats": {
    "ES 3.0 table 3.2, sized internal formats": [
        ["R8",                  "RED",              "UNSIGNED_BYTE",                   ["unorm8"],                                    null,                  true,                            true],
        ["R8_SNORM",            "RED",              "BYTE",                            ["snorm8"],                                    null,                  "EXT_render_snorm",              true],
        ["R16F",                "RED",              "HALF_FLOAT",                      ["half"],                                      null,                  "EXT_color_buffer_float",        true],
        ["R16F",                "RED",              "FLOAT",                           ["half"],                                      null,                  "EXT_color_buffer_float",        true],
        ["R32F",                "RED",              "FLOAT",                           ["float"],                                     null,                  "EXT_color_buffer_float",        true],
        ["R8UI",                "RED_INTEGER",      "UNSIGNED_BYTE",                   ["uint8"],                                     null,                  true,                            true],
        ["R8I",                 "RED_INTEGER",      "BYTE",                            ["int8"],                                      null,                  true,                            true],
        ["R16UI",               "RED_INTEGER",      "UNSIGNED_SHORT",                  ["uint16"],                                    null,                  true,                            true],
        ["R16I",                "RED_INTEGER",      "SHORT",                           ["int16"],                                     null,                  true,                            true],
        ["R32UI",               "RED_INTEGER",      "UNSIGNED_INT",                    ["uint32"],                                    null,                  true,                            true],
        ["R32I",                "RED_INTEGER",      "INT",                             ["int32"],                                     null,                  true,                            true],
        ["RG8",                 "RG",               "UNSIGNED_BYTE",                   ["unorm8", "unorm8"],                          null,                  true,                            true],
        ["RG8_SNORM",           "RG",               "BYTE",                            ["snorm8", "snorm8"],                          null,                  "EXT_render_snorm",              true],
        ["RG16F",               "RG",               "HALF_FLOAT",                      ["half", "half"],                              null,                  "EXT_color_buffer_float",        true],
        ["RG16F",               "RG",               "FLOAT",                           ["half", "half"],                              null,                  "EXT_color_buffer_float",        true],
        ["RG32F",               "RG",               "FLOAT",                           ["float", "float"],                            null,                  "EXT_color_buffer_float",        true],
        ["RG8UI",               "RG_INTEGER",       "UNSIGNED_BYTE",                   ["uint8", "uint8"],                            null,                  true,                            true],
        ["RG8I",                "RG_INTEGER",       "BYTE",                            ["int8", "int8"],                              null,                  true,                            true],
        ["RG16UI",              "RG_INTEGER",       "UNSIGNED_SHORT",                  ["uint16", "uint16"],                          null,                  true,                            true],
        ["RG16I",               "RG_INTEGER",       "SHORT",                           ["int16", "int16"],                            null,                  true,                            true],
        ["RG32UI",              "RG_INTEGER",       "UNSIGNED_INT",                    ["uint32", "uint32"],                          null,                  true,                            true],
        ["RG32I",               "RG_INTEGER",       "INT",                             ["int32", "int32"],                            null,                  true,                            true],
        ["RGB8",                "RGB",              "UNSIGNED_BYTE",                   ["unorm8", "unorm8", "unorm8"],                null,                  true,                            true],
        ["SRGB8",               "RGB",              "UNSIGNED_BYTE",                   ["srgb8", "srgb8", "srgb8"],                   null,                  false,                           true],
        ["RGB565",              "RGB",              "UNSIGNED_BYTE",                   ["unorm5", "unorm6", "unorm5"],                null,                  true,                            true],
        ["RGB565",              "RGB",              "UNSIGNED_SHORT_5_6_5",            ["unorm5", "unorm6", "unorm5"],                null,                  true,                            true],
        ["RGB8_SNORM",          "RGB",              "BYTE",                            ["snorm8", "snorm8", "snorm8"],                null,                  false,                           true],
        ["R11F_G11F_B10F",      "RGB",              "UNSIGNED_INT_10F_11F_11F_REV",    ["uf11", "uf11", "uf10"],                      null,                  "EXT_color_buffer_float",        true],
        ["R11F_G11F_B10F",      "RGB",              "HALF_FLOAT",                      ["uf11", "uf11", "uf10"],                      null,                  "EXT_color_buffer_float",        true],
        ["R11F_G11F_B10F",      "RGB",              "FLOAT",                           ["uf11", "uf11", "uf10"],                      null,                  "EXT_color_buffer_float",        true],
        ["RGB9_E5",             "RGB",              "UNSIGNED_INT_5_9_9_9_REV",        ["e5", "e5", "e5"],                            null,                  "WEBGL_render_shared_exponent",  true],
        ["RGB9_E5",             "RGB",              "HALF_FLOAT",                      ["e5", "e5", "e5"],                            null,                  "WEBGL_render_shared_exponent",  true],
        ["RGB9_E5",             "RGB",              "FLOAT",                           ["e5", "e5", "e5"],                            null,                  "WEBGL_render_shared_exponent",  true],
        ["RGB16F",              "RGB",              "HALF_FLOAT",                      ["half", "half", "half"],                      null,                  false,                           true],
        ["RGB16F",              "RGB",              "FLOAT",                           ["half", "half", "half"],                      null,                  false,                           true],
        ["RGB32F",              "RGB",              "FLOAT",                           ["float", "float", "float"],                   null,                  false,                           true],
        ["RGB8UI",              "RGB_INTEGER",      "UNSIGNED_BYTE",                   ["uint8", "uint8", "uint8"],                   null,                  false,                           true],
        ["RGB8I",               "RGB_INTEGER",      "BYTE",                            ["int8", "int8", "int8"],                      null,                  false,                           true],
        ["RGB16UI",             "RGB_INTEGER",      "UNSIGNED_SHORT",                  ["uint16", "uint16", "uint16"],                null,                  false,                           true],
        ["RGB16I",              "RGB_INTEGER",      "SHORT",                           ["int16", "int16", "int16"],                   null,                  false,                           true],
        ["RGB32UI",             "RGB_INTEGER",      "UNSIGNED_INT",                    ["uint32", "uint32", "uint32"],                null,                  false,                           true],
        ["RGB32I",              "RGB_INTEGER",      "INT",                             ["int32", "int32", "int32"],                   null,                  false,                           true],
        ["RGBA8",               "RGBA",             "UNSIGNED_BYTE",                   ["unorm8", "unorm8", "unorm8", "unorm8"],      null,                  true,                            true],
        ["SRGB8_ALPHA8",        "RGBA",             "UNSIGNED_BYTE",                   ["srgb8", "srgb8", "srgb8", "unorm8"],         null,                  true,                            true],
        ["RGBA8_SNORM",         "RGBA",             "BYTE",                            ["snorm8", "snorm8", "snorm8", "snorm8"],      null,                  "EXT_render_snorm",              true],
        ["RGB5_A1",             "RGBA",             "UNSIGNED_BYTE",                   ["unorm5", "unorm5", "unorm5", "unorm1"],      null,                  true,                            true],
        ["RGB5_A1",             "RGBA",             "UNSIGNED_SHORT_5_5_5_1",          ["unorm5", "unorm5", "unorm5", "unorm1"],      null,                  true,                            true],
        ["RGB5_A1",             "RGBA",             "UNSIGNED_INT_2_10_10_10_REV",     ["unorm5", "unorm5", "unorm5", "unorm1"],      null,                  true,                            true],
        ["RGBA4",               "RGBA",             "UNSIGNED_BYTE",                   ["unorm4", "unorm4", "unorm4", "unorm4"],      null,                  true,                            true],
        ["RGBA4",               "RGBA",             "UNSIGNED_SHORT_4_4_4_4",          ["unorm4", "unorm4", "unorm4", "unorm4"],      null,                  true,                            true],
        ["RGB10_A2",            "RGBA",             "UNSIGNED_INT_2_10_10_10_REV",     ["unorm10", "unorm10", "unorm10", "unorm2"],   null,                  true,                            true],
        ["RGBA16F",             "RGBA",             "HALF_FLOAT",                      ["half", "half", "half", "half"],              null,                  "EXT_color_buffer_float",        true],
        ["RGBA16F",             "RGBA",             "FLOAT",                           ["half", "half", "half", "half"],              null,                  "EXT_color_buffer_float",        true],
        ["RGBA32F",             "RGBA",             "FLOAT",                           ["float", "float", "float", "float"],          null,                  "EXT_color_buffer_float",        true],
        ["RGBA8UI",             "RGBA_INTEGER",     "UNSIGNED_BYTE",                   ["uint8", "uint8", "uint8", "uint8"],          null,                  true,                            true],
        ["RGBA8I",              "RGBA_INTEGER",     "BYTE",                            ["int8", "int8", "int8", "int8"],              null,                  true,                            true],
        ["RGB10_A2UI",          "RGBA_INTEGER",     "UNSIGNED_INT_2_10_10_10_REV",     ["uint10", "uint10", "uint10", "uint2"],       null,                  true,                            true],
        ["RGBA16UI",            "RGBA_INTEGER",     "UNSIGNED_SHORT",                  ["uint16", "uint16", "uint16", "uint16"],      null,                  true,                            true],
        ["RGBA16I",             "RGBA_INTEGER",     "SHORT",                           ["int16", "int16", "int16", "int16"],          null,                  true,                            true],
        ["RGBA32UI",            "RGBA_INTEGER",     "UNSIGNED_INT",                    ["uint32", "uint32", "uint32", "uint32"],      null,                  true,                            true],
        ["RGBA32I",             "RGBA_INTEGER",     "INT",                             ["int32", "int32", "int32", "int32"],          null,                  true,                            true],
        ["DEPTH_COMPONENT16",   "DEPTH_COMPONENT",  "UNSIGNED_SHORT",                  ["depth16"],                                   null,                  true,                            true],
        ["DEPTH_COMPONENT16",   "DEPTH_COMPONENT",  "UNSIGNED_INT",                    ["depth16"],                                   null,                  true,                            true],
        ["DEPTH_COMPONENT24",   "DEPTH_COMPONENT",  "UNSIGNED_INT",                    ["depth24"],                                   null,                  true,                            true],
        ["DEPTH_COMPONENT32F",  "DEPTH_COMPONENT",  "FLOAT",                           ["depth32f"],                                  null,                  true,                            true],
        ["DEPTH24_STENCIL8",    "DEPTH_STENCIL",    "UNSIGNED_INT_24_8",               ["depth24", "stencil8"],                       null,                  true,                            true],
        ["DEPTH32F_STENCIL8",   "DEPTH_STENCIL",    "FLOAT_32_UNSIGNED_INT_24_8_REV",  ["depth32f", "stencil8"],                      null,                  true,                            false]
    ],
    "ES 3.0 table 3.3, unsized internal formats": [
        ["RGBA",                "RGBA",             "UNSIGNED_BYTE",                   ["unorm8", "unorm8", "unorm8", "unorm8"],      null,                  true,                            true],
        ["RGBA",                "RGBA",             "UNSIGNED_SHORT_4_4_4_4",          ["unorm4", "unorm4", "unorm4", "unorm4"],      null,                  true,                            true],
        ["RGBA",                "RGBA",             "UNSIGNED_SHORT_5_5_5_1",          ["unorm5", "unorm5", "unorm5", "unorm1"],      null,                  true,                            true],
        ["RGB",                 "RGB",              "UNSIGNED_BYTE",                   ["unorm8", "unorm8", "unorm8"],                null,                  true,                            true],
        ["RGB",                 "RGB",              "UNSIGNED_SHORT_5_6_5",            ["unorm5", "unorm6", "unorm5"],                null,                  true,                            true],
        ["LUMINANCE_ALPHA",     "LUMINANCE_ALPHA",  "UNSIGNED_BYTE",                   ["unorm8", "unorm8"],                          null,                  false,                           true],
        ["LUMINANCE",           "LUMINANCE",        "UNSIGNED_BYTE",                   ["unorm8"],                                    null,                  false,                           true],
        ["ALPHA",               "ALPHA",            "UNSIGNED_BYTE",                   ["unorm8"],                                    null,                  false,                           true]
    ],
    "Unsized depth formats that WebKit accepts in WebGL 2": [
        ["DEPTH_COMPONENT",     "DEPTH_COMPONENT",  "UNSIGNED_SHORT",                  ["depth16"],                                   null,                  true,                            true],
        ["DEPTH_COMPONENT",     "DEPTH_COMPONENT",  "UNSIGNED_INT",                    ["depth32"],                                   null,                  true,                            true],
        ["DEPTH_STENCIL",       "DEPTH_STENCIL",    "UNSIGNED_INT_24_8",               ["depth24", "stencil8"],                       null,                  true,                            true]
    ],
    "EXT_texture_norm16": [
        ["R16_EXT",             "RED",              "UNSIGNED_SHORT",                  ["unorm16"],                                   "EXT_texture_norm16",  true,                            true],
        ["RG16_EXT",            "RG",               "UNSIGNED_SHORT",                  ["unorm16", "unorm16"],                        "EXT_texture_norm16",  true,                            true],
        ["RGB16_EXT",           "RGB",              "UNSIGNED_SHORT",                  ["unorm16", "unorm16", "unorm16"],             "EXT_texture_norm16",  false,                           true],
        ["RGBA16_EXT",          "RGBA",             "UNSIGNED_SHORT",                  ["unorm16", "unorm16", "unorm16", "unorm16"],  "EXT_texture_norm16",  true,                            true],
        ["R16_SNORM_EXT",       "RED",              "SHORT",                           ["snorm16"],                                   "EXT_texture_norm16",  "EXT_render_snorm",              true],
        ["RG16_SNORM_EXT",      "RG",               "SHORT",                           ["snorm16", "snorm16"],                        "EXT_texture_norm16",  "EXT_render_snorm",              true],
        ["RGB16_SNORM_EXT",     "RGB",              "SHORT",                           ["snorm16", "snorm16", "snorm16"],             "EXT_texture_norm16",  false,                           true],
        ["RGBA16_SNORM_EXT",    "RGBA",             "SHORT",                           ["snorm16", "snorm16", "snorm16", "snorm16"],  "EXT_texture_norm16",  "EXT_render_snorm",              true]
    ]
},
"renderbufferOnlyFormatColumns": ["internalFormat", "channels"],
"renderbufferOnlyFormats": [
    ["STENCIL_INDEX8", ["stencil8"]]
],
"compressedTextureFormatColumns": ["internalFormat", "compression", "blockWidth", "blockHeight", "blockBytes", "srgb", "subimage"],
"compressedTextureFormats": {
    "WEBGL_compressed_texture_s3tc": [
        ["COMPRESSED_RGB_S3TC_DXT1_EXT",               "bc1",                4,   4,   8,   false,  true],
        ["COMPRESSED_RGBA_S3TC_DXT1_EXT",              "bc1",                4,   4,   8,   false,  true],
        ["COMPRESSED_RGBA_S3TC_DXT3_EXT",              "bc2",                4,   4,   16,  false,  true],
        ["COMPRESSED_RGBA_S3TC_DXT5_EXT",              "bc3",                4,   4,   16,  false,  true]
    ],
    "WEBGL_compressed_texture_s3tc_srgb": [
        ["COMPRESSED_SRGB_S3TC_DXT1_EXT",              "bc1",                4,   4,   8,   true,   true],
        ["COMPRESSED_SRGB_ALPHA_S3TC_DXT1_EXT",        "bc1",                4,   4,   8,   true,   true],
        ["COMPRESSED_SRGB_ALPHA_S3TC_DXT3_EXT",        "bc2",                4,   4,   16,  true,   true],
        ["COMPRESSED_SRGB_ALPHA_S3TC_DXT5_EXT",        "bc3",                4,   4,   16,  true,   true]
    ],
    "EXT_texture_compression_rgtc": [
        ["COMPRESSED_RED_RGTC1_EXT",                   "bc4",                4,   4,   8,   false,  true],
        ["COMPRESSED_SIGNED_RED_RGTC1_EXT",            "bc4_snorm",          4,   4,   8,   false,  true],
        ["COMPRESSED_RED_GREEN_RGTC2_EXT",             "bc5",                4,   4,   16,  false,  true],
        ["COMPRESSED_SIGNED_RED_GREEN_RGTC2_EXT",      "bc5_snorm",          4,   4,   16,  false,  true]
    ],
    "EXT_texture_compression_bptc": [
        ["COMPRESSED_RGBA_BPTC_UNORM_EXT",             "bc7",                4,   4,   16,  false,  true],
        ["COMPRESSED_SRGB_ALPHA_BPTC_UNORM_EXT",       "bc7",                4,   4,   16,  true,   true],
        ["COMPRESSED_RGB_BPTC_SIGNED_FLOAT_EXT",       "bc6h_sfloat",        4,   4,   16,  false,  true],
        ["COMPRESSED_RGB_BPTC_UNSIGNED_FLOAT_EXT",     "bc6h_ufloat",        4,   4,   16,  false,  true]
    ],
    "WEBGL_compressed_texture_etc": [
        ["COMPRESSED_R11_EAC",                         "eac_r11",            4,   4,   8,   false,  true],
        ["COMPRESSED_SIGNED_R11_EAC",                  "eac_r11_snorm",      4,   4,   8,   false,  true],
        ["COMPRESSED_RG11_EAC",                        "eac_rg11",           4,   4,   16,  false,  true],
        ["COMPRESSED_SIGNED_RG11_EAC",                 "eac_rg11_snorm",     4,   4,   16,  false,  true],
        ["COMPRESSED_RGB8_ETC2",                       "etc1",               4,   4,   8,   false,  true],
        ["COMPRESSED_SRGB8_ETC2",                      "etc1",               4,   4,   8,   true,   true],
        ["COMPRESSED_RGB8_PUNCHTHROUGH_ALPHA1_ETC2",   "etc2_punchthrough",  4,   4,   8,   false,  true],
        ["COMPRESSED_SRGB8_PUNCHTHROUGH_ALPHA1_ETC2",  "etc2_punchthrough",  4,   4,   8,   true,   true],
        ["COMPRESSED_RGBA8_ETC2_EAC",                  "etc2_eac",           4,   4,   16,  false,  true],
        ["COMPRESSED_SRGB8_ALPHA8_ETC2_EAC",           "etc2_eac",           4,   4,   16,  true,   true]
    ],
    "WEBGL_compressed_texture_etc1": [
        ["COMPRESSED_RGB_ETC1_WEBGL",                  "etc1",               4,   4,   8,   false,  false]
    ],
    "WEBGL_compressed_texture_pvrtc": [
        ["COMPRESSED_RGB_PVRTC_4BPPV1_IMG",            "pvrtc",              4,   4,   8,   false,  true],
        ["COMPRESSED_RGB_PVRTC_2BPPV1_IMG",            "pvrtc",              8,   4,   8,   false,  true],
        ["COMPRESSED_RGBA_PVRTC_4BPPV1_IMG",           "pvrtc",              4,   4,   8,   false,  true],
        ["COMPRESSED_RGBA_PVRTC_2BPPV1_IMG",           "pvrtc",              8,   4,   8,   false,  true]
    ],
    "WEBGL_compressed_texture_astc": [
        ["COMPRESSED_RGBA_ASTC_4x4_KHR",               "astc",               4,   4,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_5x4_KHR",               "astc",               5,   4,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_5x5_KHR",               "astc",               5,   5,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_6x5_KHR",               "astc",               6,   5,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_6x6_KHR",               "astc",               6,   6,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_8x5_KHR",               "astc",               8,   5,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_8x6_KHR",               "astc",               8,   6,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_8x8_KHR",               "astc",               8,   8,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_10x5_KHR",              "astc",               10,  5,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_10x6_KHR",              "astc",               10,  6,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_10x8_KHR",              "astc",               10,  8,   16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_10x10_KHR",             "astc",               10,  10,  16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_12x10_KHR",             "astc",               12,  10,  16,  false,  true],
        ["COMPRESSED_RGBA_ASTC_12x12_KHR",             "astc",               12,  12,  16,  false,  true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_4x4_KHR",       "astc",               4,   4,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_5x4_KHR",       "astc",               5,   4,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_5x5_KHR",       "astc",               5,   5,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_6x5_KHR",       "astc",               6,   5,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_6x6_KHR",       "astc",               6,   6,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_8x5_KHR",       "astc",               8,   5,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_8x6_KHR",       "astc",               8,   6,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_8x8_KHR",       "astc",               8,   8,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_10x5_KHR",      "astc",               10,  5,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_10x6_KHR",      "astc",               10,  6,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_10x8_KHR",      "astc",               10,  8,   16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_10x10_KHR",     "astc",               10,  10,  16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_12x10_KHR",     "astc",               12,  10,  16,  true,   true],
        ["COMPRESSED_SRGB8_ALPHA8_ASTC_12x12_KHR",     "astc",               12,  12,  16,  true,   true]
    ]
}
};
