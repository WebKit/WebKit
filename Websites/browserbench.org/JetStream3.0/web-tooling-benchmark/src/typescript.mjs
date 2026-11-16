// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as ts from "typescript";

const payloads = [
  {
    // Compile typescript-angular.ts to ES3 (default)
    name: "todomvc/typescript-angular.ts",
    transpileOptions: {
      compilerOptions: {
        module: ts.ModuleKind.CommonJS,
        target: ts.ScriptTarget.ES3,
      },
    },
  },
  {
    // Compile typescript-angular.ts to ESNext (latest)
    name: "todomvc/typescript-angular.ts",
    transpileOptions: {
      compilerOptions: {
        module: ts.ModuleKind.CommonJS,
        target: ts.ScriptTarget.ESNext,
      },
    },
  },
];

export function runTest(fileData) {
  const testData = payloads.map(({ name, transpileOptions }) => ({
    input: fileData[name],
    transpileOptions,
  }));

  return testData.map(({ input, transpileOptions }) =>
    ts.transpileModule(input, transpileOptions)
  );
}
