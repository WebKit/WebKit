/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { Colors } from '../../common/util/colors.js';
import { assert } from '../../common/util/util.js';
import { clamp } from './math.js';

/**
 * Encodes a JS `number` into a "normalized" (unorm/snorm) integer representation with `bits` bits.
 * Input must be between -1 and 1 if signed, or 0 and 1 if unsigned.
 */
export function floatAsNormalizedInteger(float, bits, signed) {
  if (signed) {
    assert(float >= -1 && float <= 1, () => `${float} out of bounds of snorm`);
    const max = Math.pow(2, bits - 1) - 1;
    return Math.round(float * max);
  } else {
    assert(float >= 0 && float <= 1, () => `${float} out of bounds of unorm`);
    const max = Math.pow(2, bits) - 1;
    return Math.round(float * max);
  }
}

/**
 * Decodes a JS `number` from a "normalized" (unorm/snorm) integer representation with `bits` bits.
 * Input must be an integer in the range of the specified unorm/snorm type.
 */
export function normalizedIntegerAsFloat(integer, bits, signed) {
  assert(Number.isInteger(integer));
  if (signed) {
    const max = Math.pow(2, bits - 1) - 1;
    assert(integer >= -max - 1 && integer <= max);
    if (integer === -max - 1) {
      integer = -max;
    }
    return integer / max;
  } else {
    const max = Math.pow(2, bits) - 1;
    assert(integer >= 0 && integer <= max);
    return integer / max;
  }
}

/**
 * Encodes a JS `number` into an IEEE754 floating point number with the specified number of
 * sign, exponent, mantissa bits, and exponent bias.
 * Returns the result as an integer-valued JS `number`.
 *
 * Does not handle clamping, overflow, or denormal inputs.
 * On underflow (result is subnormal), rounds to (signed) zero.
 *
 * MAINTENANCE_TODO: Replace usages of this with numberToFloatBits.
 */
export function float32ToFloatBits(n, signBits, exponentBits, mantissaBits, bias) {
  assert(exponentBits <= 8);
  assert(mantissaBits <= 23);
  assert(Number.isFinite(n));

  if (n === 0) {
    return 0;
  }

  if (signBits === 0) {
    assert(n >= 0);
  }

  const buf = new DataView(new ArrayBuffer(Float32Array.BYTES_PER_ELEMENT));
  buf.setFloat32(0, n, true);
  const bits = buf.getUint32(0, true);
  // bits (32): seeeeeeeefffffffffffffffffffffff

  const mantissaBitsToDiscard = 23 - mantissaBits;

  // 0 or 1
  const sign = (bits >> 31) & signBits;

  // >> to remove mantissa, & to remove sign, - 127 to remove bias.
  const exp = ((bits >> 23) & 0xff) - 127;

  // Convert to the new biased exponent.
  const newBiasedExp = bias + exp;
  assert(newBiasedExp < 1 << exponentBits, () => `input number ${n} overflows target type`);

  if (newBiasedExp <= 0) {
    // Result is subnormal or zero. Round to (signed) zero.
    return sign << (exponentBits + mantissaBits);
  } else {
    // Mask only the mantissa, and discard the lower bits.
    const newMantissa = (bits & 0x7fffff) >> mantissaBitsToDiscard;
    return (sign << (exponentBits + mantissaBits)) | (newBiasedExp << mantissaBits) | newMantissa;
  }
}

/**
 * Encodes a JS `number` into an IEEE754 16 bit floating point number.
 * Returns the result as an integer-valued JS `number`.
 *
 * Does not handle clamping, overflow, or denormal inputs.
 * On underflow (result is subnormal), rounds to (signed) zero.
 */
export function float32ToFloat16Bits(n) {
  return float32ToFloatBits(n, 1, 5, 10, 15);
}

/**
 * Decodes an IEEE754 16 bit floating point number into a JS `number` and returns.
 */
