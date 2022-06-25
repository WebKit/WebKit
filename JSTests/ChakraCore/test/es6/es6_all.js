﻿//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// uses es6 default parameter values as validation for ES6 'stable' features

function f(x = 'default argument') {
    print(x);
}
f();
