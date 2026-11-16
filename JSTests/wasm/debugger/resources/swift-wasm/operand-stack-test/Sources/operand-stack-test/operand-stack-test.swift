// Exercises the two places an optimized build can put a Swift local, and so the two packets that
// read them. Build with `swift-wasm/build.sh operand-stack-test` (--configuration release); at
// -Onone every local is spilled to the shadow stack in linear memory and neither form appears.
//
//   between(), between64() -- `r`/`r64` are produced by a call and consumed by a call without ever
//       being addressed, so they stay on the operand stack: qWasmStackValue. The i64 case catches a
//       reply narrowed to the wrong width.
//   pressure()             -- `c` must survive three further calls before it is used, so it is
//       spilled to a wasm local: qWasmLocal.

nonisolated(unsafe) var sink: Int32 = 0
nonisolated(unsafe) var sink64: Int64 = 0

@inline(never) func produce(_ n: Int32) -> Int32 { return n &* 3 &+ 1 }
@inline(never) func consume(_ v: Int32) { sink = v }

@inline(never) func between(_ n: Int32) {
    let r = produce(n)
    consume(r)             // <-- break here; r is on the operand stack
}

@inline(never) func pressure(_ n: Int32) -> Int32 {
    let a = produce(n)
    let b = produce(n &+ 1)
    let c = produce(n &+ 2)
    let d = produce(n &+ 3)
    consume(a)
    consume(b)             // <-- break here; c is in a wasm local
    return a &+ b &+ c &+ d
}

@inline(never) func produce64(_ n: Int64) -> Int64 { return n &* 3 &+ 0x1_0000_0000 }
@inline(never) func consume64(_ v: Int64) { sink64 = v }

@inline(never) func between64(_ n: Int64) {
    let r64 = produce64(n)
    consume64(r64)         // <-- break here; r64 is an i64 on the operand stack
}

@_cdecl("entry")
public func entry(_ n: Int32) -> Int32 {
    between(n)
    between64(Int64(n))
    return pressure(n)
}

@main
struct OperandStackTest {
    static func main() {
        _ = entry
    }
}
