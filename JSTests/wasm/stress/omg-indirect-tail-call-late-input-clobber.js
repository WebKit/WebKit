//@ runDefaultWasm("--useWasmTailCalls=1", "--useBBQJIT=1", "--useConcurrentJIT=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=50", "--thresholdForOMGOptimizeSoon=50", "--wasmInliningMaximumWasmCalleeSize=0")

const callerArgumentCount = 12;
const targetArgumentCount = 24;
const argumentBase = 0x110000;
const observedIndex = 22;

function assertEqual(actual, expected)
{
    if (actual !== expected)
        throw new Error(`expected ${expected}, got ${actual}`);
}

function makeTarget()
{
    // owner module: (func $owned (export "owned")) — an empty function whose
    // exported reference is imported by the target module below.
    const ownerBytes = new Uint8Array([
        // ── magic + version ──
        0x00, 0x61, 0x73, 0x6d,              // "\0asm"
        0x01, 0x00, 0x00, 0x00,              // version 1
        // ── Type section (id 1, size 0x04=4) ──
        0x01, 0x04,
        0x01,                                // 1 type
        0x60, 0x00, 0x00,                    // type 0: () -> ()
        // ── Function section (id 3, size 0x02=2) ──
        0x03, 0x02,
        0x01,                                // 1 function
        0x00,                                // func 0 -> type 0
        // ── Export section (id 7, size 0x09=9) ──
        0x07, 0x09,
        0x01,                                // 1 export
        0x05, 0x6f, 0x77, 0x6e, 0x65, 0x64,  // name (len 5) "owned"
        0x00, 0x00,                          // kind 0 (func), func index 0
        // ── Code section (id 10, size 0x04=4) ──
        0x0a, 0x04,
        0x01,                                // 1 body
        0x02,                                // body 0: size 2
        0x00,                                //   0 local groups
        0x0b,                                //   end
        // ── Custom "name" section (id 0, size 0x0f=15) ──
        0x00, 0x0f,
        0x04, 0x6e, 0x61, 0x6d, 0x65,        // name (len 4) "name"
        0x01, 0x08,                          // subsection 1 (function names), size 8
        0x01,                                //   1 name entry
        0x00,                                //   func index 0
        0x05, 0x6f, 0x77, 0x6e, 0x65, 0x64,  //   name (len 5) "owned"
    ]);
    const owner = new WebAssembly.Instance(new WebAssembly.Module(ownerBytes));

    // target module: imports (func (import "x" "owned")), and exports
    // (func $dump (export "dump") (param i32 x24) (result i32) local.get 22).
    const targetBytes = new Uint8Array([
        // ── magic + version ──
        0x00, 0x61, 0x73, 0x6d,              // "\0asm"
        0x01, 0x00, 0x00, 0x00,              // version 1
        // ── Type section (id 1, size 0x20=32) ──
        0x01, 0x20,
        0x02,                                // 2 types
        0x60, 0x00, 0x00,                    // type 0: () -> ()  (the imported "owned")
        0x60, 0x18,                          // type 1: func, 0x18=24 params
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[0..5]  i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[6..11] i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[12..17] i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[18..23] i32
        0x01, 0x7f,                          //   1 result: i32   (24x i32) -> i32
        // ── Import section (id 2, size 0x0b=11) ──
        0x02, 0x0b,
        0x01,                                // 1 import
        0x01, 0x78,                          // module (len 1) "x"
        0x05, 0x6f, 0x77, 0x6e, 0x65, 0x64,  // field  (len 5) "owned"
        0x00, 0x00,                          // kind 0 (func), type 0
        // ── Function section (id 3, size 0x02=2) ──
        0x03, 0x02,
        0x01,                                // 1 function
        0x01,                                // func 1 (import is func 0) -> type 1
        // ── Export section (id 7, size 0x08=8) ──
        0x07, 0x08,
        0x01,                                // 1 export
        0x04, 0x64, 0x75, 0x6d, 0x70,        // name (len 4) "dump"
        0x00, 0x01,                          // kind 0 (func), func index 1
        // ── Code section (id 10, size 0x06=6) ──
        0x0a, 0x06,
        0x01,                                // 1 body
        0x04,                                // body 0 ($dump): size 4
        0x00,                                //   0 local groups
        0x20, 0x16,                          //   local.get 0x16=22  (0x20 = local.get)
        0x0b,                                //   end
        // ── Custom "name" section (id 0, size 0x0e=14) ──
        0x00, 0x0e,
        0x04, 0x6e, 0x61, 0x6d, 0x65,        // name (len 4) "name"
        0x01, 0x07,                          // subsection 1 (function names), size 7
        0x01,                                //   1 name entry
        0x01,                                //   func index 1
        0x04, 0x64, 0x75, 0x6d, 0x70,        //   name (len 4) "dump"
    ]);
    return new WebAssembly.Instance(new WebAssembly.Module(targetBytes), { x: { owned: owner.exports.owned } });
}

