/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { compare } from '../../../util/compare.js';
import { ScalarType, TypeVec, TypeU32, Vector, VectorType } from '../../../util/conversion.js';

// Helper for converting Values to Comparators.
function toComparator(input) {
  if (input.type !== undefined) {
    return (got, cmpFloats) => compare(got, input, cmpFloats);
  }
  return input;
}

/** Case is a single expression test case. */

// Helper for returning the WGSL storage type for the given Type.
function storageType(ty) {
  if (ty instanceof ScalarType) {
    if (ty.kind === 'bool') {
      return TypeU32;
    }
  }
  if (ty instanceof VectorType) {
    return TypeVec(ty.width, storageType(ty.elementType));
  }
  return ty;
}

// Helper for converting a value of the type 'ty' from the storage type.
function fromStorage(ty, expr) {
  if (ty instanceof ScalarType) {
    if (ty.kind === 'bool') {
      return `${expr} != 0u`;
    }
  }
  if (ty instanceof VectorType) {
    if (ty.elementType.kind === 'bool') {
      return `${expr} != vec${ty.width}<u32>(0u)`;
    }
  }
  return expr;
}

// Helper for converting a value of the type 'ty' to the storage type.
function toStorage(ty, expr) {
  if (ty instanceof ScalarType) {
    if (ty.kind === 'bool') {
      return `select(0u, 1u, ${expr})`;
    }
  }
  if (ty instanceof VectorType) {
    if (ty.elementType.kind === 'bool') {
      return `select(vec${ty.width}<u32>(0u), vec${ty.width}<u32>(1u), ${expr})`;
    }
  }
  return expr;
}

// Currently all values are packed into buffers of 16 byte strides
const kValueStride = 16;

// ExpressionBuilder returns the WGSL used to test an expression.

/**
 * Runs the list of expression tests, possibly splitting the tests into multiple
 * dispatches to keep the input data within the buffer binding limits.
 * run() will pack the scalar test cases into smaller set of vectorized tests
 * if `cfg.vectorize` is defined.
 * @param t the GPUTest
 * @param expressionBuilder the expression builder function
 * @param parameterTypes the list of expression parameter types
 * @param returnType the return type for the expression overload
 * @param cfg test configuration values
 * @param cases list of test cases
 */
export function run(
  t,
  expressionBuilder,
  parameterTypes,
  returnType,
  cfg = { storageClass: 'storage_r' },
  cases
) {
  const cmpFloats = cfg.cmpFloats !== undefined ? cfg.cmpFloats : (got, expect) => got === expect;

  // If the 'vectorize' config option was provided, pack the cases into vectors.
  if (cfg.vectorize !== undefined) {
    const packed = packScalarsToVector(parameterTypes, returnType, cases, cfg.vectorize);
    cases = packed.cases;
    parameterTypes = packed.parameterTypes;
    returnType = packed.returnType;
  }

  // The size of the input buffer max exceed the maximum buffer binding size,
  // so chunk the tests up into batches that fit into the limits.
  const maxInputSize =
    cfg.storageClass === 'uniform'
      ? t.device.limits.maxUniformBufferBindingSize
      : t.device.limits.maxStorageBufferBindingSize;
  const casesPerBatch = Math.floor(maxInputSize / (parameterTypes.length * kValueStride));
  for (let i = 0; i < cases.length; i += casesPerBatch) {
    const batchCases = cases.slice(i, Math.min(i + casesPerBatch, cases.length));
    runBatch(
      t,
      expressionBuilder,
      parameterTypes,
      returnType,
      batchCases,
      cfg.storageClass,
      cmpFloats
    );
  }
}

/**
 * Runs the list of expression tests. The input data must fit within the buffer
 * binding limits of the given storageClass.
 * @param t the GPUTest
 * @param expressionBuilder the expression builder function
 * @param parameterTypes the list of expression parameter types
 * @param returnType the return type for the expression overload
 * @param cases list of test cases that fit within the binding limits of the device
 * @param storageClass the storage class to use for the input buffer
 * @param cmpFloats the method to compare floating point numbers
 */
