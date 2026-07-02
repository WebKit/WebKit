(module
  (type (;0;) (func (param i32) (result i32)))
  (type (;1;) (func (result i32)))
  (import "env" "__linear_memory" (memory (;0;) 0))
  (import "env" "callerIsOMGCompiled" (func (;0;) (type 1)))
  (func (export "test") (type 0) (param i32) (result i32)
    (local i32 i32 i32 i32)
    get_local 0
    i32.load
    i32.const 2
    i32.add
    call 0
    set_local 2
    get_local 0
    i32.load
    set_local 3
    i32.const 0
    set_local 4
    block  ;; label = @1
      get_local 2
      br_if 0 (;@1;)
      i32.const 0
      set_local 4
      loop  ;; label = @2
        get_local 0
        get_local 3
        i32.const 3
        i32.mul
        i32.store
        get_local 4
        i32.const 1
        i32.add
        set_local 4
        call 0
        set_local 2
        get_local 0
        i32.load
        set_local 3
        get_local 2
        i32.eqz
        br_if 0 (;@2;)
      end
    end
    get_local 4
    i32.add
    get_local 3
    i32.add))
