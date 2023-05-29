// RUN: %wgslc

fn testi32(x: i32)
{
    _ = x;
}

fn testVec2(x: vec2<i32>)
{
    _ = x;
}

fn testVec3(x: vec3<i32>)
{
    _ = x;
}

fn testVec4(x: vec4<i32>)
{
    _ = x;
}

fn testSwizzleVec2()
{
    let v = vec2(0i);
    _ = v.x;
    _ = v.xx;
    _ = v.xxx;
    _ = v.xxxx;

    _ = v.y;
    _ = v.yy;
    _ = v.yyy;
    _ = v.yyyy;

    _ = v.xy;

    _ = v.r;
    _ = v.rr;
    _ = v.rrr;
    _ = v.rrrr;

    _ = v.g;
    _ = v.gg;
    _ = v.ggg;
    _ = v.gggg;

    _ = v.rg;
}

fn testSwizzleVec3() {
    let v = vec3(0);
    _ = v.x;
    _ = v.xx;
    _ = v.xxx;
    _ = v.xxxx;

    _ = v.y;
    _ = v.yy;
    _ = v.yyy;
    _ = v.yyyy;

    _ = v.z;
    _ = v.zz;
    _ = v.zzz;
    _ = v.zzzz;

    _ = v.xyz;

    _ = v.r;
    _ = v.rr;
    _ = v.rrr;
    _ = v.rrrr;

    _ = v.g;
    _ = v.gg;
    _ = v.ggg;
    _ = v.gggg;

    _ = v.b;
    _ = v.bb;
    _ = v.bbb;
    _ = v.bbbb;

    _ = v.rgb;
}

fn testSwizzleVec4() {
    let v = vec4(0);
    _ = v.x;
    _ = v.xx;
    _ = v.xxx;
    _ = v.xxxx;

    _ = v.y;
    _ = v.yy;
    _ = v.yyy;
    _ = v.yyyy;

    _ = v.z;
    _ = v.zz;
    _ = v.zzz;
    _ = v.zzzz;

    _ = v.w;
    _ = v.ww;
    _ = v.www;
    _ = v.wwww;

    _ = v.xyzw;

    _ = v.r;
    _ = v.rr;
    _ = v.rrr;
    _ = v.rrrr;

    _ = v.g;
    _ = v.gg;
    _ = v.ggg;
    _ = v.gggg;

    _ = v.b;
    _ = v.bb;
    _ = v.bbb;
    _ = v.bbbb;

    _ = v.a;
    _ = v.aa;
    _ = v.aaa;
    _ = v.aaaa;

    _ = v.rgba;
}