export function float16BitsToFloat32(float16Bits) {
  return floatBitsToNumber(float16Bits, kFloat16Format);
}

/** FloatFormat defining IEEE754 32-bit float. */
export const kFloat32Format = { signed: 1, exponentBits: 8, mantissaBits: 23, bias: 127 };
/** FloatFormat defining IEEE754 16-bit float. */
export const kFloat16Format = { signed: 1, exponentBits: 5, mantissaBits: 10, bias: 15 };

const workingData = new ArrayBuffer(4);
const workingDataU32 = new Uint32Array(workingData);
const workingDataF32 = new Float32Array(workingData);
/** Bitcast u32 (represented as integer Number) to f32 (represented as floating-point Number). */
export function float32BitsToNumber(bits) {
  workingDataU32[0] = bits;
  return workingDataF32[0];
}
/** Bitcast f32 (represented as floating-point Number) to u32 (represented as integer Number). */
export function numberToFloat32Bits(number) {
  workingDataF32[0] = number;
  return workingDataU32[0];
}

/**
 * Decodes an IEEE754 float with the supplied format specification into a JS number.
 *
 * The format MUST be no larger than a 32-bit float.
 */
export function floatBitsToNumber(bits, fmt) {
  // Pad the provided bits out to f32, then convert to a `number` with the wrong bias.
  // E.g. for f16 to f32:
  // - f16: S    EEEEE MMMMMMMMMM
  //        ^ 000^^^^^ ^^^^^^^^^^0000000000000
  // - f32: S eeeEEEEE MMMMMMMMMMmmmmmmmmmmmmm

  const kNonSignBits = fmt.exponentBits + fmt.mantissaBits;
  const kNonSignBitsMask = (1 << kNonSignBits) - 1;
  const expAndMantBits = bits & kNonSignBitsMask;
  let f32BitsWithWrongBias = expAndMantBits << (kFloat32Format.mantissaBits - fmt.mantissaBits);
  f32BitsWithWrongBias |= (bits << (31 - kNonSignBits)) & 0x8000_0000;
  const numberWithWrongBias = float32BitsToNumber(f32BitsWithWrongBias);
  return numberWithWrongBias * 2 ** (kFloat32Format.bias - fmt.bias);
}

/**
 * Encodes a JS `number` into an IEEE754 floating point number with the specified format.
 * Returns the result as an integer-valued JS `number`.
 *
 * Does not handle clamping, overflow, or denormal inputs.
 * On underflow (result is subnormal), rounds to (signed) zero.
 */
export function numberToFloatBits(number, fmt) {
  return float32ToFloatBits(number, fmt.signed, fmt.exponentBits, fmt.mantissaBits, fmt.bias);
}

/**
 * Given a floating point number (as an integer representing its bits), computes how many ULPs it is
 * from zero.
 *
 * Subnormal numbers are skipped, so that 0 is one ULP from the minimum normal number.
 * Subnormal values are flushed to 0.
 * Positive and negative 0 are both considered to be 0 ULPs from 0.
 */
export function floatBitsToNormalULPFromZero(bits, fmt) {
  const mask_sign = fmt.signed << (fmt.exponentBits + fmt.mantissaBits);
  const mask_expt = ((1 << fmt.exponentBits) - 1) << fmt.mantissaBits;
  const mask_mant = (1 << fmt.mantissaBits) - 1;
  const mask_rest = mask_expt | mask_mant;

  assert(fmt.exponentBits + fmt.mantissaBits <= 31);

  const sign = bits & mask_sign ? -1 : 1;
  const rest = bits & mask_rest;
  const subnormal_or_zero = (bits & mask_expt) === 0;
  const infinity_or_nan = (bits & mask_expt) === mask_expt;
  assert(!infinity_or_nan, 'no ulp representation for infinity/nan');

  // The first normal number is mask_mant+1, so subtract mask_mant to make min_normal - zero = 1ULP.
  const abs_ulp_from_zero = subnormal_or_zero ? 0 : rest - mask_mant;
  return sign * abs_ulp_from_zero;
}

