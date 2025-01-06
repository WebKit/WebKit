// RUN: %metal main

@compute @workgroup_size(1)
fn main() 
{
    var x = 1;
    {
        var x=*&x;
    }
}
