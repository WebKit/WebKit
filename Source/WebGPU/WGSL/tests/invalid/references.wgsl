// RUN: %not %wgslc | %check

@group(0) @binding(0) var<storage, read> x: i32;
@group(0) @binding(1) var<storage, read_write> y: i32;

fn testReferenceAssignment()
{
    // CHECK-L: cannot store into a read-only type 'ref<storage, i32, read>'
    x = 0;

    // CHECK-L: cannot assign value of type 'u32' to 'i32'
    y = 0u;

    // CHECK-L: cannot assign to a value of type 'i32'
    let z: i32 = 0;
    z = 0;

    // FIXME: we can't test that we don't accept write-only references for reads
    // since there are no valid ways of declaring a write-only var
}

fn testDecrementIcrement() {
    {
        // CHECK-L: cannot modify a value of type 'i32'
        let x = 0i;
        x++;
    }
    {
        // CHECK-L: cannot modify read-only type 'ref<storage, i32, read>'
        x++;
    }
    {
        // CHECK-L: increment can only be applied to integers, found f32
        var x = 0f;
        x++;
    }
}

