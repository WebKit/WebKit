// RUN: %wgslc

fn testScope() {
    {
        let x : i32;
    }

    {
        let x : f32;
    }
}
