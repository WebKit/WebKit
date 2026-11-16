// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as acorn from "acorn";
import * as walk from "acorn-walk";

const payloads = [
  {
    name: "backbone-1.6.1.js",
    options: { ecmaVersion: 5, sourceType: "script" },
  },
  {
    name: "jquery-3.7.1.js",
    options: { ecmaVersion: 5, sourceType: "script" },
  },
  {
    name: "lodash.core-4.17.21.js",
    options: { ecmaVersion: 5, sourceType: "script" },
  },
  {
    name: "preact-8.2.5.js",
    options: { ecmaVersion: 5, sourceType: "script" },
  },
  {
    name: "redux-5.0.1.min.js",
    options: { ecmaVersion: "latest", sourceType: "module" },
  },
  {
    name: "speedometer-es2015-test-2.0.js",
    options: { ecmaVersion: 6, sourceType: "script" },
  },
  {
    name: "underscore-1.13.7.js",
    options: { ecmaVersion: 5, sourceType: "script" },
  },
  {
    name: "vue-3.5.18.runtime.esm-browser.js",
    options: { ecmaVersion: "latest", sourceType: "module" },
  },
];

export function runTest(fileData) {
  const testData = payloads.map(({ name, options }) => ({
    payload: fileData[name],
    name: name,
    options: Object.assign(options, { locations: true }, { ranges: true }),
  }));

  return testData.map(({ payload, name, options }) => {
    let count = 0;

    // Test the tokenizer by counting the resulting tokens.
    for (const token of acorn.tokenizer(payload, options)) {
      count++;
    }

    // Test the parser.
    const ast = acorn.parse(payload, options);

    // Test the AST walker.
    walk.full(ast, (node) => count++);
    return count;
  });
}
