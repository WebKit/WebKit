// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { minify } from "terser";

const payloads = [
  {
    name: "speedometer-es2015-test-2.0.js",
    options: { compress: { passes: 1, sequences: false } },
  },
];

export async function runTest(fileData) {
  const testData = payloads.map(({ name, options }) => ({
    payload: fileData[name],
    options,
  }));

  return await Promise.all(
    testData.map(({ payload, options }) => minify(payload, options))
  );
}
