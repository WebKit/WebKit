// RUN: %not %wgslc | %check

// CHECK-L: encountered a dependency cycle: S -> S
struct S {
    s: S
}
