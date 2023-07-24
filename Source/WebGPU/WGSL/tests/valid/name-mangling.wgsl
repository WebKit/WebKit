// RUN: %metal myComputeEntrypoint | %check

// CHECK-NOT-L: MyStruct1
struct MyStruct1 {
  // FIXME: mangle struct fields
  myStructField1: i32,
};

// CHECK-NOT-L: MyStruct2
struct MyStruct2 {
  // FIXME: mangle struct fields
  myStructField2: MyStruct1,
};

// FIXME: this still appears in a struct field
@group(0) @binding(0) var<storage> myGlobal1: MyStruct2;

// CHECK-NOT-L: myGlobal2
var<private> myGlobal2: MyStruct2;

// CHECK-NOT-L: myHelperFunction
fn myHelperFunction() -> i32
{
  return myGlobal1.myStructField2.myStructField1 + myGlobal2.myStructField2.myStructField1;
}

// CHECK-NOT-L: myComputeEntrypoint
@compute @workgroup_size(1)
fn myComputeEntrypoint()
{
  _ = myHelperFunction();
}