/**
 * Encodes three JS `number` values into RGB9E5, returned as an integer-valued JS `number`.
 *
 * RGB9E5 represents three partial-precision floating-point numbers encoded into a single 32-bit
 * value all sharing the same 5-bit exponent.
 * There is no sign bit, and there is a shared 5-bit biased (15) exponent and a 9-bit
 * mantissa for each channel. The mantissa does NOT have an implicit leading "1.",
 * and instead has an implicit leading "0.".
 */
export function packRGB9E5UFloat(r, g, b) {
  for (const v of [r, g, b]) {
    assert(v >= 0 && v < Math.pow(2, 16));
  }

  const buf = new DataView(new ArrayBuffer(Float32Array.BYTES_PER_ELEMENT));
  const extractMantissaAndExponent = n => {
    const mantissaBits = 9;
    buf.setFloat32(0, n, true);
    const bits = buf.getUint32(0, true);
    // >> to remove mantissa, & to remove sign
    let biasedExponent = (bits >> 23) & 0xff;
    const mantissaBitsToDiscard = 23 - mantissaBits;
    let mantissa = (bits & 0x7fffff) >> mantissaBitsToDiscard;

    // RGB9E5UFloat has an implicit leading 0. instead of a leading 1.,
    // so we need to move the 1. into the mantissa and bump the exponent.
    // For float32 encoding, the leading 1 is only present if the biased
    // exponent is non-zero.
    if (biasedExponent !== 0) {
      mantissa = (mantissa >> 1) | 0b100000000;
      biasedExponent += 1;
    }
    return { biasedExponent, mantissa };
  };

  const { biasedExponent: rExp, mantissa: rOrigMantissa } = extractMantissaAndExponent(r);
  const { biasedExponent: gExp, mantissa: gOrigMantissa } = extractMantissaAndExponent(g);
  const { biasedExponent: bExp, mantissa: bOrigMantissa } = extractMantissaAndExponent(b);

  // Use the largest exponent, and shift the mantissa accordingly
  const exp = Math.max(rExp, gExp, bExp);
  const rMantissa = rOrigMantissa >> (exp - rExp);
  const gMantissa = gOrigMantissa >> (exp - gExp);
  const bMantissa = bOrigMantissa >> (exp - bExp);

  const bias = 15;
  const biasedExp = exp === 0 ? 0 : exp - 127 + bias;
  assert(biasedExp >= 0 && biasedExp <= 31);
  return rMantissa | (gMantissa << 9) | (bMantissa << 18) | (biasedExp << 27);
}

/**
 * Asserts that a number is within the representable (inclusive) of the integer type with the
 * specified number of bits and signedness.
 *
 * MAINTENANCE_TODO: Assert isInteger? Then this function "asserts that a number is representable"
 * by the type.
 */
export function assertInIntegerRange(n, bits, signed) {
  if (signed) {
    const min = -Math.pow(2, bits - 1);
    const max = Math.pow(2, bits - 1) - 1;
    assert(n >= min && n <= max);
  } else {
    const max = Math.pow(2, bits) - 1;
    assert(n >= 0 && n <= max);
  }
}

/**
 * Converts a linear value into a "gamma"-encoded value using the sRGB-clamped transfer function.
 */
export function gammaCompress(n) {
  n = n <= 0.0031308 ? (323 * n) / 25 : (211 * Math.pow(n, 5 / 12) - 11) / 200;
  return clamp(n, { min: 0, max: 1 });
}

/**
 * Converts a "gamma"-encoded value into a linear value using the sRGB-clamped transfer function.
 */
export function gammaDecompress(n) {
  n = n <= 0.04045 ? (n * 25) / 323 : Math.pow((200 * n + 11) / 211, 12 / 5);
  return clamp(n, { min: 0, max: 1 });
}

