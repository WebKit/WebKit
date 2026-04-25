/* Capstone Disassembly Engine, https://www.capstone-engine.org */
/* By Nguyen Anh Quynh <aquynh@gmail.com>, 2013-2019 */
/* By Rot127 <unisono@quyllur.org>, 2023 */

/* This header file mirrors LLVMs MachineValueTypes.h. */

#ifndef CS_SIMPLE_TYPES_H
#define CS_SIMPLE_TYPES_H

#include <assert.h>
#include <stdbool.h>

typedef enum {

	// Simple value types that aren't explicitly part of this enumeration
	// are considered extended value types.
	CS_DATA_TYPE_INVALID_SIMPLE_VALUE_TYPE = 0,

	// If you change this numbering, you must change the values in
	// ValueTypes.td as well!
	CS_DATA_TYPE_Other = 1, // This is a non-standard value
	CS_DATA_TYPE_i1 = 2,	// This is a 1 bit integer value
	CS_DATA_TYPE_i2 = 3,	// This is a 2 bit integer value
	CS_DATA_TYPE_i4 = 4,	// This is a 4 bit integer value
	CS_DATA_TYPE_i8 = 5,	// This is an 8 bit integer value
	CS_DATA_TYPE_i16 = 6,	// This is a 16 bit integer value
	CS_DATA_TYPE_i32 = 7,	// This is a 32 bit integer value
	CS_DATA_TYPE_i64 = 8,	// This is a 64 bit integer value
	CS_DATA_TYPE_i128 = 9,	// This is a 128 bit integer value

	CS_DATA_TYPE_FIRST_INTEGER_VALUETYPE = CS_DATA_TYPE_i1,
	CS_DATA_TYPE_LAST_INTEGER_VALUETYPE = CS_DATA_TYPE_i128,

	CS_DATA_TYPE_bf16 = 10, // This is a 16 bit brain floating point value
	CS_DATA_TYPE_f16 = 11,	// This is a 16 bit floating point value
	CS_DATA_TYPE_f32 = 12,	// This is a 32 bit floating point value
	CS_DATA_TYPE_f64 = 13,	// This is a 64 bit floating point value
	CS_DATA_TYPE_f80 = 14,	// This is a 80 bit floating point value
	CS_DATA_TYPE_f128 = 15, // This is a 128 bit floating point value
	CS_DATA_TYPE_ppcf128 = 16, // This is a PPC 128-bit floating point value

	CS_DATA_TYPE_FIRST_FP_VALUETYPE = CS_DATA_TYPE_bf16,
	CS_DATA_TYPE_LAST_FP_VALUETYPE = CS_DATA_TYPE_ppcf128,

	CS_DATA_TYPE_v1i1 = 17,	    //    1 x i1
	CS_DATA_TYPE_v2i1 = 18,	    //    2 x i1
	CS_DATA_TYPE_v4i1 = 19,	    //    4 x i1
	CS_DATA_TYPE_v8i1 = 20,	    //    8 x i1
	CS_DATA_TYPE_v16i1 = 21,    //   16 x i1
	CS_DATA_TYPE_v32i1 = 22,    //   32 x i1
	CS_DATA_TYPE_v64i1 = 23,    //   64 x i1
	CS_DATA_TYPE_v128i1 = 24,   //  128 x i1
	CS_DATA_TYPE_v256i1 = 25,   //  256 x i1
	CS_DATA_TYPE_v512i1 = 26,   //  512 x i1
	CS_DATA_TYPE_v1024i1 = 27,  // 1024 x i1
	CS_DATA_TYPE_v2048i1 = 28,  // 2048 x i1

	CS_DATA_TYPE_v128i2 = 29,   //  128 x i2
	CS_DATA_TYPE_v256i2 = 30,   //  256 x i2

	CS_DATA_TYPE_v64i4 = 31,    //   64 x i4
	CS_DATA_TYPE_v128i4 = 32,   //  128 x i4

	CS_DATA_TYPE_v1i8 = 33,	    //    1 x i8
	CS_DATA_TYPE_v2i8 = 34,	    //    2 x i8
	CS_DATA_TYPE_v4i8 = 35,	    //    4 x i8
	CS_DATA_TYPE_v8i8 = 36,	    //    8 x i8
	CS_DATA_TYPE_v16i8 = 37,    //   16 x i8
	CS_DATA_TYPE_v32i8 = 38,    //   32 x i8
	CS_DATA_TYPE_v64i8 = 39,    //   64 x i8
	CS_DATA_TYPE_v128i8 = 40,   //  128 x i8
	CS_DATA_TYPE_v256i8 = 41,   //  256 x i8
	CS_DATA_TYPE_v512i8 = 42,   //  512 x i8
	CS_DATA_TYPE_v1024i8 = 43,  // 1024 x i8

	CS_DATA_TYPE_v1i16 = 44,    //   1 x i16
	CS_DATA_TYPE_v2i16 = 45,    //   2 x i16
	CS_DATA_TYPE_v3i16 = 46,    //   3 x i16
	CS_DATA_TYPE_v4i16 = 47,    //   4 x i16
	CS_DATA_TYPE_v8i16 = 48,    //   8 x i16
	CS_DATA_TYPE_v16i16 = 49,   //  16 x i16
	CS_DATA_TYPE_v32i16 = 50,   //  32 x i16
	CS_DATA_TYPE_v64i16 = 51,   //  64 x i16
	CS_DATA_TYPE_v128i16 = 52,  // 128 x i16
	CS_DATA_TYPE_v256i16 = 53,  // 256 x i16
	CS_DATA_TYPE_v512i16 = 54,  // 512 x i16

	CS_DATA_TYPE_v1i32 = 55,    //    1 x i32
	CS_DATA_TYPE_v2i32 = 56,    //    2 x i32
	CS_DATA_TYPE_v3i32 = 57,    //    3 x i32
	CS_DATA_TYPE_v4i32 = 58,    //    4 x i32
	CS_DATA_TYPE_v5i32 = 59,    //    5 x i32
	CS_DATA_TYPE_v6i32 = 60,    //    6 x i32
	CS_DATA_TYPE_v7i32 = 61,    //    7 x i32
	CS_DATA_TYPE_v8i32 = 62,    //    8 x i32
	CS_DATA_TYPE_v9i32 = 63,    //    9 x i32
	CS_DATA_TYPE_v10i32 = 64,   //   10 x i32
	CS_DATA_TYPE_v11i32 = 65,   //   11 x i32
	CS_DATA_TYPE_v12i32 = 66,   //   12 x i32
	CS_DATA_TYPE_v16i32 = 67,   //   16 x i32
	CS_DATA_TYPE_v32i32 = 68,   //   32 x i32
	CS_DATA_TYPE_v64i32 = 69,   //   64 x i32
	CS_DATA_TYPE_v128i32 = 70,  //  128 x i32
	CS_DATA_TYPE_v256i32 = 71,  //  256 x i32
	CS_DATA_TYPE_v512i32 = 72,  //  512 x i32
	CS_DATA_TYPE_v1024i32 = 73, // 1024 x i32
	CS_DATA_TYPE_v2048i32 = 74, // 2048 x i32

	CS_DATA_TYPE_v1i64 = 75,    //   1 x i64
	CS_DATA_TYPE_v2i64 = 76,    //   2 x i64
	CS_DATA_TYPE_v3i64 = 77,    //   3 x i64
	CS_DATA_TYPE_v4i64 = 78,    //   4 x i64
	CS_DATA_TYPE_v8i64 = 79,    //   8 x i64
	CS_DATA_TYPE_v16i64 = 80,   //  16 x i64
	CS_DATA_TYPE_v32i64 = 81,   //  32 x i64
	CS_DATA_TYPE_v64i64 = 82,   //  64 x i64
	CS_DATA_TYPE_v128i64 = 83,  // 128 x i64
	CS_DATA_TYPE_v256i64 = 84,  // 256 x i64

	CS_DATA_TYPE_v1i128 = 85,   //  1 x i128

	CS_DATA_TYPE_FIRST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE =
		CS_DATA_TYPE_v1i1,
	CS_DATA_TYPE_LAST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE =
		CS_DATA_TYPE_v1i128,

	CS_DATA_TYPE_v1f16 = 86,     //    1 x f16
	CS_DATA_TYPE_v2f16 = 87,     //    2 x f16
	CS_DATA_TYPE_v3f16 = 88,     //    3 x f16
	CS_DATA_TYPE_v4f16 = 89,     //    4 x f16
	CS_DATA_TYPE_v8f16 = 90,     //    8 x f16
	CS_DATA_TYPE_v16f16 = 91,    //   16 x f16
	CS_DATA_TYPE_v32f16 = 92,    //   32 x f16
	CS_DATA_TYPE_v64f16 = 93,    //   64 x f16
	CS_DATA_TYPE_v128f16 = 94,   //  128 x f16
	CS_DATA_TYPE_v256f16 = 95,   //  256 x f16
	CS_DATA_TYPE_v512f16 = 96,   //  512 x f16

	CS_DATA_TYPE_v2bf16 = 97,    //    2 x bf16
	CS_DATA_TYPE_v3bf16 = 98,    //    3 x bf16
	CS_DATA_TYPE_v4bf16 = 99,    //    4 x bf16
	CS_DATA_TYPE_v8bf16 = 100,   //    8 x bf16
	CS_DATA_TYPE_v16bf16 = 101,  //   16 x bf16
	CS_DATA_TYPE_v32bf16 = 102,  //   32 x bf16
	CS_DATA_TYPE_v64bf16 = 103,  //   64 x bf16
	CS_DATA_TYPE_v128bf16 = 104, //  128 x bf16

	CS_DATA_TYPE_v1f32 = 105,    //    1 x f32
	CS_DATA_TYPE_v2f32 = 106,    //    2 x f32
	CS_DATA_TYPE_v3f32 = 107,    //    3 x f32
	CS_DATA_TYPE_v4f32 = 108,    //    4 x f32
	CS_DATA_TYPE_v5f32 = 109,    //    5 x f32
	CS_DATA_TYPE_v6f32 = 110,    //    6 x f32
	CS_DATA_TYPE_v7f32 = 111,    //    7 x f32
	CS_DATA_TYPE_v8f32 = 112,    //    8 x f32
	CS_DATA_TYPE_v9f32 = 113,    //    9 x f32
	CS_DATA_TYPE_v10f32 = 114,   //   10 x f32
	CS_DATA_TYPE_v11f32 = 115,   //   11 x f32
	CS_DATA_TYPE_v12f32 = 116,   //   12 x f32
	CS_DATA_TYPE_v16f32 = 117,   //   16 x f32

	CS_DATA_TYPE_v32f32 = 118,   //   32 x f32
	CS_DATA_TYPE_v64f32 = 119,   //   64 x f32
	CS_DATA_TYPE_v128f32 = 120,  //  128 x f32
	CS_DATA_TYPE_v256f32 = 121,  //  256 x f32
	CS_DATA_TYPE_v512f32 = 122,  //  512 x f32
	CS_DATA_TYPE_v1024f32 = 123, // 1024 x f32
	CS_DATA_TYPE_v2048f32 = 124, // 2048 x f32

	CS_DATA_TYPE_v1f64 = 125,    //    1 x f64
	CS_DATA_TYPE_v2f64 = 126,    //    2 x f64
	CS_DATA_TYPE_v3f64 = 127,    //    3 x f64
	CS_DATA_TYPE_v4f64 = 128,    //    4 x f64
	CS_DATA_TYPE_v8f64 = 129,    //    8 x f64
	CS_DATA_TYPE_v16f64 = 130,   //   16 x f64
	CS_DATA_TYPE_v32f64 = 131,   //   32 x f64
	CS_DATA_TYPE_v64f64 = 132,   //   64 x f64
	CS_DATA_TYPE_v128f64 = 133,  //  128 x f64
	CS_DATA_TYPE_v256f64 = 134,  //  256 x f64

	CS_DATA_TYPE_FIRST_FP_FIXEDLEN_VECTOR_VALUETYPE = CS_DATA_TYPE_v1f16,
	CS_DATA_TYPE_LAST_FP_FIXEDLEN_VECTOR_VALUETYPE = CS_DATA_TYPE_v256f64,

	CS_DATA_TYPE_FIRST_FIXEDLEN_VECTOR_VALUETYPE = CS_DATA_TYPE_v1i1,
	CS_DATA_TYPE_LAST_FIXEDLEN_VECTOR_VALUETYPE = CS_DATA_TYPE_v256f64,

	CS_DATA_TYPE_nxv1i1 = 135,   // n x  1 x i1
	CS_DATA_TYPE_nxv2i1 = 136,   // n x  2 x i1
	CS_DATA_TYPE_nxv4i1 = 137,   // n x  4 x i1
	CS_DATA_TYPE_nxv8i1 = 138,   // n x  8 x i1
	CS_DATA_TYPE_nxv16i1 = 139,  // n x 16 x i1
	CS_DATA_TYPE_nxv32i1 = 140,  // n x 32 x i1
	CS_DATA_TYPE_nxv64i1 = 141,  // n x 64 x i1

	CS_DATA_TYPE_nxv1i8 = 142,   // n x  1 x i8
	CS_DATA_TYPE_nxv2i8 = 143,   // n x  2 x i8
	CS_DATA_TYPE_nxv4i8 = 144,   // n x  4 x i8
	CS_DATA_TYPE_nxv8i8 = 145,   // n x  8 x i8
	CS_DATA_TYPE_nxv16i8 = 146,  // n x 16 x i8
	CS_DATA_TYPE_nxv32i8 = 147,  // n x 32 x i8
	CS_DATA_TYPE_nxv64i8 = 148,  // n x 64 x i8

	CS_DATA_TYPE_nxv1i16 = 149,  // n x  1 x i16
	CS_DATA_TYPE_nxv2i16 = 150,  // n x  2 x i16
	CS_DATA_TYPE_nxv4i16 = 151,  // n x  4 x i16
	CS_DATA_TYPE_nxv8i16 = 152,  // n x  8 x i16
	CS_DATA_TYPE_nxv16i16 = 153, // n x 16 x i16
	CS_DATA_TYPE_nxv32i16 = 154, // n x 32 x i16

	CS_DATA_TYPE_nxv1i32 = 155,  // n x  1 x i32
	CS_DATA_TYPE_nxv2i32 = 156,  // n x  2 x i32
	CS_DATA_TYPE_nxv4i32 = 157,  // n x  4 x i32
	CS_DATA_TYPE_nxv8i32 = 158,  // n x  8 x i32
	CS_DATA_TYPE_nxv16i32 = 159, // n x 16 x i32
	CS_DATA_TYPE_nxv32i32 = 160, // n x 32 x i32

	CS_DATA_TYPE_nxv1i64 = 161,  // n x  1 x i64
	CS_DATA_TYPE_nxv2i64 = 162,  // n x  2 x i64
	CS_DATA_TYPE_nxv4i64 = 163,  // n x  4 x i64
	CS_DATA_TYPE_nxv8i64 = 164,  // n x  8 x i64
	CS_DATA_TYPE_nxv16i64 = 165, // n x 16 x i64
	CS_DATA_TYPE_nxv32i64 = 166, // n x 32 x i64

	CS_DATA_TYPE_FIRST_INTEGER_SCALABLE_VECTOR_VALUETYPE =
		CS_DATA_TYPE_nxv1i1,
	CS_DATA_TYPE_LAST_INTEGER_SCALABLE_VECTOR_VALUETYPE =
		CS_DATA_TYPE_nxv32i64,

	CS_DATA_TYPE_nxv1f16 = 167,   // n x  1 x f16
	CS_DATA_TYPE_nxv2f16 = 168,   // n x  2 x f16
	CS_DATA_TYPE_nxv4f16 = 169,   // n x  4 x f16
	CS_DATA_TYPE_nxv8f16 = 170,   // n x  8 x f16
	CS_DATA_TYPE_nxv16f16 = 171,  // n x 16 x f16
	CS_DATA_TYPE_nxv32f16 = 172,  // n x 32 x f16

	CS_DATA_TYPE_nxv1bf16 = 173,  // n x  1 x bf16
	CS_DATA_TYPE_nxv2bf16 = 174,  // n x  2 x bf16
	CS_DATA_TYPE_nxv4bf16 = 175,  // n x  4 x bf16
	CS_DATA_TYPE_nxv8bf16 = 176,  // n x  8 x bf16
	CS_DATA_TYPE_nxv16bf16 = 177, // n x 16 x bf16
	CS_DATA_TYPE_nxv32bf16 = 178, // n x 32 x bf16

	CS_DATA_TYPE_nxv1f32 = 179,   // n x  1 x f32
	CS_DATA_TYPE_nxv2f32 = 180,   // n x  2 x f32
	CS_DATA_TYPE_nxv4f32 = 181,   // n x  4 x f32
	CS_DATA_TYPE_nxv8f32 = 182,   // n x  8 x f32
	CS_DATA_TYPE_nxv16f32 = 183,  // n x 16 x f32

	CS_DATA_TYPE_nxv1f64 = 184,   // n x  1 x f64
	CS_DATA_TYPE_nxv2f64 = 185,   // n x  2 x f64
	CS_DATA_TYPE_nxv4f64 = 186,   // n x  4 x f64
	CS_DATA_TYPE_nxv8f64 = 187,   // n x  8 x f64

	CS_DATA_TYPE_FIRST_FP_SCALABLE_VECTOR_VALUETYPE = CS_DATA_TYPE_nxv1f16,
	CS_DATA_TYPE_LAST_FP_SCALABLE_VECTOR_VALUETYPE = CS_DATA_TYPE_nxv8f64,

	CS_DATA_TYPE_FIRST_SCALABLE_VECTOR_VALUETYPE = CS_DATA_TYPE_nxv1i1,
	CS_DATA_TYPE_LAST_SCALABLE_VECTOR_VALUETYPE = CS_DATA_TYPE_nxv8f64,

	CS_DATA_TYPE_FIRST_VECTOR_VALUETYPE = CS_DATA_TYPE_v1i1,
	CS_DATA_TYPE_LAST_VECTOR_VALUETYPE = CS_DATA_TYPE_nxv8f64,

	CS_DATA_TYPE_x86mmx = 188, // This is an X86 MMX value

	CS_DATA_TYPE_Glue =
		189, // This glues nodes together during pre-RA sched

	CS_DATA_TYPE_isVoid = 190,  // This has no value

	CS_DATA_TYPE_Untyped = 191, // This value takes a register, but has
	// unspecified type.  The register class
	// will be determined by the opcode.

	CS_DATA_TYPE_funcref = 192,   // WebAssembly's funcref type
	CS_DATA_TYPE_externref = 193, // WebAssembly's externref type
	CS_DATA_TYPE_x86amx = 194,    // This is an X86 AMX value
	CS_DATA_TYPE_i64x8 = 195,     // 8 Consecutive GPRs (AArch64)

	CS_DATA_TYPE_FIRST_VALUETYPE =
		1,		    // This is always the beginning of the list.
	CS_DATA_TYPE_LAST_VALUETYPE =
		CS_DATA_TYPE_i64x8, // This always remains at the end of the list.
	CS_DATA_TYPE_VALUETYPE_SIZE = CS_DATA_TYPE_LAST_VALUETYPE + 1,

	// This is the current maximum for LAST_VALUETYPE.
	// MVT::MAX_ALLOWED_VALUETYPE is used for asserts and to size bit vectors
	// This value must be a multiple of 32.
	CS_DATA_TYPE_MAX_ALLOWED_VALUETYPE = 224,

	// A value of type llvm::TokenTy
	CS_DATA_TYPE_token = 248,

	// This is MDNode or MDString.
	CS_DATA_TYPE_Metadata = 249,

	// An int value the size of the pointer of the current
	// target to any address space. This must only be used internal to
	// tblgen. Other than for overloading, we treat iPTRAny the same as iPTR.
	CS_DATA_TYPE_iPTRAny = 250,

	// A vector with any length and element size. This is used
	// for intrinsics that have overloadings based on vector types.
	// This is only for tblgen's consumption!
	CS_DATA_TYPE_vAny = 251,

	// Any floating-point or vector floating-point value. This is used
	// for intrinsics that have overloadings based on floating-point types.
	// This is only for tblgen's consumption!
	CS_DATA_TYPE_fAny = 252,

	// An integer or vector integer value of any bit width. This is
	// used for intrinsics that have overloadings based on integer bit widths.
	// This is only for tblgen's consumption!
	CS_DATA_TYPE_iAny = 253,

	// An int value the size of the pointer of the current
	// target.  This should only be used internal to tblgen!
	CS_DATA_TYPE_iPTR = 254,

	// Last element in enum.
	CS_DATA_TYPE_LAST = 255
} cs_data_type;

