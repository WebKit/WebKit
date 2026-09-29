/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/export const description = `
Execution tests for vector assignment statements.
`;import { makeTestGroup } from '../../../../../common/framework/test_group.js';

import { Float16Array } from '../../../../../external/petamoriken/float16/float16.js';
import { AllFeaturesMaxLimitsGPUTest } from '../../../../gpu_test.js';
import { align } from '../../../../util/math.js';
import { runFlowControlTest } from '../../flow_control/harness.js';

export const g = makeTestGroup(AllFeaturesMaxLimitsGPUTest);









/** Mapping from component name to corresponding element index. */
const kComponentIndices = {
  x: 0,
  y: 1,
  z: 2,
  w: 3,
  r: 0,
  g: 1,
  b: 2,
  a: 3
};

const kComponentSets = {
  xyzw: ['x', 'y', 'z', 'w'],
  rgba: ['r', 'g', 'b', 'a']
};

const kCompoundOps = {
  add: '+=',
  mul: '*='
};

/**
 * Parameters for running a vector element write execution test.
 */












/**
 * Options for executing a vector compute shader and verifying output.
 */








/**
 * Builds the compute pipeline, initializes input and output buffers, dispatches
 * the compute pass, and verifies the output buffer values against expected values.
 */
function runVectorComputeAndVerify(t, options) {
  const pipeline = t.device.createComputePipeline({
    layout: 'auto',
    compute: {
      module: t.device.createShaderModule({ code: options.wgsl }),
      entryPoint: 'main'
    }
  });

  const inputBuffer = t.createBufferTracked({
    size: 64,
    usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
  });
  t.queue.writeBuffer(inputBuffer, 0, options.hostBuffer);

  const outputBuffer = t.createBufferTracked({
    size: 64,
    usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_SRC
  });

  const bindGroupEntries = [
  { binding: 0, resource: { buffer: inputBuffer } },
  { binding: 1, resource: { buffer: outputBuffer } }];


  if (options.addressSpace === 'storage') {
    const storageBuffer = t.createBufferTracked({
      size: 64,
      usage: GPUBufferUsage.STORAGE | GPUBufferUsage.COPY_DST
    });
    bindGroupEntries.push({ binding: 2, resource: { buffer: storageBuffer } });
  }

  const bindGroup = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: bindGroupEntries
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, bindGroup);
  pass.dispatchWorkgroups(1);
  pass.end();
  t.queue.submit([encoder.finish()]);

  let outputArrayConstructor;
  switch (options.elementType) {
    case 'u32':
    case 'bool':
      outputArrayConstructor = Uint32Array;
      break;
    case 'i32':
      outputArrayConstructor = Int32Array;
      break;
    case 'f32':
      outputArrayConstructor = Float32Array;
      break;
    case 'f16':
      outputArrayConstructor = Float16Array;
      break;
  }

  t.expectGPUBufferValuesEqual(outputBuffer, new outputArrayConstructor(options.expectedValues));
}

/**
 * Builds, runs, and checks the output of a vector element assignment test.
 *
 * This function tests that assigning to a single element of a vector variable
 * (using a component accessor, a constant index, or a dynamic index) in various address spaces
 * correctly updates that element while preserving the values of all other untouched elements.
 *
 * @param t The test fixture object.
 * @param args The test configuration arguments.
 */