function makeRelay()
{
    // relay module: (memory 1) and (func $entry (export "entry")
    //   (param i32, i32 x12, (ref $targetType)) (result i32)
    //   if (result i32) local.get 0
    //     <forward 12 caller args, then 12 late i32.const argumentBase+i values>
    //     local.get 13  return_call_ref $targetType
    //   else i32.const 31337 end).
    // The i32.const operands use oversized 5-byte LEB encodings produced by the
    // builder, so these bytes must be captured, not recompiled from WAT.
    const relayBytes = new Uint8Array([
        // ── magic + version ──
        0x00, 0x61, 0x73, 0x6d,              // "\0asm"
        0x01, 0x00, 0x00, 0x00,              // version 1
        // ── Type section (id 1, size 0x30=48) ──
        0x01, 0x30,
        0x02,                                // 2 types
        0x60, 0x18,                          // type 0: func, 0x18=24 params (the $dump target sig)
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[0..5]  i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[6..11] i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[12..17] i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[18..23] i32
        0x01, 0x7f,                          //   1 result: i32   (24x i32) -> i32
        0x60, 0x0e,                          // type 1: func, 0x0e=14 params ($entry sig)
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[0..5]  i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[6..11] i32
        0x7f,                                //   params[12] i32
        0x64, 0x00,                          //   params[13] (ref 0)  (0x64 = ref, heap type 0)
        0x01, 0x7f,                          //   1 result: i32   (13x i32, (ref 0)) -> i32
        // ── Function section (id 3, size 0x02=2) ──
        0x03, 0x02,
        0x01,                                // 1 function
        0x01,                                // func 0 -> type 1
        // ── Memory section (id 5, size 0x03=3) ──
        0x05, 0x03,
        0x01,                                // 1 memory
        0x00, 0x01,                          // limits: flags 0, min 1
        // ── Export section (id 7, size 0x09=9) ──
        0x07, 0x09,
        0x01,                                // 1 export
        0x05, 0x65, 0x6e, 0x74, 0x72, 0x79,  // name (len 5) "entry"
        0x00, 0x00,                          // kind 0 (func), func index 0
        // ── Code section (id 10, size 0x66=102) ──
        0x0a, 0x66,
        0x01,                                // 1 body
        0x64,                                // body 0 ($entry): size 0x64=100
        0x00,                                //   0 local groups
        0x20, 0x00,                          //   local.get 0            (0x20 = local.get)
        0x04, 0x7f,                          //   if (result i32)        (0x04 = if)
        // forward the 12 caller args local.get 1..12:
        0x20, 0x01,                          //     local.get 1
        0x20, 0x02,                          //     local.get 2
        0x20, 0x03,                          //     local.get 3
        0x20, 0x04,                          //     local.get 4
        0x20, 0x05,                          //     local.get 5
        0x20, 0x06,                          //     local.get 6
        0x20, 0x07,                          //     local.get 7
        0x20, 0x08,                          //     local.get 8
        0x20, 0x09,                          //     local.get 9
        0x20, 0x0a,                          //     local.get 10
        0x20, 0x0b,                          //     local.get 11
        0x20, 0x0c,                          //     local.get 12
        // 12 late i32.const argumentBase+i (i=12..23): each is a 5-byte i32.const
        // (0x41 opcode + 4-byte signed-LEB operand) — see the note above.
        0x41, 0x8c, 0x80, 0xc4, 0x00,        //     i32.const 0x11000c (argumentBase+12 = 1114124)
        0x41, 0x8d, 0x80, 0xc4, 0x00,        //     i32.const 0x11000d (argumentBase+13 = 1114125)
        0x41, 0x8e, 0x80, 0xc4, 0x00,        //     i32.const 0x11000e (argumentBase+14 = 1114126)
        0x41, 0x8f, 0x80, 0xc4, 0x00,        //     i32.const 0x11000f (argumentBase+15 = 1114127)
        0x41, 0x90, 0x80, 0xc4, 0x00,        //     i32.const 0x110010 (argumentBase+16 = 1114128)
        0x41, 0x91, 0x80, 0xc4, 0x00,        //     i32.const 0x110011 (argumentBase+17 = 1114129)
        0x41, 0x92, 0x80, 0xc4, 0x00,        //     i32.const 0x110012 (argumentBase+18 = 1114130)
        0x41, 0x93, 0x80, 0xc4, 0x00,        //     i32.const 0x110013 (argumentBase+19 = 1114131)
        0x41, 0x94, 0x80, 0xc4, 0x00,        //     i32.const 0x110014 (argumentBase+20 = 1114132)
        0x41, 0x95, 0x80, 0xc4, 0x00,        //     i32.const 0x110015 (argumentBase+21 = 1114133)
        0x41, 0x96, 0x80, 0xc4, 0x00,        //     i32.const 0x110016 (argumentBase+22 = 1114134)
        0x41, 0x97, 0x80, 0xc4, 0x00,        //     i32.const 0x110017 (argumentBase+23 = 1114135)
        0x20, 0x0d,                          //     local.get 13 (the (ref 0) callee)
        0x15, 0x00,                          //     return_call_ref type 0   (0x15 = return_call_ref)
        0x05,                                //   else                     (0x05 = else)
        0x41, 0xe9, 0xf4, 0x01,              //     i32.const 0x7a69 = 31337
        0x0b,                                //   end (if)                 (0x0b = end)
        0x0b,                                //   end (body)
        // ── Custom "name" section (id 0, size 0x0f=15) ──
        0x00, 0x0f,
        0x04, 0x6e, 0x61, 0x6d, 0x65,        // name (len 4) "name"
        0x01, 0x08,                          // subsection 1 (function names), size 8
        0x01,                                //   1 name entry
        0x00,                                //   func index 0
        0x05, 0x65, 0x6e, 0x74, 0x72, 0x79,  //   name (len 5) "entry"
    ]);
    return new WebAssembly.Instance(new WebAssembly.Module(relayBytes));
}

