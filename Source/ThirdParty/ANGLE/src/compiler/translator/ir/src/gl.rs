// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// GL constants that are relevant to the compiler, copied out of include/GLES3/gl32.h,
// include/GLES2/gl2ext.h and include/GLES2/gl2ext_angle.h

pub type Enum = u32;

pub const NONE: Enum = 0;

// For ShaderVariable::gl_type
pub const FLOAT: Enum = 0x1406;
pub const FLOAT_VEC2: Enum = 0x8B50;
pub const FLOAT_VEC3: Enum = 0x8B51;
pub const FLOAT_VEC4: Enum = 0x8B52;
pub const INT: Enum = 0x1404;
pub const INT_VEC2: Enum = 0x8B53;
pub const INT_VEC3: Enum = 0x8B54;
pub const INT_VEC4: Enum = 0x8B55;
pub const UNSIGNED_INT: Enum = 0x1405;
pub const UNSIGNED_INT_VEC2: Enum = 0x8DC6;
pub const UNSIGNED_INT_VEC3: Enum = 0x8DC7;
pub const UNSIGNED_INT_VEC4: Enum = 0x8DC8;
pub const BOOL: Enum = 0x8B56;
pub const BOOL_VEC2: Enum = 0x8B57;
pub const BOOL_VEC3: Enum = 0x8B58;
pub const BOOL_VEC4: Enum = 0x8B59;
pub const FLOAT_MAT2: Enum = 0x8B5A;
pub const FLOAT_MAT3: Enum = 0x8B5B;
pub const FLOAT_MAT4: Enum = 0x8B5C;
pub const FLOAT_MAT2X3: Enum = 0x8B65;
pub const FLOAT_MAT2X4: Enum = 0x8B66;
pub const FLOAT_MAT3X2: Enum = 0x8B67;
pub const FLOAT_MAT3X4: Enum = 0x8B68;
pub const FLOAT_MAT4X2: Enum = 0x8B69;
pub const FLOAT_MAT4X3: Enum = 0x8B6A;

pub const SAMPLER_2D: Enum = 0x8B5E;
pub const SAMPLER_3D: Enum = 0x8B5F;
pub const SAMPLER_CUBE: Enum = 0x8B60;
pub const SAMPLER_2D_SHADOW: Enum = 0x8B62;
pub const SAMPLER_2D_ARRAY: Enum = 0x8DC1;
pub const SAMPLER_2D_ARRAY_SHADOW: Enum = 0x8DC4;
pub const SAMPLER_CUBE_SHADOW: Enum = 0x8DC5;
pub const SAMPLER_2D_MULTISAMPLE: Enum = 0x9108;
pub const INT_SAMPLER_2D_MULTISAMPLE: Enum = 0x9109;
pub const UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE: Enum = 0x910A;
pub const SAMPLER_2D_MULTISAMPLE_ARRAY: Enum = 0x910B;
pub const INT_SAMPLER_2D_MULTISAMPLE_ARRAY: Enum = 0x910C;
pub const UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY: Enum = 0x910D;
pub const SAMPLER_CUBE_MAP_ARRAY: Enum = 0x900C;
pub const SAMPLER_CUBE_MAP_ARRAY_SHADOW: Enum = 0x900D;
pub const INT_SAMPLER_CUBE_MAP_ARRAY: Enum = 0x900E;
pub const UNSIGNED_INT_SAMPLER_CUBE_MAP_ARRAY: Enum = 0x900F;
pub const INT_SAMPLER_2D: Enum = 0x8DCA;
pub const INT_SAMPLER_3D: Enum = 0x8DCB;
pub const INT_SAMPLER_CUBE: Enum = 0x8DCC;
pub const INT_SAMPLER_2D_ARRAY: Enum = 0x8DCF;
pub const UNSIGNED_INT_SAMPLER_2D: Enum = 0x8DD2;
pub const UNSIGNED_INT_SAMPLER_3D: Enum = 0x8DD3;
pub const UNSIGNED_INT_SAMPLER_CUBE: Enum = 0x8DD4;
pub const UNSIGNED_INT_SAMPLER_2D_ARRAY: Enum = 0x8DD7;

pub const IMAGE_2D: Enum = 0x904D;
pub const IMAGE_3D: Enum = 0x904E;
pub const IMAGE_CUBE: Enum = 0x9050;
pub const IMAGE_2D_ARRAY: Enum = 0x9053;
pub const INT_IMAGE_2D: Enum = 0x9058;
pub const INT_IMAGE_3D: Enum = 0x9059;
pub const INT_IMAGE_CUBE: Enum = 0x905B;
pub const INT_IMAGE_2D_ARRAY: Enum = 0x905E;
pub const UNSIGNED_INT_IMAGE_2D: Enum = 0x9063;
pub const UNSIGNED_INT_IMAGE_3D: Enum = 0x9064;
pub const UNSIGNED_INT_IMAGE_CUBE: Enum = 0x9066;
pub const UNSIGNED_INT_IMAGE_2D_ARRAY: Enum = 0x9069;
pub const IMAGE_CUBE_MAP_ARRAY: Enum = 0x9054;
pub const INT_IMAGE_CUBE_MAP_ARRAY: Enum = 0x905F;
pub const UNSIGNED_INT_IMAGE_CUBE_MAP_ARRAY: Enum = 0x906A;

pub const SAMPLER_BUFFER: Enum = 0x8DC2;
pub const INT_SAMPLER_BUFFER: Enum = 0x8DD0;
pub const UNSIGNED_INT_SAMPLER_BUFFER: Enum = 0x8DD8;
pub const IMAGE_BUFFER: Enum = 0x9051;
pub const INT_IMAGE_BUFFER: Enum = 0x905C;
pub const UNSIGNED_INT_IMAGE_BUFFER: Enum = 0x9067;

pub const UNSIGNED_INT_ATOMIC_COUNTER: Enum = 0x92DB;

pub const SAMPLER_EXTERNAL_OES: Enum = 0x8D66;
pub const SAMPLER_EXTERNAL_2D_Y2Y_EXT: Enum = 0x8BE7;
pub const SAMPLER_2D_RECT_ANGLE: Enum = 0x8B63;
pub const SAMPLER_VIDEO_IMAGE_WEBGL: Enum = 0x9249;

// For ShaderVariable::gl_precision
pub const LOW_FLOAT: Enum = 0x8DF0;
pub const MEDIUM_FLOAT: Enum = 0x8DF1;
pub const HIGH_FLOAT: Enum = 0x8DF2;
pub const LOW_INT: Enum = 0x8DF3;
pub const MEDIUM_INT: Enum = 0x8DF4;
pub const HIGH_INT: Enum = 0x8DF5;

// For ShaderVariable::gl_image_unit_format
pub const RGBA32F: Enum = 0x8814;
pub const RGBA16F: Enum = 0x881A;
pub const RGBA8: Enum = 0x8058;
pub const RGBA8_SNORM: Enum = 0x8F97;
pub const RGBA32UI: Enum = 0x8D70;
pub const RGBA16UI: Enum = 0x8D76;
pub const RGBA8UI: Enum = 0x8D7C;
pub const RGBA32I: Enum = 0x8D82;
pub const RGBA16I: Enum = 0x8D88;
pub const RGBA8I: Enum = 0x8D8E;
pub const R32F: Enum = 0x822E;
pub const R32UI: Enum = 0x8236;
pub const R32I: Enum = 0x8235;
