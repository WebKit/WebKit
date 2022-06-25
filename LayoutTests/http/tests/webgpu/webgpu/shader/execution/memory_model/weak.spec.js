/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests for properties of the WebGPU memory model involving two memory locations.
Specifically, the acquire/release ordering provided by WebGPU's barriers can be used to disallow
weak behaviors in several classic memory model litmus tests.`;
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
  numMemLocations: 2,
  numReadOutputs: 2,
  numBehaviors: 4,
};

const workgroupMemoryMessagePassingTestCode = `
  atomicStore(&wg_test_locations[x_0], 1u);
  workgroupBarrier();
  atomicStore(&wg_test_locations[y_0], 1u);
  let r0 = atomicLoad(&wg_test_locations[y_1]);
  workgroupBarrier();
  let r1 = atomicLoad(&wg_test_locations[x_1]);
  atomicStore(&results.value[shuffled_workgroup * workgroupXSize + id_1].r0, r0);
  atomicStore(&results.value[shuffled_workgroup * workgroupXSize + id_1].r1, r1);
`;

const storageMemoryMessagePassingTestCode = `
  atomicStore(&test_locations.value[x_0], 1u);
  storageBarrier();
  atomicStore(&test_locations.value[y_0], 1u);
  let r0 = atomicLoad(&test_locations.value[y_1]);
  storageBarrier();
  let r1 = atomicLoad(&test_locations.value[x_1]);
  atomicStore(&results.value[shuffled_workgroup * u32(workgroupXSize) + id_1].r0, r0);
  atomicStore(&results.value[shuffled_workgroup * u32(workgroupXSize) + id_1].r1, r1);
`;

g.test('message_passing_workgroup_memory')
  .desc(
    `Checks whether two reads on one thread can observe two writes in another thread in a way
    that is inconsistent with sequential consistency. In the message passing litmus test, one
    thread writes the value 1 to some location x and then 1 to some location y. The second thread
    reads y and then x. If the second thread reads y == 1 and x == 0, then sequential consistency
    has not been respected. The acquire/release semantics of WebGPU's barrier functions should disallow
    this behavior within a workgroup.
    `
  )
  .paramsSimple([
    { memType: MemoryType.AtomicWorkgroupClass, _testCode: workgroupMemoryMessagePassingTestCode },
    { memType: MemoryType.AtomicStorageClass, _testCode: storageMemoryMessagePassingTestCode },
  ])
  .fn(async t => {
    const testShader = buildTestShader(
      t.params._testCode,
      t.params.memType,
      TestType.IntraWorkgroup
    );

    const messagePassingResultShader = buildResultShader(
      `
      if ((r0 == 0u && r1 == 0u)) {
        atomicAdd(&test_results.seq0, 1u);
      } else if ((r0 == 1u && r1 == 1u)) {
        atomicAdd(&test_results.seq1, 1u);
      } else if ((r0 == 0u && r1 == 1u)) {
        atomicAdd(&test_results.interleaved, 1u);
      } else if ((r0 == 1u && r1 == 0u)) {
        atomicAdd(&test_results.weak, 1u);
      }
      `,
      TestType.IntraWorkgroup,
      ResultType.FourBehavior
    );

    const memModelTester = new MemoryModelTester(
      t,
      memoryModelTestParams,
      testShader,
      messagePassingResultShader
    );

    await memModelTester.run(20, 3);
  });
