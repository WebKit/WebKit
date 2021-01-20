//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

import { circular_foo } from "ModuleCircularFoo.js"
export function circular_bar() {
    increment();
    return circular_foo();
}
export function increment() { 
    counter++;
}
export var counter = 0;
