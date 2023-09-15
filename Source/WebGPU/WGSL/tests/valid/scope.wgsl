// RUN: %wgslc

fn testScope() {
    {
        let x : i32 = 0;
    }

    {
        let x : f32 = 0;
    }
}
