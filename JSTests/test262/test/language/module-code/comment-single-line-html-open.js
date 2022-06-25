// Copyright (C) 2016 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    HTML-like comments are not available in module code
    (SingleLineHTMLOpenComment)
esid: sec-html-like-comments
negative:
  phase: parse
  type: SyntaxError
flags: [module]
---*/

$DONOTEVALUATE();

<!--
