alias Aliased = array<vec3f, 1>;
alias AliasedRuntimeSized = array<vec3f>;

@group(0) @binding(0) var<storage, read_write> un: array<vec3f, 1>;
@group(0) @binding(1) var<storage, read_write> aliased: Aliased;
@group(0) @binding(2) var<storage, read_write> aliasedRuntimeSized: AliasedRuntimeSized;

@compute @workgroup_size(1)
fn main() {
  {
    let x = un[0].x;
    let xy = un[0].xy;
    let y = un[0][1];
    un[0].x = x;
    un[0][1] = y;
  }

  {
    var v = un[0];
    let x = v.x;
    let xy = v.xy;
    let y = v[1];
    v.x = x;
    v[1] = y;
  }

  {
    let v = &un[0];
    let x = v.x;
    let xy = v.xy;
    let y = v[1];
    v.x = x;
    v[1] = y;
  }

  {
    let x = aliased[0].x;
    aliased[0].x = x;
    aliased[0] = un[0];
    var v = aliased[0];
    v[1] = x;
  }

  {
    let x = aliasedRuntimeSized[0].x;
    aliasedRuntimeSized[0].x = x;
    aliasedRuntimeSized[0] = aliased[0];
  }
}
