/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ export const description = `
Tests that all threads see a sequentially consistent view of the order of memory
accesses to a single memory location. Uses a parallel testing strategy along with stressing
threads to increase coverage of possible bugs.`;
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
  permuteSecond: 1,
  memStride: 4,
  aliasedMemory: true,
  numMemLocations: 1,
  numReadOutputs: 2,
  numBehaviors: 4,
};

const storageMemoryCorrTestCode = `
  atomicStore(&test_locations.value[x_0], 1u);
  let r0 = atomicLoad(&test_locations.value[x_1]);
  let r1 = atomicLoad(&test_locations.value[y_1]);
  atomicStore(&results.value[id_1].r0, r0);
  atomicStore(&results.value[id_1].r1, r1);
`;

const workgroupMemoryCorrTestCode = `
  atomicStore(&wg_test_locations[x_0], 1u);
  let r0 = atomicLoad(&wg_test_locations[x_1]);
  let r1 = atomicLoad(&wg_test_locations[y_1]);
  atomicStore(&results.value[shuffled_workgroup * workgroupXSize + id_1].r0, r0);
  atomicStore(&results.value[shuffled_workgroup * workgroupXSize + id_1].r1, r1);
`;

g.test('corr')
  .desc(
    `Ensures two reads on one thread cannot observe an inconsistent view of a write on a second thread.
     The first thread writes the value 1 some location x, and the second thread reads x twice in a row.
     If the first read returns 1 but the second read returns 0, then there has been a coherence violation.
    `
  )
  .paramsSimple([
    {
      memType: MemoryType.AtomicStorageClass,
      testType: TestType.InterWorkgroup,
      _testCode: storageMemoryCorrTestCode,
    },

    {
      memType: MemoryType.AtomicStorageClass,
      testType: TestType.IntraWorkgroup,
      _testCode: storageMemoryCorrTestCode,
    },

    {
      memType: MemoryType.AtomicWorkgroupClass,
      testType: TestType.IntraWorkgroup,
      _testCode: workgroupMemoryCorrTestCode,
    },
  ])
  .fn(async t => {
    const resultCode = `
      if ((r0 == 0u && r1 == 0u)) {
        atomicAdd(&test_results.seq0, 1u);
      } else if ((r0 == 1u && r1 == 1u)) {
        atomicAdd(&test_results.seq1, 1u);
      } else if ((r0 == 0u && r1 == 1u)) {
        atomicAdd(&test_results.interleaved, 1u);
      } else if ((r0 == 1u && r1 == 0u)) {
        atomicAdd(&test_results.weak, 1u);
      }
    `;

    const testShader = buildTestShader(t.params._testCode, t.params.memType, t.params.testType);
    const resultShader = buildResultShader(
      resultCode,
      TestType.InterWorkgroup,
      ResultType.FourBehavior
    );

    const memModelTester = new MemoryModelTester(
      t,
      memoryModelTestParams,
      testShader,
      resultShader
    );

    await memModelTester.run(20, 3);
  });