/// Return true if this is a valid simple valuetype.
inline bool isValid(cs_data_type SimpleTy)
{
	return (SimpleTy >= CS_DATA_TYPE_FIRST_VALUETYPE &&
		SimpleTy <= CS_DATA_TYPE_LAST_VALUETYPE);
}

/// Return true if this is a FP or a vector FP type.
inline bool isFloatingPoint(cs_data_type SimpleTy)
{
	return ((SimpleTy >= CS_DATA_TYPE_FIRST_FP_VALUETYPE &&
		 SimpleTy <= CS_DATA_TYPE_LAST_FP_VALUETYPE) ||
		(SimpleTy >= CS_DATA_TYPE_FIRST_FP_FIXEDLEN_VECTOR_VALUETYPE &&
		 SimpleTy <= CS_DATA_TYPE_LAST_FP_FIXEDLEN_VECTOR_VALUETYPE) ||
		(SimpleTy >= CS_DATA_TYPE_FIRST_FP_SCALABLE_VECTOR_VALUETYPE &&
		 SimpleTy <= CS_DATA_TYPE_LAST_FP_SCALABLE_VECTOR_VALUETYPE));
}

/// Return true if this is an integer or a vector integer type.
inline bool isInteger(cs_data_type SimpleTy)
{
	return ((SimpleTy >= CS_DATA_TYPE_FIRST_INTEGER_VALUETYPE &&
		 SimpleTy <= CS_DATA_TYPE_LAST_INTEGER_VALUETYPE) ||
		(SimpleTy >=
			 CS_DATA_TYPE_FIRST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE &&
		 SimpleTy <=
			 CS_DATA_TYPE_LAST_INTEGER_FIXEDLEN_VECTOR_VALUETYPE) ||
		(SimpleTy >=
			 CS_DATA_TYPE_FIRST_INTEGER_SCALABLE_VECTOR_VALUETYPE &&
		 SimpleTy <=
			 CS_DATA_TYPE_LAST_INTEGER_SCALABLE_VECTOR_VALUETYPE));
}

