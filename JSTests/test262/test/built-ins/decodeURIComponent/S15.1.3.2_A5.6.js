// Copyright 2009 the Sputnik authors.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
info: The decodeURIComponent property has not prototype property
es5id: 15.1.3.2_A5.6
es6id: 18.2.6.3
esid: sec-decodeuricomponent-encodeduricomponent
description: Checking decodeURIComponent.prototype
---*/

//CHECK#1
if (decodeURIComponent.prototype !== undefined) {
  $ERROR('#1: decodeURIComponent.prototype === undefined. Actual: ' + (decodeURIComponent.prototype));
}
