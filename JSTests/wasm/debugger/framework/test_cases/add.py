from ..base import BaseTestCase, PatternMatchMode


class AddTestCase(BaseTestCase):

    def __init__(self, build_config: str = None, port: int = None):
        super().__init__(build_config, port)

    def execute(self):

        self.setup_debugging_session_or_raise("add/main.js")

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
                "[0x4000000000000000-0x40000000000013de) r-x wasm_module_0",
                "[0x40000000000013de-0xffffffffffffffff) ---",
            ],
        )

        self.send_lldb_command_or_raise(
            "mem reg 0x4000000000000000",
            patterns=["[0x4000000000000000-0x40000000000013de) r-x wasm_module_0"],
        )

        self.send_lldb_command_or_raise(
            "mem reg 0x0000000000000000",
            patterns=["[0x0000000000000000-0x0000000001010000) rw- wasm_memory_0_0"],
        )