/// Return true if this is an integer, not including vectors.
inline bool isScalarInteger(cs_data_type SimpleTy)
{
	return (SimpleTy >= CS_DATA_TYPE_FIRST_INTEGER_VALUETYPE &&
		SimpleTy <= CS_DATA_TYPE_LAST_INTEGER_VALUETYPE);
}

/// Return true if this is a vector value type.
inline bool isVector(cs_data_type SimpleTy)
{
	return (SimpleTy >= CS_DATA_TYPE_FIRST_VECTOR_VALUETYPE &&
		SimpleTy <= CS_DATA_TYPE_LAST_VECTOR_VALUETYPE);
}

/// Return true if this is a vector value type where the
/// runtime length is machine dependent
inline bool isScalableVector(cs_data_type SimpleTy)
{
	return (SimpleTy >= CS_DATA_TYPE_FIRST_SCALABLE_VECTOR_VALUETYPE &&
		SimpleTy <= CS_DATA_TYPE_LAST_SCALABLE_VECTOR_VALUETYPE);
}

inline bool isFixedLengthVector(cs_data_type SimpleTy)
{
	return (SimpleTy >= CS_DATA_TYPE_FIRST_FIXEDLEN_VECTOR_VALUETYPE &&
		SimpleTy <= CS_DATA_TYPE_LAST_FIXEDLEN_VECTOR_VALUETYPE);
}

