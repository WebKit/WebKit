// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { rollup } from "rollup";

const payloads = [
  {
    name: "preact-8.2.5.js",
  },
];

export async function runTest(fileData) {
  const testData = payloads.map(({ name }) => ({
    payload: fileData[name],
    options: {
      input: `third_party/${name}`,
    },
  }));

  return await Promise.all(testData.map(({ options }) => rollup(options)));
}
