(module
  (type (;0;) (func (result i32)))
  (type (;1;) (func (result i32 funcref funcref funcref funcref externref funcref funcref i32 funcref i32 f32 externref f64 funcref f32 f32 funcref i64 i64 externref)))
  (type (;2;) (func))
  (type (;3;) (func))
  (memory (;0;) (import "m2" "memory0") 3510 4275)
  (func (;0;) (import "m0" "fn0") (type 1) (result i32 funcref funcref funcref funcref externref funcref funcref i32 funcref i32 f32 externref f64 funcref f32 f32 funcref i64 i64 externref))
  (func (;1;) (import "m2" "fn1") (type 1) (result i32 funcref funcref funcref funcref externref funcref funcref i32 funcref i32 f32 externref f64 funcref f32 f32 funcref i64 i64 externref))
  (func (;2;) (import "m2" "fn2") (type 3))
  (func (;3;) (import "extra" "isJIT") (type 0) (result i32))
  (func $debug (import "m1" "debuggingHelper"))
  (tag (;0;) (export "tag0") (import "m0" "tag2") (type 3))
  (tag (;1;) (export "tag1") (import "m2" "tag3") (type 3))
  (tag (;2;) (import "m2" "tag4") (type 3))
  (tag (;3;) (import "m1" "tag5") (type 2))
  (tag (;4;) (import "m2" "tag6") (type 2))
  (tag (;5;) (import "m0" "tag7") (type 2))
  (tag (;6;) (import "m1" "tag8") (type 3))
  (tag (;7;) (import "m1" "tag9") (type 2))
  (tag (;8;) (import "m0" "tag10") (type 3))
  (global (;0;) (import "m2" "global0") (mut funcref))
  (global (;1;) (import "m0" "global1") (mut i64))
  (global (;2;) (import "m1" "global2") (mut i64))
  (global (;3;) (import "m0" "global3") (mut f64))
  (global (;4;) (import "m0" "global4") (mut i32))
  (global (;5;) (import "m1" "global5") (mut i64))
  (global (;6;) (import "m0" "global6") (mut f64))
  (global (;7;) (import "m1" "global7") (mut i64))
  (global (;8;) (import "m1" "global8") (mut funcref))
  (func (;4;) (export "fn3") (type 1) (result i32 funcref funcref funcref funcref externref funcref funcref i32 funcref i32 f32 externref f64 funcref f32 f32 funcref i64 i64 externref)
    (local funcref f32 i32 funcref funcref i32 i32 i32 i64 f32 f64)
    
    
    i32.const 256648
    global.get 7
    i32.const 180
    i32.const 175
    global.set 4
    memory.size
    i32.lt_s
    i64.const 19882
    loop (result funcref)  ;; label = @1
      block (result f64)  ;; label = @2
        f32.const -0x1.fbc71p+113 (;=-2.05979e+34;)
        try (result f32)  ;; label = @3
            
          i32.const 41
          ;; last place to execute
          
          call_indirect 5 (type 1)
          (table.set 2 (i32.const 1002) (local.get 3))
          try (result f32)  ;; label = @4
            i64.const -17603189750099
            block (result funcref)  ;; label = @5
                (table.set 2 (i32.const 1003) (local.get 3))
              call 3
              drop
              i64.const 386799028551681301
              ref.func 1
              local.get 2
              global.set 4
             
              i64.const -996799636031790628
              i32.const 117
              block (result f32)  ;; label = @6
                block (result funcref)  ;; label = @7
                (table.set 2 (i32.const 1002) (local.get 3))
                  i32.const 0
                  i32.const 65535
                  i32.and
                  i32.load8_s offset=557
                  local.set 2
                  block (result i64)  ;; label = @8
                  (table.set 2 (i32.const 1002) (local.get 3))
                    try (result i32 funcref funcref funcref funcref externref funcref funcref i32 funcref i32 f32 externref f64 funcref f32 f32 funcref i64 i64 externref)  ;; label = @9
                    (table.set 2 (i32.const 1002) (local.get 3))
                      local.get 1
                      ref.func 0
                      global.get 0
                      local.set 4
                      global.get 6
                      f64.ceil
                      f64.const -0x1.2aaf86eb2b29bp+878 (;=-2.35131e+264;)
                      global.get 7
                      local.get 1
                      br 5 (;@4;)
                    end
                    i64.const 787736748045
                    local.get 0
                    ref.func 3
                    global.get 7
                    f64.const -0x1.b1b5101be202ep-726 (;=-4.79932e-219;)
                    ref.func 5
                    memory.size
                    i32.const 2
                    i32.rem_u
                    (table.set 2 (i32.const 1002) (local.get 3))
                    if (result i32)  ;; label = @9
                      block (result i32)  ;; label = @10
                        try  ;; label = @11
                          block  ;; label = @12
                          (table.set 2 (i32.const 1002) (local.get 3))
                            call 2
                            try (result i64)  ;; label = @13
                            (table.set 2 (i32.const 1002) (local.get 3))
                              local.get 10
                              f64.const 0x1p+0 (;=1;)
                              f64.add
                              local.tee 10
                              f64.const 0x1.8p+1 (;=3;)
                              f64.lt
                              if  ;; label = @14
                                br 13 (;@1;)
                              end
                              br 1 (;@12;)
                            end
                            i64.popcnt
                            try (result f64)  ;; label = @13
                            (table.set 2 (i32.const 1002) (local.get 3))
                              try  ;; label = @14
                                try  ;; label = @15
                                  br 4 (;@11;)
                                end
                                br 2 (;@12;)
                                nop
                                unreachable
                              end
                              global.get 0
                              br 6 (;@7;)
                            end
                            local.get 3
                            try  ;; label = @13
                              br 1 (;@12;)
                              unreachable
                            end
                            loop (result i32)  ;; label = @13
                              br 2 (;@11;)
                              unreachable
                            end
                            br_if 0 (;@12;)
                            f64.const -0x1.dffdc874e71bap+386 (;=-2.9551e+116;)
                            br 10 (;@2;)
                          end
                          try  ;; label = @12
                          catch 7
                            local.get 7
                            i32.const 1
                            i32.add
                            local.tee 7
                            i32.const 21
                            i32.lt_u
                            if  ;; label = @13
                              br 12 (;@1;)
                            end
                          end
                          f64.const 0x1.96fcd800c5096p-666 (;=5.19235e-201;)
                          br 9 (;@2;)
                        end
                        ref.func 1
                        global.get 6
                        i64.trunc_sat_f64_s
                        global.set 7
                        local.get 1
                        unreachable
                        
                      end
                      unreachable
                    else
                      ref.func 5
                      i32.const 109
                      br 0 (;@9;)
                      unreachable
                    end
                    (table.set 2 (i32.const 1002) (local.get 3))
                    memory.size
                    i32.lt_s
                    i32.const 2
                    i32.rem_u
                    if (result i32 funcref funcref funcref funcref externref funcref funcref i32 funcref i32 f32 externref f64 funcref f32 f32 funcref i64 i64 externref)  ;; label = @9
                      unreachable
                    else
                      unreachable
                    end
                    br 8 (;@0;)
                    unreachable
                  end
                  (table.set 2 (i32.const 1002) (local.get 3))
                  table.size 5
                  i32.const 2
                  i32.rem_u
                  if (result i32)  ;; label = @8
                  (table.set 2 (i32.const 1002) (local.get 3))
                    f32.const 0x1.9d0a26p-62 (;=3.49858e-19;)
                    block (result f32)  ;; label = @9
                      try  ;; label = @10
                      (table.set 2 (i32.const 1002) (local.get 3))
                        local.get 6
                        i32.const 1
                        i32.add
                        local.tee 6
                        i32.const 34
                        i32.lt_u
                        br_if 9 (;@1;)
                        br 0 (;@10;)
                      end
                      global.get 0
                      i32.const 112
                      i32.const 2
                      i32.rem_u
                      br_table 4 (;@5;) 2 (;@7;) 2 (;@7;)
                    end
                    memory.size
                    br 0 (;@8;)
                    unreachable
                  else
                  (table.set 2 (i32.const 1002) (local.get 3))
                    table.size 0
                    br 0 (;@8;)
                    unreachable
                  end
                  i32.const 2
                  i32.rem_u
                  (table.set 2 (i32.const 1002) (local.get 3))
                  if (result i32)  ;; label = @8
                    loop  ;; label = @9
                      try (result f32)  ;; label = @10
                        try  ;; label = @11
                        (table.set 2 (i32.const 1002) (local.get 3))
                          ref.func 0
                          table.size 6
                          br 3 (;@8;)
                        end
                        try  ;; label = @11
                        (table.set 2 (i32.const 1002) (local.get 3))
                          call 3
                          i32.eqz
                          br_if 10 (;@1;)
                          try  ;; label = @12
                          (table.set 2 (i32.const 1002) (local.get 3))
                            local.get 8
                            i64.const 1
                            i64.add
                            local.tee 8
                            i64.const 7
                            i64.lt_u
                            br_if 11 (;@1;)
                            br 0 (;@12;)
                          end
                          (table.set 2 (i32.const 1002) (local.get 3))
                          global.get 8
                          local.tee 0
                          local.set 4
                        delegate 6
                        ref.func 3
                        table.size 2
                        i32.const 2
                        i32.rem_u
                        if (result externref)  ;; label = @11
                        (table.set 2 (i32.const 1002) (local.get 3))
                          i32.const 26
                          f64.const -0x1.dd4117fe1bb69p-111 (;=-7.18092e-34;)
                          br 9 (;@2;)
                          unreachable
                        else
                          try  ;; label = @12
                            block (result externref)  ;; label = @13
                              br 1 (;@12;)
                              unreachable
                            end
                            f64.const -nan:0x17ae91dd6869 (;=nan;)
                            global.set 3
                            (table.set 2 (i32.const 1002) (local.get 3))
                            table.size 3
                            i32.const 1
                            i32.rem_u
                            br_table 0 (;@12;) 0 (;@12;)
                          end
                          throw 7
                          unreachable
                          unreachable
                        end
                        f64.const nan:0xc0141d2b84cd2 (;=nan;)
                        ref.func 3
                        (table.set 2 (i32.const 1002) (local.get 3))
                        i64.const 137
                        try (result i64)  ;; label = @11
                          f64.const 0x1.e9e47aeb49017p-34 (;=1.11389e-10;)
                          br 9 (;@2;)
                          nop
                        end
                        global.set 2
                        global.get 7
                        memory.size
                        global.get 6
                        i32.trunc_sat_f64_s
                        br 2 (;@8;)
                      end
                      br 3 (;@6;)
                      unreachable
                    end
                    (table.set 2 (i32.const 1002) (local.get 3))
                    f32.const -0x1.8b15bcp+119 (;=-1.0257e+36;)
                    i64.const 7
                    i64.const -164
                    global.set 2
                    f64.const 0x1.aa7b150efdc8ep+486 (;=3.32841e+146;)
                    ref.func 1
                    local.get 1
                    br 2 (;@6;)
                  else
                  (table.set 2 (i32.const 1002) (local.get 3))
                    i32.const 41
                    call_indirect 5 (type 1)
                    local.get 3
                    i64.const 56
                    i64.clz
                    f64.reinterpret_i64
                    global.set 6
                    br 1 (;@7;)
                    unreachable
                  end
                  
                  global.set 4
                  global.get 8
                  local.get 4
                  global.set 8
                  global.set 8
                  f32.const -0x1.4efc4ep+54 (;=-2.35725e+16;)
                  local.get 0
                  (table.set 2 (i32.const 1002) (local.get 3))
                  br 2 (;@5;)
                end
                (table.set 2 (i32.const 1002) (local.get 3))
                br 1 (;@5;)
              end
              (table.set 2 (i32.const 1002) (local.get 3))
              br 1 (;@4;)
            end
            (table.set 2 (i32.const 1002) (local.get 3))
            memory.size
            f32.reinterpret_i32
            br 1 (;@3;)
            unreachable
          end
          (table.set 2 (i32.const 1002) (local.get 3))
          ref.func 2
          local.get 3
          global.set 8
          local.get 1
          global.get 1
          f32.const 0
          br 0 (;@3;)
        catch_all
        
        
        
            (table.set 2 (i32.const 42) (local.get 3))
            (table.set 2 (i32.const 1002) (local.get 3))
            
            
            
            
            
          local.get 10
          f64.const 0x1p+0 (;=1;)
          f64.add
          local.tee 10
          f64.const 0x1.08p+5 (;=33;)
          f64.lt
          if  ;; label = @4
            br 3 (;@1;)
          end
          local.get 2
          global.set 4
          local.get 7
          i32.const 1
          i32.add
          local.tee 7
          i32.const 8
          i32.lt_u
          if  ;; label = @4
            br 3 (;@1;)
          end
          ref.func 2
          drop
          local.get 1
          i32.const 0
          i32.const 0
          i32.const 0
          table.init 1 0
          (table.set 2 (i32.rem_u (table.size 2) (i32.const 1)) (local.get 3))
          br 0 (;@3;)
        end
        local.set 1
        local.get 3
        f32.const 0x1.8d6324p-61 (;=6.732e-19;)
        ref.func 0
        drop
        local.set 1
        global.set 0
        local.set 1
        f64.const -0x1.5e1f9bc56d88dp-537 (;=-3.04e-162;)
      end
      global.set 6
      local.get 4
      (table.set 2 (i32.rem_u (table.size 2) (i32.const 1)) (local.get 3))
    end
    
    (table.set 2 (i32.rem_u (table.size 2) (i32.const 1)) (local.get 3))
    
    
    
    
    
    ref.func 2
    local.get 2
    i32.const 2
    i32.rem_u
    drop
      i64.const -1182
    ref.func 2
    local.get 3
    f32.const 0x1.85a42ep-32 (;=3.54377e-10;)
    f32.floor
    local.set 1
    global.set 0
    drop
    global.get 0
    global.set 8
    f32.convert_i64_s
    ref.func 2
    local.get 2
    i32.const 2
    i32.rem_u
    drop
    i32.const 57478
    i32.const 48039
    i32.const 54069
    memory.copy
    i32.const 53901
    (f32.const 0)
    local.set 1
    i32.const 65535
    i32.and
    local.get 1
    f32.store offset=437
    drop
    f32.trunc
    i32.const 319570
    local.tee 2
    i32.const 10
    i32.const 0
    i32.const 0
    table.init 4 2
    global.set 4
    f32.abs
    drop
    i32.const 2568
    global.set 4
    local.get 1
    i32.trunc_sat_f32_s
    local.set 2
    global.get 1
    local.get 2
    local.set 2
    drop
    i64.const -10528202581769789
    global.set 2
    i32.const 23681
    i32.const 29417
    i32.const 17805
    memory.copy
    drop
    ref.func 3
    local.get 2
    f64.convert_i32_u
    f64.const -0x1.c6a5c761abf1ep+336 (;=-2.48607e+101;)
    f64.mul
    global.set 3
    drop
    global.set 8
    global.set 1
    
    
    
    unreachable    
    )
  (table (;0;) (export "table0") 72 319 externref)
  (table (;1;) (export "table1") 1 funcref)
  (table (;2;) (export "table2") 1500 1500 funcref)
  (table (;3;) (export "table3") 95 externref)
  (table (;4;) (export "table4") 65 funcref)
  (table (;5;) (export "table5") 85 869 funcref)
  (table (;6;) (export "table6") 63 externref)
  (table (;7;) (export "table7") 13 externref)
  (tag (;9;) (type 3))
  (tag (;10;) (type 2))
  (elem (;0;) (table 4) (i32.const 31) func 0 1 4 0 2 3 4 1)
  (elem (;1;) (table 5) (i32.const 1) func 1 1 3 4 2 1 4 0 3 3 3 3 2 4 0 0 2 3 4 0 0 0 3 0 0 4 3 1 4 0 4 0 0 2 1 2 1 4 2 2 0 2 0 0 1 2 4 4 4 0 4 4 1 4 2 1 3 3 3 2 2 4 1 1 0 4 1 1 4 2 2 3 0 2 2 1 4 0 0 3 1 0 3 1)
  (elem (;2;) declare func 1 0 3 1 3 1 0 0 1 0 2 2 2 2 4 2 2 4 3 1 1 1 1 2 1)
  (elem (;3;) (table 5) (i32.const 39) func 0 1 5)
  (elem (;4;) (table 2) (i32.const 0) func 2)
  (elem (;5;) (table 1) (i32.const 0) func 3)
  (data (;0;) (i32.const 46972) "Ff\93\b5Z\86:r")
  (data (;1;) "\08\07k\fd&\bbq\ad")
  (data (;2;) "Q['\14\07\ab\a6"))
