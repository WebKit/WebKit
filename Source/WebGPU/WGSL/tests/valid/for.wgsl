// RUN: %wgslc

@compute
@workgroup_size(1, 1, 1)
fn testForStatement() {
    for (var i = -1; i <= 1; i++)
    @diagnostic(off,derivative_uniformity)
    {
    }

    var i = 0;
    for (i++; i <= 0; i--) {
    }
}