/** Converts a 32-bit float value to a 32-bit unsigned integer value */
export function float32ToUint32(f32) {
  const f32Arr = new Float32Array(1);
  f32Arr[0] = f32;
  const u32Arr = new Uint32Array(f32Arr.buffer);
  return u32Arr[0];
}

/** Converts a 32-bit unsigned integer value to a 32-bit float value */
export function uint32ToFloat32(u32) {
  const u32Arr = new Uint32Array(1);
  u32Arr[0] = u32;
  const f32Arr = new Float32Array(u32Arr.buffer);
  return f32Arr[0];
}

/** Converts a 32-bit float value to a 32-bit signed integer value */
export function float32ToInt32(f32) {
  const f32Arr = new Float32Array(1);
  f32Arr[0] = f32;
  const i32Arr = new Int32Array(f32Arr.buffer);
  return i32Arr[0];
}

/** Converts a 32-bit unsigned integer value to a 32-bit signed integer value */
export function uint32ToInt32(u32) {
  const u32Arr = new Uint32Array(1);
  u32Arr[0] = u32;
  const i32Arr = new Int32Array(u32Arr.buffer);
  return i32Arr[0];
}

/** A type of number representable by Scalar. */

/** ScalarType describes the type of WGSL Scalar. */
export class ScalarType {
  // The named type
  // In bytes
  // reads a scalar from a buffer

  constructor(kind, size, read) {
    this.kind = kind;
    this.size = size;
    this.read = read;
  }

  toString() {
    return this.kind;
  }
}

/** ScalarType describes the type of WGSL Vector. */
export class VectorType {
  // Number of elements in the vector
  // Element type

  constructor(width, elementType) {
    this.width = width;
    this.elementType = elementType;
  }

  /**
   * @returns a vector constructed from the values read from the buffer at the
   * given byte offset
   */
  read(buf, offset) {
    const elements = [];
    for (let i = 0; i < this.width; i++) {
      elements[i] = this.elementType.read(buf, offset);
      offset += this.elementType.size;
    }
    return new Vector(elements);
  }

  toString() {
    return `vec${this.width}<${this.elementType}>`;
  }
}

// Maps a string representation of a vector type to vector type.
const vectorTypes = new Map();

export function TypeVec(width, elementType) {
  const key = `${elementType.toString()} ${width}}`;
  let ty = vectorTypes.get(key);
  if (ty !== undefined) {
    return ty;
  }
  ty = new VectorType(width, elementType);
  vectorTypes.set(key, ty);
  return ty;
}

/** Type is a ScalarType or VectorType. */

export const TypeI32 = new ScalarType('i32', 4, (buf, offset) =>
  i32(new Int32Array(buf.buffer, offset)[0])
);

export const TypeU32 = new ScalarType('u32', 4, (buf, offset) =>
  u32(new Uint32Array(buf.buffer, offset)[0])
);

export const TypeF32 = new ScalarType('f32', 4, (buf, offset) =>
  f32(new Float32Array(buf.buffer, offset)[0])
);

export const TypeI16 = new ScalarType('i16', 2, (buf, offset) =>
  i16(new Int16Array(buf.buffer, offset)[0])
);

export const TypeU16 = new ScalarType('u16', 2, (buf, offset) =>
  u16(new Uint16Array(buf.buffer, offset)[0])
);

export const TypeF16 = new ScalarType('f16', 2, (buf, offset) =>
  f16Bits(new Uint16Array(buf.buffer, offset)[0])
);

export const TypeI8 = new ScalarType('i8', 1, (buf, offset) =>
  i8(new Int8Array(buf.buffer, offset)[0])
);

export const TypeU8 = new ScalarType('u8', 1, (buf, offset) =>
  u8(new Uint8Array(buf.buffer, offset)[0])
);

