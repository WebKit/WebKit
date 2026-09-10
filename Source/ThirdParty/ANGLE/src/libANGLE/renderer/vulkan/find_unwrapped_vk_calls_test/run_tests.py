#!/usr/bin/env python3
"""End-to-end tests for find_unwrapped_vk_calls.py.

Runs the script against test C++ files and compares the complete
stdout, stderr, and returncode against expected values.
"""

import os
import re
import subprocess
import sys
import unittest

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", "..", ".."))

VULKAN_DIR = os.path.join("src", "libANGLE", "renderer", "vulkan")
SCRIPT_PATH = os.path.join(VULKAN_DIR, "find_unwrapped_vk_calls.py")
TEST_FILES_DIR = os.path.join(VULKAN_DIR, "find_unwrapped_vk_calls_test", "test_files")


class EndToEndTest(unittest.TestCase):
    """End-to-end tests running the full script against test files."""

    maxDiff = None  # Show full diff on assertion failures

    def _run_script(self, extra_args=None):
        """Run the script and return (stdout, stderr, returncode).

        Args:
            extra_args: Additional arguments to pass to the script.
        """

        cmd = [sys.executable, SCRIPT_PATH, "--search-dir", TEST_FILES_DIR]
        if extra_args:
            cmd.extend(extra_args)

        result = subprocess.run(cmd, capture_output=True, text=True, cwd=ROOT_DIR)

        return result.stdout, result.stderr, result.returncode

    @staticmethod
    def _normalize_volatile_values(text):
        """Replace volatile values in output with a stable placeholder.

        The number of Vulkan function names found in volk.h and the total
        number of naming variations depend on the version of volk.h being
        used, so they should be ignored during comparison.
        """
        text = re.sub(r"Found \d+ Vulkan function names in volk\.h",
                      "Found N Vulkan function names in volk.h", text)
        text = re.sub(r"Total function names and variations: \d+",
                      "Total function names and variations: N", text)
        return text

    def test_unwrapped_calls_found(self):
        """Script should detect unwrapped calls in test files.

        Compares complete stdout, stderr, and returncode against expected values.
        """

        volk_h_path = os.path.join("src", "third_party", "volk", "volk.h")
        test_files_dir_plus_sep = os.path.join(TEST_FILES_DIR, "")

        expected_stdout = f"""\
Extracting Vulkan function names from: {volk_h_path}
Found N Vulkan function names in volk.h
Generating naming variations...
Total function names and variations: N

Searching for source files in: {TEST_FILES_DIR}
Found 4 source files to analyze

================================================================================
Found 26 unwrapped Vulkan API calls
================================================================================


{test_files_dir_plus_sep}comments_and_strings.cpp: 3 unwrapped call(s)
------------------------------------------------------------
  Line 10: vkQueueSubmit(queue, 1, &submitIn
  Line 14: vkCmdDraw(cmdBuffer, vertexCo
  Line 25: vkDestroyDevice(device, nullptr);

{test_files_dir_plus_sep}declarations_and_members.cpp: 1 unwrapped call(s)
------------------------------------------------------------
  Line 33: vkGetDeviceQueue(device, queueFamily

{test_files_dir_plus_sep}unwrapped_calls.cpp: 22 unwrapped call(s)
------------------------------------------------------------
  Line 6: vkQueueSubmit(queue, 1, &submitIn
  Line 7: vkCmdDraw(cmdBuffer, vertexCo
  Line 8: vkCreateDevice(physicalDevice, &cr
  Line 11: queueSubmit(queue, 1, &submitIn
  Line 12: p_queueSubmit(queue, 1, &submitIn
  Line 13: p_vkQueueSubmit(queue, 1, &submitIn
  Line 14: pfn_queueSubmit(queue, 1, &submitIn
  Line 15: pfn_vkQueueSubmit(queue, 1, &submitIn
  Line 18: pCmdDraw(cmdBuffer, vertexCo
  Line 19: p_CmdDraw(cmdBuffer, vertexCo
  Line 20: pfnCmdDraw(cmdBuffer, vertexCo
  Line 21: pfn_CmdDraw(cmdBuffer, vertexCo
  Line 22: pVkCmdDraw(cmdBuffer, vertexCo
  Line 23: p_VkCmdDraw(cmdBuffer, vertexCo
  Line 24: pfnVkCmdDraw(cmdBuffer, vertexCo
  Line 25: pfn_VkCmdDraw(cmdBuffer, vertexCo
  Line 28: cmd_draw(cmdBuffer, vertexCo
  Line 29: p_cmd_draw(cmdBuffer, vertexCo
  Line 30: pfn_cmd_draw(cmdBuffer, vertexCo
  Line 31: cmd_set_viewport_w_scaling_nv(cmdBuffer, 0, 1, &v
  Line 32: create_direct_fb_surface_ext(instance, &createIn
  Line 33: create_ios_surface_mvk(instance, &createIn

================================================================================
SUMMARY
================================================================================
Total files with unwrapped calls: 3
Total unwrapped Vulkan API calls: 26

Unique Vulkan functions called without wrapping: 24
  cmd_draw: 1 occurrence(s)
  cmd_set_viewport_w_scaling_nv: 1 occurrence(s)
  create_direct_fb_surface_ext: 1 occurrence(s)
  create_ios_surface_mvk: 1 occurrence(s)
  pCmdDraw: 1 occurrence(s)
  pVkCmdDraw: 1 occurrence(s)
  p_CmdDraw: 1 occurrence(s)
  p_VkCmdDraw: 1 occurrence(s)
  p_cmd_draw: 1 occurrence(s)
  p_queueSubmit: 1 occurrence(s)
  p_vkQueueSubmit: 1 occurrence(s)
  pfnCmdDraw: 1 occurrence(s)
  pfnVkCmdDraw: 1 occurrence(s)
  pfn_CmdDraw: 1 occurrence(s)
  pfn_VkCmdDraw: 1 occurrence(s)
  pfn_cmd_draw: 1 occurrence(s)
  pfn_queueSubmit: 1 occurrence(s)
  pfn_vkQueueSubmit: 1 occurrence(s)
  queueSubmit: 1 occurrence(s)
  vkCmdDraw: 2 occurrence(s)
  vkCreateDevice: 1 occurrence(s)
  vkDestroyDevice: 1 occurrence(s)
  vkGetDeviceQueue: 1 occurrence(s)
  vkQueueSubmit: 2 occurrence(s)
"""
        expected_stderr = ""
        expected_returncode = 2

        stdout, stderr, returncode = self._run_script()

        self.assertEqual(returncode, expected_returncode, "returncode mismatch")
        self.assertMultiLineEqual(stderr, expected_stderr, "stderr mismatch")
        self.assertMultiLineEqual(
            self._normalize_volatile_values(stdout), expected_stdout, "stdout mismatch")

    def test_search_directory_not_found(self):
        """Script should fail with error if search directory is not found.

        Compares complete stdout, stderr, and returncode against expected values.
        """

        expected_stdout = ""
        expected_stderr = "Error: Search directory not found: /nonexistent/directory\n"
        expected_returncode = 1

        stdout, stderr, returncode = self._run_script(["--search-dir", "/nonexistent/directory"])

        self.assertEqual(returncode, expected_returncode, "returncode mismatch")
        self.assertMultiLineEqual(stderr, expected_stderr, "stderr mismatch")
        self.assertMultiLineEqual(stdout, expected_stdout, "stdout mismatch")


if __name__ == "__main__":
    unittest.main()
