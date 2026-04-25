/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for evaluation order of expressions
`;
import { makeTestGroup } from '../../../common/framework/test_group.js';
import { GPUTest } from '../../gpu_test.js';
import { TypeI32 } from '../../util/conversion.js';

export const g = makeTestGroup(GPUTest);

const common_source = `
var<private> a : i32 = 2;
var<private> b : i32 = 3;
var<private> c : i32 = 4;

var<private> arr2D : array<array<i32, 10>, 10> = array<array<i32, 10>, 10>(
  array<i32, 10>( 0,  1,  2,  3,  4,  5,  6,  7,  8,  9),
  array<i32, 10>(10, 11, 12, 13, 14, 15, 16, 17, 18, 19),
  array<i32, 10>(20, 22, 22, 23, 24, 25, 26, 27, 28, 29),
  array<i32, 10>(30, 32, 32, 33, 34, 35, 36, 37, 38, 33),
  array<i32, 10>(40, 42, 42, 43, 44, 45, 46, 47, 48, 44),
  array<i32, 10>(50, 52, 52, 53, 54, 55, 56, 57, 58, 55),
  array<i32, 10>(60, 62, 62, 63, 64, 65, 66, 67, 68, 66),
  array<i32, 10>(70, 72, 72, 73, 74, 75, 76, 77, 78, 77),
  array<i32, 10>(80, 82, 82, 83, 84, 85, 86, 87, 88, 98),
  array<i32, 10>(90, 92, 92, 93, 94, 95, 96, 97, 98, 99),
);

var<private> arr1D_zero : array<i32, 50>;
var<private> arr2D_zero : array<array<i32, 50>, 50>;
var<private> arr3D_zero : array<array<array<i32, 50>, 50>, 50>;

var<private> vec4_zero : vec4<i32>;

struct S {
  x : i32,
  y : i32,
  z : i32,
}

fn mul(p1 : ptr<private, i32>, multiplicand : i32) -> i32 {
  *p1 = *p1 * multiplicand;
  return *p1;
}

fn add(p1 : ptr<private, i32>, addend : i32) -> i32 {
  *p1 = *p1 + addend;
  return *p1;
}

fn sub_mul3(a : i32, a_mul : i32, b : i32, b_mul : i32, c : i32, c_mul : i32) -> i32 {
  return a * a_mul - b * b_mul - c * c_mul;
}

fn sub_mul4(a : i32, a_mul : i32, b : i32, b_mul : i32, c : i32, c_mul : i32, d : i32, d_mul : i32) -> i32 {
  return a * a_mul - b * b_mul - c * c_mul - d * d_mul;
}

fn set_vec4_x(p : ptr<private, vec4<i32>>, v : i32) -> i32 {
  (*p).x = v;
  return 0;
}

fn make_S(init : i32) -> S {
  return S(init, init, init);
}