/// Return true if this is a 16-bit vector type.
inline bool is16BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v2i8 ||
		SimpleTy == CS_DATA_TYPE_v1i16 ||
		SimpleTy == CS_DATA_TYPE_v16i1 ||
		SimpleTy == CS_DATA_TYPE_v1f16);
}

/// Return true if this is a 32-bit vector type.
inline bool is32BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v32i1 ||
		SimpleTy == CS_DATA_TYPE_v4i8 ||
		SimpleTy == CS_DATA_TYPE_v2i16 ||
		SimpleTy == CS_DATA_TYPE_v1i32 ||
		SimpleTy == CS_DATA_TYPE_v2f16 ||
		SimpleTy == CS_DATA_TYPE_v2bf16 ||
		SimpleTy == CS_DATA_TYPE_v1f32);
}

/// Return true if this is a 64-bit vector type.
inline bool is64BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v64i1 ||
		SimpleTy == CS_DATA_TYPE_v8i8 ||
		SimpleTy == CS_DATA_TYPE_v4i16 ||
		SimpleTy == CS_DATA_TYPE_v2i32 ||
		SimpleTy == CS_DATA_TYPE_v1i64 ||
		SimpleTy == CS_DATA_TYPE_v4f16 ||
		SimpleTy == CS_DATA_TYPE_v4bf16 ||
		SimpleTy == CS_DATA_TYPE_v2f32 ||
		SimpleTy == CS_DATA_TYPE_v1f64);
}

