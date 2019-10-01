// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The Date property "parse" has { DontEnum } attributes
esid: sec-date.parse
description: Checking DontEnum attribute
---*/

if (Date.propertyIsEnumerable('parse')) {
  $ERROR('#1: The Date.parse property has the attribute DontEnum');
}

for (var x in Date) {
  if (x === "parse") {
    $ERROR('#2: The Date.parse has the attribute DontEnum');
  }
}
