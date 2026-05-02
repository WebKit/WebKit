

class CWasmTestCase:
    test_file = "resources/c-wasm/add/main.js"

    def execute(self):
        self.breakpointTest()
        self.inspectionTest()
        self.stepTest()
        self.memoryTest()
        self.memoryReadWriteTest()
        self.detachTest()
        self.quitTest()

    def breakpointTest(self):
        self.session.cmd("b add")

        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "add.c:2"])

        self.session.cmd("b main")

        self.session.cmd(
            "br list",
            patterns=[
                "Current breakpoints:",
                "name = 'add'",
                "name = 'main'",
            ],
        )

        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "add.c:7"])

        self.session.cmd("br del -f", patterns=["All breakpoints removed. (2 breakpoints)"])

    def inspectionTest(self):
        self.session.cmd("target modules list", patterns=["(0x4000000000000000)"])

        self.session.cmd(
            "list 1",
            patterns=[
                "add(int a, int b)",
                "main()",
            ],
        )

        self.session.cmd("b add")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "add.c:2"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x400000000000018b",
                "frame #1: 0x40000000000001e0",
                "frame #2: 0x4000000000000201",
            ],
        )

        self.session.cmd(
            "var",
            patterns=[
                "a =",
                "b =",
                "result =",
            ],
        )

        self.session.cmd(
            "thread list",
            patterns=[
                "0x400000000000018b",
                "add.c:2:18",
            ],
        )

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def stepTest(self):
        self.session.cmd("b main")

        # Step over
        self.session.cmd(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.session.cmd(
            "dis", patterns=["->  0x40000000000001c0 <+29>: local.get 0"]
        )

        patterns = [
            ["->  0x40000000000001c7 <+36>: local.get 0"],
            ["->  0x40000000000001ce <+43>: local.get 0"],
            ["->  0x40000000000001e3 <+64>: local.get 0"],
            ["->  0x4000000000000201"],
            ["->  0x40000000000001c0 <+29>: local.get 0"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("n")
                self.session.cmd("dis", patterns=pattern)

        # Step Into
        self.session.cmd(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.session.cmd(
            "dis", patterns=["->  0x40000000000001c0 <+29>: local.get 0"]
        )

        patterns = [
            ["->  0x40000000000001c7 <+36>: local.get 0"],
            ["->  0x40000000000001ce <+43>: local.get 0"],
            ["->  0x4000000000000172 <+3>:  global.get 0"],
            ["->  0x400000000000018b <+28>: local.get 2"],
            ["->  0x400000000000019b <+44>: local.get 2"],
            ["->  0x40000000000001e0 <+61>: i32.store 0"],
            ["->  0x40000000000001e3 <+64>: local.get 0"],
            ["->  0x4000000000000201"],
            ["->  0x40000000000001c0 <+29>: local.get 0"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("s")
                self.session.cmd("dis", patterns=pattern)

        # Step Out
        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        self.session.cmd("b add")

        for _ in range(10):
            self.session.cmd(
                "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
            )
            self.session.cmd(
                "dis", patterns=["->  0x400000000000018b <+28>: local.get 2"]
            )

            self.session.cmd("fin")
            self.session.cmd(
                "dis", patterns=["->  0x40000000000001e0 <+61>: i32.store 0"]
            )

        # Step Instruction
        self.session.cmd(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )

        self.session.cmd(
            "dis", patterns=["->  0x400000000000018b <+28>: local.get 2"]
        )

        for _ in range(10):
            patterns = [
                ["->  0x400000000000018d <+30>: local.get 2"],
                ["->  0x400000000000018f <+32>: i32.load 12"],
                ["->  0x4000000000000192 <+35>: local.get 2"],
                ["->  0x4000000000000194 <+37>: i32.load 8"],
                ["->  0x4000000000000197 <+40>: i32.add"],
                ["->  0x4000000000000198 <+41>: i32.store 4"],
                ["->  0x400000000000019b <+44>: local.get 2"],
                ["->  0x400000000000019d <+46>: i32.load 4"],
                ["->  0x40000000000001a0 <+49>: return"],
                ["->  0x40000000000001e0 <+61>: i32.store 0"],
                ["->  0x40000000000001e3 <+64>: local.get 0"],
                ["->  0x40000000000001e5 <+66>: i32.load 0"],
                ["->  0x40000000000001e8 <+69>: local.set 1"],
                ["->  0x40000000000001ea <+71>: local.get 0"],
                ["->  0x40000000000001ec <+73>: i32.const 16"],
                ["->  0x40000000000001ee <+75>: i32.add"],
                ["->  0x40000000000001ef <+76>: global.set 0"],
                ["->  0x40000000000001f5 <+82>: local.get 1"],
                ["->  0x40000000000001f7 <+84>: return"],
                ["->  0x4000000000000201"],
                ["->  0x400000000000018b <+28>: local.get 2"],
            ]

        for pattern in patterns:
            self.session.cmd("si")
            self.session.cmd("dis", patterns=pattern)

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def memoryTest(self):
        self.session.cmd(
            "mem reg --all",
            patterns=[
                "[0x0000000000000000-0x0000000001010000) rw- wasm_memory_0_0",
                "[0x0000000001010000-0x4000000000000000) ---",
                "[0x4000000000000000-0x40000000000014e0) r-x wasm_module_0",
                "[0x40000000000014e0-0xffffffffffffffff) ---",
            ],
        )

        self.session.cmd(
            "mem reg 0x4000000000000000",
            patterns=["[0x4000000000000000-0x40000000000014e0) r-x wasm_module_0"],
        )

        self.session.cmd(
            "mem reg 0x0000000000000000",
            patterns=["[0x0000000000000000-0x0000000001010000) rw- wasm_memory_0_0"],
        )

    def memoryReadWriteTest(self):
        self.session.cmd("memory write 0x1000 0x42 0x43 0x44 0x45", [])
        self.session.cmd("memory read -c 4 0x1000", patterns=["0x00001000: 42 43 44 45"])

        self.session.cmd("memory write 0x0000000001000000 0x46", [])
        self.session.cmd("memory read -c 1 0x0000000001000000", patterns=["0x01000000: 46"])

        self.session.cmd("memory write 0x0000000001010000 0x00", patterns=["error:"])
        self.session.cmd("memory write 0x4000000000000000 0x00", patterns=["error:"])
        self.session.cmd("memory write 0x40000000000014e0 0x00", patterns=["error:"])

    def detachTest(self):
        self.session.cmd(
            "detach",
            patterns=[
                "Process 1 detached",
            ],
        )

    def quitTest(self):
        self.session.cmd("quit", [])


class SwiftWasmTestCase:
    test_file = "resources/swift-wasm/test/main.js"

    def execute(self):
        self.breakpointTest()
        self.inspectionTest()
        self.stepTest()
        self.memoryTest()
        self.memoryReadWriteTest()
        self.variableTest()

    def breakpointTest(self):
        self.session.cmd("b processNumber")

        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint"])

        self.session.cmd("b test.swift:17")

        self.session.cmd(
            "br list",
            patterns=[
                "Current breakpoints:",
                "name = 'processNumber'",
                "file = 'test.swift', line = 17",
            ],
        )

        self.session.cmd("br del -f", patterns=["All breakpoints removed. (2 breakpoints)"])

    def inspectionTest(self):
        self.session.cmd(
            "target modules list", patterns=["(0x4000000000000000)"]
        )

        self.session.cmd(
            "list 1",
            patterns=[
                "func doubleValue(_ x: Int32) -> Int32",
                "func addNumbers(_ a: Int32, _ b: Int32) -> Int32",
                "func isEven(_ n: Int32) -> Bool",
            ],
        )

        self.session.cmd("b isEven")
        self.session.cmd(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.session.cmd(
            "dis", patterns=["->  0x4000000000010960 <+28>: block"]
        )

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000010960",
                "frame #1: 0x4000000000010a05",
                "frame #2: 0x400000000001098f",
                "frame #3: 0xc000000000000000",
            ],
        )

        self.session.cmd("var", patterns=["n ="])

        self.session.cmd(
            "thread list",
            patterns=[
                "0x4000000000010960",
                "test.swift:10:7",
            ],
        )

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def stepTest(self):
        self.session.cmd("b processNumber")

        # FIXME: Step over - Current Swift LLDB is problematic with step over

        # Step Into
        self.session.cmd(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.session.cmd(
            "dis", patterns=["->  0x40000000000109cb <+56>:  call   18"]
        )

        patterns = [
            ["->  0x40000000000108b9 <+3>:  global.get 0"],
            ["->  0x40000000000108d6 <+32>: i32.lt_s"],
            ["->  0x40000000000108ef <+57>: local.get 5"],
            ["->  0x40000000000109d1 <+62>:  local.set 4"],
            ["->  0x40000000000109e2 <+79>:  call   19"],
            ["->  0x40000000000108f9 <+3>:  global.get 0"],
            ["->  0x4000000000010924 <+46>: i32.lt_s"],
            ["->  0x400000000001093d <+71>: local.get 6"],
            ["->  0x40000000000109e8 <+85>:  local.set 5"],
            ["->  0x40000000000109ff <+108>: call   20"],
            ["->  0x4000000000010947 <+3>:  global.get 0"],
            ["->  0x4000000000010960 <+28>: block"],
            ["->  0x400000000001097f <+59>: return"],
            ["->  0x4000000000010a05 <+114>: i32.const 1"],
        ]

        for pattern in patterns:
            self.session.cmd("s")
            self.session.cmd("dis", patterns=pattern)

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        # Step Out
        self.session.cmd("b addNumbers")

        for _ in range(10):
            self.session.cmd(
                "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
            )
            self.session.cmd(
                "dis", patterns=["->  0x4000000000010924 <+46>: i32.lt_s"]
            )

            self.session.cmd("fin")
            self.session.cmd(
                "dis", patterns=["->  0x40000000000109e8 <+85>:  local.set 5"]
            )

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        # Step Instruction
        self.session.cmd("b processNumber")
        self.session.cmd(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.session.cmd(
            "dis", patterns=["->  0x40000000000109cb <+56>:  call   18"]
        )

        patterns = [
            ["->  0x40000000000108b9 <+3>:  global.get 0"],
            ["->  0x40000000000108bf <+9>:  i32.const 16"],
            ["->  0x40000000000108c1 <+11>: i32.sub"],
            ["->  0x40000000000108c2 <+12>: local.set 3"],
            ["->  0x40000000000108c4 <+14>: local.get 3"],
            ["->  0x40000000000108c6 <+16>: i32.const 0"],
            ["->  0x40000000000108c8 <+18>: i32.store 12"],
            ["->  0x40000000000108cb <+21>: local.get 3"],
            ["->  0x40000000000108cd <+23>: local.get 0"],
            ["->  0x40000000000108cf <+25>: i32.store 12"],
            ["->  0x40000000000108d2 <+28>: local.get 0"],
            ["->  0x40000000000108d4 <+30>: i32.const 0"],
            ["->  0x40000000000108d6 <+32>: i32.lt_s"],
            ["->  0x40000000000108d7 <+33>: local.set 4"],
            ["->  0x40000000000108d9 <+35>: local.get 0"],
            ["->  0x40000000000108db <+37>: local.get 0"],
            ["->  0x40000000000108dd <+39>: i32.add"],
            ["->  0x40000000000108de <+40>: local.set 5"],
            ["->  0x40000000000108e0 <+42>: block"],
            ["->  0x40000000000108e2 <+44>: local.get 4"],
            ["->  0x40000000000108e4 <+46>: local.get 5"],
            ["->  0x40000000000108e6 <+48>: local.get 0"],
            ["->  0x40000000000108e8 <+50>: i32.lt_s"],
            ["->  0x40000000000108e9 <+51>: i32.xor"],
            ["->  0x40000000000108ea <+52>: i32.const 1"],
            ["->  0x40000000000108ec <+54>: i32.and"],
            ["->  0x40000000000108ed <+55>: br_if  0"],
            ["->  0x40000000000108ef <+57>: local.get 5"],
            ["->  0x40000000000108f1 <+59>: return"],
            ["->  0x40000000000109d1 <+62>:  local.set 4"],
        ]

        for pattern in patterns:
            self.session.cmd("si")
            self.session.cmd("dis", patterns=pattern)

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def memoryTest(self):
        self.session.cmd(
            "mem reg --all",
            patterns=[
                "[0x0000000000000000-0x0000000000130000) rw- wasm_memory_0_0",
                "[0x0000000000130000-0x4000000000000000) ---",
                "[0x4000000000000000-0x40000000006b1239) r-x wasm_module_0",
                "[0x40000000006b1239-0xffffffffffffffff) ---",
            ],
        )

        self.session.cmd(
            "mem reg 0x4000000000000000",
            patterns=["[0x4000000000000000-0x40000000006b1239) r-x wasm_module_0"],
        )

        self.session.cmd(
            "mem reg 0x0000000000000000",
            patterns=["[0x0000000000000000-0x0000000000130000) rw- wasm_memory_0_0"],
        )

    def memoryReadWriteTest(self):
        self.session.cmd("memory write 0x1000 0xde 0xad 0xbe 0xef", [])
        self.session.cmd("memory read -c 4 0x1000", patterns=["0x00001000: de ad be ef"])

        self.session.cmd("memory write 0x0000000000120000 0xab", [])
        self.session.cmd("memory read -c 1 0x0000000000120000", patterns=["0x00120000: ab"])

        self.session.cmd("memory write 0x0000000000130000 0x00", patterns=["error:"])
        self.session.cmd("memory write 0x4000000000000000 0x00", patterns=["error:"])
        self.session.cmd("memory write 0x40000000006b1239 0x00", patterns=["error:"])

    def variableTest(self):
        self.session.cmd("b isEven")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint"])
        self.session.cmd("v", patterns=["(Int32) n ="])
        self.session.cmd("up", patterns=["-> 17  \t    guard isEven(sum) else {"])
        self.session.cmd("v", patterns=[
            "(Int32) input =",
            "(Int32) doubled =",
            "(Int32) sum =",
        ])


class SwiftWasmGlobalTestCase:
    test_file = "resources/swift-wasm/globals-test/main.js"

    def execute(self):
        self.session.cmd("b helper")
        self.session.cmd(
            "c",
            patterns=[
                "Process 1 stopped",
                "stop reason = breakpoint",
                "-> 7   \t    globalCounter2 = 2",
            ],
        )

        self.session.cmd("n", patterns=["-> 8   \t    globalCounter3 = 3"])
        self.session.cmd("n", patterns=["-> 9   \t}"])

        self.session.cmd("p globalCounter2", patterns=["(Int32) 2"])
        self.session.cmd("p globalCounter3", patterns=["(Int32) 3"])

        self.session.cmd("up", patterns=["-> 14  \t    helper()"])

        self.session.cmd("p globalCounter1", patterns=["(Int32) 1"])

        self.session.cmd("br del -f", patterns=["All breakpoints removed."])


class NopDropSelectEndTestCase:
    test_file = "resources/wasm/nop-drop-select-end.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000021")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000021: nop"])

        patterns = [
            ["->  0x4000000000000021: nop"],
            ["->  0x4000000000000022: i32.const 42"],
            ["->  0x4000000000000024: drop"],
            ["->  0x4000000000000025: i32.const 1"],
            ["->  0x4000000000000027: i32.const 2"],
            ["->  0x4000000000000029: i32.const 1"],
            ["->  0x400000000000002b: f32.select"],
            ["->  0x400000000000002c: drop"],
            ["->  0x400000000000002d: block"],
            ["->  0x400000000000002f: i32.const 5"],
            ["->  0x4000000000000031: drop"],
            ["->  0x4000000000000032: end"],
            ["->  0x4000000000000033: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class CallTestCase:
    test_file = "resources/wasm/call.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000036")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000036: call   2"])

        patterns = [
            ["->  0x4000000000000036: call   2"],
            ["->  0x400000000000003f: i32.const 42"],
            ["->  0x4000000000000041: end"],
            ["->  0x4000000000000038: drop"],
            ["->  0x4000000000000039: call   0"],
            ["->  0x400000000000003b: drop"],
            ["->  0x400000000000003c: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class CallIndirectTestCase:
    test_file = "resources/wasm/call-indirect.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000046")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000046: i32.const 0"])

        patterns = [
            ["->  0x4000000000000046: i32.const 0"],
            ["->  0x4000000000000048: call_indirect 1"],
            ["->  0x4000000000000055: i32.const 42"],
            ["->  0x4000000000000057: end"],
            ["->  0x400000000000004b: drop"],
            ["->  0x400000000000004c: i32.const 1"],
            ["->  0x400000000000004e: call_indirect 1"],
            ["->  0x4000000000000051: drop"],
            ["->  0x4000000000000052: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class CallRefTestCase:
    test_file = "resources/wasm/call-ref.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000043")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000043"])

        patterns = [
            ["->  0x4000000000000043"],
            ["->  0x4000000000000045"],
            ["->  0x400000000000003e: i32.const 42"],
            ["->  0x4000000000000040: end"],
            ["->  0x4000000000000047: drop"],
            ["->  0x4000000000000048"],
            ["->  0x400000000000004a"],
            ["->  0x400000000000004c: drop"],
            ["->  0x400000000000004d: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class ReturnCallTestCase:
    test_file = "resources/wasm/return-call.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000035")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000035: call   2"])

        patterns = [
            ["->  0x4000000000000035: call   2"],
            ["->  0x4000000000000040: return_call 4"],
            ["->  0x400000000000004e: i32.const 42"],
            ["->  0x4000000000000050: end"],
            ["->  0x4000000000000037: call   3"],
            ["->  0x4000000000000047: return_call 0"],
            ["->  0x4000000000000039: i32.add"],
            ["->  0x400000000000003a: i32.const 1"],
            ["->  0x400000000000003c: i32.add"],
            ["->  0x400000000000003d: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class ReturnCallIndirectTestCase:
    test_file = "resources/wasm/return-call-indirect.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000045")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000045: call   2"])

        patterns = [
            ["->  0x4000000000000045: call   2"],
            ["->  0x4000000000000050: i32.const 0"],
            ["->  0x4000000000000052: return_call_indirect 0"],
            ["->  0x4000000000000060: i32.const 42"],
            ["->  0x4000000000000062: end"],
            ["->  0x4000000000000047: call   3"],
            ["->  0x4000000000000058: i32.const 1"],
            ["->  0x400000000000005a: return_call_indirect 0"],
            ["->  0x4000000000000049: i32.add"],
            ["->  0x400000000000004a: i32.const 1"],
            ["->  0x400000000000004c: i32.add"],
            ["->  0x400000000000004d: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class ReturnCallRefTestCase:
    test_file = "resources/wasm/return-call-ref.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("b 0x400000000000003e")
            self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x400000000000003e: call   2"])

            patterns = [
                ["->  0x400000000000003e: call   2"],
                ["->  0x4000000000000049"],
                ["->  0x400000000000004b"],
                ["->  0x4000000000000057: i32.const 42"],
                ["->  0x4000000000000059: end"],
                ["->  0x4000000000000040: call   3"],
                ["->  0x4000000000000050"],
                ["->  0x4000000000000052"],
                ["->  0x4000000000000042: i32.add"],
                ["->  0x4000000000000043: i32.const 1"],
                ["->  0x4000000000000045: i32.add"],
                ["->  0x4000000000000046: end"],
            ]

            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

            self.session.cmd(
                "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
            )


class ThrowCatchTestCase:
    test_file = "resources/wasm/throw-catch.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000071")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000071: call   1"])

        patterns = [
            ["->  0x4000000000000071: call   1"],
            ["->  0x400000000000003c: try    i32"],
            ["->  0x400000000000003e: throw  0"],
            ["->  0x4000000000000044: i32.const 1"],
            ["->  0x4000000000000046: end"],
            ["->  0x4000000000000047: end"],
            ["->  0x4000000000000073: call   3"],
            ["->  0x400000000000004f: try    i32"],
            ["->  0x4000000000000051: call   0"],
            ["->  0x4000000000000035: throw  0"],
            ["->  0x4000000000000055: i32.const 2"],
            ["->  0x4000000000000057: end"],
            ["->  0x4000000000000058: end"],
            ["->  0x4000000000000075: i32.add"],
            ["->  0x4000000000000076: try    i32"],
            ["->  0x4000000000000078: call   6"],
            ["->  0x400000000000006c: call   4"],
            ["->  0x400000000000005b: call   2"],
            ["->  0x400000000000004a: call   0"],
            ["->  0x4000000000000035: throw  0"],
            ["->  0x400000000000007c: i32.const 4"],
            ["->  0x400000000000007e: end"],
            ["->  0x400000000000007f: i32.add"],
            ["->  0x4000000000000080: call   5"],
            ["->  0x4000000000000060: try    i32"],
            ["->  0x4000000000000062: call   4"],
            ["->  0x400000000000005b: call   2"],
            ["->  0x400000000000004a: call   0"],
            ["->  0x4000000000000035: throw  0"],
            ["->  0x4000000000000066: i32.const 3"],
            ["->  0x4000000000000068: end"],
            ["->  0x4000000000000069: end"],
            ["->  0x4000000000000082: i32.add"],
            ["->  0x4000000000000083: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class ThrowCatchAllTestCase:
    test_file = "resources/wasm/throw-catch-all.js"

    def execute(self):
        self.session.cmd("b 0x400000000000006e")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x400000000000006e: call   1"])

        patterns = [
            ["->  0x400000000000006e: call   1"],
            ["->  0x400000000000003c: try    i32"],
            ["->  0x400000000000003e: throw  0"],
            ["->  0x4000000000000043: i32.const 1"],
            ["->  0x4000000000000045: end"],
            ["->  0x4000000000000046: end"],
            ["->  0x4000000000000070: call   3"],
            ["->  0x400000000000004e: try    i32"],
            ["->  0x4000000000000050: call   0"],
            ["->  0x4000000000000035: throw  0"],
            ["->  0x4000000000000053: i32.const 2"],
            ["->  0x4000000000000055: end"],
            ["->  0x4000000000000056: end"],
            ["->  0x4000000000000072: i32.add"],
            ["->  0x4000000000000073: try    i32"],
            ["->  0x4000000000000075: call   6"],
            ["->  0x4000000000000069: call   4"],
            ["->  0x4000000000000059: call   2"],
            ["->  0x4000000000000049: call   0"],
            ["->  0x4000000000000035: throw  0"],
            ["->  0x4000000000000078: i32.const 4"],
            ["->  0x400000000000007a: end"],
            ["->  0x400000000000007b: i32.add"],
            ["->  0x400000000000007c: call   5"],
            ["->  0x400000000000005e: try    i32"],
            ["->  0x4000000000000060: call   4"],
            ["->  0x4000000000000059: call   2"],
            ["->  0x4000000000000049: call   0"],
            ["->  0x4000000000000035: throw  0"],
            ["->  0x4000000000000063: i32.const 3"],
            ["->  0x4000000000000065: end"],
            ["->  0x4000000000000066: end"],
            ["->  0x400000000000007e: i32.add"],
            ["->  0x400000000000007f: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class DelegateTestCase:
    test_file = "resources/wasm/delegate.js"

    def execute(self):
        self.session.cmd("b 0x40000000000000c6")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x40000000000000c6: call   1"])

        patterns = [
            # Test function entry
            ["->  0x40000000000000c6: call   1"],
            # Function 1: delegate_to_parent - delegate depth 0
            ["->  0x400000000000003e: try    i32"],
            ["->  0x4000000000000040: try    i32"],
            ["->  0x4000000000000042: throw  0"],
            ["->  0x4000000000000048: i32.const 1"],  # caught by parent
            ["->  0x400000000000004a: end"],
            ["->  0x400000000000004b: end"],
            # Back to test_all
            ["->  0x40000000000000c8: call   2"],
            # Function 2: delegate_to_grandparent - delegate depth 1
            ["->  0x400000000000004e: try    i32"],
            ["->  0x4000000000000050: try    i32"],
            ["->  0x4000000000000052: try    i32"],
            ["->  0x4000000000000054: throw  0"],
            [
                "->  0x4000000000000060: i32.const 2"
            ],  # caught by grandparent (skipped parent)
            ["->  0x4000000000000062: end"],
            ["->  0x4000000000000063: end"],
            # Back to test_all
            ["->  0x40000000000000ca: i32.add"],
            ["->  0x40000000000000cb: call   3"],
            # Function 3: delegate_to_great_grandparent - delegate depth 2
            ["->  0x4000000000000066: try    i32"],
            ["->  0x4000000000000068: try    i32"],
            ["->  0x400000000000006a: try    i32"],
            ["->  0x400000000000006c: try    i32"],
            ["->  0x400000000000006e: throw  0"],
            [
                "->  0x4000000000000080: i32.const 3"
            ],  # caught by great-grandparent (skipped 2 levels)
            ["->  0x4000000000000082: end"],
            ["->  0x4000000000000083: end"],
            # Back to test_all
            ["->  0x40000000000000cd: i32.add"],
            ["->  0x40000000000000ce: call   5"],
            # Function 5: catches_delegated_from_callee
            ["->  0x400000000000008f: try    i32"],
            ["->  0x4000000000000091: call   4"],
            # Function 4: no_handler_delegates_to_caller - delegates to caller
            ["->  0x4000000000000086: try    i32"],
            ["->  0x4000000000000088: throw  0"],
            # Back to function 5's catch handler
            [
                "->  0x4000000000000095: i32.const 4"
            ],  # caller catches delegated exception
            ["->  0x4000000000000097: end"],
            ["->  0x4000000000000098: end"],
            # Back to test_all
            ["->  0x40000000000000d0: i32.add"],
            ["->  0x40000000000000d1: call   6"],
            # Function 6: delegate_chain - multiple delegates in chain
            ["->  0x400000000000009b: try    i32"],
            ["->  0x400000000000009d: try    i32"],
            ["->  0x400000000000009f: try    i32"],
            ["->  0x40000000000000a1: throw  0"],
            # Inner delegates to middle, middle delegates to outer
            [
                "->  0x40000000000000a9: i32.const 5"
            ],  # outer catches after delegate chain
            ["->  0x40000000000000ab: end"],
            ["->  0x40000000000000ac: end"],
            # Back to test_all
            ["->  0x40000000000000d3: i32.add"],
            ["->  0x40000000000000d4: call   7"],
            # Function 7: delegate_past_catch_all - delegate skips catch_all
            ["->  0x40000000000000af: try    i32"],
            ["->  0x40000000000000b1: try    i32"],
            ["->  0x40000000000000b3: try    i32"],
            ["->  0x40000000000000b5: throw  0"],
            [
                "->  0x40000000000000c0: i32.const 6"
            ],  # grandparent catches (skipped parent's catch_all)
            ["->  0x40000000000000c2: end"],
            ["->  0x40000000000000c3: end"],
            # Back to test_all final add
            ["->  0x40000000000000d6: i32.add"],
            ["->  0x40000000000000d7: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class RethrowTestCase:
    test_file = "resources/wasm/rethrow.js"

    def execute(self):
        self.session.cmd("b 0x40000000000000b8")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x40000000000000b8: call   0"])

        patterns = [
            # Test function entry
            ["->  0x40000000000000b8: call   0"],
            # Function 0: basic_rethrow_in_catch
            ["->  0x4000000000000036: try    i32"],
            ["->  0x4000000000000038: try    i32"],
            ["->  0x400000000000003a: throw  0"],
            ["->  0x400000000000003e: rethrow 0"],
            ["->  0x4000000000000043: i32.const 1"],  # outer catches rethrown exception
            ["->  0x4000000000000045: end"],
            ["->  0x4000000000000046: end"],
            # Back to test_all
            ["->  0x40000000000000ba: call   1"],
            # Function 1: rethrow_in_catch_all
            ["->  0x4000000000000049: try    i32"],
            ["->  0x400000000000004b: try    i32"],
            ["->  0x400000000000004d: throw  0"],
            ["->  0x4000000000000050: rethrow 0"],
            [
                "->  0x4000000000000055: i32.const 2"
            ],  # outer catches rethrown from catch_all
            ["->  0x4000000000000057: end"],
            ["->  0x4000000000000058: end"],
            # Back to test_all
            ["->  0x40000000000000bc: i32.add"],
            ["->  0x40000000000000bd: call   2"],
            # Function 2: rethrow_after_partial_handling
            ["->  0x400000000000005b: try    i32"],
            ["->  0x400000000000005d: try    i32"],
            ["->  0x400000000000005f: throw  0"],
            ["->  0x4000000000000063: rethrow 0"],
            [
                "->  0x4000000000000068: i32.const 3"
            ],  # outer catches rethrown after partial handling
            ["->  0x400000000000006a: end"],
            ["->  0x400000000000006b: end"],
            # Back to test_all
            ["->  0x40000000000000bf: i32.add"],
            ["->  0x40000000000000c0: call   4"],
            # Function 4: catches_rethrown_from_callee
            ["->  0x400000000000007a: try    i32"],
            ["->  0x400000000000007c: call   3"],
            # Function 3: rethrows_to_caller
            ["->  0x400000000000006e: try    i32"],
            ["->  0x4000000000000070: throw  0"],
            ["->  0x4000000000000074: rethrow 0"],
            # Back to function 4's catch handler
            [
                "->  0x4000000000000080: i32.const 4"
            ],  # caller catches rethrown exception
            ["->  0x4000000000000082: end"],
            ["->  0x4000000000000083: end"],
            # Back to test_all
            ["->  0x40000000000000c2: i32.add"],
            ["->  0x40000000000000c3: call   5"],
            # Function 5: rethrow_chain - multiple rethrows through nested handlers
            ["->  0x4000000000000086: try    i32"],
            ["->  0x4000000000000088: try    i32"],
            ["->  0x400000000000008a: try    i32"],
            ["->  0x400000000000008c: throw  0"],
            ["->  0x4000000000000090: rethrow 0"],  # inner rethrows to middle
            ["->  0x4000000000000095: rethrow 0"],  # middle rethrows to outer
            [
                "->  0x400000000000009a: i32.const 5"
            ],  # outer catches after rethrow chain
            ["->  0x400000000000009c: end"],
            ["->  0x400000000000009d: end"],
            # Back to test_all
            ["->  0x40000000000000c5: i32.add"],
            ["->  0x40000000000000c6: call   6"],
            # Function 6: mixed_rethrow_with_catch_all
            ["->  0x40000000000000a0: try    i32"],
            ["->  0x40000000000000a2: try    i32"],
            ["->  0x40000000000000a4: try    i32"],
            ["->  0x40000000000000a6: throw  0"],
            [
                "->  0x40000000000000a9: rethrow 0"
            ],  # rethrow from catch_all to middle catch
            [
                "->  0x40000000000000ae: rethrow 0"
            ],  # rethrow from catch to outer catch_all
            [
                "->  0x40000000000000b2: i32.const 6"
            ],  # outer catch_all catches rethrown exception
            ["->  0x40000000000000b4: end"],
            ["->  0x40000000000000b5: end"],
            # Back to test_all final add
            ["->  0x40000000000000c8: i32.add"],
            ["->  0x40000000000000c9: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class ThrowRefTestCase:
    test_file = "resources/wasm/throw-ref.js"

    def execute(self):
        # Define all test patterns at the top for clarity
        # Function 0: catch-throw_ref-0 - basic throw_ref
        func0_iter1 = [
            "->  0x40000000000000e6: try_table",
            "->  0x40000000000000ec: throw  0",
            "->  0x40000000000000f1: throw_ref",
        ]

        # Function 1: catch-throw_ref-1 - conditional throw_ref (param=0 throws, param=1 returns 23)
        func1_iter1 = [
            "->  0x40000000000000fb: try_table i32",
            "->  0x4000000000000101: throw  0",
            "->  0x4000000000000106: local.get 0",
            "->  0x4000000000000108: i32.eqz",
            "->  0x4000000000000109: if",
            "->  0x400000000000010b: throw_ref",
        ]
        func1_iter2 = [
            "->  0x40000000000000fb: try_table i32",
            "->  0x4000000000000101: throw  0",
            "->  0x4000000000000106: local.get 0",
            "->  0x4000000000000108: i32.eqz",
            "->  0x4000000000000109: if",
            "->  0x400000000000010d: drop",
            "->  0x400000000000010e: end",
            "->  0x400000000000010f: i32.const 23",
            "->  0x4000000000000111: end",
        ]

        # Function 2: catchall-throw_ref-0 - catch_all_ref with throw_ref
        func2_iter1 = [
            "->  0x400000000000011a: try_table exnref",
            "->  0x400000000000011f: throw  0",
            "->  0x4000000000000123: throw_ref",
        ]

        # Function 3: catchall-throw_ref-1 - conditional catch_all_ref
        func3_iter1 = [
            "->  0x400000000000012d: try_table i32",
            "->  0x4000000000000132: throw  0",
            "->  0x4000000000000137: local.get 0",
            "->  0x4000000000000139: i32.eqz",
            "->  0x400000000000013a: if",
            "->  0x400000000000013c: throw_ref",
        ]
        func3_iter2 = [
            "->  0x400000000000012d: try_table i32",
            "->  0x4000000000000132: throw  0",
            "->  0x4000000000000137: local.get 0",
            "->  0x4000000000000139: i32.eqz",
            "->  0x400000000000013a: if",
            "->  0x400000000000013e: drop",
            "->  0x400000000000013f: end",
            "->  0x4000000000000140: i32.const 23",
            "->  0x4000000000000142: end",
        ]

        # Function 4: throw_ref-nested - stores two different exceptions (3 iterations)
        # Entry at 0x14b is verified by helper, patterns start from 0x14d
        # Structure: Two sequential blocks (not nested), each catches a different exception
        func4_nested_common = [
            "->  0x400000000000014d: try_table i32",
            "->  0x4000000000000153: throw  1",
            "->  0x4000000000000158: local.set 1",
            "->  0x400000000000015a: block  exnref",
            "->  0x400000000000015c: try_table i32",
            "->  0x4000000000000162: throw  0",
            "->  0x4000000000000167: local.set 2",
            "->  0x4000000000000169: local.get 0",
            "->  0x400000000000016b: i32.const 0",
            "->  0x400000000000016d: i32.eq",
            "->  0x400000000000016e: if",
        ]
        func4_iter1 = func4_nested_common + [
            "->  0x4000000000000170: local.get 1",
            "->  0x4000000000000172: throw_ref",
        ]
        func4_iter2 = func4_nested_common + [
            "->  0x4000000000000174: local.get 0",
            "->  0x4000000000000176: i32.const 1",
            "->  0x4000000000000178: i32.eq",
            "->  0x4000000000000179: if",
            "->  0x400000000000017b: local.get 2",
            "->  0x400000000000017d: throw_ref",
        ]
        func4_iter3 = func4_nested_common + [
            "->  0x4000000000000174: local.get 0",
            "->  0x4000000000000176: i32.const 1",
            "->  0x4000000000000178: i32.eq",
            "->  0x4000000000000179: if",
            "->  0x400000000000017f: i32.const 23",
            "->  0x4000000000000181: end",
        ]

        # Function 5: throw_ref-recatch - catch, store, re-throw_ref, re-catch
        # Two sequential try_table blocks
        func5_iter1 = [
            "->  0x400000000000018c: try_table i32",
            "->  0x4000000000000192: throw  0",
            "->  0x4000000000000197: local.set 1",
            "->  0x4000000000000199: block  exnref",
            "->  0x400000000000019b: try_table i32",
            "->  0x40000000000001a1: local.get 0",
            "->  0x40000000000001a3: i32.eqz",
            "->  0x40000000000001a4: if",
            "->  0x40000000000001a6: local.get 1",
            "->  0x40000000000001a8: throw_ref",
            "->  0x40000000000001af: drop",
            "->  0x40000000000001b0: i32.const 23",
            "->  0x40000000000001b2: end",
        ]
        func5_iter2 = [
            "->  0x400000000000018c: try_table i32",
            "->  0x4000000000000192: throw  0",
            "->  0x4000000000000197: local.set 1",
            "->  0x4000000000000199: block  exnref",
            "->  0x400000000000019b: try_table i32",
            "->  0x40000000000001a1: local.get 0",
            "->  0x40000000000001a3: i32.eqz",
            "->  0x40000000000001a4: if",
            "->  0x40000000000001aa: i32.const 42",
            "->  0x40000000000001ac: end",
            "->  0x40000000000001ad: return",
        ]

        # Function 6: throw_ref-stack-polymorphism - tests polymorphic throw_ref
        func6_iter1 = [
            "->  0x40000000000001bd: try_table f64",
            "->  0x40000000000001c3: throw  0",
            "->  0x40000000000001c8: local.set 0",
            "->  0x40000000000001ca: i32.const 1",
            "->  0x40000000000001cc: local.get 0",
            "->  0x40000000000001ce: throw_ref",
        ]

        # Execute all test functions
        self.test_function_with_breakpoint("0x40000000000000e4", [func0_iter1])
        self.test_function_with_breakpoint(
            "0x40000000000000f9", [func1_iter1, func1_iter2]
        )
        self.test_function_with_breakpoint("0x4000000000000118", [func2_iter1])
        self.test_function_with_breakpoint(
            "0x400000000000012b", [func3_iter1, func3_iter2]
        )
        self.test_function_with_breakpoint(
            "0x400000000000014b", [func4_iter1, func4_iter2, func4_iter3]
        )
        self.test_function_with_breakpoint(
            "0x400000000000018a", [func5_iter1, func5_iter2]
        )
        self.test_function_with_breakpoint("0x40000000000001bb", [func6_iter1])

    def test_function_iteration(self, patterns):
        for pattern in patterns:
            self.session.cmd("si")
            self.session.cmd("dis", patterns=[pattern])

    def test_function_with_breakpoint(self, entry_address, iterations):
        # Set breakpoint at function entry
        self.session.cmd(f"b {entry_address}")
        entry_pattern = f"->  {entry_address}: block  exnref"
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", entry_pattern])

        # Verify we're at function entry
        self.session.cmd("dis", patterns=[entry_pattern])

        # Run each iteration
        for i, patterns in enumerate(iterations):
            if i > 0:
                # Continue to next iteration and verify we're at entry
                self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", entry_pattern])
                self.session.cmd("dis", patterns=[entry_pattern])

            # Step through all patterns for this iteration
            self.test_function_iteration(patterns)

        # Remove breakpoint
        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class TryTableTestCase:
    test_file = "resources/wasm/try-table.js"

    def execute(self):
        self.session.cmd("b 0x40000000000000aa")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x40000000000000aa: call   1"])

        patterns = [
            ["->  0x40000000000000aa: call   1"],
            ["->  0x400000000000003f: block"],
            ["->  0x4000000000000041: try_table i32"],
            ["->  0x4000000000000047: throw  0"],
            ["->  0x400000000000004e: i32.const 1"],
            ["->  0x4000000000000050: end"],
            ["->  0x40000000000000ac: call   3"],
            ["->  0x4000000000000058: block"],
            ["->  0x400000000000005a: try_table i32"],
            ["->  0x4000000000000060: call   0"],
            ["->  0x4000000000000038: throw  0"],
            ["->  0x4000000000000065: i32.const 2"],
            ["->  0x4000000000000067: end"],
            ["->  0x40000000000000ae: i32.add"],
            ["->  0x40000000000000af: block"],
            ["->  0x40000000000000b1: try_table i32"],
            ["->  0x40000000000000b7: call   6"],
            ["->  0x4000000000000081: call   4"],
            ["->  0x400000000000006a: call   2"],
            ["->  0x4000000000000053: call   0"],
            ["->  0x4000000000000038: throw  0"],
            ["->  0x40000000000000c8: drop"],
            ["->  0x40000000000000c9: i32.const 4"],
            ["->  0x40000000000000cb: i32.const 3"],
            ["->  0x40000000000000cd: i32.add"],
            ["->  0x40000000000000ce: call   5"],
            ["->  0x400000000000006f: block"],
            ["->  0x4000000000000071: try_table i32"],
            ["->  0x4000000000000077: call   4"],
            ["->  0x400000000000006a: call   2"],
            ["->  0x4000000000000053: call   0"],
            ["->  0x4000000000000038: throw  0"],
            ["->  0x400000000000007c: i32.const 3"],
            ["->  0x400000000000007e: end"],
            ["->  0x40000000000000d0: i32.add"],
            ["->  0x40000000000000d1: call   7"],
            ["->  0x4000000000000086: block"],
            ["->  0x4000000000000088: try_table"],
            ["->  0x400000000000008d: throw  0"],
            ["->  0x4000000000000094: i32.const 5"],
            ["->  0x4000000000000096: end"],
            ["->  0x40000000000000d3: i32.add"],
            ["->  0x40000000000000d4: call   8"],
            ["->  0x4000000000000099: block"],
            ["->  0x400000000000009b: try_table"],
            ["->  0x40000000000000a0: call   0"],
            ["->  0x4000000000000038: throw  0"],
            ["->  0x40000000000000a5: i32.const 6"],
            ["->  0x40000000000000a7: end"],
            ["->  0x40000000000000d6: i32.add"],
            ["->  0x40000000000000d7: end"],
        ]

        for _ in range(10):
            for pattern in patterns:
                self.session.cmd("dis", patterns=pattern)
                self.session.cmd("si")

        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class SystemCallTestCase:
    test_file = "resources/wasm/system-call.js"

    def execute(self):
        self.session.cmd("dis", patterns=["error: read memory from 0x8000000000000000 failed"])


class MultiVMSameModuleSameFunctionTestCase:
    test_file = "resources/wasm/multi-vm-same-module-same-func.js"

    def execute(self):
        self.session.cmd("th list", patterns=["thread #1", "thread #2", "thread #3"])
        self.session.cmd("b 0x4000000100000024")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000100000024: nop"])
        patterns = [
            ["->  0x4000000100000025: i32.const 42"],
            ["->  0x4000000100000027: drop"],
            ["->  0x4000000100000028: nop"],
            ["->  0x4000000100000029: end"],
        ]
        for pattern in patterns:
            self.session.cmd("si", patterns=pattern)


class MultiVMSameModuleDifferentFunctionsTestCase:
    test_file = "resources/wasm/multi-vm-same-module-different-funcs.js"

    def execute(self):
        self.session.cmd("th list", patterns=["thread #1", "thread #2"])

        self.session.cmd("b 0x4000000000000034")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x4000000000000034"])
        patterns = [
            ["->  0x4000000000000035: i32.const 10"],
            ["->  0x4000000000000037: drop"],
            ["->  0x4000000000000038: nop"],
            ["->  0x4000000000000039: i32.const 20"],
            ["->  0x400000000000003b: drop"],
            ["->  0x400000000000003c: end"],
        ]
        for pattern in patterns:
            self.session.cmd("si", patterns=pattern)
        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        self.session.cmd("b 0x400000010000003f")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "->  0x400000010000003f"])
        patterns = [
            ["->  0x4000000100000040: i32.const 30"],
            ["->  0x4000000100000042: drop"],
            ["->  0x4000000100000043: nop"],
            ["->  0x4000000100000044: i32.const 40"],
            ["->  0x4000000100000046: drop"],
            ["->  0x4000000100000047: end"],
        ]
        for pattern in patterns:
            self.session.cmd("si", patterns=pattern)
        self.session.cmd(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )


class MemoryAtomicWaitTestCase:
    test_file = "resources/wasm/memory-atomic-wait.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        self.session.cmd("th list", patterns=["thread #1", "thread #2"])

        self.session.cmd("b 0x4000000000000030")
        self.session.cmd(
            "c",
            patterns=[
                "Process 1 stopped",
                "stop reason = breakpoint",
                "->  0x4000000000000030",
            ],
        )

        self.session.cmd("si", patterns=["->  0x4000000000000034: end"])

        self.session.cmd(
            "c",
            patterns=[
                "Process 1 stopped",
                "stop reason = breakpoint",
                "->  0x4000000000000030: memory.atomic.wait32",
            ],
        )

        self.session.cmd("br del -f", patterns=["All breakpoints removed. (1 breakpoint)"])


class MemoryAtomicWaitNoTimeoutTestCase:
    test_file = "resources/wasm/memory-atomic-wait-no-timeout.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        self.session.cmd("th list", patterns=["thread #1", "thread #2"])
        self.session.cmd("dis", patterns=["->  0x4000000000000030: memory.atomic.wait32 0"])


class DoCatchThrowTestCase:
    test_file = "resources/swift-wasm/do-catch-throw/main.js"

    def execute(self):
        self.session.cmd("b testDoThrowCatch")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "-> 14"])
        self.session.cmd("n", patterns=["-> 15"])
        self.session.cmd("n", patterns=["-> 18"])
        self.session.cmd("n", patterns=["-> 19"])
        self.session.cmd("br del -f", patterns=["All breakpoints removed. (1 breakpoint)"])

        self.session.cmd("b testDoThrowCatchNested")
        self.session.cmd("c", patterns=["Process 1 stopped", "stop reason = breakpoint", "-> 26"])
        self.session.cmd("n", patterns=["-> 31"])
        self.session.cmd("n", patterns=["-> 32"])
        self.session.cmd("n", patterns=["-> 37"])
        self.session.cmd("n", patterns=["-> 42"])
        self.session.cmd("br del -f", patterns=["All breakpoints removed. (1 breakpoint)"])

        # FIXME: Swift WASM Compiler Bug Incorrect DWARF Line Numbers rdar://169306069
        # self.session.cmd("b testDoThrowCatch")
        # self.session.cmd("b testDoThrowFuncCatchNested")


class WasmWasmWasmCallStackTestCase:
    test_file = "resources/wasm/wasm-wasm-wasm.js"

    def execute(self):
        self.session.cmd("b 0x400000000000003a")
        self.session.cmd("c", patterns=["stop reason = breakpoint", "0x400000000000003a"])
        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x400000000000003a",  # wasm_inner (nop)
                "frame #1: 0x4000000000000037",  # wasm_middle return PC (after call wasm_inner)
                "frame #2: 0x4000000000000032",  # test return PC (after call wasm_middle)
                "frame #3: 0xc000000000000000",  # JS main loop
            ],
        )


class JsWasmJsWasmCallStackTestCase:
    test_file = "resources/wasm/js-wasm-js-wasm.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000045")
        self.session.cmd("c", patterns=["stop reason = breakpoint", "0x4000000000000045"])
        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000000045",  # wasm_inner (nop)
                "frame #1: 0xc000000000000000",  # JS callback
                "frame #2: 0x4000000000000042",  # test call-site return PC (after 'call 0' at 0x40)
                "frame #3: 0xc000000000000000",  # JS main loop
            ],
        )


class JsJsWasmJsJsWasmCallStackTestCase:
    test_file = "resources/wasm/js-js-wasm-js-js-wasm.js"

    def execute(self):
        self.session.cmd("b 0x4000000000000045")
        self.session.cmd("c", patterns=["stop reason = breakpoint", "0x4000000000000045"])
        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000000045",  # wasm_inner (nop)
                "frame #1: 0xc000000000000000",  # js_c
                "frame #2: 0xc000000000000000",  # callback
                "frame #3: 0x4000000000000042",  # test call-site return PC (after 'call 0' at 0x40)
                "frame #4: 0xc000000000000000",  # js_b
                "frame #5: 0xc000000000000000",  # js_a
                "frame #6: 0xc000000000000000",  # JS main loop
            ],
        )


class WasmWasmJsWasmWasmCallStackTestCase:
    test_file = "resources/wasm/wasm-wasm-js-wasm-wasm.js"

    def execute(self):
        self.session.cmd("b 0x400000000000005f")
        self.session.cmd("c", patterns=["stop reason = breakpoint", "0x400000000000005f"])
        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x400000000000005f",  # wasm_leaf nop
                "frame #1: 0x400000000000005c",  # test_c return PC (after call wasm_leaf at 0x5a)
                "frame #2: 0xc000000000000000",  # mid_callback JS frame
                "frame #3: 0x4000000000000057",  # test_b return PC (after call mid_callback at 0x55, from WasmToJSIPIntReturnPCSlot)
                "frame #4: 0x4000000000000052",  # test_a return PC (after call test_b at 0x50, from test_b cfr−8)
                "frame #5: 0xc000000000000000",  # JS main loop
            ],
        )


class WasmJsWasmJsWasmCallStackTestCase:
    test_file = "resources/wasm/wasm-js-wasm-js-wasm.js"

    def execute(self):
        self.session.cmd("b 0x400000000000006c")
        self.session.cmd("c", patterns=["stop reason = breakpoint", "0x400000000000006c"])
        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x400000000000006c",  # wasm_inner nop
                "frame #1: 0xc000000000000000",  # js_mid_inner JS frame
                "frame #2: 0x4000000000000069",  # test_mid return PC (after call mid_inner at 0x67, from WasmToJSIPIntReturnPCSlot)
                "frame #3: 0xc000000000000000",  # js_mid_outer JS frame
                "frame #4: 0x4000000000000064",  # test_outer return PC (after call mid_outer at 0x62, from WasmToJSIPIntReturnPCSlot)
                "frame #5: 0xc000000000000000",  # JS main loop
            ],
        )


class SwiftWasmCrashTestCase:
    test_file = "resources/swift-wasm/crash-test/main.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped"])
            self.session.cmd("dis", patterns=["->  0x4000000000010a43 <+1>: unreachable"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000010a43",
                "frame #1: 0x4000000000010a43",
                "frame #2: 0x4000000000010a6a",
                "frame #3: 0x4000000000010a46",
                "frame #4: 0xc000000000000000",
            ]
        )


class WasmUnreachableFaultTestCase:
    test_file = "resources/wasm/unreachable.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped"])
            self.session.cmd("dis", patterns=["->  0x4000000000000024: unreachable"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000000024",
                "frame #1: 0xc000000000000000",
            ]
        )


class WasmDivByZeroTrapTestCase:
    test_file = "resources/wasm/trap-div-by-zero.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped", "Division by zero"])
            self.session.cmd("dis", patterns=["->  0x400000000000002d: i32.div_s"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x400000000000002d",
                "frame #1: 0xc000000000000000",
            ]
        )


