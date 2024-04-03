// RUN: %wgslc

const_assert(true);
const_assert(!false);

const t = true;
const_assert(t);

const f = false;
const_assert(!false);

const_assert(all(vec2(true)));

fn main()
{
    const_assert(true);
    const_assert(!false);

    const t = true;
    const_assert(t);

    const f = false;
    const_assert(!false);

    const_assert(all(vec2(true)));
}
