(module
  (type (;0;) (func (param i32) (result i32)))
  (type (;1;) (func (result i32)))
  (import "env" "__linear_memory" (memory (;0;) 0))
  (import "env" "callerIsOMGCompiled" (func (;0;) (type 1)))
  (func (export "test") (type 0) (param i32) (result i32)
    (local i32 i32 i32 i32)
    get_local 0
    get_local 0
    i32.load
    set_local 1
    i32.const 2
    set_local 2
    i32.const 0
    set_local 3
    loop  ;; label = @1
      ;; add a bunch of values so there is a bigger stack here than in the if block.
      i32.const 1
      get_local 1
      f32.const 3
      get_local 3
      i64.const 4

      ;; real program
      i32.const 0
      set_local 4
      get_local 1
      get_local 2
      if i32
        get_local 2
      else
        get_local 2
      end
      block  ;; label = @2
        call 0
        br_if 0 (;@2;)
        i32.const 0
        set_local 4
        loop  ;; label = @3
          get_local 0
          get_local 0
          i32.load
          i32.const 3
          i32.mul
          i32.store
          get_local 4
          i32.const 1
          i32.add
          set_local 4
          call 0
          i32.eqz
          br_if 0 (;@3;)
        end
      end
      i32.add
      set_local 2
      get_local 0
      i32.load
      set_local 1
      get_local 3
      i32.const 1
      i32.add
      tee_local 3
      i32.const 20
      i32.ne
      br_if 0 (;@1;)

      ;; drop our extra values so we can compile
      drop
      drop
      drop
      drop
      drop
    end
    get_local 1
    get_local 2
    i32.add
    i32.add
    get_local 4
    i32.add))