function runVectorElementWriteTest(t, args) {
  // Check whether optional device features are required.
  if (args.elementType === 'f16') {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  // Determine the target element index being updated.
  let targetIndex;
  if (args.access.kind === 'component') {
    targetIndex = kComponentIndices[args.access.component];
  } else {
    targetIndex = args.access.index;
  }

  // Populate distinct initial values and a replacement value so that modifying
  // any element or failing to modify is clearly detectable.
  let initialValues;
  let replacementValue;

  switch (args.elementType) {
    case 'bool':
      initialValues = [0, 0, 0, 0].slice(0, args.width);
      replacementValue = 1;
      break;
    case 'i32':
      initialValues = [-10, -20, -30, -40].slice(0, args.width);
      replacementValue = -5;
      break;
    case 'u32':
      initialValues = [10, 20, 30, 40].slice(0, args.width);
      replacementValue = 5;
      break;
    case 'f32':
    case 'f16':
      initialValues = [10.0, 20.0, 30.0, 40.0].slice(0, args.width);
      replacementValue = 2.5;
      break;
  }

  const expectedValues = [...initialValues];
  if (args.compoundOp === '+=') {
    expectedValues[targetIndex] += replacementValue;
  } else if (args.compoundOp === '*=') {
    expectedValues[targetIndex] *= replacementValue;
  } else {
    expectedValues[targetIndex] = replacementValue;
  }

  const storageElemType = args.elementType === 'bool' ? 'u32' : args.elementType;
  const outputElemType = args.elementType === 'bool' ? 'u32' : args.elementType;
  const vecType = `vec${args.width}<${args.elementType}>`;
  const storageVecType = `vec${args.width}<${storageElemType}>`;

  const varRef = args.addressSpace === 'storage' ? 'storage_buffer.v' : 'v';
  const target = args.memoryView === 'ptr' ? '(*ptr)' : varRef;
  const ptrDecl = args.memoryView === 'ptr' ? `let ptr = &${varRef};\n  ` : '';

  let lhs;
  let structIndexField = '';

  if (args.access.kind === 'component') {
    lhs = `${target}.${args.access.component}`;
  } else if (args.access.kind === 'constant_index') {
    lhs = `${target}[${args.access.index}]`;
  } else {
    structIndexField = `index : ${args.access.indexType},`;
    lhs = `${target}[inputs.index]`;
  }

  const rhs = args.elementType === 'bool' ? 'inputs.value != 0u' : 'inputs.value';
  const op = args.compoundOp ?? '=';
  const assignmentStatement = `${ptrDecl}${lhs} ${op} ${rhs};`;

  // Construct WGSL compute shader.
  const wgsl = `
${args.elementType === 'f16' ? 'enable f16;' : ''}

struct Inputs {
  initial_vector : ${storageVecType},
  value : ${storageElemType},
  ${structIndexField}
};

@group(0) @binding(0) var<uniform> inputs : Inputs;

struct Outputs {
  data : array<${outputElemType}>,
};

@group(0) @binding(1) var<storage, read_write> outputs : Outputs;

${
  args.addressSpace === 'storage' ?
  `struct Storage {
  v : ${storageVecType},
};
@group(0) @binding(2) var<storage, read_write> storage_buffer : Storage;` :
  ''
  }
${args.addressSpace === 'private' ? `var<private> v : ${vecType};` : ''}
${args.addressSpace === 'workgroup' ? `var<workgroup> v : ${vecType};` : ''}

@compute @workgroup_size(1)
fn main() {
  ${args.addressSpace === 'function' ? `var v : ${vecType};` : ''}
  ${
  args.addressSpace === 'storage' ?
  `storage_buffer.v = inputs.initial_vector;` :
  args.elementType === 'bool' ?
  `${varRef} = inputs.initial_vector != vec${args.width}<u32>(0u);` :
  `${varRef} = inputs.initial_vector;`
  }

  ${assignmentStatement}

  for (var i = 0u; i < ${args.width}u; i++) {
    outputs.data[i] = ${args.elementType === 'bool' ? `u32(${varRef}[i])` : `${varRef}[i]`};
  }
}
`;

  // Prepare input buffer data matching WGSL struct natural alignment.
  const hostBuffer = new ArrayBuffer(64);
  const elemBytes = args.elementType === 'f16' ? 2 : 4;
  const valueOffset = args.width * elemBytes;

  if (args.elementType === 'f16') {
    const f16Init = new Float16Array(hostBuffer, 0, args.width);
    for (let i = 0; i < args.width; i++) {
      f16Init[i] = initialValues[i];
    }
    const f16Val = new Float16Array(hostBuffer, valueOffset, 1);
    f16Val[0] = replacementValue;
  } else if (args.elementType === 'f32') {
    const f32Init = new Float32Array(hostBuffer, 0, args.width);
    for (let i = 0; i < args.width; i++) {
      f32Init[i] = initialValues[i];
    }
    const f32Val = new Float32Array(hostBuffer, valueOffset, 1);
    f32Val[0] = replacementValue;
  } else if (args.elementType === 'i32') {
    const i32Init = new Int32Array(hostBuffer, 0, args.width);
    for (let i = 0; i < args.width; i++) {
      i32Init[i] = initialValues[i];
    }
    const i32Val = new Int32Array(hostBuffer, valueOffset, 1);
    i32Val[0] = replacementValue;
  } else {
    const u32Init = new Uint32Array(hostBuffer, 0, args.width);
    for (let i = 0; i < args.width; i++) {
      u32Init[i] = initialValues[i];
    }
    const u32Val = new Uint32Array(hostBuffer, valueOffset, 1);
    u32Val[0] = replacementValue;
  }

  if (args.access.kind === 'dynamic_index') {
    const indexOffset = align((args.width + 1) * elemBytes, 4);
    if (args.access.indexType === 'i32') {
      new Int32Array(hostBuffer, indexOffset, 1)[0] = args.access.index;
    } else {
      new Uint32Array(hostBuffer, indexOffset, 1)[0] = args.access.index;
    }
  }

  runVectorComputeAndVerify(t, {
    wgsl,
    hostBuffer,
    addressSpace: args.addressSpace,
    elementType: args.elementType,
    expectedValues
  });
}

/**
 * Parameters for running a full vector assignment test.
 */








/**
 * Builds, runs, and checks the output of a full vector assignment test (v = rhs).
 *
 * @param t The test fixture object.
 * @param args The test configuration arguments.
 */
function runVectorFullAssignmentTest(t, args) {
  if (args.elementType === 'f16') {
    t.skipIfDeviceDoesNotHaveFeature('shader-f16');
  }

  let initialValues;
  let replacementValues;

  switch (args.elementType) {
    case 'bool':
      initialValues = [0, 0, 0, 0].slice(0, args.width);
      replacementValues = [1, 0, 1, 1].slice(0, args.width);
      break;
    case 'i32':
      initialValues = [-1, -2, -3, -4].slice(0, args.width);
      replacementValues = [-10, -20, -30, -40].slice(0, args.width);
      break;
    case 'u32':
      initialValues = [1, 2, 3, 4].slice(0, args.width);
      replacementValues = [10, 20, 30, 40].slice(0, args.width);
      break;
    case 'f32':
    case 'f16':
      initialValues = [1.0, 2.0, 3.0, 4.0].slice(0, args.width);
      replacementValues = [10.5, 20.5, 30.5, 40.5].slice(0, args.width);
      break;
  }

  const storageElemType = args.elementType === 'bool' ? 'u32' : args.elementType;
  const outputElemType = args.elementType === 'bool' ? 'u32' : args.elementType;
  const vecType = `vec${args.width}<${args.elementType}>`;
  const storageVecType = `vec${args.width}<${storageElemType}>`;

  const varRef = args.addressSpace === 'storage' ? 'storage_buffer.v' : 'v';
  const target = args.memoryView === 'ptr' ? '(*ptr)' : varRef;
  const ptrDecl = args.memoryView === 'ptr' ? `let ptr = &${varRef};\n  ` : '';

  const rhsLiterals =
  args.elementType === 'bool' ?
  replacementValues.map((v) => v ? 'true' : 'false').join(', ') :
  args.elementType === 'f16' ?
  replacementValues.map((v) => `${v}h`).join(', ') :
  args.elementType === 'f32' ?
  replacementValues.map((v) => `${v}f`).join(', ') :
  args.elementType === 'i32' ?
  replacementValues.map((v) => `${v}i`).join(', ') :
  replacementValues.map((v) => `${v}u`).join(', ');

  let rhsDecl = '';
  let rhsExpr = '';
  if (args.rhsSource === 'constructor') {
    rhsExpr = `${vecType}(${rhsLiterals})`;
  } else if (args.rhsSource === 'variable') {
    rhsDecl = `let rhs_val = ${vecType}(${rhsLiterals});\n  `;
    rhsExpr = 'rhs_val';
  } else {
    rhsExpr =
    args.elementType === 'bool' ?
    `inputs.replacement_vector != vec${args.width}<u32>(0u)` :
    `inputs.replacement_vector`;
  }

  const assignmentStatement = `${rhsDecl}${ptrDecl}${target} = ${rhsExpr};`;

  const wgsl = `
${args.elementType === 'f16' ? 'enable f16;' : ''}

struct Inputs {
  initial_vector : ${storageVecType},
  replacement_vector : ${storageVecType},
};

@group(0) @binding(0) var<uniform> inputs : Inputs;

struct Outputs {
  data : array<${outputElemType}>,
};

@group(0) @binding(1) var<storage, read_write> outputs : Outputs;

${
  args.addressSpace === 'storage' ?
  `struct Storage {
  v : ${storageVecType},
};
@group(0) @binding(2) var<storage, read_write> storage_buffer : Storage;` :
  ''
  }
${args.addressSpace === 'private' ? `var<private> v : ${vecType};` : ''}
${args.addressSpace === 'workgroup' ? `var<workgroup> v : ${vecType};` : ''}

@compute @workgroup_size(1)
fn main() {
  ${args.addressSpace === 'function' ? `var v : ${vecType};` : ''}
  ${
  args.addressSpace === 'storage' ?
  `storage_buffer.v = inputs.initial_vector;` :
  args.elementType === 'bool' ?
  `${varRef} = inputs.initial_vector != vec${args.width}<u32>(0u);` :
  `${varRef} = inputs.initial_vector;`
  }

  ${assignmentStatement}

  for (var i = 0u; i < ${args.width}u; i++) {
    outputs.data[i] = ${args.elementType === 'bool' ? `u32(${varRef}[i])` : `${varRef}[i]`};
  }
}
`;

  // Prepare input buffer data matching WGSL struct natural alignment.
  const hostBuffer = new ArrayBuffer(64);
  const elemBytes = args.elementType === 'f16' ? 2 : 4;
  const vecAlign = (args.width === 2 ? 2 : 4) * elemBytes;
  const replacementOffset = align(args.width * elemBytes, vecAlign);

  if (args.elementType === 'f16') {
    const f16Init = new Float16Array(hostBuffer, 0, args.width);
    const f16Replacement = new Float16Array(hostBuffer, replacementOffset, args.width);
    for (let i = 0; i < args.width; i++) {
      f16Init[i] = initialValues[i];
      f16Replacement[i] = replacementValues[i];
    }
  } else if (args.elementType === 'f32') {
    const f32Init = new Float32Array(hostBuffer, 0, args.width);
    const f32Replacement = new Float32Array(hostBuffer, replacementOffset, args.width);
    for (let i = 0; i < args.width; i++) {
      f32Init[i] = initialValues[i];
      f32Replacement[i] = replacementValues[i];
    }
  } else if (args.elementType === 'i32') {
    const i32Init = new Int32Array(hostBuffer, 0, args.width);
    const i32Replacement = new Int32Array(hostBuffer, replacementOffset, args.width);
    for (let i = 0; i < args.width; i++) {
      i32Init[i] = initialValues[i];
      i32Replacement[i] = replacementValues[i];
    }
  } else {
    const u32Init = new Uint32Array(hostBuffer, 0, args.width);
    const u32Replacement = new Uint32Array(hostBuffer, replacementOffset, args.width);
    for (let i = 0; i < args.width; i++) {
      u32Init[i] = initialValues[i];
      u32Replacement[i] = replacementValues[i];
    }
  }

  runVectorComputeAndVerify(t, {
    wgsl,
    hostBuffer,
    addressSpace: args.addressSpace,
    elementType: args.elementType,
    expectedValues: replacementValues
  });
}

g.test('vector_component').
desc('Tests writing to a vector component via component accessor (e.g. v.x = val).').
params((u) =>
u.
combine('elementType', ['bool', 'i32', 'u32', 'f32', 'f16']).
combine('width', [2, 3, 4]).
beginSubcases().
combine('address_space', ['function', 'private', 'workgroup', 'storage']).
filter((t) => t.address_space !== 'storage' || t.elementType !== 'bool').
combine('memory_view', ['ref', 'ptr']).
combine('component_set', ['xyzw', 'rgba']).
expand('component', (u) => kComponentSets[u.component_set].slice(0, u.width))
).
fn((t) => {
  runVectorElementWriteTest(t, {
    elementType: t.params.elementType,
    width: t.params.width,
    addressSpace: t.params.address_space,
    memoryView: t.params.memory_view,
    access: {
      kind: 'component',
      component: t.params.component
    }
  });
});

g.test('vector_constant_index').
desc('Tests writing to a vector element via constant index (e.g. v[0] = val).').
params((u) =>
u.
combine('elementType', ['bool', 'i32', 'u32', 'f32', 'f16']).
combine('width', [2, 3, 4]).
beginSubcases().
combine('address_space', ['function', 'private', 'workgroup', 'storage']).
filter((t) => t.address_space !== 'storage' || t.elementType !== 'bool').
combine('memory_view', ['ref', 'ptr']).
expand('index', (u) => [...Array(u.width).keys()])
).
fn((t) => {
  runVectorElementWriteTest(t, {
    elementType: t.params.elementType,
    width: t.params.width,
    addressSpace: t.params.address_space,
    memoryView: t.params.memory_view,
    access: {
      kind: 'constant_index',
      index: t.params.index
    }
  });
});

g.test('vector_dynamic_index').
desc(
  'Tests writing to a vector element via dynamic index loaded from uniform buffer (e.g. v[index] = val).'
).
params((u) =>
u.
combine('elementType', ['bool', 'i32', 'u32', 'f32', 'f16']).
combine('width', [2, 3, 4]).
combine('indexType', ['i32', 'u32']).
beginSubcases().
combine('address_space', ['function', 'private', 'workgroup', 'storage']).
filter((t) => t.address_space !== 'storage' || t.elementType !== 'bool').
combine('memory_view', ['ref', 'ptr']).
expand('index', (u) => [...Array(u.width).keys()])
).
fn((t) => {
  runVectorElementWriteTest(t, {
    elementType: t.params.elementType,
    width: t.params.width,
    addressSpace: t.params.address_space,
    memoryView: t.params.memory_view,
    access: {
      kind: 'dynamic_index',
      index: t.params.index,
      indexType: t.params.indexType
    }
  });
});

g.test('vector_element_compound').
desc('Tests compound assignment to a vector element (e.g. v.x += val, v[i] += val).').
params((u) =>
u.
combine('elementType', ['i32', 'u32', 'f32', 'f16']).
combine('width', [2, 3, 4]).
combine('op', ['add', 'mul']).
beginSubcases().
combine('access', ['component', 'constant_index', 'dynamic_index']).
combine('address_space', ['function', 'private', 'workgroup', 'storage']).
combine('memory_view', ['ref', 'ptr'])
).
fn((t) => {
  const targetIdx = 1;
  let access;
  if (t.params.access === 'component') {
    access = { kind: 'component', component: 'y' };
  } else if (t.params.access === 'constant_index') {
    access = { kind: 'constant_index', index: targetIdx };
  } else {
    access = { kind: 'dynamic_index', index: targetIdx, indexType: 'u32' };
  }

  runVectorElementWriteTest(t, {
    elementType: t.params.elementType,
    width: t.params.width,
    addressSpace: t.params.address_space,
    memoryView: t.params.memory_view,
    access,
    compoundOp: kCompoundOps[t.params.op]
  });
});

g.test('vector_full_assignment').
desc('Tests whole-vector assignment (e.g. v = rhs).').
params((u) =>
u.
combine('elementType', ['bool', 'i32', 'u32', 'f32', 'f16']).
combine('width', [2, 3, 4]).
combine('rhsSource', ['constructor', 'variable', 'uniform']).
beginSubcases().
combine('address_space', ['function', 'private', 'workgroup', 'storage']).
filter((t) => t.address_space !== 'storage' || t.elementType !== 'bool').
combine('memory_view', ['ref', 'ptr'])
).
fn((t) => {
  runVectorFullAssignmentTest(t, {
    elementType: t.params.elementType,
    width: t.params.width,
    addressSpace: t.params.address_space,
    memoryView: t.params.memory_view,
    rhsSource: t.params.rhsSource
  });
});

g.test('indexed_assignment_eval_order').
desc(
  'Tests that the lhs components and rhs of an indexed vector assignment are evaluated left-to-right.'
).
fn((t) => {
  runFlowControlTest(t, (f) => ({
    entrypoint: `
  arr[0] = vec4u(1, 10, 1, 1);
  ${f.expect_order(0)}
  arr[foo()][bar()] = baz();
  ${f.expect_order(4)}
  if (all(arr[0] == vec4u(1, 99, 1, 1))) {
    ${f.expect_order(5)}
  } else {
    ${f.expect_not_reached()}
  }
`,
    extra: `
var<private> arr : array<vec4u, 1>;
fn foo() -> u32 {
  ${f.expect_order(1)}
  return 0;
}
fn bar() -> u32 {
  ${f.expect_order(2)}
  return 1;
}
fn baz() -> u32 {
  ${f.expect_order(3)}
  return 99;
}
`
  }));
});

g.test('indexed_compound_assignment_eval_order').
desc(
  'Tests that compound assignment on a vector element evaluates left-to-right and evaluates the index expression only once.'
).
fn((t) => {
  runFlowControlTest(t, (f) => ({
    entrypoint: `
  arr[0] = vec4u(1, 10, 1, 1);
  ${f.expect_order(0)}
  arr[foo()][bar()] += baz();
  ${f.expect_order(4)}
  if (all(arr[0] == vec4u(1, 15, 1, 1))) {
    ${f.expect_order(5)}
  } else {
    ${f.expect_not_reached()}
  }
`,
    extra: `
var<private> arr : array<vec4u, 1>;
fn foo() -> u32 {
  ${f.expect_order(1)}
  return 0;
}
fn bar() -> u32 {
  ${f.expect_order(2)}
  return 1;
}
fn baz() -> u32 {
  ${f.expect_order(3)}
  return 5;
}
`
  }));
});