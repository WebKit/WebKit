// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import postcss from "postcss";
import nested from "postcss-nested";
import autoprefixer from "autoprefixer";

const nestedRules = `
.phone {
  &_title {
      width: 500px;
      @media (max-width: 500px) {
          width: auto;
      }
      body.is_dark & {
          color: white;
      }
  }
  img {
      display: block;
  }
}`;

const payloads = [
  {
    name: "bootstrap-5.3.7.css",
    options: { from: `third_party/bootstrap-5.3.7.css`, map: false },
  },
  {
    name: "foundation-6.9.0.css",
    options: { from: `third_party/foundation-6.9.0.css`, map: false },
  },
  {
    name: "angular-material-20.1.6.css",
    options: { from: `third_party/angular-material-20.1.6.css`, map: false },
  },
];

export function runTest(fileData) {
  const cleaner = postcss([
    autoprefixer({ add: false, overrideBrowserslist: [] }),
  ]);
  const processor = postcss([autoprefixer, nested]);

  const testData = payloads.map(({ name, options }) => {
    // Clean prefixes.
    const source = fileData[name];
    // Add some nested rules.
    const css = cleaner.process(source).css + nestedRules;

    return {
      payload: css,
      options,
    };
  });

  return testData.map(
    ({ payload, options }) => processor.process(payload, options).css
  );
}
