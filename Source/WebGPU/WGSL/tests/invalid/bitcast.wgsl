// RUN: %not %wgslc | %check

fn testWrongNumberOfArguments()
{
    // CHECK: bitcast expects a single argument, found 2
    _ = bitcast<i32>(0, 0);

    // CHECK: bitcast expects a single argument, found 0
    _ = bitcast<i32>();
}

fn testWrongNumberOfTemplateArguments()
{
    // CHECK: bitcast expects a single template argument, found 2
    _ = bitcast<i32, i32>(0);

    // CHECK: bitcast expects a single template argument, found 0
    _ = bitcast(0);
}

fn testInvalidConversion()
{
    // CHECK: cannot bitcast from 'i32' to 'vec2<i32>'
    _ = bitcast<vec2<i32>>(0);

    // CHECK: cannot bitcast from 'vec2<i32>' to 'i32'
    _ = bitcast<i32>(vec2(0));
}