function makeOuter(relayFunction, targetFunction)
{
    // outer module: imports (func $dump (import "t" "dump")) and
    // (func $entry (import "r" "entry")), has a declarative element segment
    // referencing $dump, and exports (func $entry (export "entry") (result i32)
    //   i32.const 1  <12 late i32.const argumentBase+i>  ref.func $dump
    //   return_call $relay).
    const outerBytes = new Uint8Array([
        // ── magic + version ──
        0x00, 0x61, 0x73, 0x6d,              // "\0asm"
        0x01, 0x00, 0x00, 0x00,              // version 1
        // ── Type section (id 1, size 0x34=52) ──
        0x01, 0x34,
        0x03,                                // 3 types
        0x60, 0x18,                          // type 0: func, 0x18=24 params ($dump sig)
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[0..5]  i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[6..11] i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[12..17] i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[18..23] i32
        0x01, 0x7f,                          //   1 result: i32   (24x i32) -> i32
        0x60, 0x0e,                          // type 1: func, 0x0e=14 params ($entry relay sig)
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[0..5]  i32
        0x7f, 0x7f, 0x7f, 0x7f, 0x7f, 0x7f,  //   params[6..11] i32
        0x7f,                                //   params[12] i32
        0x64, 0x00,                          //   params[13] (ref 0)  (0x64 = ref, heap type 0)
        0x01, 0x7f,                          //   1 result: i32   (13x i32, (ref 0)) -> i32
        0x60, 0x00,                          // type 2: func, 0 params ($entry export sig)
        0x01, 0x7f,                          //   1 result: i32   () -> i32
        // ── Import section (id 2, size 0x14=20) ──
        0x02, 0x14,
        0x02,                                // 2 imports
        0x01, 0x74,                          // import 0: module (len 1) "t"
        0x04, 0x64, 0x75, 0x6d, 0x70,        //           field  (len 4) "dump"
        0x00, 0x00,                          //           kind 0 (func), type 0  -> func 0 ($dump)
        0x01, 0x72,                          // import 1: module (len 1) "r"
        0x05, 0x65, 0x6e, 0x74, 0x72, 0x79,  //           field  (len 5) "entry"
        0x00, 0x01,                          //           kind 0 (func), type 1  -> func 1 ($relay)
        // ── Function section (id 3, size 0x02=2) ──
        0x03, 0x02,
        0x01,                                // 1 function
        0x02,                                // func 2 (imports are 0,1) -> type 2
        // ── Export section (id 7, size 0x09=9) ──
        0x07, 0x09,
        0x01,                                // 1 export
        0x05, 0x65, 0x6e, 0x74, 0x72, 0x79,  // name (len 5) "entry"
        0x00, 0x02,                          // kind 0 (func), func index 2
        // ── Element section (id 9, size 0x05=5) ──
        0x09, 0x05,
        0x01,                                // 1 element segment
        0x03,                                // flags 3 = declarative, funcref, explicit funcs
        0x00,                                // elemkind 0 (funcref)
        0x01,                                // 1 function
        0x00,                                // func index 0 ($dump) — declares ref.func 0 usable
        // ── Code section (id 10, size 0x46=70) ──
        0x0a, 0x46,
        0x01,                                // 1 body
        0x44,                                // body 0 ($entry): size 0x44=68
        0x00,                                //   0 local groups
        0x41, 0x01,                          //   i32.const 1            (0x41 = i32.const)
        // 12 late i32.const argumentBase+i (i=0..11): each is a 5-byte i32.const
        // (0x41 opcode + 4-byte signed-LEB operand) — see the note above.
        0x41, 0x80, 0x80, 0xc4, 0x00,        //   i32.const 0x110000 (argumentBase+0 = 1114112)
        0x41, 0x81, 0x80, 0xc4, 0x00,        //   i32.const 0x110001 (argumentBase+1 = 1114113)
        0x41, 0x82, 0x80, 0xc4, 0x00,        //   i32.const 0x110002 (argumentBase+2 = 1114114)
        0x41, 0x83, 0x80, 0xc4, 0x00,        //   i32.const 0x110003 (argumentBase+3 = 1114115)
        0x41, 0x84, 0x80, 0xc4, 0x00,        //   i32.const 0x110004 (argumentBase+4 = 1114116)
        0x41, 0x85, 0x80, 0xc4, 0x00,        //   i32.const 0x110005 (argumentBase+5 = 1114117)
        0x41, 0x86, 0x80, 0xc4, 0x00,        //   i32.const 0x110006 (argumentBase+6 = 1114118)
        0x41, 0x87, 0x80, 0xc4, 0x00,        //   i32.const 0x110007 (argumentBase+7 = 1114119)
        0x41, 0x88, 0x80, 0xc4, 0x00,        //   i32.const 0x110008 (argumentBase+8 = 1114120)
        0x41, 0x89, 0x80, 0xc4, 0x00,        //   i32.const 0x110009 (argumentBase+9 = 1114121)
        0x41, 0x8a, 0x80, 0xc4, 0x00,        //   i32.const 0x11000a (argumentBase+10 = 1114122)
        0x41, 0x8b, 0x80, 0xc4, 0x00,        //   i32.const 0x11000b (argumentBase+11 = 1114123)
        0xd2, 0x00,                          //   ref.func 0 ($dump)     (0xd2 = ref.func)
        0x12, 0x01,                          //   return_call func 1 ($relay)  (0x12 = return_call)
        0x0b,                                //   end
        // ── Custom "name" section (id 0, size 0x0f=15) ──
        0x00, 0x0f,
        0x04, 0x6e, 0x61, 0x6d, 0x65,        // name (len 4) "name"
        0x01, 0x08,                          // subsection 1 (function names), size 8
        0x01,                                //   1 name entry
        0x02,                                //   func index 2
        0x05, 0x65, 0x6e, 0x74, 0x72, 0x79,  //   name (len 5) "entry"
    ]);
    return new WebAssembly.Instance(new WebAssembly.Module(outerBytes), {
        t: { dump: targetFunction },
        r: { entry: relayFunction },
    });
}

const target = makeTarget();
const relay = makeRelay();
const outer = makeOuter(relay.exports.entry, target.exports.dump);
const warmupArguments = [0];
for (let index = 0; index < callerArgumentCount; ++index)
    warmupArguments.push(argumentBase + index);
warmupArguments.push(target.exports.dump);

for (let index = 0; index < wasmTestLoopCount; ++index)
    assertEqual(relay.exports.entry(...warmupArguments), 31337);

assertEqual(outer.exports.entry(), argumentBase + observedIndex);