class WasmOutOfBoundsCallIndirectTrapTestCase:
    test_file = "resources/wasm/trap-out-of-bounds-call-indirect.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped", "Out of bounds call_indirect"])
            self.session.cmd("dis", patterns=["->  0x4000000000000031: call_indirect 0"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000000031",
                "frame #1: 0xc000000000000000",
            ]
        )


class WasmStackOverflowTrapTestCase:
    test_file = "resources/wasm/trap-stack-overflow.js"
    extra_jsc_options = ["--maxPerThreadStackUsage=524288"]

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped", "Stack overflow", "->  0x4000000000000028: call"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000000028",
                "frame #1: 0x400000000000002a",
                "frame #99: 0x400000000000002a",
            ]
        )


class WasmOobMemoryTrapTestCase:
    test_file = "resources/wasm/trap-oob-memory.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped", "Out of bounds memory access"])
            self.session.cmd("dis", patterns=["->  0x4000000000000032: i32.load"])

        self.session.cmd(
            "bt",
            patterns=[
                "frame #0: 0x4000000000000032",
                "frame #1: 0xc000000000000000",
            ]
        )


class DynamicModuleLoadTestCase:
    test_file = "resources/wasm/dynamic-module-load.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        # Set a breakpoint at the 'end' instruction of func_a in module 1 (virtual address
        # 0x4000000000000023).  Module 1 is already loaded, so the breakpoint resolves immediately.
        self.session.cmd("b 0x4000000000000023", patterns=["Breakpoint 1"])

        # Resume: stops at the func_a breakpoint in module 1.
        # Disassembly confirms the stopped instruction is 'end' at the expected address.
        self.session.cmd("c", patterns=["Process 1 stopped", "->  0x4000000000000023: end"])

        # Resume: stops for the module-load notification (library:; T-packet) when module 2 is
        # instantiated.  LLDB re-queries qXfer:libraries:read and loads module 2.
        self.session.cmd("c", patterns=["Process 1 stopped", "loaded new wasm module with ids: 1"])

        # Set a breakpoint at the 'end' instruction of func_b in module 2 (virtual address
        # 0x4000000100000023).  Module 2 is now loaded, so the breakpoint resolves immediately.
        self.session.cmd("b 0x4000000100000023", patterns=["Breakpoint 2"])

        # Resume: stops at the func_b breakpoint in module 2.
        # Disassembly confirms the stopped instruction is 'end' at the expected address.
        self.session.cmd("c", patterns=["Process 1 stopped", "->  0x4000000100000023: end"])