/// Return true if this is a 128-bit vector type.
inline bool is128BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v128i1 ||
		SimpleTy == CS_DATA_TYPE_v16i8 ||
		SimpleTy == CS_DATA_TYPE_v8i16 ||
		SimpleTy == CS_DATA_TYPE_v4i32 ||
		SimpleTy == CS_DATA_TYPE_v2i64 ||
		SimpleTy == CS_DATA_TYPE_v1i128 ||
		SimpleTy == CS_DATA_TYPE_v8f16 ||
		SimpleTy == CS_DATA_TYPE_v8bf16 ||
		SimpleTy == CS_DATA_TYPE_v4f32 ||
		SimpleTy == CS_DATA_TYPE_v2f64);
}

/// Return true if this is a 256-bit vector type.
inline bool is256BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v16f16 ||
		SimpleTy == CS_DATA_TYPE_v16bf16 ||
		SimpleTy == CS_DATA_TYPE_v8f32 ||
		SimpleTy == CS_DATA_TYPE_v4f64 ||
		SimpleTy == CS_DATA_TYPE_v32i8 ||
		SimpleTy == CS_DATA_TYPE_v16i16 ||
		SimpleTy == CS_DATA_TYPE_v8i32 ||
		SimpleTy == CS_DATA_TYPE_v4i64 ||
		SimpleTy == CS_DATA_TYPE_v256i1 ||
		SimpleTy == CS_DATA_TYPE_v128i2 ||
		SimpleTy == CS_DATA_TYPE_v64i4);
}

/// Return true if this is a 512-bit vector type.
inline bool is512BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v32f16 ||
		SimpleTy == CS_DATA_TYPE_v32bf16 ||
		SimpleTy == CS_DATA_TYPE_v16f32 ||
		SimpleTy == CS_DATA_TYPE_v8f64 ||
		SimpleTy == CS_DATA_TYPE_v512i1 ||
		SimpleTy == CS_DATA_TYPE_v256i2 ||
		SimpleTy == CS_DATA_TYPE_v128i4 ||
		SimpleTy == CS_DATA_TYPE_v64i8 ||
		SimpleTy == CS_DATA_TYPE_v32i16 ||
		SimpleTy == CS_DATA_TYPE_v16i32 ||
		SimpleTy == CS_DATA_TYPE_v8i64);
}

/// Return true if this is a 1024-bit vector type.
inline bool is1024BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v1024i1 ||
		SimpleTy == CS_DATA_TYPE_v128i8 ||
		SimpleTy == CS_DATA_TYPE_v64i16 ||
		SimpleTy == CS_DATA_TYPE_v32i32 ||
		SimpleTy == CS_DATA_TYPE_v16i64 ||
		SimpleTy == CS_DATA_TYPE_v64f16 ||
		SimpleTy == CS_DATA_TYPE_v32f32 ||
		SimpleTy == CS_DATA_TYPE_v16f64 ||
		SimpleTy == CS_DATA_TYPE_v64bf16);
}

/// Return true if this is a 2048-bit vector type.
inline bool is2048BitVector(cs_data_type SimpleTy)
{
	return (SimpleTy == CS_DATA_TYPE_v256i8 ||
		SimpleTy == CS_DATA_TYPE_v128i16 ||
		SimpleTy == CS_DATA_TYPE_v64i32 ||
		SimpleTy == CS_DATA_TYPE_v32i64 ||
		SimpleTy == CS_DATA_TYPE_v128f16 ||
		SimpleTy == CS_DATA_TYPE_v64f32 ||
		SimpleTy == CS_DATA_TYPE_v32f64 ||
		SimpleTy == CS_DATA_TYPE_v128bf16 ||
		SimpleTy == CS_DATA_TYPE_v2048i1);
}

