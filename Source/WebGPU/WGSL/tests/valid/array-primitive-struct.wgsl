// RUN: %metal-compile main

enable f16;
@compute @workgroup_size(1)
fn main(){
    var x = array(modf(2h));
}
