// RUN: %metal-compile main

@compute @workgroup_size(1)
fn main() {
    switch 1 {
        case 1,: { }
        case 2 { }
        case 4, 5, 6 { }
        case 7, 8, 9, { }
        case 10, 11, 12: { }
        case 13, 14, 15: { }
        default { }
    }

    switch 1 {
        default { }
        case 1,: { }
        case 2 { }
        case 4, 5, 6 { }
        case 7, 8, 9, { }
        case 10, 11, 12: { }
        case 13, 14, 15: { }
    }

    switch 1 {
        case 1,: { }
        case 2 { }
        default { }
        case 4, 5, 6 { }
        case 7, 8, 9, { }
    }

    switch 1 {
        case 1,: { }
        case 2 { }
        case 4, 5, 6 { }
        case 7, default, 8, 9, { }
    }

    switch 1 {
        case default { }
    }

    switch 1 {
        case default,: { }
    }

    switch 1 {
        case default: { }
    }

    switch 1 {
        case default,: { }
    }

    switch 1 {
        case default, 1, 2, 3,: { }
    }

    switch 1 {
        case 1, 2, 3, default,: { }
    }
}
