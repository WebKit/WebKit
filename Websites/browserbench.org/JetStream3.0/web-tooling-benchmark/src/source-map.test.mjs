// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import sourceMap from "./source-map.mjs";

it("source-map runs to completion", () =>
  void sourceMap.fn());
