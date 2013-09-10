description("This ensures that BOM's scattered through a source file do not break parsing");
﻿function g(){﻿function ﻿f(){﻿successfullyReparsed=true;﻿shouldBeTrue('successfullyReparsed');successfullyParsed=true}﻿f();} g()