fn mul3_ret0(p1 : ptr<private, i32>, p2 : ptr<private, i32>, p3 : ptr<private, i32>, multiplicand : i32) -> i32 {
  *p1 = *p1 * multiplicand;
  *p2 = *p2 * multiplicand;
  *p3 = *p3 * multiplicand;
  return 0;
}
`;

g.test('binary_arith')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#arithmetic-expr')
  .desc('Tests order of evaluation of arithmetic binary expressions.')
  .paramsSimple([
    {
      name: 'BothSE', // SE = Side Effects
      _body: 'return mul(&a, 10) - mul(&a, 10);',
      _result: 20 - 200,
    },
    {
      name: 'LeftSE', //
      _body: 'return mul(&a, 10) - a;',
      _result: 20 - 20,
    },
    {
      name: 'RightSE', //
      _body: 'return a - mul(&a, 10);',
      _result: 2 - 20,
    },
    {
      name: 'ThreeSE',
      _body: 'return mul(&a, 10) - mul(&a, 10) - mul(&a, 10);',
      _result: 20 - 200 - 2000,
    },
    {
      name: 'LeftmostSE',
      _body: 'return mul3_ret0(&a, &b, &c, 10) - a - b - c;',
      _result: 0 - 20 - 30 - 40,
    },
    {
      name: 'RightmostSE', //
      _body: 'return a - b - c - mul3_ret0(&a, &b, &c, 10);',
      _result: 2 - 3 - 4 - 0,
    },
    {
      name: 'MiddleSE', //
      _body: 'return a - b - mul3_ret0(&a, &b, &c, 10) - c;',
      _result: 2 - 3 - 0 - 40,
    },
    {
      name: 'LiteralAndSEAndVar', //
      _body: 'return 1 - mul(&a, 10) - a;',
      _result: 1 - 20 - 20,
    },
    {
      name: 'VarAndSEAndLiteral', //
      _body: 'return a - mul(&a, 10) - 1;',
      _result: 2 - 20 - 1,
    },
    {
      name: 'SEAndVarAndLiteral', //
      _body: 'return mul(&a, 10) - a - 1;',
      _result: 20 - 20 - 1,
    },
    {
      name: 'VarAndLiteralAndSE', //
      _body: 'return a - 1 - mul(&a, 10);',
      _result: 2 - 1 - 20,
    },
    {
      name: 'MemberAccessAndSE',
      _body: 'return vec4_zero.x + set_vec4_x(&vec4_zero, 42);',
      _result: 0,
    },
    {
      name: 'SEAndMemberAccess',
      _body: 'return set_vec4_x(&vec4_zero, 42) + vec4_zero.x;',
      _result: 42,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('binary_logical')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#logical-expr')
  .desc('Tests order of evaluation of logical binary expressions.')
  .paramsSimple([
    {
      name: 'BothSE',
      _body:
        'let r = (add(&a, 1) == 3) && (add(&a, 1) == 4);' + //
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'LeftSE',
      _body:
        'let r = (add(&a, 1) == 3) && (a == 3);' + //
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'RightSE',
      _body:
        'let r = (a == 2) && (add(&a, 1) == 3);' + //
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'LeftmostSE',
      _body:
        'let r = (mul3_ret0(&a, &b, &c, 10) == 0) && (a == 20) && (b == 30) && (c == 40);' +
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'RightmostSE',
      _body:
        'let r = (a == 2) && (b == 3) && (c == 4) && (mul3_ret0(&a, &b, &c, 10) == 0);' +
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'MiddleSE',
      _body:
        'let r = (a == 2) && (b == 3) && (mul3_ret0(&a, &b, &c, 10) == 0) && (c == 40);' +
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'ShortCircuit_And_LhsOnly',
      _body:
        // rhs should not execute
        'let t = (a != 2) && (mul(&a, 10) == 20);' + //
        'return a;',
      _result: 2,
    },
    {
      name: 'ShortCircuit_And_LhsAndRhs',
      _body:
        // rhs should execute
        'let t = (a == 2) && (mul(&a, 10) == 20);' + //
        'return a;',
      _result: 20,
    },
    {
      name: 'ShortCircuit_Or_LhsOnly',
      _body:
        // rhs should not execute
        'let t = (a == 2) || (mul(&a, 10) == 20);' + //
        'let r = (a == 2);' +
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'ShortCircuit_Or_LhsAndRhs',
      _body:
        // rhs should execute
        'let t = (a != 2) || (mul(&a, 10) == 20);' + //
        'let r = (a == 20);' +
        'return i32(r);',
      _result: 1,
    },
    {
      name: 'NoShortCircuit_And',
      _body:
        // rhs should execute
        'let t = (a != 2) & (mul(&a, 10) == 20);' + //
        'return a;',
      _result: 20,
    },
    {
      name: 'NoShortCircuit_Or',
      _body:
        // rhs should execute
        'let t = (a == 2) | (mul(&a, 10) == 20);' + //
        'return a;',
      _result: 20,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('binary_mixed')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#arithmetic-expr')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#logical-expr')
  .desc('Tests order of evaluation of both arithmetic and logical binary expressions.')
  .paramsSimple([
    {
      name: 'ArithAndLogical',
      _body: 'return mul(&a, 10) - i32(mul(&a, 10) == 200 && mul(&a, 10) == 2000);',
      _result: 20 - 1,
    },
    {
      name: 'LogicalAndArith',
      _body: 'return i32(mul(&a, 10) == 20 && mul(&a, 10) == 200) - mul(&a, 10);',
      _result: 1 - 2000,
    },
    {
      name: 'ArithAndLogical_ShortCircuit',
      _body: 'return mul(&a, 10) - i32(mul(&a, 10) != 200 && mul(&a, 10) == 2000);',
      _result: 20 - 0,
    },
    {
      name: 'LogicalAndArith_ShortCircuit',
      _body: 'return i32(mul(&a, 10) != 20 && mul(&a, 10) == 200) - mul(&a, 10);',
      _result: 0 - 200,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('call')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#function-calls')
  .desc('Tests order of evaluation of call expressions.')
  .paramsSimple([
    {
      name: 'OneSE', //
      _body: 'return sub_mul3(mul(&a, 2), 2, a, 3, 3, 4);',
      _result: 4 * 2 - 4 * 3 - 3 * 4,
    },
    {
      name: 'AllSE',
      _body: 'return sub_mul3(mul(&a, 2), 2, mul(&a, 2), 3, mul(&a, 2), 4);',
      _result: 4 * 2 - 8 * 3 - 16 * 4,
    },
    {
      name: 'MiddleNotSE',
      _body: 'return sub_mul3(mul(&a, 2), 2, a, 3, mul(&a, 2), 4);',
      _result: 4 * 2 - 4 * 3 - 8 * 4,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('index_accessor')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#array-access-expr')
  .desc('Tests order of evaluation of index accessor expressions.')
  .paramsSimple([
    {
      name: 'LeftSE', //
      _body: 'return arr2D[mul(&a, 2)][a];',
      _result: 4 * 10 + 4,
    },
    {
      name: 'RightSE', //
      _body: 'return arr2D[a][mul(&a, 2)];',
      _result: 2 * 10 + 4,
    },
    {
      name: 'BothSE',
      _body: 'return arr2D[mul(&a, 2)][mul(&a, 2)];',
      _result: 4 * 10 + 8,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('assignment')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#assignment')
  .desc('Tests order of evaluation of assignment statements.')
  .paramsSimple([
    {
      name: 'ToArray1D',
      _body:
        'arr1D_zero[mul(&a, 2)] = mul(&a, 2);' + //
        'return arr1D_zero[8];',
      _result: 4,
    },
    {
      name: 'ToArray2D',
      _body:
        'arr2D_zero[mul(&a, 2)][mul(&a, 2)] = mul(&a, 2);' + //
        'return arr2D_zero[8][16];',
      _result: 4,
    },
    {
      name: 'ToArrayFromArray',
      _body:
        'arr2D_zero[4][8] = 123;' +
        'arr1D_zero[mul(&a, 2)] = arr2D_zero[mul(&a, 2)][mul(&a, 2)];' +
        'return arr1D_zero[16];',
      _result: 123,
    },
    {
      name: 'ToArrayIndexedByArrayIndexedBySE',
      _body:
        'var arr1 = arr1D_zero;' +
        'var arr2 = arr1D_zero;' +
        'arr2[8] = 3;' +
        'arr1[arr2[mul(&a, 2)]] = mul(&a, 2);' +
        'return arr1[3];',
      _result: 4,
    },
    {
      name: 'ToVec_BothSE',
      _body:
        'a = 0;' +
        'vec4_zero[add(&a, 1)] = add(&a, 1);' + //
        'return vec4_zero[2];',
      _result: 1,
    },
    {
      name: 'ToVec_LeftSE',
      _body:
        'vec4_zero[add(&a, 1)] = a;' + //
        'return vec4_zero[3];',
      _result: 2,
    },
    {
      name: 'ToVec_RightSE',
      _body:
        'vec4_zero[a] = add(&a, 1);' + //
        'return vec4_zero[3];',
      _result: 3,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('type_constructor')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#type-constructor-expr')
  .desc('Tests order of evaluation of type constructor expressions.')
  .paramsSimple([
    {
      name: 'Struct',
      _body:
        'let r = S(mul(&a, 2), mul(&a, 2), mul(&a, 2));' + //
        'return sub_mul3(r.x, 2, r.y, 3, r.z, 4);',
      _result: 4 * 2 - 8 * 3 - 16 * 4,
    },
    {
      name: 'Array1D',
      _body:
        'let r = array<i32, 3>(mul(&a, 2), mul(&a, 2), mul(&a, 2));' + //
        'return sub_mul3(r[0], 2, r[1], 3, r[2], 4);',
      _result: 4 * 2 - 8 * 3 - 16 * 4,
    },
    {
      name: 'Array2D',
      _body:
        'let r = array<array<i32, 2>, 2>(array<i32, 2>(mul(&a, 2), mul(&a, 2)), array<i32, 2>(mul(&a, 2), mul(&a, 2)));' +
        'return sub_mul4(r[0][0], 2, r[0][1], 3, r[1][0], 4, r[1][1], 5);',
      _result: 4 * 2 - 8 * 3 - 16 * 4 - 32 * 5,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

g.test('member_accessor')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#struct-access-expr')
  .specURL('https://gpuweb.github.io/gpuweb/wgsl/#vector-access-expr')
  .desc('Tests order of evaluation of member accessor expressions.')
  .paramsSimple([
    {
      name: 'Vec',
      _body: 'return vec3(mul(&a, 2)).x - vec3(mul(&a, 3)).x;',
      _result: 4 - 12,
    },
    {
      name: 'Struct',
      _body: 'return make_S(mul(&a, 2)).x - make_S(mul(&a, 3)).x;',
      _result: 4 - 12,
    },
  ])
  .fn(t => run(t, t.params._body, t.params._result));

function run(t, body, result) {
  // WGSL source
  const source =
    common_source +
    `
fn test_body() -> i32 {
  ${body}
}

@group(0) @binding(0) var<storage, read_write> output : i32;

@compute @workgroup_size(1)
fn main() {
  output = test_body();
}
`;

  // Construct a buffer to hold the results of the expression tests
  const outputBufferSize = 4; // result : i32
  const outputBuffer = t.device.createBuffer({
    size: outputBufferSize,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.STORAGE,
  });

  const module = t.device.createShaderModule({ code: source });
  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: { module, entryPoint: 'main' },
  });

  const group = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [{ binding: 0, resource: { buffer: outputBuffer } }],
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, group);
  pass.dispatchWorkgroups(1);
  pass.end();

  t.queue.submit([encoder.finish()]);

  const checkExpectation = outputData => {
    const output = TypeI32.read(outputData, 0);
    const got = output.value;
    const expected = result;
    if (got !== expected) {
      return new Error(`returned: ${got}, expected: ${expected}`);
    }
    return undefined;
  };

  t.expectGPUBufferValuesPassCheck(outputBuffer, checkExpectation, {
    type: Uint8Array,
    typedLength: outputBufferSize,
  });
}
