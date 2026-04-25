// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import postcss from "./postcss.mjs";

it("postcss runs to completion", () => void postcss.fn());
