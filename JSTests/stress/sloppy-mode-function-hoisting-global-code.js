function assert(x) {
    if (!x)
        throw new Error("Bad assertion!");
}


{
  const MY_CONST = 1e6;
  function foo1() { return MY_CONST; }
  assert(foo1() === MY_CONST);
}
assert(foo1() === 1e6);


try {
    throw new Error();
} catch ({foo3}) {
    { function foo3() {} }
}
assert(!globalThis.hasOwnProperty("foo3"));


with ({foo4: 4}) {
  function foo5() { return foo4; }
  assert(foo5() === 4);
}
assert(foo5() === 4);


with({}) {
  let foo6 = 6;
  function foo7() { return foo6; }
  assert(foo7() === foo6);
}
assert(foo7() === 6);


let foo8 = 8;
{ function foo8 () {} }
assert(foo8 === 8);
assert(!globalThis.hasOwnProperty("foo8"));


assert(foo10 === undefined);
{
    assert(foo10() === 1);
    function foo10() { return 1; }
    assert(foo10() === 1);
}
assert(foo10() === 1);
{
    let foo10 = 1;

    {
        assert(foo10() === 2);
        function foo10() { return 2; }
        assert(foo10() === 2);
    }
}
assert(foo10() === 1);


assert(foo11 === undefined);
{
    assert(foo11() === 1);
    function foo11() { return 1; }
    assert(foo11() === 1);
}
assert(foo11() === 1);
{
    {{{
        assert(foo11() === 2);
        function foo11() { return 2; }
        assert(foo11() === 2);
    }}}
    let foo11 = 1;
}
assert(foo11() === 1);


assert(foo12 === undefined);
const err12 = new Error();
try {
    assert(foo12() === 1);
    function foo12() { return 1; }
    throw err12;
} catch (foo12) {
    assert(foo12 === err12);
    {
        assert(foo12() === 2);
        function foo12() { return 2; }
        assert(foo12() === 2);
    }
    assert(foo12 === err12);
}
assert(foo12() === 2);


assert(foo13 === undefined);
const err13 = new Error();
err13.foo13 = err13;
try {
    assert(foo13() === 1);
    function foo13() { return 1; }
    throw err13;
} catch ({foo13}) {
    assert(foo13 === err13);
    {
        assert(foo13() === 2);
        function foo13() { return 2; }
        assert(foo13() === 2);
    }
    assert(foo13 === err13);
}
assert(foo13() === 1);


assert(foo14 === undefined);
const err14 = new Error();
err14.foo14 = err14;
try {
    assert(foo14() === 1);
    function foo14() { return 1; }
    throw err14;
} catch (foo14) {
    assert(foo14 === err14);
    {
        {{
            assert(foo14() === 2);
            function foo14() { return 2; }
            assert(foo14() === 2);
        }}
        const foo14 = 1;
    }
    assert(foo14 === err14);
}
assert(foo14() === 1);


if (true) { { function foo15() {} } } let foo15 = 15;
assert(foo15 === 15);


if (true) { function foo16() {} } let foo16 = 16;
assert(foo16 === 16);


{ if (true) function foo17() {} } let foo17 = 17;
assert(foo17 === 17);


assert(foo18 === undefined);
{
    assert(foo18() === 1);
    function foo18() { return 1; }
    assert(foo18() === 1);
    {
        assert(foo18() === 2);
        function foo18() { return 2; }
        assert(foo18() === 2);
    }
}
assert(foo18() === 1);