class SwiftWasmDynamicModuleLoadTestCase:
    test_file = "resources/swift-wasm/dynamic-module-load/main.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        # Module A is loaded at the start, so this breakpoint is resolved immediately.
        self.session.cmd("b func_a", patterns=["Breakpoint 1", "func_a"])

        # Module B is loaded dynamically later, so this breakpoint is pending at the start.
        self.session.cmd("b func_b", patterns=["pending"])

        # Resume: stops at the func_a breakpoint (module A), confirming that the breakpoint is set and hit correctly.
        self.session.cmd("c", patterns=["Process 1 stopped", "func_a"])

        # Resume: stops at when Module B is loaded and the associated instance is created. This stop trigger
        # LLDB re-querying debug info and resolving the pending breakpoint for func_b, confirming that dynamic
        # module load triggers pending breakpoint resolution.
        self.session.cmd("c", patterns=["Process 1 stopped", "loaded new wasm module with ids: 1"])
        self.session.cmd("br list", patterns=["func_a", "func_b"])

        # Resume: stops at the func_b breakpoint (module B) on the first call, confirming that the pending breakpoint was resolved via debug info.
        self.session.cmd("c", patterns=["Process 1 stopped", "func_b"])


class ModuleNamingFromNameSectionTestCase:
    test_file = "resources/wasm/named-streaming-module.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        self.session.cmd("image list", patterns=["mymodule"])

        # Set a breakpoint at func_a's 'end' instruction by virtual address.
        self.session.cmd("b 0x4000000000000023", patterns=["Breakpoint 1"])

        # Resume: stops at the breakpoint; disassembly confirms the module name in the frame.
        self.session.cmd("c", patterns=["Process 1 stopped", "->  0x4000000000000023: end"])

        self.session.cmd("br del -f", patterns=["All breakpoints removed. (1 breakpoint)"])


