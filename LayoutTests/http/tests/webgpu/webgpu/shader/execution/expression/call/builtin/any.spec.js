/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Execution Tests for the 'any' builtin function
`;
import { makeTestGroup } from '../../../../../../common/framework/test_group.js';
import { GPUTest } from '../../../../../gpu_test.js';
import {
  False,
  True,
  TypeBool,
  TypeVec,
  vec2,
  vec3,
  vec4,
} from '../../../../../util/conversion.js';
import { run } from '../../expression.js';

import { builtin } from './builtin.js';

export const g = makeTestGroup(GPUTest);

g.test('bool')
  .uniqueId('ac2b3a100379d70f')
  .specURL('https://www.w3.org/TR/2021/WD-WGSL-20210929/#logical-builtin-functions')
  .desc(
    `
vector any:
e: vecN<bool> any(e): bool Returns true if any component of e is true. (OpAny)

Please read the following guidelines before contributing:
https://github.com/gpuweb/cts/blob/main/docs/plan_autogen.md
`
  )
  .params(u =>
    u
      .combine('storageClass', ['uniform', 'storage_r', 'storage_rw'])
      .combine('overload', ['scalar', 'vec2', 'vec3', 'vec4'])
  )
  .fn(async t => {
    const overloads = {
      scalar: {
        type: TypeBool,
        cases: [
          { input: False, expected: False },
          { input: True, expected: True },
        ],
      },

      vec2: {
        type: TypeVec(2, TypeBool),
        cases: [
          { input: vec2(False, False), expected: False },
          { input: vec2(True, False), expected: True },
          { input: vec2(False, True), expected: True },
          { input: vec2(True, True), expected: True },
        ],
      },

      vec3: {
        type: TypeVec(3, TypeBool),
        cases: [
          { input: vec3(False, False, False), expected: False },
          { input: vec3(True, False, False), expected: True },
          { input: vec3(False, True, False), expected: True },
          { input: vec3(True, True, False), expected: True },
          { input: vec3(False, False, True), expected: True },
          { input: vec3(True, False, True), expected: True },
          { input: vec3(False, True, True), expected: True },
          { input: vec3(True, True, True), expected: True },
        ],
      },

      vec4: {
        type: TypeVec(4, TypeBool),
        cases: [
          { input: vec4(False, False, False, False), expected: False },
          { input: vec4(False, True, False, False), expected: True },
          { input: vec4(False, False, True, False), expected: True },
          { input: vec4(False, True, True, False), expected: True },
          { input: vec4(False, False, False, True), expected: True },
          { input: vec4(False, True, False, True), expected: True },
          { input: vec4(False, False, True, True), expected: True },
          { input: vec4(False, True, True, True), expected: True },
          { input: vec4(True, False, False, False), expected: True },
          { input: vec4(True, False, False, True), expected: True },
          { input: vec4(True, False, True, False), expected: True },
          { input: vec4(True, False, True, True), expected: True },
          { input: vec4(True, True, False, False), expected: True },
          { input: vec4(True, True, False, True), expected: True },
          { input: vec4(True, True, True, False), expected: True },
          { input: vec4(True, True, True, True), expected: True },
        ],
      },
    };

    const overload = overloads[t.params.overload];

    run(t, builtin('any'), [overload.type], TypeBool, t.params, overload.cases);
  });
