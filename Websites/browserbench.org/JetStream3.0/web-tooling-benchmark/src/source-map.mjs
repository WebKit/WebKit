// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as sourceMap from "source-map";

const payloads = [
  "lodash-4.17.4.min.js.map",
  "preact-10.27.1.min.module.js.map",
  "source-map.min-0.5.7.js.map",
  "underscore-1.13.7.min.js.map",
];

export async function runTest(fileData) {
  const testData = payloads.map((name) => fileData[name]);

  sourceMap.SourceMapConsumer.initialize({
    "lib/mappings.wasm": fileData["source-map/lib/mappings.wasm"],
  });

  for (const payload of testData) {
    // Parse the source map first...
    const smc = await new sourceMap.SourceMapConsumer(payload);
    // ...then serialize the parsed source map to a String.
    const smg = sourceMap.SourceMapGenerator.fromSourceMap(smc);

    // Create a SourceNode from the generated code and a SourceMapConsumer.
    const fswsm = await sourceMap.SourceNode.fromStringWithSourceMap(
      payload,
      smc
    );

    return [smg.toString(), fswsm.toString()];
  }
}