export const TypeBool = new ScalarType('bool', 4, (buf, offset) =>
  bool(new Uint32Array(buf.buffer, offset)[0] !== 0)
);

/** @returns the ScalarType from the ScalarKind */
export function scalarType(kind) {
  switch (kind) {
    case 'f32':
      return TypeF32;
    case 'f16':
      return TypeF16;
    case 'u32':
      return TypeU32;
    case 'u16':
      return TypeU16;
    case 'u8':
      return TypeU8;
    case 'i32':
      return TypeI32;
    case 'i16':
      return TypeI16;
    case 'i8':
      return TypeI8;
    case 'bool':
      return TypeBool;
  }
}

/** @returns the number of scalar (element) types of the given Type */
export function numElementsOf(ty) {
  if (ty instanceof ScalarType) {
    return 1;
  }
  if (ty instanceof VectorType) {
    return ty.width;
  }
  throw new Error(`unhandled type ${ty}`);
}

/** @returns the scalar (element) type of the given Type */
export function scalarTypeOf(ty) {
  if (ty instanceof ScalarType) {
    return ty;
  }
  if (ty instanceof VectorType) {
    return ty.elementType;
  }
  throw new Error(`unhandled type ${ty}`);
}

/** ScalarValue is the JS type that can be held by a Scalar */

/** Class that encapsulates a single scalar value of various types. */
export class Scalar {
  // The scalar value
  // The type of the scalar
  // The scalar value packed in a Uint8Array

  constructor(type, value, bits) {
    this.value = value;
    this.type = type;
    this.bits = new Uint8Array(bits.buffer);
  }

  /**
   * Copies the scalar value to the Uint8Array buffer at the provided byte offset.
   * @param buffer the destination buffer
   * @param offset the byte offset within buffer
   */
  copyTo(buffer, offset) {
    for (let i = 0; i < this.bits.length; i++) {
      buffer[offset + i] = this.bits[i];
    }
  }

  toString() {
    if (this.type.kind === 'bool') {
      return Colors.bold(this.value.toString());
    }
    switch (this.value) {
      case 0:
      case Infinity:
      case -Infinity:
        return Colors.bold(this.value.toString());
      default:
        return Colors.bold(this.value.toString()) + ' (0x' + this.value.toString(16) + ')';
    }
  }
}

/** Create an f32 from a numeric value, a JS `number`. */
export function f32(value) {
  const arr = new Float32Array([value]);
  return new Scalar(TypeF32, arr[0], arr);
}
/** Create an f32 from a bit representation, a uint32 represented as a JS `number`. */
export function f32Bits(bits) {
  const arr = new Uint32Array([bits]);
  return new Scalar(TypeF32, new Float32Array(arr.buffer)[0], arr);
}
/** Create an f16 from a bit representation, a uint16 represented as a JS `number`. */
export function f16Bits(bits) {
  const arr = new Uint16Array([bits]);
  return new Scalar(TypeF16, float16BitsToFloat32(bits), arr);
}

/** Create an i32 from a numeric value, a JS `number`. */
export function i32(value) {
  const arr = new Int32Array([value]);
  return new Scalar(TypeI32, arr[0], arr);
}
/** Create an i16 from a numeric value, a JS `number`. */
export function i16(value) {
  const arr = new Int16Array([value]);
  return new Scalar(TypeI16, arr[0], arr);
}
/** Create an i8 from a numeric value, a JS `number`. */
export function i8(value) {
  const arr = new Int8Array([value]);
  return new Scalar(TypeI8, arr[0], arr);
}

/** Create an i32 from a bit representation, a uint32 represented as a JS `number`. */
export function i32Bits(bits) {
  const arr = new Uint32Array([bits]);
  return new Scalar(TypeI32, new Int32Array(arr.buffer)[0], arr);
}
/** Create an i16 from a bit representation, a uint16 represented as a JS `number`. */
export function i16Bits(bits) {
  const arr = new Uint16Array([bits]);
  return new Scalar(TypeI16, new Int16Array(arr.buffer)[0], arr);
}
/** Create an i8 from a bit representation, a uint8 represented as a JS `number`. */
export function i8Bits(bits) {
  const arr = new Uint8Array([bits]);
  return new Scalar(TypeI8, new Int8Array(arr.buffer)[0], arr);
}

