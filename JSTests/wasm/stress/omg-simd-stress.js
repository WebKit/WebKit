//@ requireOptions("--useWasmSIMD=1")
//@ skip if !$isSIMDPlatform
import { instantiate } from "../wabt-wrapper.js"
import * as assert from "../assert.js"

async function test() {
    let fnBase = read("omg-simd-stress.wat")
    let wat = `(module
      (memory $imports.mem (import "imports" "mem") 1 1)

      (func $probe_begin (import "imports" "probe_begin") (param i32))
      (func $probe_end (import "imports" "probe_end") (param i32))
      (func $probe (import "imports" "probe") (param i64))
      (func $probe32 (import "imports" "probe") (param i32))

      (func $bad (export "bad")
    ` + fnBase + `
      (func $good (export "good")
    ` + fnBase + ")"

    let memory = new WebAssembly.Memory({ initial: 1, maximum: 1 })

    let probe_log = []
    let y = 0
    let enableLogging = false

    const instance = await instantiate(wat, {
        imports: {
            mem: memory,
            probe_begin: function(x) {
              if (!enableLogging)
                return
              probe_log.push({event: "probe_begin", id: x})
              ++y
            },
            probe_end: function(x) {
              if (!enableLogging)
                return
              probe_log.push({event: "probe_end", id: x})
              ++y
            },
            probe: function(x) {
              if (!enableLogging)
                return
              let id = 0
              let idOffset = 0
              let depth = 0
              let offset = 0
              for (let l of probe_log) {
                if (l.event == "probe_begin") {
                  ++depth
                  if (depth > 1) {
                    console.log("Probe depth > 1 at: ", l)
                    breakpoint10
                  }
                  id = l.id
                  idOffset = offset
                } else if (l.event == "probe_end") {
                  --depth
                  if (depth < 0) {
                    console.log("Probe depth < 0 at: ", l)
                    breakpoint11
                  }
                  id = -9007199254740991
                  idOffset = -9007199254740991
                }

                offset++
              }

              probe_log.push({event: "probe", id, val: x, offset: -idOffset + probe_log.length - 1})
            }
          }
    }, { simd: true, log: false })
    const { bad, good } = instance.exports

    for (let i = 0; i < 100000; ++i) {
      bad()
    }

    enableLogging = true

    for (let i = 0; i < 10; ++i) {
      probe_log = []
      good()
      let goodLog = probe_log
      probe_log = []
      bad()
      let badLog = probe_log

      for (let i = 0; i < goodLog.length && i < badLog.length; ++i) {
        if (goodLog[i].event != badLog[i].event || goodLog[i].id != badLog[i].id || (goodLog[i].event == "probe" && goodLog[i].val != badLog[i].val)) {
          print("Different at: ", i, "Bad: ", badLog[i], " Good:", goodLog[i])

          for (let j = 0; j < badLog.length; ++j) {
            let str = (key, value) => {
                return typeof value === 'bigint'
                    ? value.toString()
                    : value // return everything else unchanged
            }
            print(JSON.stringify(badLog[i], str), JSON.stringify(goodLog[i], str))
          }

          $vm.abort()
        }
      }
    }
}

await assert.asyncTest(test())
