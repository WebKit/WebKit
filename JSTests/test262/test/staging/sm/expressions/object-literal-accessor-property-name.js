/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-expressions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
var gTestfile = 'object-literal-accessor-property-name.js';
//-----------------------------------------------------------------------------
var BUGNUMBER = 715682;
var summary =
  "Permit numbers and strings containing numbers as accessor property names";
print(BUGNUMBER + ": " + summary);

//-----------------------------------------------------------------------------

({ get "0"() { } });
({ get 0() { } });
({ get 0.0() { } });
({ get 0.() { } });
({ get 1.() { } });
({ get 5.2322341234123() { } });

({ set "0"(q) { } });
({ set 0(q) { } });
({ set 0.0(q) { } });
({ set 0.(q) { } });
({ set 1.(q) { } });
({ set 5.2322341234123(q) { } });

//-----------------------------------------------------------------------------

