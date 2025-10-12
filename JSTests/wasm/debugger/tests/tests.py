from lib.core.base import BaseTestCase, PatternMatchMode


class CWasmTestCase(BaseTestCase):

    def __init__(self, build_config: str = None, port: int = None):
        super().__init__(build_config, port)

    def execute(self):

        self.setup_debugging_session_or_raise("resources/c-wasm/add/main.js")

        try:
            for _ in range(1):
                self.continueInterruptTest()
                self.breakpointTest()
                self.inspectionTest()
                self.stepTest()
                self.memoryTest()

        except Exception as e:
            raise Exception(f"Breakpoint test failed: {e}")

    def continueInterruptTest(self):
        cycles = 10
        for cycle in range(1, cycles + 1):
            try:
                self.send_lldb_command_or_raise("c")
                self.send_lldb_command_or_raise("process interrupt")
            except Exception as e:
                raise Exception(f"Cycle {cycle} failed: {e}")

    def breakpointTest(self):
        self.send_lldb_command_or_raise(f"b add")

        for _ in range(10):
            self.send_lldb_command_or_raise(
                "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
            )

        self.send_lldb_command_or_raise("b main")

        self.send_lldb_command_or_raise(
            "br list",
            patterns=[
                "Current breakpoints:",
                "name = 'add'",
                "name = 'main'",
            ],
        )

        for _ in range(10):
            self.send_lldb_command_or_raise(
                "c",
                patterns=[
                    "add.c:7:9",
                    "add.c:2:18",
                ],
                mode=PatternMatchMode.ANY,
            )

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (2 breakpoints)"]
        )

    def inspectionTest(self):
        self.send_lldb_command_or_raise(
            "target modules list", patterns=["(0x4000000000000000)"]
        )

        self.send_lldb_command_or_raise(
            "list 1",
            patterns=[
                "add(int a, int b)",
                "main()",
            ],
        )

        self.send_lldb_command_or_raise(f"b add")
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.send_lldb_command_or_raise(
            "dis", patterns=["->  0x400000000000018b <+28>: local.get 2"]
        )

        self.send_lldb_command_or_raise(
            "bt",
            patterns=[
                "frame #0: 0x400000000000018b",
                "frame #1: 0x40000000000001e0",
                "frame #2: 0x4000000000000201",
            ],
        )

        self.send_lldb_command_or_raise(
            "var",
            patterns=[
                "a =",
                "b =",
                "result =",
            ],
        )

        self.send_lldb_command_or_raise(
            "thread list",
            patterns=[
                "0x400000000000018b",
                "add.c:2:18",
            ],
        )

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def stepTest(self):

        self.send_lldb_command_or_raise("b main")

        # Step over
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.send_lldb_command_or_raise(
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
                self.send_lldb_command_or_raise("n")
                self.send_lldb_command_or_raise("dis", patterns=pattern)

        # Step Into
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.send_lldb_command_or_raise(
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
                self.send_lldb_command_or_raise("s")
                self.send_lldb_command_or_raise("dis", patterns=pattern)

        # Step Out
        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        self.send_lldb_command_or_raise("b add")

        for _ in range(10):
            self.send_lldb_command_or_raise(
                "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
            )
            self.send_lldb_command_or_raise(
                "dis", patterns=["->  0x400000000000018b <+28>: local.get 2"]
            )

            self.send_lldb_command_or_raise("fin")
            self.send_lldb_command_or_raise(
                "dis", patterns=["->  0x40000000000001e0 <+61>: i32.store 0"]
            )

        # Step Instruction
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )

        self.send_lldb_command_or_raise(
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
            self.send_lldb_command_or_raise("si")
            self.send_lldb_command_or_raise("dis", patterns=pattern)

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def memoryTest(self):
        self.send_lldb_command_or_raise(
            "mem reg --all",
            patterns=[
                "[0x0000000000000000-0x0000000001010000) rw- wasm_memory_0_0",
                "[0x0000000001010000-0x4000000000000000) ---",
                "[0x4000000000000000-0x40000000000014e0) r-x wasm_module_0",
                "[0x40000000000014e0-0xffffffffffffffff) ---",
            ],
        )

        self.send_lldb_command_or_raise(
            "mem reg 0x4000000000000000",
            patterns=["[0x4000000000000000-0x40000000000014e0) r-x wasm_module_0"],
        )

        self.send_lldb_command_or_raise(
            "mem reg 0x0000000000000000",
            patterns=["[0x0000000000000000-0x0000000001010000) rw- wasm_memory_0_0"],
        )


class SwiftWasmTestCase(BaseTestCase):

    def __init__(self, build_config: str = None, port: int = None):
        super().__init__(build_config, port)

    def execute(self):

        self.setup_debugging_session_or_raise("resources/swift-wasm/test/main.js")

        try:
            for _ in range(1):
                self.continueInterruptTest()
                self.breakpointTest()
                self.inspectionTest()
                self.stepTest()

        except Exception as e:
            raise Exception(f"Breakpoint test failed: {e}")

    def continueInterruptTest(self):
        cycles = 10
        for cycle in range(1, cycles + 1):
            try:
                self.send_lldb_command_or_raise("c")
                self.send_lldb_command_or_raise("process interrupt")
            except Exception as e:
                raise Exception(f"Cycle {cycle} failed: {e}")

    def breakpointTest(self):
        self.send_lldb_command_or_raise(f"b processNumber")

        for _ in range(10):
            self.send_lldb_command_or_raise(
                "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
            )

        self.send_lldb_command_or_raise("b test.swift:17")

        self.send_lldb_command_or_raise(
            "br list",
            patterns=[
                "Current breakpoints:",
                "name = 'processNumber'",
                "file = 'test.swift', line = 17",
            ],
        )

        for _ in range(10):
            self.send_lldb_command_or_raise(
                "c",
                patterns=[
                    "test.swift:15",
                    "test.swift:17",
                ],
                mode=PatternMatchMode.ANY,
            )

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (2 breakpoints)"]
        )

    def inspectionTest(self):
        self.send_lldb_command_or_raise(
            "target modules list", patterns=["(0x4000000000000000)"]
        )

        self.send_lldb_command_or_raise(
            "list 1",
            patterns=[
                "func doubleValue(_ x: Int32) -> Int32",
                "func addNumbers(_ a: Int32, _ b: Int32) -> Int32",
                "func isEven(_ n: Int32) -> Bool",
            ],
        )

        self.send_lldb_command_or_raise(f"b isEven")
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.send_lldb_command_or_raise(
            "dis", patterns=["->  0x4000000000010960 <+28>: block"]
        )

        self.send_lldb_command_or_raise(
            "bt",
            patterns=[
                "frame #0: 0x4000000000010960",
                "frame #1: 0x4000000000010a05",
                "frame #2: 0x400000000001098f",
            ],
        )

        self.send_lldb_command_or_raise("var", patterns=["n ="])

        self.send_lldb_command_or_raise(
            "thread list",
            patterns=[
                "0x4000000000010960",
                "test.swift:10:7",
            ],
        )

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def stepTest(self):
        self.send_lldb_command_or_raise("b processNumber")

        # TODO: Step over - Current Swift LLDB is problematic with step over

        # Step Into
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.send_lldb_command_or_raise(
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
            self.send_lldb_command_or_raise("s")
            self.send_lldb_command_or_raise("dis", patterns=pattern)

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        # Step Out
        self.send_lldb_command_or_raise("b addNumbers")

        for _ in range(10):
            self.send_lldb_command_or_raise(
                "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
            )
            self.send_lldb_command_or_raise(
                "dis", patterns=["->  0x4000000000010924 <+46>: i32.lt_s"]
            )

            self.send_lldb_command_or_raise("fin")
            self.send_lldb_command_or_raise(
                "dis", patterns=["->  0x40000000000109e8 <+85>:  local.set 5"]
            )

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

        # Step Instruction
        self.send_lldb_command_or_raise("b processNumber")
        self.send_lldb_command_or_raise(
            "c", patterns=["Process 1 stopped", "stop reason = breakpoint"]
        )
        self.send_lldb_command_or_raise(
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
            self.send_lldb_command_or_raise("si")
            self.send_lldb_command_or_raise("dis", patterns=pattern)

        self.send_lldb_command_or_raise(
            "br del -f", patterns=["All breakpoints removed. (1 breakpoint)"]
        )

    def memoryTest(self):
        self.send_lldb_command_or_raise(
            "mem reg --all",
            patterns=[
                "[0x0000000000000000-0x0000000000130000) rw- wasm_memory_0_0",
                "[0x0000000000130000-0x4000000000000000) ---",
                "[0x4000000000000000-0x40000000006b1239) r-x wasm_module_0",
                "[0x40000000006b1239-0xffffffffffffffff) ---",
            ],
        )

        self.send_lldb_command_or_raise(
            "mem reg 0x4000000000000000",
            patterns=["[0x4000000000000000-0x40000000006b1239) r-x wasm_module_0"],
        )

        self.send_lldb_command_or_raise(
            "mem reg 0x0000000000000000",
            patterns=["[0x0000000000000000-0x0000000000130000) rw- wasm_memory_0_0"],
        )
