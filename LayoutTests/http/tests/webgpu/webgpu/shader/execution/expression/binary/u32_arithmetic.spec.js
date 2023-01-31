/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the u32 arithmetic binary expression operations
`;
import { makeTestGroup } from '../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../gpu_test.js';
import { TypeU32, TypeVec } from '../../../../util/conversion.js';
import { fullU32Range, sparseU32Range, vectorU32Range } from '../../../../util/math.js';
import { makeCaseCache } from '../case_cache.js';
import {
  allInputSources,
  generateBinaryToU32Cases,
  generateU32VectorBinaryToVectorCases,
  generateVectorU32BinaryToVectorCases,
  run,
} from '../expression.js';

import { binary } from './binary.js';

export const g = makeTestGroup(GPUTest);

export const d = makeCaseCache('binary/u32_arithmetic', {
  addition: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      return x + y;
    });
  },
  subtraction: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      return x - y;
    });
  },
  multiplication: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      return Math.imul(x, y);
    });
  },
  division_non_const: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      if (y === 0) {
        return x;
      }
      return x / y;
    });
  },
  division_const: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      if (y === 0) {
        return undefined;
      }
      return x / y;
    });
  },
  remainder_non_const: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      if (y === 0) {
        return 0;
      }
      return x % y;
    });
  },
  remainder_const: () => {
    return generateBinaryToU32Cases(fullU32Range(), fullU32Range(), (x, y) => {
      if (y === 0) {
        return undefined;
      }
      return x % y;
    });
  },
  multiplication_scalar_vector2: () => {
    return generateU32VectorBinaryToVectorCases(sparseU32Range(), vectorU32Range(2), (x, y) => {
      return Math.imul(x, y);
    });
  },
  multiplication_scalar_vector3: () => {
    return generateU32VectorBinaryToVectorCases(sparseU32Range(), vectorU32Range(3), (x, y) => {
      return Math.imul(x, y);
    });
  },
  multiplication_scalar_vector4: () => {
    return generateU32VectorBinaryToVectorCases(sparseU32Range(), vectorU32Range(4), (x, y) => {
      return Math.imul(x, y);
    });
  },
  multiplication_vector2_scalar: () => {
    return generateVectorU32BinaryToVectorCases(vectorU32Range(2), sparseU32Range(), (x, y) => {
      return Math.imul(x, y);
    });
  },
  multiplication_vector3_scalar: () => {
    return generateVectorU32BinaryToVectorCases(vectorU32Range(3), sparseU32Range(), (x, y) => {
      return Math.imul(x, y);
    });
  },
  multiplication_vector4_scalar: () => {
    return generateVectorU32BinaryToVectorCases(vectorU32Range(4), sparseU32Range(), (x, y) => {
      return Math.imul(x, y);
    });
  },
});

g.test('addition')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x + y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .fn(async t => {
    const cases = await d.get('addition');
    await run(t, binary('+'), [TypeU32, TypeU32], TypeU32, t.params, cases);
  });

g.test('subtraction')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x - y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .fn(async t => {
    const cases = await d.get('subtraction');
    await run(t, binary('-'), [TypeU32, TypeU32], TypeU32, t.params, cases);
  });

g.test('multiplication')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x * y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .fn(async t => {
    const cases = await d.get('multiplication');
    await run(t, binary('*'), [TypeU32, TypeU32], TypeU32, t.params, cases);
  });

g.test('division')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x / y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'division_const' : 'division_non_const'
    );

    await run(t, binary('/'), [TypeU32, TypeU32], TypeU32, t.params, cases);
  });

g.test('remainder')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x % y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize', [undefined, 2, 3, 4]))
  .fn(async t => {
    const cases = await d.get(
      t.params.inputSource === 'const' ? 'remainder_const' : 'remainder_non_const'
    );

    await run(t, binary('%'), [TypeU32, TypeU32], TypeU32, t.params, cases);
  });

g.test('multiplication_scalar_vector')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x * y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize_rhs', [2, 3, 4]))
  .fn(async t => {
    const vec_size = t.params.vectorize_rhs;
    const vec_type = TypeVec(vec_size, TypeU32);
    const cases = await d.get(`multiplication_scalar_vector${vec_size}`);
    await run(t, binary('*'), [TypeU32, vec_type], vec_type, t.params, cases);
  });

g.test('multiplication_vector_scalar')
  .specURL('https://www.w3.org/TR/WGSL/#arithmetic-expr')
  .desc(
    `
Expression: x * y
`
  )
  .params(u => u.combine('inputSource', allInputSources).combine('vectorize_lhs', [2, 3, 4]))
  .fn(async t => {
    const vec_size = t.params.vectorize_lhs;
    const vec_type = TypeVec(vec_size, TypeU32);
    const cases = await d.get(`multiplication_vector${vec_size}_scalar`);
    await run(t, binary('*'), [vec_type, TypeU32], vec_type, t.params, cases);
  });
