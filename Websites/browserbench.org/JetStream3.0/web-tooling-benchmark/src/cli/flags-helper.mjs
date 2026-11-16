// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import { parseArgs } from "node:util";

export const targetList = new Set([
  "acorn",
  "babel",
  "babel-minify",
  "babylon",
  "chai",
  "espree",
  "esprima-next",
  "lebab",
  "postcss",
  "prettier",
  "source-map",
  "terser",
  "typescript",
]);

function getOnlyFlag() {
  const options = {
    only: {
      type: "string",
    },
  };
  const { values } = parseArgs({ options });
  return values.only;
}

export function getTarget() {
  const onlyArg = getOnlyFlag();
  if (targetList.has(onlyArg)) {
    return [onlyArg];
  } else if (typeof ONLY != "undefined" && targetList.has(ONLY)) {
    return [ONLY];
  } else {
    return [...targetList];
  }
}
