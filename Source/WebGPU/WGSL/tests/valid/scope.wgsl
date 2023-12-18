// RUN: %wgslc

fn testScope() {
    var x = 1;
    if (true) {
        var x = 2;
    } else if (true) {
        var x = 2;
    } else {
        var x = 3;
    }
    {
        let x : i32 = 0;
    }

    {
        let x : f32 = 0;
    }
}
