/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
 * Copyright 2025 Google LLC
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

import path from "path";
import { fileURLToPath } from "url";
import UnicodeEscapePlugin from "@dapplets/unicode-escape-webpack-plugin";
import { LicenseWebpackPlugin } from "license-webpack-plugin";


const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

function config({ filename, minify }) {
  return {
    entry: "./src/test.mjs",
    mode: "production",
    devtool: "source-map",
    target: "web",
    output: {
      path: path.resolve(__dirname, "dist"),
      filename: filename,
      library: {
        name: "ValidatorJSBenchmark",
        type: "globalThis",
      },
      libraryTarget: "assign",
      chunkFormat: "commonjs",
    },
    plugins: [
      new UnicodeEscapePlugin(),
      new LicenseWebpackPlugin({
        perChunkOutput: true, 
        outputFilename: "LICENSE.txt",
      })
    ],
    optimization: {
      minimize: minify,
    },
  };
}

export default [
  config({ filename: "bundle.es6.min.js", minify: true }),
  config({ filename: "bundle.es6.js", minify: false }),
];