class StreamingModuleSourceURLTestCase:
    test_file = "resources/wasm/url-named-streaming-module.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        self.session.cmd("image list", patterns=["cdn.example.com/path/canvaskit.wasm"])

        # Set a breakpoint at func_b's 'end' instruction by virtual address.
        self.session.cmd("b 0x4000000000000023", patterns=["Breakpoint 1"])

        # Resume: stops at the breakpoint in the URL-named streaming-compiled module.
        self.session.cmd("c", patterns=["Process 1 stopped", "->  0x4000000000000023: end"])

        self.session.cmd("br del -f", patterns=["All breakpoints removed. (1 breakpoint)"])


class StreamingModuleLoadTestCase:
    test_file = "resources/wasm/streaming-module-load.js"
    extra_jsc_options = ["--useDollarVM=1"]

    def execute(self):
        # The streaming module is already loaded, so the breakpoint resolves immediately.
        self.session.cmd("b 0x4000000000000023", patterns=["Breakpoint 1"])

        # Resume: stops at the func_a breakpoint in the streaming-compiled module.
        # Disassembly confirms the stopped instruction is 'end' at the expected address.
        self.session.cmd("c", patterns=["Process 1 stopped", "->  0x4000000000000023: end"])

        self.session.cmd("br del -f", patterns=["All breakpoints removed. (1 breakpoint)"])