/** Create a u32 from a numeric value, a JS `number`. */
export function u32(value) {
  const arr = new Uint32Array([value]);
  return new Scalar(TypeU32, arr[0], arr);
}
/** Create a u16 from a numeric value, a JS `number`. */
export function u16(value) {
  const arr = new Uint16Array([value]);
  return new Scalar(TypeU16, arr[0], arr);
}
/** Create a u8 from a numeric value, a JS `number`. */
export function u8(value) {
  const arr = new Uint8Array([value]);
  return new Scalar(TypeU8, arr[0], arr);
}

/** Create an u32 from a bit representation, a uint32 represented as a JS `number`. */
export function u32Bits(bits) {
  const arr = new Uint32Array([bits]);
  return new Scalar(TypeU32, bits, arr);
}
/** Create an u16 from a bit representation, a uint16 represented as a JS `number`. */
export function u16Bits(bits) {
  const arr = new Uint16Array([bits]);
  return new Scalar(TypeU16, bits, arr);
}
/** Create an u8 from a bit representation, a uint8 represented as a JS `number`. */
export function u8Bits(bits) {
  const arr = new Uint8Array([bits]);
  return new Scalar(TypeU8, bits, arr);
}

/** Create a boolean value. */
export function bool(value) {
  // WGSL does not support using 'bool' types directly in storage / uniform
  // buffers, so instead we pack booleans in a u32, where 'false' is zero and
  // 'true' is any non-zero value.
  const arr = new Uint32Array([value ? 1 : 0]);
  return new Scalar(TypeBool, value, arr);
}

/** A 'true' literal value */
export const True = bool(true);

/** A 'false' literal value */
export const False = bool(false);

/**
 * Class that encapsulates a vector value.
 */
export class Vector {
  constructor(elements) {
    if (elements.length < 2 || elements.length > 4) {
      throw new Error(`vector element count must be between 2 and 4, got ${elements.length}`);
    }
    for (let i = 1; i < elements.length; i++) {
      const a = elements[0].type;
      const b = elements[i].type;
      if (a !== b) {
        throw new Error(
          `cannot mix vector element types. Found elements with types '${a}' and '${b}'`
        );
      }
    }
    this.elements = elements;
    this.type = TypeVec(elements.length, elements[0].type);
  }

  /**
   * Copies the vector value to the Uint8Array buffer at the provided byte offset.
   * @param buffer the destination buffer
   * @param offset the byte offset within buffer
   */
  copyTo(buffer, offset) {
    for (const element of this.elements) {
      element.copyTo(buffer, offset);
      offset += this.type.elementType.size;
    }
  }

  toString() {
    return `${this.type}(${this.elements.map(e => e.toString()).join(', ')})`;
  }

  get x() {
    assert(0 < this.elements.length);
    return this.elements[0];
  }

  get y() {
    assert(1 < this.elements.length);
    return this.elements[1];
  }

  get z() {
    assert(2 < this.elements.length);
    return this.elements[2];
  }

  get w() {
    assert(3 < this.elements.length);
    return this.elements[3];
  }
}

/** Helper for constructing a new two-element vector with the provided values */
export function vec2(x, y) {
  return new Vector([x, y]);
}

/** Helper for constructing a new three-element vector with the provided values */
export function vec3(x, y, z) {
  return new Vector([x, y, z]);
}

/** Helper for constructing a new four-element vector with the provided values */
export function vec4(x, y, z, w) {
  return new Vector([x, y, z, w]);
}

/** Value is a Scalar or Vector value. */
