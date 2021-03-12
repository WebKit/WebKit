function assert(b) {
    if (!b)
        throw new Error("Bad assertion!")
}

function obj() { 
    return {};
}
noInline(obj);

// This test makes sure that when wrapper() is called with the closure created in foo() as |this|
// that we to_this the |this| that is a closure before the arrow function captures its value.
// This crashes if there is a bug in debug builds.

const globalThis = this;
function foo() {
    function capture() { return wrapper; }
    function wrapper() {
        let x = () => {
            Object.defineProperty(this, "baz", {
                get: function() { },
                set: function() { }
            });
            assert(!("bar" in this));
            assert(this === globalThis);
        }

        x();
    }
    wrapper();
}
foo();


function foo2() {
    function capture() { return wrapper; }
    function wrapper() {
        let x = () => {
            Object.defineProperty(this, "baz2", {
                get: function() { },
                set: function() { }
            });
            assert(this === globalThis);
        }

        x();

        function bar() {
            with (obj()) {
                assert;
            }
        }
        bar();
    }
    wrapper();
}
foo2();

assert(this.hasOwnProperty("baz"));
assert(this.hasOwnProperty("baz2"));
