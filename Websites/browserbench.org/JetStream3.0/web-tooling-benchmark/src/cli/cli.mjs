// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { getTarget } from "./flags-helper.mjs";
import fileData from "./file-data.mjs";

async function main() {
  const targets = getTarget();
  for (const target of targets) {
    console.log(`${target}:`);
    const then = performance.now();
    const benchmark = await import(`../${target}.mjs`);
    await benchmark.runTest(fileData);
    const duration = performance.now() - then;
    console.log(`   duration: ${duration.toFixed(2)}ms`);
  }
}

main();