inline cs_data_type getVectorElementType(cs_data_type SimpleTy)
{
	switch (SimpleTy) {
	default:
		assert(0 && "Not a vector MVT!");
	case CS_DATA_TYPE_v1i1:
	case CS_DATA_TYPE_v2i1:
	case CS_DATA_TYPE_v4i1:
	case CS_DATA_TYPE_v8i1:
	case CS_DATA_TYPE_v16i1:
	case CS_DATA_TYPE_v32i1:
	case CS_DATA_TYPE_v64i1:
	case CS_DATA_TYPE_v128i1:
	case CS_DATA_TYPE_v256i1:
	case CS_DATA_TYPE_v512i1:
	case CS_DATA_TYPE_v1024i1:
	case CS_DATA_TYPE_v2048i1:
	case CS_DATA_TYPE_nxv1i1:
	case CS_DATA_TYPE_nxv2i1:
	case CS_DATA_TYPE_nxv4i1:
	case CS_DATA_TYPE_nxv8i1:
	case CS_DATA_TYPE_nxv16i1:
	case CS_DATA_TYPE_nxv32i1:
	case CS_DATA_TYPE_nxv64i1:
		return CS_DATA_TYPE_i1;
	case CS_DATA_TYPE_v128i2:
	case CS_DATA_TYPE_v256i2:
		return CS_DATA_TYPE_i2;
	case CS_DATA_TYPE_v64i4:
	case CS_DATA_TYPE_v128i4:
		return CS_DATA_TYPE_i4;
	case CS_DATA_TYPE_v1i8:
	case CS_DATA_TYPE_v2i8:
	case CS_DATA_TYPE_v4i8:
	case CS_DATA_TYPE_v8i8:
	case CS_DATA_TYPE_v16i8:
	case CS_DATA_TYPE_v32i8:
	case CS_DATA_TYPE_v64i8:
	case CS_DATA_TYPE_v128i8:
	case CS_DATA_TYPE_v256i8:
	case CS_DATA_TYPE_v512i8:
	case CS_DATA_TYPE_v1024i8:
	case CS_DATA_TYPE_nxv1i8:
	case CS_DATA_TYPE_nxv2i8:
	case CS_DATA_TYPE_nxv4i8:
	case CS_DATA_TYPE_nxv8i8:
	case CS_DATA_TYPE_nxv16i8:
	case CS_DATA_TYPE_nxv32i8:
	case CS_DATA_TYPE_nxv64i8:
		return CS_DATA_TYPE_i8;
	case CS_DATA_TYPE_v1i16:
	case CS_DATA_TYPE_v2i16:
	case CS_DATA_TYPE_v3i16:
	case CS_DATA_TYPE_v4i16:
	case CS_DATA_TYPE_v8i16:
	case CS_DATA_TYPE_v16i16:
	case CS_DATA_TYPE_v32i16:
	case CS_DATA_TYPE_v64i16:
	case CS_DATA_TYPE_v128i16:
	case CS_DATA_TYPE_v256i16:
	case CS_DATA_TYPE_v512i16:
	case CS_DATA_TYPE_nxv1i16:
	case CS_DATA_TYPE_nxv2i16:
	case CS_DATA_TYPE_nxv4i16:
	case CS_DATA_TYPE_nxv8i16:
	case CS_DATA_TYPE_nxv16i16:
	case CS_DATA_TYPE_nxv32i16:
		return CS_DATA_TYPE_i16;
	case CS_DATA_TYPE_v1i32:
	case CS_DATA_TYPE_v2i32:
	case CS_DATA_TYPE_v3i32:
	case CS_DATA_TYPE_v4i32:
	case CS_DATA_TYPE_v5i32:
	case CS_DATA_TYPE_v6i32:
	case CS_DATA_TYPE_v7i32:
	case CS_DATA_TYPE_v8i32:
	case CS_DATA_TYPE_v9i32:
	case CS_DATA_TYPE_v10i32:
	case CS_DATA_TYPE_v11i32:
	case CS_DATA_TYPE_v12i32:
	case CS_DATA_TYPE_v16i32:
	case CS_DATA_TYPE_v32i32:
	case CS_DATA_TYPE_v64i32:
	case CS_DATA_TYPE_v128i32:
	case CS_DATA_TYPE_v256i32:
	case CS_DATA_TYPE_v512i32:
	case CS_DATA_TYPE_v1024i32:
	case CS_DATA_TYPE_v2048i32:
	case CS_DATA_TYPE_nxv1i32:
	case CS_DATA_TYPE_nxv2i32:
	case CS_DATA_TYPE_nxv4i32:
	case CS_DATA_TYPE_nxv8i32:
	case CS_DATA_TYPE_nxv16i32:
	case CS_DATA_TYPE_nxv32i32:
		return CS_DATA_TYPE_i32;
	case CS_DATA_TYPE_v1i64:
	case CS_DATA_TYPE_v2i64:
	case CS_DATA_TYPE_v3i64:
	case CS_DATA_TYPE_v4i64:
	case CS_DATA_TYPE_v8i64:
	case CS_DATA_TYPE_v16i64:
	case CS_DATA_TYPE_v32i64:
	case CS_DATA_TYPE_v64i64:
	case CS_DATA_TYPE_v128i64:
	case CS_DATA_TYPE_v256i64:
	case CS_DATA_TYPE_nxv1i64:
	case CS_DATA_TYPE_nxv2i64:
	case CS_DATA_TYPE_nxv4i64:
	case CS_DATA_TYPE_nxv8i64:
	case CS_DATA_TYPE_nxv16i64:
	case CS_DATA_TYPE_nxv32i64:
		return CS_DATA_TYPE_i64;
	case CS_DATA_TYPE_v1i128:
		return CS_DATA_TYPE_i128;
	case CS_DATA_TYPE_v1f16:
	case CS_DATA_TYPE_v2f16:
	case CS_DATA_TYPE_v3f16:
	case CS_DATA_TYPE_v4f16:
	case CS_DATA_TYPE_v8f16:
	case CS_DATA_TYPE_v16f16:
	case CS_DATA_TYPE_v32f16:
	case CS_DATA_TYPE_v64f16:
	case CS_DATA_TYPE_v128f16:
	case CS_DATA_TYPE_v256f16:
	case CS_DATA_TYPE_v512f16:
	case CS_DATA_TYPE_nxv1f16:
	case CS_DATA_TYPE_nxv2f16:
	case CS_DATA_TYPE_nxv4f16:
	case CS_DATA_TYPE_nxv8f16:
	case CS_DATA_TYPE_nxv16f16:
	case CS_DATA_TYPE_nxv32f16:
		return CS_DATA_TYPE_f16;
	case CS_DATA_TYPE_v2bf16:
	case CS_DATA_TYPE_v3bf16:
	case CS_DATA_TYPE_v4bf16:
	case CS_DATA_TYPE_v8bf16:
	case CS_DATA_TYPE_v16bf16:
	case CS_DATA_TYPE_v32bf16:
	case CS_DATA_TYPE_v64bf16:
	case CS_DATA_TYPE_v128bf16:
	case CS_DATA_TYPE_nxv1bf16:
	case CS_DATA_TYPE_nxv2bf16:
	case CS_DATA_TYPE_nxv4bf16:
	case CS_DATA_TYPE_nxv8bf16:
	case CS_DATA_TYPE_nxv16bf16:
	case CS_DATA_TYPE_nxv32bf16:
		return CS_DATA_TYPE_bf16;
	case CS_DATA_TYPE_v1f32:
	case CS_DATA_TYPE_v2f32:
	case CS_DATA_TYPE_v3f32:
	case CS_DATA_TYPE_v4f32:
	case CS_DATA_TYPE_v5f32:
	case CS_DATA_TYPE_v6f32:
	case CS_DATA_TYPE_v7f32:
	case CS_DATA_TYPE_v8f32:
	case CS_DATA_TYPE_v9f32:
	case CS_DATA_TYPE_v10f32:
	case CS_DATA_TYPE_v11f32:
	case CS_DATA_TYPE_v12f32:
	case CS_DATA_TYPE_v16f32:
	case CS_DATA_TYPE_v32f32:
	case CS_DATA_TYPE_v64f32:
	case CS_DATA_TYPE_v128f32:
	case CS_DATA_TYPE_v256f32:
	case CS_DATA_TYPE_v512f32:
	case CS_DATA_TYPE_v1024f32:
	case CS_DATA_TYPE_v2048f32:
	case CS_DATA_TYPE_nxv1f32:
	case CS_DATA_TYPE_nxv2f32:
	case CS_DATA_TYPE_nxv4f32:
	case CS_DATA_TYPE_nxv8f32:
	case CS_DATA_TYPE_nxv16f32:
		return CS_DATA_TYPE_f32;
	case CS_DATA_TYPE_v1f64:
	case CS_DATA_TYPE_v2f64:
	case CS_DATA_TYPE_v3f64:
	case CS_DATA_TYPE_v4f64:
	case CS_DATA_TYPE_v8f64:
	case CS_DATA_TYPE_v16f64:
	case CS_DATA_TYPE_v32f64:
	case CS_DATA_TYPE_v64f64:
	case CS_DATA_TYPE_v128f64:
	case CS_DATA_TYPE_v256f64:
	case CS_DATA_TYPE_nxv1f64:
	case CS_DATA_TYPE_nxv2f64:
	case CS_DATA_TYPE_nxv4f64:
	case CS_DATA_TYPE_nxv8f64:
		return CS_DATA_TYPE_f64;
	}
}

