// Copyright (C) 2018 Leo Balter. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
description: >
    Dynamic Import receives an AssignmentExpression (Ternary)
esid: prod-ImportCall
info: |
    ImportCall [Yield]:
        import ( AssignmentExpression[+In, ?Yield] )

    AssignmentExpression[In, Yield, Await]:
        ConditionalExpression[?In, ?Yield, ?Await]
        [+Yield]YieldExpression[?In, ?Await]
        ArrowFunction[?In, ?Yield, ?Await]
        AsyncArrowFunction[?In, ?Yield, ?Await]
        LeftHandSideExpression[?Yield, ?Await] = AssignmentExpression[?In, ?Yield, ?Await]
        LeftHandSideExpression[?Yield, ?Await] AssignmentOperator AssignmentExpression[?In, ?Yield, ?Await]
flags: [async]
features: [dynamic-import]
---*/

const a = './module-code_FIXTURE.js';
const b = './module-code-other_FIXTURE.js';

async function fn() {
    const ns1 = await import(true ? a : b); // import('./module-code_FIXTURE.js')

    assert.sameValue(ns1.local1, 'Test262');
    assert.sameValue(ns1.default, 42);

    const ns2 = await import(false ? a : b); // import('./module-code-other_FIXTURE.js')

    assert.sameValue(ns2.local1, 'one six one two');
    assert.sameValue(ns2.default, 1612);
}

fn().then($DONE, $DONE).catch($DONE);
