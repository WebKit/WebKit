// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as babylon from "babylon";

const payloads = [
  {
    name: "jquery-3.7.1.js",
    options: { sourceType: "script" },
  },
  {
    name: "lodash.core-4.17.21.js",
    options: { sourceType: "script" },
  },
  {
    name: "preact-8.2.5.js",
    options: { sourceType: "script" },
  },
  {
    name: "redux-5.0.1.min.js",
    options: { sourceType: "module", plugins: ["objectRestSpread"] },
  },
  {
    name: "speedometer-es2015-test-2.0.js",
    options: { sourceType: "script" },
  },
  {
    name: "todomvc/react/app.jsx",
    options: { sourceType: "script", plugins: ["jsx"] },
  },
  {
    name: "todomvc/react/footer.jsx",
    options: { sourceType: "script", plugins: ["jsx"] },
  },
  {
    name: "todomvc/react/todoItem.jsx",
    options: { sourceType: "script", plugins: ["jsx"] },
  },
  {
    name: "underscore-1.13.7.js",
    options: { sourceType: "script" },
  },
  {
    name: "vue-3.5.18.runtime.esm-browser.js",
    options: { sourceType: "module" },
  },
];

export function runTest(fileData) {
  const testData = payloads.map(({ name, options }) => ({
    payload: fileData[name],
    name,
    options,
  }));

  return testData.map(({ payload, name, options }) => {
    babylon.parse(payload, options);
  });
}
