// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as lebab from "lebab";

const payloads = [
  {
    name: "preact-8.2.5.js",
    options: [
      "arg-rest",
      "arg-spread",
      "arrow",
      "class",
      "for-of",
      "let",
      "template",
      "includes",
      "obj-method",
      "obj-shorthand",
    ],
  },
];

export function runTest(fileData) {
  const testData = payloads.map(({ name, options }) => ({
    payload: fileData[name],
    options,
  }));

  return testData.map(({ payload, options }) =>
    lebab.transform(payload, options)
  );
}
