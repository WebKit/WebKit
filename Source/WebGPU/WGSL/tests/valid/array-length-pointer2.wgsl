// RUN: %metal-compile main

struct ad {
    c: u32,
    af: ae,
}

struct ae {
  c: u32,
  af: array<u32>,
}

@group(0) @binding(94) var<storage, read_write> ac: ad;

fn z(t: ptr<storage, array<u32>, read_write>) {
    t[2083] = 9;
    ac.af.af[2083] = 9;
}

fn y(t: ptr<storage, ae, read_write>) {
    t.af[2083] = 9;
    ac.af.af[2083] = 9;
    z(&t.af);
}

fn x(t: ptr<storage, ad, read_write>) {
    t.af.af[2083] = 9;
    ac.af.af[2083] = 9;
    y(&t.af);
}

@fragment fn main()
{
    x(&ac);
}
