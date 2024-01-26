// RUN: %wgslc

// 🧪
fn testU32ConstantsThatOverflowI32()
{
    let x1 = u32(4294967295);
    let x2 = u32(3735928559);
}

fn testLiteralConstants()
{
    {
        const x = 1;
        _ = countOneBits(x);
    }

    {
        const x = 1i;
        _ = countOneBits(x);
    }

    {
        const x = true;
        if x {
        }
    }

    {
        const x = 3.0;
        _ = sqrt(x);
    }

    {
        const x = 3f;
        _ = sqrt(x);
    }
}

fn testArrayConstants() -> i32
{
    if (false) {
        const a = array(0, 0, 0);
        return a[0];
    }
    return 0;
}

fn testConstantMultiplication()
{
  const x1 = 2 * 2;
  const x2 = 2 * 2.0;
  const x3 = 2.0 * 2.0;
  const x4 = 2.0 * vec2(2.0);
  const x5 = vec2(2.0) * vec2(2.0);
  const x6 = vec2(2.0) * 2.0;
}

fn testConstantAddition()
{
  const x1 = 2 + 2;
  const x2 = 2 + 2.0;
  const x3 = 2.0 + 2.0;
  const x4 = 2.0 + vec2(2.0);
  const x5 = vec2(2.0) + vec2(2.0);
  const x6 = vec2(2.0) + 2.0;
}


@compute @workgroup_size(1)
fn testVectorConstants() -> i32
{
    if (false) {
        const a = vec3(0, 0, 0);
        return a.x;
    }
    return 0;
}

fn testAbstractIntPromotion()
{
    const f = pow(vec2(0), vec2(0));
}

fn testMixedConstantValue()
{
    const x = 1 - 0.5;
    _ = x;
}

fn testPrimitiveStructAccess()
{
  const f = frexp(1.25);
  const fract = f.fract;
  const exp = f.exp;
}

// Attribute constants
const group = 0;
const binding = 1;
@group(group) @binding(binding) var<storage> w: i32;

const x = 8;
const y = 4;
const z = 2;
@compute @workgroup_size(x, y, z)
fn main()
{
    _ = y;
}