/// Given a vector type, return the minimum number of elements it contains.
inline unsigned getVectorMinNumElements(cs_data_type SimpleTy)
{
	switch (SimpleTy) {
	default:
		assert(0 && "Not a vector MVT!");
	case CS_DATA_TYPE_v2048i1:
	case CS_DATA_TYPE_v2048i32:
	case CS_DATA_TYPE_v2048f32:
		return 2048;
	case CS_DATA_TYPE_v1024i1:
	case CS_DATA_TYPE_v1024i8:
	case CS_DATA_TYPE_v1024i32:
	case CS_DATA_TYPE_v1024f32:
		return 1024;
	case CS_DATA_TYPE_v512i1:
	case CS_DATA_TYPE_v512i8:
	case CS_DATA_TYPE_v512i16:
	case CS_DATA_TYPE_v512i32:
	case CS_DATA_TYPE_v512f16:
	case CS_DATA_TYPE_v512f32:
		return 512;
	case CS_DATA_TYPE_v256i1:
	case CS_DATA_TYPE_v256i2:
	case CS_DATA_TYPE_v256i8:
	case CS_DATA_TYPE_v256i16:
	case CS_DATA_TYPE_v256f16:
	case CS_DATA_TYPE_v256i32:
	case CS_DATA_TYPE_v256i64:
	case CS_DATA_TYPE_v256f32:
	case CS_DATA_TYPE_v256f64:
		return 256;
	case CS_DATA_TYPE_v128i1:
	case CS_DATA_TYPE_v128i2:
	case CS_DATA_TYPE_v128i4:
	case CS_DATA_TYPE_v128i8:
	case CS_DATA_TYPE_v128i16:
	case CS_DATA_TYPE_v128i32:
	case CS_DATA_TYPE_v128i64:
	case CS_DATA_TYPE_v128f16:
	case CS_DATA_TYPE_v128bf16:
	case CS_DATA_TYPE_v128f32:
	case CS_DATA_TYPE_v128f64:
		return 128;
	case CS_DATA_TYPE_v64i1:
	case CS_DATA_TYPE_v64i4:
	case CS_DATA_TYPE_v64i8:
	case CS_DATA_TYPE_v64i16:
	case CS_DATA_TYPE_v64i32:
	case CS_DATA_TYPE_v64i64:
	case CS_DATA_TYPE_v64f16:
	case CS_DATA_TYPE_v64bf16:
	case CS_DATA_TYPE_v64f32:
	case CS_DATA_TYPE_v64f64:
	case CS_DATA_TYPE_nxv64i1:
	case CS_DATA_TYPE_nxv64i8:
		return 64;
	case CS_DATA_TYPE_v32i1:
	case CS_DATA_TYPE_v32i8:
	case CS_DATA_TYPE_v32i16:
	case CS_DATA_TYPE_v32i32:
	case CS_DATA_TYPE_v32i64:
	case CS_DATA_TYPE_v32f16:
	case CS_DATA_TYPE_v32bf16:
	case CS_DATA_TYPE_v32f32:
	case CS_DATA_TYPE_v32f64:
	case CS_DATA_TYPE_nxv32i1:
	case CS_DATA_TYPE_nxv32i8:
	case CS_DATA_TYPE_nxv32i16:
	case CS_DATA_TYPE_nxv32i32:
	case CS_DATA_TYPE_nxv32i64:
	case CS_DATA_TYPE_nxv32f16:
	case CS_DATA_TYPE_nxv32bf16:
		return 32;
	case CS_DATA_TYPE_v16i1:
	case CS_DATA_TYPE_v16i8:
	case CS_DATA_TYPE_v16i16:
	case CS_DATA_TYPE_v16i32:
	case CS_DATA_TYPE_v16i64:
	case CS_DATA_TYPE_v16f16:
	case CS_DATA_TYPE_v16bf16:
	case CS_DATA_TYPE_v16f32:
	case CS_DATA_TYPE_v16f64:
	case CS_DATA_TYPE_nxv16i1:
	case CS_DATA_TYPE_nxv16i8:
	case CS_DATA_TYPE_nxv16i16:
	case CS_DATA_TYPE_nxv16i32:
	case CS_DATA_TYPE_nxv16i64:
	case CS_DATA_TYPE_nxv16f16:
	case CS_DATA_TYPE_nxv16bf16:
	case CS_DATA_TYPE_nxv16f32:
		return 16;
	case CS_DATA_TYPE_v12i32:
	case CS_DATA_TYPE_v12f32:
		return 12;
	case CS_DATA_TYPE_v11i32:
	case CS_DATA_TYPE_v11f32:
		return 11;
	case CS_DATA_TYPE_v10i32:
	case CS_DATA_TYPE_v10f32:
		return 10;
	case CS_DATA_TYPE_v9i32:
	case CS_DATA_TYPE_v9f32:
		return 9;
	case CS_DATA_TYPE_v8i1:
	case CS_DATA_TYPE_v8i8:
	case CS_DATA_TYPE_v8i16:
	case CS_DATA_TYPE_v8i32:
	case CS_DATA_TYPE_v8i64:
	case CS_DATA_TYPE_v8f16:
	case CS_DATA_TYPE_v8bf16:
	case CS_DATA_TYPE_v8f32:
	case CS_DATA_TYPE_v8f64:
	case CS_DATA_TYPE_nxv8i1:
	case CS_DATA_TYPE_nxv8i8:
	case CS_DATA_TYPE_nxv8i16:
	case CS_DATA_TYPE_nxv8i32:
	case CS_DATA_TYPE_nxv8i64:
	case CS_DATA_TYPE_nxv8f16:
	case CS_DATA_TYPE_nxv8bf16:
	case CS_DATA_TYPE_nxv8f32:
	case CS_DATA_TYPE_nxv8f64:
		return 8;
	case CS_DATA_TYPE_v7i32:
	case CS_DATA_TYPE_v7f32:
		return 7;
	case CS_DATA_TYPE_v6i32:
	case CS_DATA_TYPE_v6f32:
		return 6;
	case CS_DATA_TYPE_v5i32:
	case CS_DATA_TYPE_v5f32:
		return 5;
	case CS_DATA_TYPE_v4i1:
	case CS_DATA_TYPE_v4i8:
	case CS_DATA_TYPE_v4i16:
	case CS_DATA_TYPE_v4i32:
	case CS_DATA_TYPE_v4i64:
	case CS_DATA_TYPE_v4f16:
	case CS_DATA_TYPE_v4bf16:
	case CS_DATA_TYPE_v4f32:
	case CS_DATA_TYPE_v4f64:
	case CS_DATA_TYPE_nxv4i1:
	case CS_DATA_TYPE_nxv4i8:
	case CS_DATA_TYPE_nxv4i16:
	case CS_DATA_TYPE_nxv4i32:
	case CS_DATA_TYPE_nxv4i64:
	case CS_DATA_TYPE_nxv4f16:
	case CS_DATA_TYPE_nxv4bf16:
	case CS_DATA_TYPE_nxv4f32:
	case CS_DATA_TYPE_nxv4f64:
		return 4;
	case CS_DATA_TYPE_v3i16:
	case CS_DATA_TYPE_v3i32:
	case CS_DATA_TYPE_v3i64:
	case CS_DATA_TYPE_v3f16:
	case CS_DATA_TYPE_v3bf16:
	case CS_DATA_TYPE_v3f32:
	case CS_DATA_TYPE_v3f64:
		return 3;
	case CS_DATA_TYPE_v2i1:
	case CS_DATA_TYPE_v2i8:
	case CS_DATA_TYPE_v2i16:
	case CS_DATA_TYPE_v2i32:
	case CS_DATA_TYPE_v2i64:
	case CS_DATA_TYPE_v2f16:
	case CS_DATA_TYPE_v2bf16:
	case CS_DATA_TYPE_v2f32:
	case CS_DATA_TYPE_v2f64:
	case CS_DATA_TYPE_nxv2i1:
	case CS_DATA_TYPE_nxv2i8:
	case CS_DATA_TYPE_nxv2i16:
	case CS_DATA_TYPE_nxv2i32:
	case CS_DATA_TYPE_nxv2i64:
	case CS_DATA_TYPE_nxv2f16:
	case CS_DATA_TYPE_nxv2bf16:
	case CS_DATA_TYPE_nxv2f32:
	case CS_DATA_TYPE_nxv2f64:
		return 2;
	case CS_DATA_TYPE_v1i1:
	case CS_DATA_TYPE_v1i8:
	case CS_DATA_TYPE_v1i16:
	case CS_DATA_TYPE_v1i32:
	case CS_DATA_TYPE_v1i64:
	case CS_DATA_TYPE_v1i128:
	case CS_DATA_TYPE_v1f16:
	case CS_DATA_TYPE_v1f32:
	case CS_DATA_TYPE_v1f64:
	case CS_DATA_TYPE_nxv1i1:
	case CS_DATA_TYPE_nxv1i8:
	case CS_DATA_TYPE_nxv1i16:
	case CS_DATA_TYPE_nxv1i32:
	case CS_DATA_TYPE_nxv1i64:
	case CS_DATA_TYPE_nxv1f16:
	case CS_DATA_TYPE_nxv1bf16:
	case CS_DATA_TYPE_nxv1f32:
	case CS_DATA_TYPE_nxv1f64:
		return 1;
	}
}

#endif // CS_SIMPLE_TYPES_H
