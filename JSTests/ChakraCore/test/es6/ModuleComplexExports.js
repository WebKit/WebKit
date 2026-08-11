//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

export function foo() { return 'foo'; };
export { foo as foo2 };

function bar() { return 'bar'; };
export { bar, bar as bar2 };

export let let2 = 'let2';
export const const2 = 'const2';
export var var2 = 'var2';
export { let2 as let4, const2 as const4, var2 as var4 };

let let3 = 'let3';
const const3 = 'const3';
var var3 = 'var3';
export { let3, let3 as let5, const3, const3 as const5, var3, var3 as var5 };

export class class2 { 
    member() { return 'class2'; } 
    static static_member() { return 'class2'; } 
};
export { class2 as class3 };

class class4 { 
    member() { return 'class4'; } 
    static static_member() { return 'class4'; } 
};
export { class4, class4 as class5 };

export async function asyncfoo() { };
async function asyncbar() { };
export { asyncfoo as asyncfoo2, asyncbar, asyncbar as asyncbar2 };

export function* genfoo() { };
function* genbar() { };
export { genfoo as genfoo2, genbar, genbar as genbar2 };

export default function () { return 'default'; };

var mutatingExportTarget = function() { return 'before'; };
function changeMutatingExportTarget() {
    mutatingExportTarget = function() { return 'after'; };
    return 'ok';
}

export { mutatingExportTarget as target, changeMutatingExportTarget as changeTarget };

var exportedAsKeyword = 'ModuleComplexExports';
export { exportedAsKeyword as export };
export { exportedAsKeyword as function };

var as = function() { return 'as'; };
export { as as as };
