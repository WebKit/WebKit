// RUN: %wgslc

@compute
@workgroup_size(1, 1, 1)
fn testForStatement() {
    for (var i = -1; i <= 1; i++) {
    }

    for (i++; i <= 0; i--) {
    }
}
