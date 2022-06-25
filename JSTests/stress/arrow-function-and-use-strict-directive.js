// This test should not crash.

dispatch => accessible.children()
"use strict";

dispatch2 => accessible.children()
"use strict"

var protected = 42;

dispatch3 => "use strict"
protected;

async dispatch4 => hey
"use strict";

async dispatch4 => "use strict"
protected;
