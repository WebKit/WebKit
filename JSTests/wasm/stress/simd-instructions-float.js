//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { runSIMDTests } from "./simd-instructions-lib.js"

const verbose = false;

// Table-driven test data for SIMD floating-point arithmetic instructions
// Each entry: [instruction, input0, input1, expected_output]
const floatTests = [
    // f32x4.add tests
    [
        "f32x4.add",
        [1.0, 2.5, -3.0, 0.0],
        [0.5, 1.5, 2.0, -1.0],
        [1.5, 4.0, -1.0, -1.0]
    ],
    [
        "f32x4.add",
        [Number.POSITIVE_INFINITY, 1.0, Number.NaN, 0.0],
        [1.0, Number.NEGATIVE_INFINITY, 1.0, -0.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0]
    ],

    // f32x4.sub tests
    [
        "f32x4.sub",
        [1.5, 4.0, -1.0, -1.0],
        [0.5, 1.5, 2.0, -1.0],
        [1.0, 2.5, -3.0, 0.0]
    ],
    [
        "f32x4.sub",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0],
        [1.0, 1.0, 1.0, Number.POSITIVE_INFINITY],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, Number.NEGATIVE_INFINITY]
    ],

    // f32x4.mul tests
    [
        "f32x4.mul",
        [2.0, 3.0, -4.0, 0.5],
        [1.5, 2.0, -0.5, 4.0],
        [3.0, 6.0, 2.0, 2.0]
    ],
    [
        "f32x4.mul",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0],
        [2.0, 2.0, 2.0, Number.POSITIVE_INFINITY],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, Number.NaN]
    ],

    // f32x4.div tests
    [
        "f32x4.div",
        [6.0, 8.0, -4.0, 1.0],
        [2.0, 4.0, -2.0, 0.5],
        [3.0, 2.0, 2.0, 2.0]
    ],
    [
        "f32x4.div",
        [1.0, -1.0, Number.POSITIVE_INFINITY, Number.NaN],
        [0.0, 0.0, 2.0, 2.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY, Number.NaN]
    ],

    // f32x4.abs tests
    [
        "f32x4.abs",
        [1.0, -2.5, 0.0, -0.0],
        [1.0, 2.5, 0.0, 0.0]
    ],
    [
        "f32x4.abs",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, Number.NaN],
        [Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY, Number.NaN, Number.NaN]
    ],

    // f32x4.neg tests
    [
        "f32x4.neg",
        [1.0, -2.5, 0.0, -0.0],
        [-1.0, 2.5, -0.0, 0.0]
    ],
    [
        "f32x4.neg",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, Number.NaN],
        [Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY, Number.NaN, Number.NaN]
    ],

    // f32x4.sqrt tests
    [
        "f32x4.sqrt",
        [4.0, 9.0, 16.0, 25.0],
        [2.0, 3.0, 4.0, 5.0]
    ],
    [
        "f32x4.sqrt",
        [0.0, -0.0, Number.POSITIVE_INFINITY, Number.NaN],
        [0.0, -0.0, Number.POSITIVE_INFINITY, Number.NaN]
    ],

    // f32x4.min tests (NaN propagating)
    [
        "f32x4.min",
        [1.0, 2.0, -3.0, 0.0],
        [2.0, 1.0, -2.0, -0.0],
        [1.0, 1.0, -3.0, -0.0]
    ],
    [
        "f32x4.min",
        [Number.NaN, 1.0, 2.0, Number.POSITIVE_INFINITY],
        [1.0, Number.NaN, Number.NEGATIVE_INFINITY, 2.0],
        [Number.NaN, Number.NaN, Number.NEGATIVE_INFINITY, 2.0]
    ],

    // f32x4.max tests (Nan propagating)
    [
        "f32x4.max",
        [1.0, 2.0, -3.0, -0.0],
        [2.0, 1.0, -2.0, 0.0],
        [2.0, 2.0, -2.0, 0.0]
    ],
    [
        "f32x4.max",
        [Number.NaN, 1.0, 2.0, Number.NEGATIVE_INFINITY],
        [1.0, Number.NaN, Number.POSITIVE_INFINITY, 2.0],
        [Number.NaN, Number.NaN, Number.POSITIVE_INFINITY, 2.0]
    ],

    // f32x4.pmin tests (pseudo-minimum, b < a ? b : a)
    [
        "f32x4.pmin",
        [1.0, 2.0, -3.0, 0.0],
        [2.0, 1.0, -2.0, -0.0],
        [1.0, 1.0, -3.0, 0.0]
    ],
    [
        "f32x4.pmin",
        [Number.NaN, 1.0, 2.0, Number.POSITIVE_INFINITY],
        [1.0, Number.NaN, Number.NEGATIVE_INFINITY, 2.0],
        [Number.NaN, 1.0, Number.NEGATIVE_INFINITY, 2.0]
    ],

    // f32x4.pmax tests (pseudo-maximum, a < b ? b : a)
    [
        "f32x4.pmax",
        [1.0, 2.0, -3.0, -0.0],
        [2.0, 1.0, -2.0, 0.0],
        [2.0, 2.0, -2.0, -0.0]
    ],
    [
        "f32x4.pmax",
        [Number.NaN, 1.0, 2.0, Number.NEGATIVE_INFINITY],
        [1.0, Number.NaN, Number.POSITIVE_INFINITY, 2.0],
        [Number.NaN, 1.0, Number.POSITIVE_INFINITY, 2.0]
    ],

    // f64x2.add tests
    [
        "f64x2.add",
        [1.0, 2.5],
        [0.5, 1.5],
        [1.5, 4.0]
    ],
    [
        "f64x2.add",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [1.0, 1.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY]
    ],

    // f64x2.sub tests
    [
        "f64x2.sub",
        [1.5, 4.0],
        [0.5, 1.5],
        [1.0, 2.5]
    ],
    [
        "f64x2.sub",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [1.0, 1.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY]
    ],

    // f64x2.mul tests
    [
        "f64x2.mul",
        [2.0, 3.0],
        [1.5, 2.0],
        [3.0, 6.0]
    ],
    [
        "f64x2.mul",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [2.0, 2.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY]
    ],

    // f64x2.div tests
    [
        "f64x2.div",
        [6.0, 8.0],
        [2.0, 4.0],
        [3.0, 2.0]
    ],
    [
        "f64x2.div",
        [1.0, -1.0],
        [0.0, 0.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY]
    ],

    // f64x2.abs tests
    [
        "f64x2.abs",
        [1.0, -2.5],
        [1.0, 2.5]
    ],
    [
        "f64x2.abs",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [Number.POSITIVE_INFINITY, Number.POSITIVE_INFINITY]
    ],

    // f64x2.neg tests
    [
        "f64x2.neg",
        [1.0, -2.5],
        [-1.0, 2.5]
    ],
    [
        "f64x2.neg",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [Number.NEGATIVE_INFINITY, Number.POSITIVE_INFINITY]
    ],

    // f64x2.sqrt tests
    [
        "f64x2.sqrt",
        [4.0, 9.0],
        [2.0, 3.0]
    ],
    [
        "f64x2.sqrt",
        [0.0, -0.0],
        [0.0, -0.0]
    ],

    // f64x2.min tests (Nan propagating)
    [
        "f64x2.min",
        [1.0, 2.0],
        [2.0, 1.0],
        [1.0, 1.0]
    ],
    [
        "f64x2.min",
        [Number.NaN, 1.0],
        [1.0, Number.NaN],
        [Number.NaN, Number.NaN]
    ],

    // f64x2.max tests  (Nan propagating)
    [
        "f64x2.max",
        [1.0, 2.0],
        [2.0, 1.0],
        [2.0, 2.0]
    ],
    [
        "f64x2.max",
        [Number.NaN, 1.0],
        [1.0, Number.NaN],
        [Number.NaN, Number.NaN]
    ],

     // f64x2.pmin tests (pseudo-minimum, b < a ? b : a)
    [
        "f64x2.pmin",
        [1.0, 0.0],
        [2.0, -0.0],
        [1.0, 0.0]
    ],
    [
        "f64x2.pmin",
        [Number.NaN, 1.0],
        [1.0, Number.NaN],
        [Number.NaN, 1.0]
    ],

    // f64x2.pmax tests (pseudo-maximum, a < b ? b : a)
    [
        "f64x2.pmax",
        [1.0, -0.0],
        [2.0, 0.0],
        [2.0, -0.0]
    ],
    [
        "f64x2.pmax",
        [Number.NaN, 1.0],
        [1.0, Number.NaN],
        [Number.NaN, 1.0]
    ],

    // f32x4 rounding operations
    [
        "f32x4.ceil",
        [1.1, -2.7, 3.9, -0.5],
        [2.0, -2.0, 4.0, -0.0]
    ],
    [
        "f32x4.ceil",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0]
    ],

    [
        "f32x4.floor",
        [1.1, -2.7, 3.9, -0.5],
        [1.0, -3.0, 3.0, -1.0]
    ],
    [
        "f32x4.floor",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, -0.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, -0.0]
    ],

    [
        "f32x4.trunc",
        [1.9, -2.1, 3.7, -0.8],
        [1.0, -2.0, 3.0, -0.0]
    ],
    [
        "f32x4.trunc",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 0.0]
    ],

    [
        "f32x4.nearest",
        [1.5, 2.5, -1.5, -2.5],
        [2.0, 2.0, -2.0, -2.0]
    ],
    [
        "f32x4.nearest",
        [0.5, 1.4, Number.POSITIVE_INFINITY, Number.NaN],
        [0.0, 1.0, Number.POSITIVE_INFINITY, Number.NaN]
    ],

    // f64x2 rounding operations
    [
        "f64x2.ceil",
        [1.1, -2.7],
        [2.0, -2.0]
    ],
    [
        "f64x2.ceil",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY]
    ],

    [
        "f64x2.floor",
        [1.1, -2.7],
        [1.0, -3.0]
    ],
    [
        "f64x2.floor",
        [Number.NaN, -0.0],
        [Number.NaN, -0.0]
    ],

    [
        "f64x2.trunc",
        [1.9, -2.1],
        [1.0, -2.0]
    ],
    [
        "f64x2.trunc",
        [Number.POSITIVE_INFINITY, Number.NaN],
        [Number.POSITIVE_INFINITY, Number.NaN]
    ],

    [
        "f64x2.nearest",
        [1.5, -2.5],
        [2.0, -2.0]
    ],
    [
        "f64x2.nearest",
        [0.5, Number.NEGATIVE_INFINITY],
        [0.0, Number.NEGATIVE_INFINITY]
    ],

    // f32x4/f64x2 conversion operations
    [
        "f32x4.demote_f64x2_zero",
        [1.5, 2.5],
        [1.5, 2.5, 0.0, 0.0]
    ],
    [
        "f32x4.demote_f64x2_zero",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, 0.0, 0.0]
    ],

    [
        "f64x2.promote_low_f32x4",
        [1.5, 2.5, 3.5, 4.5],
        [1.5, 2.5]
    ],
    [
        "f64x2.promote_low_f32x4",
        [Number.POSITIVE_INFINITY, Number.NaN, 0.0, -0.0],
        [Number.POSITIVE_INFINITY, Number.NaN]
    ],

    // Integer/float conversion operations
    [
        "i32x4.trunc_sat_f32x4_s",
        [1.5, -2.7, 2147483647.0, -2147483648.0],
        [0x00000001, 0xFFFFFFFE, 0x7FFFFFFF, 0x80000000] // 1, -2, 2147483647, -2147483648
    ],
    [
        "i32x4.trunc_sat_f32x4_s",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 3000000000.0],
        [0x7FFFFFFF, 0x80000000, 0x00000000, 0x7FFFFFFF] // 2147483647, -2147483648, 0, 2147483647
    ],

    [
        "i32x4.trunc_sat_f32x4_u",
        [1.5, 2.7, 4294967295.0, -1.0],
        [0x00000001, 0x00000002, 0xFFFFFFFF, 0x00000000] // 1, 2, 4294967295, 0
    ],
    [
        "i32x4.trunc_sat_f32x4_u",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY, Number.NaN, 5000000000.0],
        [0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF] // 4294967295, 0, 0, 4294967295
    ],

    [
        "f32x4.convert_i32x4_s",
        [1, -2, 2147483647, -2147483648],
        [1.0, -2.0, 2147483648.0, -2147483648.0]
    ],
    [
        "f32x4.convert_i32x4_s",
        [0, -1, 1000000, -1000000],
        [0.0, -1.0, 1000000.0, -1000000.0]
    ],

    [
        "f32x4.convert_i32x4_u",
        [1, 2, 4294967295, 0],
        [1.0, 2.0, 4294967296.0, 0.0]
    ],
    [
        "f32x4.convert_i32x4_u",
        [1000000, 2000000, 3000000000, 4000000000],
        [1000000.0, 2000000.0, 3000000000.0, 4000000000.0]
    ],

    [
        "i32x4.trunc_sat_f64x2_s_zero",
        [1.5, -2.7],
        [0x00000001, 0xFFFFFFFE, 0x00000000, 0x00000000] // 1, -2, 0, 0
    ],
    [
        "i32x4.trunc_sat_f64x2_s_zero",
        [Number.POSITIVE_INFINITY, Number.NEGATIVE_INFINITY],
        [0x7FFFFFFF, 0x80000000, 0x00000000, 0x00000000] // 2147483647, -2147483648, 0, 0
    ],

    [
        "i32x4.trunc_sat_f64x2_u_zero",
        [1.5, 2.7],
        [1, 2, 0, 0]
    ],
    [
        "i32x4.trunc_sat_f64x2_u_zero",
        [Number.NaN, -1.0],
        [0, 0, 0, 0]
    ],

    [
        "f64x2.convert_low_i32x4_s",
        [1, -2, 3, 4],
        [1.0, -2.0]
    ],
    [
        "f64x2.convert_low_i32x4_s",
        [2147483647, -2147483648, 0, -1],
        [2147483647.0, -2147483648.0]
    ],

    [
        "f64x2.convert_low_i32x4_u",
        [1, 2, 3, 4],
        [1.0, 2.0]
    ],
    [
        "f64x2.convert_low_i32x4_u",
        [4294967295, 0, 1000000, 2000000],
        [4294967295.0, 0.0]
    ]
];

await runSIMDTests(floatTests, verbose, "SIMD floating-point");