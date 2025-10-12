struct MyStruct1 {
  myStructField1: i32,
};

struct MyStruct2 {
  myStructField2: MyStruct1,
};

@group(0) @binding(0) var<storage> myGlobal1: MyStruct2;

var<private> myGlobal2: MyStruct2;

fn myHelperFunction() -> i32
{
  return myGlobal1.myStructField2.myStructField1 + myGlobal2.myStructField2.myStructField1;
}

@compute @workgroup_size(1)
fn myComputeEntrypoint()
{
  _ = myHelperFunction();
}
