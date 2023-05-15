// RUN: %wgslc

fn testIfStmt() -> i32 {
    if true {
        return 42;
    } else if false {
        return -1;
    } else {
        return 0;
    }
}
