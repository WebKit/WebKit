//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

import { circular_bar, increment, counter } from "ModuleCircularBar.js"
export function circular_foo() {
    if (counter == 0) {
        return circular_bar();
    } else {
        increment();
        return counter;
    }
}
export { circular_bar as rexportbar } from "ModuleCircularBar.js"
