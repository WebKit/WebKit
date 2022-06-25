//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

export { bar2 as ModuleComplexReexports_foo } from 'ModuleComplexExports.js';

export { function as switch } from 'ModuleComplexExports.js';

import { foo, foo2, foo as localfoo, foo2 as localfoo2 } from "ModuleComplexExports.js";
export { foo, foo2 as baz, localfoo, localfoo as bar, localfoo2, localfoo2 as bar2 };
