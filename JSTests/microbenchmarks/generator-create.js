//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
function *gen()
{
    yield 42;
    yield 400;
}
noInline(gen);

for (var i = 0; i < 1e4; ++i)
    gen();
