/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

/*---
includes: [sm/non262.js, sm/non262-shell.js, sm/non262-extensions-shell.js]
flags:
  - noStrict
description: |
  pending
esid: pending
---*/
//-----------------------------------------------------------------------------
var BUGNUMBER = 547941;
var summary = 'js weak maps';
var actual = '';
var expect = '';

//-----------------------------------------------------------------------------
test();
//-----------------------------------------------------------------------------

function test()
{
    printBugNumber(BUGNUMBER);
    printStatus(summary);

    var TestPassCount = 0;
    var TestFailCount = 0;
    var TestTodoCount = 0;

    var TODO = 1;

    function check(fun, todo) {
        var thrown = null;
        var success = false;
        try {
            success = fun();
        } catch (x) {
            thrown = x;
        }

        if (thrown)
            success = false;

        if (todo) {
            TestTodoCount++;

            if (success) {
                var ex = new Error;
                print ("=== TODO but PASSED? ===");
                print (ex.stack);
                print ("========================");
            }

            return;
        }

        if (success) {
            TestPassCount++;
        } else {
            TestFailCount++;

            var ex = new Error;
            print ("=== FAILED ===");
            print (ex.stack);
            if (thrown) {
                print ("    threw exception:");
                print (thrown);
            }
            print ("==============");
        }
    }

    function checkThrows(fun, todo) {
        let thrown = false;
        try {
            fun();
        } catch (x) {
            thrown = true;
        }

        check(() => thrown, todo);
    }

    var key = {};
    var map = new WeakMap();

    check(() => !map.has(key));
    check(() => map.delete(key) == false);
    check(() => map.set(key, 42) === map);
    check(() => map.get(key) == 42);
    check(() => typeof map.get({}) == "undefined");
    check(() => map.get({}, "foo") == undefined);

    $262.gc(); $262.gc(); $262.gc();

    check(() => map.get(key) == 42);
    check(() => map.delete(key) == true);
    check(() => map.delete(key) == false);
    check(() => map.delete({}) == false);

    check(() => typeof map.get(key) == "undefined");
    check(() => !map.has(key));
    check(() => map.delete(key) == false);

    var value = { };
    check(() => map.set(new Object(), value) === map);
    $262.gc(); $262.gc(); $262.gc();

    check(() => map.has("non-object key") == false);
    check(() => map.has() == false);
    check(() => map.get("non-object key") == undefined);
    check(() => map.get() == undefined);
    check(() => map.delete("non-object key") == false);
    check(() => map.delete() == false);

    check(() => map.set(key) === map);
    check(() => map.get(key) == undefined);

    checkThrows(() => map.set("non-object key", value));

    print ("done");

    assert.sameValue(0, TestFailCount, "weak map tests");
}