class SwiftWasmFatalErrorTestCase:
    test_file = "resources/swift-wasm/fatal-error-test/main.js"

    def execute(self):
        for _ in range(10):
            self.session.cmd("c", patterns=["Process 1 stopped"])

        self.session.cmd("bt", patterns=["main.swift:4"])


ALL_TESTS = [
    CWasmTestCase,
    SwiftWasmTestCase,
    SwiftWasmGlobalTestCase,
    NopDropSelectEndTestCase,
    CallTestCase,
    CallIndirectTestCase,
    CallRefTestCase,
    ReturnCallTestCase,
    ReturnCallIndirectTestCase,
    ReturnCallRefTestCase,
    ThrowCatchTestCase,
    ThrowCatchAllTestCase,
    DelegateTestCase,
    RethrowTestCase,
    ThrowRefTestCase,
    TryTableTestCase,
    SystemCallTestCase,
    MultiVMSameModuleSameFunctionTestCase,
    MultiVMSameModuleDifferentFunctionsTestCase,
    MemoryAtomicWaitTestCase,
    MemoryAtomicWaitNoTimeoutTestCase,
    DoCatchThrowTestCase,
    WasmWasmWasmCallStackTestCase,
    JsWasmJsWasmCallStackTestCase,
    JsJsWasmJsJsWasmCallStackTestCase,
    WasmWasmJsWasmWasmCallStackTestCase,
    WasmJsWasmJsWasmCallStackTestCase,
    SwiftWasmCrashTestCase,
    WasmUnreachableFaultTestCase,
    WasmDivByZeroTrapTestCase,
    WasmOutOfBoundsCallIndirectTrapTestCase,
    WasmStackOverflowTrapTestCase,
    WasmOobMemoryTrapTestCase,
    DynamicModuleLoadTestCase,
    SwiftWasmDynamicModuleLoadTestCase,
    ModuleNamingFromNameSectionTestCase,
    StreamingModuleSourceURLTestCase,
    StreamingModuleLoadTestCase,
    SwiftWasmFatalErrorTestCase,
]
