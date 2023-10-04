// RUN: %metal-compile testWhileStatement

@compute
@workgroup_size(1, 1, 1)
fn testWhileStatement() {
    var i = 0;
    while i < 10 {
        i = i + 1;
    }
}
