/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for non-atomic memory synchronization within a workgroup in the presence of a WebGPU barrier`;
import { makeTestGroup } from '../../../../common/framework/test_group.js';
import { GPUTest } from '../../../gpu_test.js';

import {
  MemoryModelTester,
  buildTestShader,
  MemoryType,
  TestType,
  buildResultShader,
  ResultType,
} from './memory_model_setup.js';

export const g = makeTestGroup(GPUTest);

// A reasonable parameter set, determined heuristically.
const memoryModelTestParams = {
  workgroupSize: 256,
  testingWorkgroups: 512,
  maxWorkgroups: 1024,
  shufflePct: 100,
  barrierPct: 100,
  memStressPct: 100,
  memStressIterations: 1024,
  memStressStoreFirstPct: 50,
  memStressStoreSecondPct: 50,
  preStressPct: 100,
  preStressIterations: 1024,
  preStressStoreFirstPct: 50,
  preStressStoreSecondPct: 50,
  scratchMemorySize: 2048,
  stressLineSize: 64,
  stressTargetLines: 2,
  stressStrategyBalancePct: 50,
  permuteFirst: 109,
  permuteSecond: 419,
  memStride: 4,
  aliasedMemory: false,
  numMemLocations: 1,
  numReadOutputs: 1,
  numBehaviors: 2,
};

const storageMemoryBarrierStoreLoadTestCode = `
  test_locations.value[x_0] = 1u;
  workgroupBarrier();
  let r0 = test_locations.value[x_1];
  atomicStore(&results.value[shuffled_workgroup * workgroupXSize + id_1].r0, r0);
`;

const workgroupMemoryBarrierStoreLoadTestCode = `
  wg_test_locations[x_0] = 1u;
  workgroupBarrier();
  let r0 = wg_test_locations[x_1];
  atomicStore(&results.value[shuffled_workgroup * workgroupXSize + id_1].r0, r0);
`;

g.test('workgroup_barrier_store_load')
  .desc(
    `Checks whether the workgroup barrier properly synchronizes a non-atomic write and read on
    separate threads in the same workgroup. Within a workgroup, the barrier should force an invocation
    after the barrier to read a write from an invocation before the barrier.
    `
  )
  .paramsSimple([
    { memType: MemoryType.NonAtomicStorageClass, _testCode: storageMemoryBarrierStoreLoadTestCode },
    {
      memType: MemoryType.NonAtomicWorkgroupClass,
      _testCode: workgroupMemoryBarrierStoreLoadTestCode,
    },
  ])
  .fn(async t => {
    const resultCode = `
      if (r0 == 1u) {
        atomicAdd(&test_results.seq, 1u);
      } else if (r0 == 0u) {
        atomicAdd(&test_results.weak, 1u);
      }
    `;
    const testShader = buildTestShader(
      t.params._testCode,
      t.params.memType,
      TestType.IntraWorkgroup
    );

    const resultShader = buildResultShader(
      resultCode,
      TestType.IntraWorkgroup,
      ResultType.TwoBehavior
    );

    const memModelTester = new MemoryModelTester(
      t,
      memoryModelTestParams,
      testShader,
      resultShader
    );

    await memModelTester.run(20, 1);
  });
