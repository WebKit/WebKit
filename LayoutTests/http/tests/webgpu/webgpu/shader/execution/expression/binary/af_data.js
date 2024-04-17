/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { crc32 } from '../../../../../common/util/crc32.js';import { kValue } from '../../../../util/constants.js';import { FP } from '../../../../util/floating_point.js';
import { sparseScalarF64Range } from '../../../../util/math.js';
import { reinterpretU64AsF64 } from '../../../../util/reinterpret.js';

/** Line format of af_data_gen/main.cpp outputs */






/**
 * Externally generated values for lhs + rhs.
 * These values are generated using af_data_gen/main.cpp and manually copied in
 * here. This is done to get around the limitations of AbstractFloats in WGSL
 * and TS numbers both being f64 internally.
 *
 * This contains entries for performing addition of the cartesian product of
 * kInterestingF64Values with itself. Thus, can be used to create a lookup table
 * to replace using the FPTraits framework for calculating intervals where
 * sparseScalarF64Range, etc are being used to generate inputs.
 */

export const kAdditionRawValues = [
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.infinity, kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: -10.0, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: -1.0, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: -0.125, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.negative.max, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.negative.subnormal.min, expected: [kValue.f64.negative.min, 0.0] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.negative.subnormal.max, expected: [kValue.f64.negative.min, 0.0] },
{ lhs: kValue.f64.negative.min, rhs: 0.0, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.positive.subnormal.min, expected: [kValue.f64.negative.min, 0.0] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.positive.subnormal.max, expected: [kValue.f64.negative.min, 0.0] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.positive.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: 0.125, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: 1.0, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: 10.0, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.min, rhs: kValue.f64.positive.max, expected: [0.0] },
{ lhs: -10.0, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: -10.0, rhs: -10.0, expected: [-20] },
{ lhs: -10.0, rhs: -1.0, expected: [-11] },
{ lhs: -10.0, rhs: -0.125, expected: [-10.125] },
{ lhs: -10.0, rhs: kValue.f64.negative.max, expected: [-10.0] },
{ lhs: -10.0, rhs: kValue.f64.negative.subnormal.min, expected: [-10.0, 0.0] },
{ lhs: -10.0, rhs: kValue.f64.negative.subnormal.max, expected: [-10.0, 0.0] },
{ lhs: -10.0, rhs: 0.0, expected: [-10.0] },
{ lhs: -10.0, rhs: kValue.f64.positive.subnormal.min, expected: [-10.0, 0.0] },
{ lhs: -10.0, rhs: kValue.f64.positive.subnormal.max, expected: [-10.0, 0.0] },
{ lhs: -10.0, rhs: kValue.f64.positive.min, expected: [-10.0] },
{ lhs: -10.0, rhs: 0.125, expected: [-9.875] },
{ lhs: -10.0, rhs: 1.0, expected: [-9] },
{ lhs: -10.0, rhs: 10.0, expected: [0.0] },
{ lhs: -10.0, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: -1.0, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: -1.0, rhs: -10.0, expected: [-11] },
{ lhs: -1.0, rhs: -1.0, expected: [-2] },
{ lhs: -1.0, rhs: -0.125, expected: [-1.125] },
{ lhs: -1.0, rhs: kValue.f64.negative.max, expected: [-1.0] },
{ lhs: -1.0, rhs: kValue.f64.negative.subnormal.min, expected: [-1.0, 0.0] },
{ lhs: -1.0, rhs: kValue.f64.negative.subnormal.max, expected: [-1.0, 0.0] },
{ lhs: -1.0, rhs: 0.0, expected: [-1.0] },
{ lhs: -1.0, rhs: kValue.f64.positive.subnormal.min, expected: [-1.0, 0.0] },
{ lhs: -1.0, rhs: kValue.f64.positive.subnormal.max, expected: [-1.0, 0.0] },
{ lhs: -1.0, rhs: kValue.f64.positive.min, expected: [-1.0] },
{ lhs: -1.0, rhs: 0.125, expected: [-0.875] },
{ lhs: -1.0, rhs: 1.0, expected: [0.0] },
{ lhs: -1.0, rhs: 10.0, expected: [9] },
{ lhs: -1.0, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: -0.125, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: -0.125, rhs: -10.0, expected: [-10.125] },
{ lhs: -0.125, rhs: -1.0, expected: [-1.125] },
{ lhs: -0.125, rhs: -0.125, expected: [-0.25] },
{ lhs: -0.125, rhs: kValue.f64.negative.max, expected: [-0.125] },
{ lhs: -0.125, rhs: kValue.f64.negative.subnormal.min, expected: [-0.125, 0.0] },
{ lhs: -0.125, rhs: kValue.f64.negative.subnormal.max, expected: [-0.125, 0.0] },
{ lhs: -0.125, rhs: 0.0, expected: [-0.125] },
{ lhs: -0.125, rhs: kValue.f64.positive.subnormal.min, expected: [-0.125, 0.0] },
{ lhs: -0.125, rhs: kValue.f64.positive.subnormal.max, expected: [-0.125, 0.0] },
{ lhs: -0.125, rhs: kValue.f64.positive.min, expected: [-0.125] },
{ lhs: -0.125, rhs: 0.125, expected: [0.0] },
{ lhs: -0.125, rhs: 1.0, expected: [0.875] },
{ lhs: -0.125, rhs: 10.0, expected: [9.875] },
{ lhs: -0.125, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.max, rhs: -10.0, expected: [-10.0] },
{ lhs: kValue.f64.negative.max, rhs: -1.0, expected: [-1.0] },
{ lhs: kValue.f64.negative.max, rhs: -0.125, expected: [-0.125] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.negative.max, expected: [reinterpretU64AsF64(0x8020000000000000n) /* ~-4.45015e-308 */] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.negative.subnormal.min, expected: [reinterpretU64AsF64(0x801fffffffffffffn) /* ~-4.45015e-308 */, kValue.f64.negative.max, 0.0] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.negative.subnormal.max, expected: [reinterpretU64AsF64(0x8010000000000001n) /* ~-2.22507e-308 */, kValue.f64.negative.max, 0.0] },
{ lhs: kValue.f64.negative.max, rhs: 0.0, expected: [kValue.f64.negative.max] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.positive.subnormal.min, expected: [kValue.f64.negative.max, kValue.f64.negative.subnormal.min, 0.0] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.positive.subnormal.max, expected: [kValue.f64.negative.max, kValue.f64.negative.subnormal.max, 0.0] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.positive.min, expected: [0.0] },
{ lhs: kValue.f64.negative.max, rhs: 0.125, expected: [0.125] },
{ lhs: kValue.f64.negative.max, rhs: 1.0, expected: [1.0] },
{ lhs: kValue.f64.negative.max, rhs: 10.0, expected: [10.0] },
{ lhs: kValue.f64.negative.max, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: -10.0, expected: [-10.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: -1.0, expected: [-1.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: -0.125, expected: [-0.125] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.negative.max, expected: [reinterpretU64AsF64(0x801fffffffffffffn) /* ~-4.45015e-308 */, kValue.f64.negative.max] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.negative.subnormal.min, expected: [reinterpretU64AsF64(0x801ffffffffffffen) /* ~-4.45015e-308 */, kValue.f64.negative.subnormal.min, 0.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.negative.subnormal.max, expected: [kValue.f64.negative.max, kValue.f64.negative.subnormal.min, kValue.f64.negative.subnormal.max, 0.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: 0.0, expected: [kValue.f64.negative.subnormal.min, 0.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.positive.subnormal.min, expected: [kValue.f64.negative.subnormal.min, reinterpretU64AsF64(0x800ffffffffffffen) /* ~-2.22507e-308 */, 0.0, kValue.f64.positive.subnormal.min] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.positive.subnormal.max, expected: [kValue.f64.negative.subnormal.min, 0.0, kValue.f64.positive.subnormal.max] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.positive.min, expected: [kValue.f64.positive.subnormal.min, kValue.f64.positive.min] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: 0.125, expected: [0.125] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: 1.0, expected: [1.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: 10.0, expected: [10.0] },
{ lhs: kValue.f64.negative.subnormal.min, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: -10.0, expected: [-10.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: -1.0, expected: [-1.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: -0.125, expected: [-0.125] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.negative.max, expected: [reinterpretU64AsF64(0x8010000000000001n) /* ~-2.22507e-308 */, kValue.f64.negative.max] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.negative.subnormal.min, expected: [kValue.f64.negative.max, kValue.f64.negative.subnormal.min, kValue.f64.negative.subnormal.max, 0.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.negative.subnormal.max, expected: [reinterpretU64AsF64(0x8000000000000002n) /* ~-9.88131e-324 */, kValue.f64.negative.subnormal.max, 0.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: 0.0, expected: [kValue.f64.negative.subnormal.max, 0.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.positive.subnormal.min, expected: [kValue.f64.negative.subnormal.max, 0.0, kValue.f64.positive.subnormal.min] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.positive.subnormal.max, expected: [kValue.f64.negative.subnormal.max, 0.0, reinterpretU64AsF64(0x000ffffffffffffen) /* ~2.22507e-308 */, kValue.f64.positive.subnormal.max] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.positive.min, expected: [kValue.f64.positive.subnormal.max, kValue.f64.positive.min] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: 0.125, expected: [0.125] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: 1.0, expected: [1.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: 10.0, expected: [10.0] },
{ lhs: kValue.f64.negative.subnormal.max, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: 0.0, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: 0.0, rhs: -10.0, expected: [-10.0] },
{ lhs: 0.0, rhs: -1.0, expected: [-1.0] },
{ lhs: 0.0, rhs: -0.125, expected: [-0.125] },
{ lhs: 0.0, rhs: kValue.f64.negative.max, expected: [kValue.f64.negative.max] },
{ lhs: 0.0, rhs: kValue.f64.negative.subnormal.min, expected: [kValue.f64.negative.subnormal.min, 0.0] },
{ lhs: 0.0, rhs: kValue.f64.negative.subnormal.max, expected: [kValue.f64.negative.subnormal.max, 0.0] },
{ lhs: 0.0, rhs: 0.0, expected: [0.0] },
{ lhs: 0.0, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, kValue.f64.positive.subnormal.min] },
{ lhs: 0.0, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, kValue.f64.positive.subnormal.max] },
{ lhs: 0.0, rhs: kValue.f64.positive.min, expected: [kValue.f64.positive.min] },
{ lhs: 0.0, rhs: 0.125, expected: [0.125] },
{ lhs: 0.0, rhs: 1.0, expected: [1.0] },
{ lhs: 0.0, rhs: 10.0, expected: [10.0] },
{ lhs: 0.0, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: -10.0, expected: [-10.0] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: -1.0, expected: [-1.0] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: -0.125, expected: [-0.125] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.negative.max, expected: [kValue.f64.negative.max, kValue.f64.negative.subnormal.min] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.negative.subnormal.min, expected: [kValue.f64.negative.subnormal.min, reinterpretU64AsF64(0x800ffffffffffffen) /* ~-2.22507e-308 */, 0.0, kValue.f64.positive.subnormal.min] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.negative.subnormal.max, expected: [kValue.f64.negative.subnormal.max, 0.0, kValue.f64.positive.subnormal.min] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: 0.0, expected: [0.0, kValue.f64.positive.subnormal.min] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, kValue.f64.positive.subnormal.min, reinterpretU64AsF64(0x0000000000000002n) /* ~9.88131e-324 */] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, kValue.f64.positive.subnormal.min, kValue.f64.positive.subnormal.max, kValue.f64.positive.min] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.positive.min, expected: [kValue.f64.positive.min, reinterpretU64AsF64(0x0010000000000001n) /* ~2.22507e-308 */] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: 0.125, expected: [0.125] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: 1.0, expected: [1.0] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: 10.0, expected: [10.0] },
{ lhs: kValue.f64.positive.subnormal.min, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: -10.0, expected: [-10.0] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: -1.0, expected: [-1.0] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: -0.125, expected: [-0.125] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.negative.max, expected: [kValue.f64.negative.max, kValue.f64.negative.subnormal.max] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.negative.subnormal.min, expected: [kValue.f64.negative.subnormal.min, 0.0, kValue.f64.positive.subnormal.max] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.negative.subnormal.max, expected: [kValue.f64.negative.subnormal.max, 0.0, reinterpretU64AsF64(0x000ffffffffffffen) /* ~2.22507e-308 */, kValue.f64.positive.subnormal.max] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: 0.0, expected: [0.0, kValue.f64.positive.subnormal.max] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, kValue.f64.positive.subnormal.min, kValue.f64.positive.subnormal.max, kValue.f64.positive.min] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, kValue.f64.positive.subnormal.max, reinterpretU64AsF64(0x001ffffffffffffen) /* ~4.45015e-308 */] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.positive.min, expected: [kValue.f64.positive.min, reinterpretU64AsF64(0x001fffffffffffffn) /* ~4.45015e-308 */] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: 0.125, expected: [0.125] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: 1.0, expected: [1.0] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: 10.0, expected: [10.0] },
{ lhs: kValue.f64.positive.subnormal.max, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: kValue.f64.positive.min, rhs: -10.0, expected: [-10.0] },
{ lhs: kValue.f64.positive.min, rhs: -1.0, expected: [-1.0] },
{ lhs: kValue.f64.positive.min, rhs: -0.125, expected: [-0.125] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.negative.max, expected: [0.0] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.negative.subnormal.min, expected: [0.0, kValue.f64.positive.subnormal.min, kValue.f64.positive.min] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.negative.subnormal.max, expected: [0.0, kValue.f64.positive.subnormal.max, kValue.f64.positive.min] },
{ lhs: kValue.f64.positive.min, rhs: 0.0, expected: [kValue.f64.positive.min] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, kValue.f64.positive.min, reinterpretU64AsF64(0x0010000000000001n) /* ~2.22507e-308 */] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, kValue.f64.positive.min, reinterpretU64AsF64(0x001fffffffffffffn) /* ~4.45015e-308 */] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.positive.min, expected: [reinterpretU64AsF64(0x0020000000000000n) /* ~4.45015e-308 */] },
{ lhs: kValue.f64.positive.min, rhs: 0.125, expected: [0.125] },
{ lhs: kValue.f64.positive.min, rhs: 1.0, expected: [1.0] },
{ lhs: kValue.f64.positive.min, rhs: 10.0, expected: [10.0] },
{ lhs: kValue.f64.positive.min, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: 0.125, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: 0.125, rhs: -10.0, expected: [-9.875] },
{ lhs: 0.125, rhs: -1.0, expected: [-0.875] },
{ lhs: 0.125, rhs: -0.125, expected: [0.0] },
{ lhs: 0.125, rhs: kValue.f64.negative.max, expected: [0.125] },
{ lhs: 0.125, rhs: kValue.f64.negative.subnormal.min, expected: [0.0, 0.125] },
{ lhs: 0.125, rhs: kValue.f64.negative.subnormal.max, expected: [0.0, 0.125] },
{ lhs: 0.125, rhs: 0.0, expected: [0.125] },
{ lhs: 0.125, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, 0.125] },
{ lhs: 0.125, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, 0.125] },
{ lhs: 0.125, rhs: kValue.f64.positive.min, expected: [0.125] },
{ lhs: 0.125, rhs: 0.125, expected: [0.25] },
{ lhs: 0.125, rhs: 1.0, expected: [1.125] },
{ lhs: 0.125, rhs: 10.0, expected: [10.125] },
{ lhs: 0.125, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: 1.0, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: 1.0, rhs: -10.0, expected: [-9] },
{ lhs: 1.0, rhs: -1.0, expected: [0.0] },
{ lhs: 1.0, rhs: -0.125, expected: [0.875] },
{ lhs: 1.0, rhs: kValue.f64.negative.max, expected: [1.0] },
{ lhs: 1.0, rhs: kValue.f64.negative.subnormal.min, expected: [0.0, 1.0] },
{ lhs: 1.0, rhs: kValue.f64.negative.subnormal.max, expected: [0.0, 1.0] },
{ lhs: 1.0, rhs: 0.0, expected: [1.0] },
{ lhs: 1.0, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, 1.0] },
{ lhs: 1.0, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, 1.0] },
{ lhs: 1.0, rhs: kValue.f64.positive.min, expected: [1.0] },
{ lhs: 1.0, rhs: 0.125, expected: [1.125] },
{ lhs: 1.0, rhs: 1.0, expected: [2] },
{ lhs: 1.0, rhs: 10.0, expected: [11] },
{ lhs: 1.0, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: 10.0, rhs: kValue.f64.negative.min, expected: [kValue.f64.negative.min] },
{ lhs: 10.0, rhs: -10.0, expected: [0.0] },
{ lhs: 10.0, rhs: -1.0, expected: [9] },
{ lhs: 10.0, rhs: -0.125, expected: [9.875] },
{ lhs: 10.0, rhs: kValue.f64.negative.max, expected: [10.0] },
{ lhs: 10.0, rhs: kValue.f64.negative.subnormal.min, expected: [0.0, 10.0] },
{ lhs: 10.0, rhs: kValue.f64.negative.subnormal.max, expected: [0.0, 10.0] },
{ lhs: 10.0, rhs: 0.0, expected: [10.0] },
{ lhs: 10.0, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, 10.0] },
{ lhs: 10.0, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, 10.0] },
{ lhs: 10.0, rhs: kValue.f64.positive.min, expected: [10.0] },
{ lhs: 10.0, rhs: 0.125, expected: [10.125] },
{ lhs: 10.0, rhs: 1.0, expected: [11] },
{ lhs: 10.0, rhs: 10.0, expected: [20] },
{ lhs: 10.0, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.negative.min, expected: [0.0] },
{ lhs: kValue.f64.positive.max, rhs: -10.0, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: -1.0, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: -0.125, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.negative.max, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.negative.subnormal.min, expected: [0.0, kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.negative.subnormal.max, expected: [0.0, kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: 0.0, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.positive.subnormal.min, expected: [0.0, kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.positive.subnormal.max, expected: [0.0, kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.positive.min, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: 0.125, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: 1.0, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: 10.0, expected: [kValue.f64.positive.max] },
{ lhs: kValue.f64.positive.max, rhs: kValue.f64.positive.max, expected: [kValue.f64.positive.max, kValue.f64.positive.infinity] }];


/** Hashing function for keys in lookup tables */
function hashRawDataInputs(lhs, rhs) {
  return crc32(`lhs: ${lhs}, rhs: ${rhs}`);
}

/** Table mapping addition binary inputs to expectation intervals */
const kAdditionTable = Object.fromEntries(
  kAdditionRawValues.map((value) => [
  hashRawDataInputs(value.lhs, value.rhs),
  FP.abstract.spanIntervals(...value.expected.map((e) => FP.abstract.correctlyRoundedInterval(e)))]
  )
);

/** External interface for fetching addition expectation intervals. */
export function getAdditionAFInterval(lhs, rhs) {
  return kAdditionTable[hashRawDataInputs(lhs, rhs)];
}

/** Simplified version of kSparseVectorF64Range that only uses values from sparseScalarF64Range() */
export const kSparseVectorAFValues = {
  2: sparseScalarF64Range().map((f) => Array(2).fill(f)),
  3: sparseScalarF64Range().map((f) => Array(3).fill(f)),
  4: sparseScalarF64Range().map((f) => Array(4).fill(f))
};

/** Simplified version of kSparseMatrixF64Range that only uses values from sparseScalarF64Range() */
export const kSparseMatrixAFValues = {
  2: {
    2: sparseScalarF64Range().map((f) => Array(2).fill(Array(2).fill(f))),
    3: sparseScalarF64Range().map((f) => Array(2).fill(Array(3).fill(f))),
    4: sparseScalarF64Range().map((f) => Array(2).fill(Array(4).fill(f)))
  },
  3: {
    2: sparseScalarF64Range().map((f) => Array(3).fill(Array(2).fill(f))),
    3: sparseScalarF64Range().map((f) => Array(3).fill(Array(3).fill(f))),
    4: sparseScalarF64Range().map((f) => Array(3).fill(Array(4).fill(f)))
  },
  4: {
    2: sparseScalarF64Range().map((f) => Array(4).fill(Array(2).fill(f))),
    3: sparseScalarF64Range().map((f) => Array(4).fill(Array(3).fill(f))),
    4: sparseScalarF64Range().map((f) => Array(4).fill(Array(4).fill(f)))
  }
};