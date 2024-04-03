// RUN: %metal-compile main

@compute @workgroup_size(1)
fn main() {
  var x = 1;
  loop {
    var z = 1;
    x += 1;
    if (x > 2) {
      continue;
    }
    if (x % 2 == 0) {
     continue;
    } else {
      break;
    }
    x = 1;
    continuing {
      let y = z * 2;
      break if y > 10;
    }
  }

  loop {
    if (false) { break; }
    continuing { while(false) { continue; } }
  }
}