function runBatch(
  t,
  expressionBuilder,
  parameterTypes,
  returnType,
  cases,
  storageClass,
  cmpFloats
) {
  // returns the WGSL expression to load the ith parameter of the given type from the input buffer
  const paramExpr = (ty, i) => fromStorage(ty, `inputs[i].param${i}`);

  // resolves to the expression that calls the builtin
  const expr = toStorage(returnType, expressionBuilder(parameterTypes.map(paramExpr)));

  const storage = storageClass === 'storage_r' ? 'read' : 'read_write';

  // the full WGSL shader source
  const source = `
struct Input {
${parameterTypes
  .map((ty, i) => `  @size(${kValueStride}) param${i} : ${storageType(ty)},`)
  .join('\n')}
};

struct Output {
  @size(${kValueStride}) value : ${storageType(returnType)}
};

@group(0) @binding(0)
${
  storageClass === 'uniform'
    ? `var<uniform> inputs : array<Input, ${cases.length}>;`
    : `var<storage, ${storage}> inputs : array<Input, ${cases.length}>;`
}
@group(0) @binding(1) var<storage, write> outputs : array<Output, ${cases.length}>;

@stage(compute) @workgroup_size(1)
fn main() {
  for(var i = 0; i < ${cases.length}; i = i + 1) {
    outputs[i].value = ${expr};
  }
}
`;
  const inputSize = cases.length * parameterTypes.length * kValueStride;

  // Holds all the parameter values for all cases
  const inputData = new Uint8Array(inputSize);

  // Pack all the input parameter values into the inputData buffer
  {
    const caseStride = kValueStride * parameterTypes.length;
    for (let caseIdx = 0; caseIdx < cases.length; caseIdx++) {
      const caseBase = caseIdx * caseStride;
      for (let paramIdx = 0; paramIdx < parameterTypes.length; paramIdx++) {
        const offset = caseBase + paramIdx * kValueStride;
        const params = cases[caseIdx].input;
        if (params instanceof Array) {
          params[paramIdx].copyTo(inputData, offset);
        } else {
          params.copyTo(inputData, offset);
        }
      }
    }
  }
  const inputBuffer = t.makeBufferWithContents(
    inputData,
    GPUBufferUsage.COPY_SRC |
      (storageClass === 'uniform' ? GPUBufferUsage.UNIFORM : GPUBufferUsage.STORAGE)
  );

  // Construct a buffer to hold the results of the expression tests
  const outputBufferSize = cases.length * kValueStride;
  const outputBuffer = t.device.createBuffer({
    size: outputBufferSize,
    usage: GPUBufferUsage.COPY_SRC | GPUBufferUsage.COPY_DST | GPUBufferUsage.STORAGE,
  });

  const module = t.device.createShaderModule({ code: source });
  const pipeline = t.device.createComputePipeline({
    compute: { module, entryPoint: 'main' },
  });

  const group = t.device.createBindGroup({
    layout: pipeline.getBindGroupLayout(0),
    entries: [
      { binding: 0, resource: { buffer: inputBuffer } },
      { binding: 1, resource: { buffer: outputBuffer } },
    ],
  });

  const encoder = t.device.createCommandEncoder();
  const pass = encoder.beginComputePass();
  pass.setPipeline(pipeline);
  pass.setBindGroup(0, group);
  pass.dispatch(1);
  pass.end();

  t.queue.submit([encoder.finish()]);

  const checkExpectation = outputData => {
    // Read the outputs from the output buffer
    const outputs = new Array(cases.length);
    for (let i = 0; i < cases.length; i++) {
      outputs[i] = returnType.read(outputData, i * kValueStride);
    }

    // The list of expectation failures
    const errs = [];

    // For each case...
    for (let caseIdx = 0; caseIdx < cases.length; caseIdx++) {
      const c = cases[caseIdx];
      const got = outputs[caseIdx];
      const cmp = toComparator(c.expected)(got, cmpFloats);
      if (!cmp.matched) {
        errs.push(`(${c.input instanceof Array ? c.input.join(', ') : c.input})
    returned: ${cmp.got}
    expected: ${cmp.expected}`);
      }
    }

    return errs.length > 0 ? new Error(errs.join('\n\n')) : undefined;
  };

  t.expectGPUBufferValuesPassCheck(outputBuffer, checkExpectation, {
    type: Uint8Array,
    typedLength: outputBufferSize,
  });
}

/**
 * Packs a list of scalar test cases into a smaller list of vector cases.
 * Requires that all parameters of the expression overload are of a scalar type,
 * and the return type of the expression overload is also a scalar type.
 * If `cases.length` is not a multiple of `vectorWidth`, then the last scalar
 * test case value is repeated to fill the vector value.
 */
function packScalarsToVector(parameterTypes, returnType, cases, vectorWidth) {
  // Validate that the parameters and return type are all vectorizable
  for (let i = 0; i < parameterTypes.length; i++) {
    const ty = parameterTypes[i];
    if (!(ty instanceof ScalarType)) {
      throw new Error(
        `packScalarsToVector() can only be used on scalar parameter types, but the ${i}'th parameter type is a ${ty}'`
      );
    }
  }
  if (!(returnType instanceof ScalarType)) {
    throw new Error(
      `packScalarsToVector() can only be used with a scalar return type, but the return type is a ${returnType}'`
    );
  }

  const packedCases = [];
  const packedParameterTypes = parameterTypes.map(p => TypeVec(vectorWidth, p));
  const packedReturnType = new VectorType(vectorWidth, returnType);

  const clampCaseIdx = idx => Math.min(idx, cases.length - 1);

  let caseIdx = 0;
  while (caseIdx < cases.length) {
    // Construct the vectorized inputs from the scalar cases
    const packedInputs = new Array(parameterTypes.length);
    for (let paramIdx = 0; paramIdx < parameterTypes.length; paramIdx++) {
      const inputElements = new Array(vectorWidth);
      for (let i = 0; i < vectorWidth; i++) {
        const input = cases[clampCaseIdx(caseIdx + i)].input;
        inputElements[i] = input instanceof Array ? input[paramIdx] : input;
      }
      packedInputs[paramIdx] = new Vector(inputElements);
    }

    // Gather the comparators for the packed cases
    const comparators = new Array(vectorWidth);
    for (let i = 0; i < vectorWidth; i++) {
      comparators[i] = toComparator(cases[clampCaseIdx(caseIdx + i)].expected);
    }
    const packedComparator = (got, cmpFloats) => {
      let matched = true;
      const gElements = new Array(vectorWidth);
      const eElements = new Array(vectorWidth);
      for (let i = 0; i < vectorWidth; i++) {
        const d = comparators[i](got.elements[i], cmpFloats);
        matched = matched && d.matched;
        gElements[i] = d.got;
        eElements[i] = d.expected;
      }
      return {
        matched,
        got: `${packedReturnType}(${gElements.join(', ')})`,
        expected: `${packedReturnType}(${eElements.join(', ')})`,
      };
    };

    // Append the new packed case
    packedCases.push({ input: packedInputs, expected: packedComparator });
    caseIdx += vectorWidth;
  }

  return {
    cases: packedCases,
    parameterTypes: packedParameterTypes,
    returnType: packedReturnType,
  };
}
