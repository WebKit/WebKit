#!/usr/bin/env python3
"""
Script to find Vulkan API calls that are NOT wrapped with wrapper macros (see WRAPPER_MACROS).
Uses parenthesis-based parsing instead of regex for detecting wrapped calls.
Searches for Vulkan functions defined in volk.h as PFN_vk* types, including naming variations
(e.g., vkGetDeviceQueue, GetDeviceQueue, getDeviceQueue, pfn_vkGetDeviceQueue, etc.).
"""

import argparse
import os
import re
import sys
from concurrent.futures import ProcessPoolExecutor
from dataclasses import dataclass
from typing import List, Set

# =============================================================================
# Configuration
# =============================================================================

ROOT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "..", "..", ".."))

# Path to volk.h file containing PFN_vk* function definitions
VOLK_H_PATH = os.path.join(ROOT_DIR, "src", "third_party", "volk", "volk.h")

# Default directory to search for source files
DEFAULT_SEARCH_DIR = os.path.join(ROOT_DIR, "src", "libANGLE", "renderer", "vulkan")

# Test directory to exclude when searching in default directory
TEST_DIR = os.path.join(DEFAULT_SEARCH_DIR, "find_unwrapped_vk_calls_test")

# File extensions to search for source files
SOURCE_FILE_EXTENSIONS = ('.h', '.cpp', '.mm')

# Wrapper macros that indicate a Vulkan call is properly wrapped
# Note: VK_CALL is not included because it does not look like a Vulkan call. Example:
#       VK_CALL(vkGetDeviceQueue2, device, &queueInfo2, queue);
WRAPPER_MACROS = {'VK_CALL_WITH_GROUP', 'VK_SECONDARY_CMD_CALL'}

# Timer class that indicates Vulkan calls are being timed (also considered wrapped)
TIMER_CLASS_NAME = 'ScopedVulkanApiPerfTimer'

# Number of characters to show before and after the function name in context output
CONTEXT_CHAR_LIMIT = 20

# =============================================================================
# Denylist of function names that may look like Vulkan functions but are not
# Vulkan API calls. This includes member functions and other non-Vulkan functions.
# Add more names here as needed.
# =============================================================================
NON_VULKAN_FUNCTIONS = {'createPipelineLayout', 'endCommandBuffer'}

# =============================================================================
# Keywords that indicate a function declaration rather than a function call.
# Used by is_function_declaration() to skip function declarations/definitions.
# Includes return types (e.g., 'void', 'VkResult') and modifiers (e.g., 'inline', 'static').
#
# Supported formats:
#   - Simple types: 'void', 'VkResult', 'size_t'
#   - Namespace-qualified types: 'angle::Result'
#   - Modifiers: 'inline', 'static', 'constexpr', 'const'
#
# Unsupported formats (will not be matched):
#   - Template types: 'SomeType<T>', 'std::vector<int>'
#   - Types with modifiers: 'const char*' (only base type 'char' would match)
#   - Pointer/reference notation: 'Type*', 'Type&' (modifiers are stripped before matching)
#
# Add more keywords here as needed.
# =============================================================================
FUNCTION_DECLARATION_KEYWORDS = {
    # Standard types
    'int32_t',
    'int64_t',
    'size_t',
    'uint32_t',
    'uint64_t',
    'void',
    # Modifiers
    'const',
    'constexpr',
    'inline',
    'static',
    'ANGLE_INLINE',
    # Vulkan types
    'PFN_vkVoidFunction',
    'VkBool32',
    'VkDeviceSize',
    'VkExtensionProperties',
    'VkExtent2D',
    'VkExtent3D',
    'VkFlags',
    'VkFormatProperties',
    'VkImageFormatProperties',
    'VkLayerProperties',
    'VkMemoryHeap',
    'VkMemoryType',
    'VkPhysicalDeviceFeatures',
    'VkPhysicalDeviceFeatures2',
    'VkPhysicalDeviceMemoryProperties',
    'VkPhysicalDeviceProperties',
    'VkPhysicalDeviceProperties2',
    'VkQueueFamilyProperties',
    'VkRect2D',
    'VkResult',
    'VkSampleCountFlags',
    'VkSparseImageFormatProperties',
    # ANGLE types
    'angle::Result',
    'Result',
    # ANGLE Impl types
    'BufferImpl',
    'ContextImpl',
    'DeviceImpl',
    'DisplayImpl',
    'FenceImpl',
    'FramebufferImpl',
    'ImageImpl',
    'ProgramImpl',
    'QueryImpl',
    'RenderbufferImpl',
    'SamplerImpl',
    'SemaphoreImpl',
    'ShaderImpl',
    'StreamImpl',
    'SurfaceImpl',
    'TextureImpl',
    'TransformFeedbackImpl',
}


@dataclass
class VkCallLocation:
    """Represents a Vulkan API call location in source code."""
    file_path: str
    line_number: int
    call_text: str
    context: str


def camel_to_snake(name: str) -> str:
    """Convert CamelCase to snake_case.

    Handles consecutive uppercase letters as a group (acronyms):
      QueuePresentKHR          -> queue_present_khr
      CmdSetViewportWScalingNV -> cmd_set_viewport_w_scaling_nv
      CreateDirectFBSurfaceEXT -> create_direct_fb_surface_ext
      CreateIOSSurfaceMVK      -> create_ios_surface_mvk
    """
    if not name:
        return ''

    result = []
    n = len(name)
    for i, char in enumerate(name):
        if char.isupper() and i > 0:
            prev_lower = name[i - 1].islower()
            next_lower = (i + 1 < n) and name[i + 1].islower()
            # Add underscore before this uppercase char if:
            # 1. Previous char is lowercase (e.g., "getD" -> "get_d")
            # 2. Or this is the last uppercase in a run followed by lowercase
            #    (e.g., "VKStructure" -> "vk_structure", not "v_ks_tructure")
            if prev_lower or next_lower:
                result.append('_')
        result.append(char.lower())
    return ''.join(result)


def generate_function_variations(func_name: str) -> Set[str]:
    """
    Generate naming variations for a vk* function name.

    For example, vkGetDeviceQueue2 produces:

    1. Lower camelCase with ("", "p_", "pfn_") prefixes:
       - getDeviceQueue2, p_getDeviceQueue2, pfn_getDeviceQueue2
       - vkGetDeviceQueue2, p_vkGetDeviceQueue2, pfn_vkGetDeviceQueue2

    2. Capital CamelCase with ("p", "pfn", "p_", "pfn_") prefixes (NOT starting with capital letter):
       - pGetDeviceQueue2, pfnGetDeviceQueue2, p_GetDeviceQueue2, pfn_GetDeviceQueue2
       - pVkGetDeviceQueue2, pfnVkGetDeviceQueue2, p_VkGetDeviceQueue2, pfn_VkGetDeviceQueue2

    3. Lower snake_case with ("", "p_", "pfn_") prefixes:
       - get_device_queue2, p_get_device_queue2, pfn_get_device_queue2
    """
    variations = set()

    # Strip the 'vk' prefix to get the capital camel case name
    if not func_name.startswith('vk'):
        return variations

    capital_camel = func_name[2:]  # Remove 'vk' prefix

    # Check that capital_camel starts with a capital letter
    if not capital_camel or not capital_camel[0].isupper():
        return variations

    # 1. Lower camelCase variations (with "", "p_", "pfn_" prefixes)
    lower_camel = capital_camel[0].lower() + capital_camel[1:]
    for prefix in ('', 'p_', 'pfn_'):
        variations.add(prefix + lower_camel)
        # Also add with 'vk' prefix (lowercase) for lower camelCase
        variations.add(prefix + 'vk' + capital_camel)

    # 2. Capital CamelCase variations with prefixes that prevent starting with capital letter
    # (with "p", "pfn", "p_", "pfn_" prefixes - NOT empty prefix to avoid capital-first names)
    for prefix in ('p', 'pfn', 'p_', 'pfn_'):
        variations.add(prefix + capital_camel)
        # Also add with 'Vk' prefix (capital V) for capital CamelCase
        variations.add(prefix + 'Vk' + capital_camel)

    # 3. Lower snake_case variations (with "", "p_", "pfn_" prefixes)
    snake_case = camel_to_snake(capital_camel)
    for prefix in ('', 'p_', 'pfn_'):
        variations.add(prefix + snake_case)

    return variations


def extract_vk_function_names_from_volk(volk_h_path: str) -> Set[str]:
    """
    Extract all vk* function names from volk.h that are defined as PFN_vk* types.
    Returns a set of base function names (e.g., 'vkCreateDevice', 'vkQueueSubmit').
    Does NOT include naming variations - use generate_all_function_variations() for that.
    """
    vk_functions = set()

    try:
        with open(volk_h_path, 'r', encoding='utf-8', errors='replace') as f:
            content = f.read()
    except Exception as e:
        print(f"Error reading {volk_h_path}: {e}", file=sys.stderr)
        return vk_functions

    # Pattern to match PFN_vkFunctionName declarations
    # Examples:
    #   PFN_vkAllocateCommandBuffers vkAllocateCommandBuffers;
    #   extern PFN_vkCreateDevice vkCreateDevice;
    pattern = r'PFN_(vk\w+)\s+(vk\w+)\s*[;=]'

    for match in re.finditer(pattern, content):
        func_name = match.group(2)
        vk_functions.add(func_name)

    return vk_functions


def generate_all_function_variations(func_names: Set[str]) -> Set[str]:
    """
    Generate all naming variations for a set of Vulkan function names.
    Returns a set containing the original names plus all variations.
    """
    all_variations = set()
    for func_name in func_names:
        all_variations.add(func_name)
        all_variations.update(generate_function_variations(func_name))
    return all_variations


def remove_comments_and_string_contents(content: str) -> str:
    """
    Remove C++ comments (single-line // and multi-line /* */) and neutralize string literals.
    Macro definitions (preprocessor directives) are kept as-is.
    Returns the cleaned content with newlines preserved for line counting.
    String literals are preserved but their content is replaced with spaces to avoid
    false positives from strings containing macro names.
    Handles C++11 raw string literals: R"delimiter(content)delimiter"
    """
    result = []
    i = 0
    n = len(content)

    while i < n:
        # Check for C++11 raw string literal: R"delimiter(content)delimiter"
        # The format is: R"delimiter(content)delimiter" where delimiter can be empty or any sequence
        if content[i] == 'R' and i + 1 < n and content[i + 1] == '"':
            result.append('R')
            result.append('"')
            i += 2

            # Extract the delimiter (characters between R" and opening paren)
            delimiter_start = i
            while i < n and content[i] != '(':
                result.append(content[i])
                i += 1

            if i < n and content[i] == '(':
                delimiter = content[delimiter_start:i]
                result.append('(')
                i += 1

                # Find the closing )delimiter"
                # We need to find ) followed by delimiter followed by "
                while i < n:
                    if content[i] == ')':
                        # Check if this is followed by the delimiter and closing quote
                        closing_pos = i + 1
                        delim_pos = 0
                        while delim_pos < len(delimiter) and closing_pos < n and content[
                                closing_pos] == delimiter[delim_pos]:
                            closing_pos += 1
                            delim_pos += 1

                        if delim_pos == len(
                                delimiter) and closing_pos < n and content[closing_pos] == '"':
                            # Found the closing )delimiter"
                            result.append(')')
                            for ch in delimiter:
                                result.append(ch)
                            result.append('"')
                            i = closing_pos + 1
                            break
                        else:
                            # Just a ) in the content, replace with space
                            result.append(' ')
                            i += 1
                    elif content[i] == '\n':
                        result.append('\n')
                        i += 1
                    else:
                        result.append(' ')
                        i += 1
            continue

        # Check for string literal
        if content[i] == '"':
            # Handle string literal
            result.append('"')
            i += 1
            while i < n:
                if content[i] == '\\' and i + 1 < n:
                    # Escape sequence - replace with spaces
                    result.append(' ')
                    result.append(' ')
                    i += 2
                elif content[i] == '"':
                    result.append('"')
                    i += 1
                    break
                elif content[i] == '\n':
                    result.append('\n')
                    i += 1
                else:
                    result.append(' ')
                    i += 1
        # Check for character literal
        elif content[i] == "'":
            result.append("'")
            i += 1
            while i < n:
                if content[i] == '\\' and i + 1 < n:
                    result.append(' ')
                    result.append(' ')
                    i += 2
                elif content[i] == "'":
                    result.append("'")
                    i += 1
                    break
                elif content[i] == '\n':
                    result.append('\n')
                    i += 1
                else:
                    result.append(' ')
                    i += 1
        # Check for single-line comment
        elif i + 1 < n and content[i:i + 2] == '//':
            # Skip until end of line
            while i < n and content[i] != '\n':
                i += 1
            # Keep the newline for line counting
            if i < n:
                result.append('\n')
                i += 1
        # Check for multi-line comment
        elif i + 1 < n and content[i:i + 2] == '/*':
            i += 2
            # Skip until */
            while i + 1 < n and content[i:i + 2] != '*/':
                # Preserve newlines for line counting
                if content[i] == '\n':
                    result.append('\n')
                else:
                    result.append(' ')
                i += 1
            if i + 1 < n:
                i += 2  # Skip */
        else:
            # Keep all other characters including preprocessor directives
            result.append(content[i])
            i += 1

    return ''.join(result)


def get_line_number(content: str, pos: int) -> int:
    """Get the 1-based line number for a position in the content."""
    return content[:pos].count('\n') + 1


def is_member_or_namespace_call(content: str, func_name_start: int) -> bool:
    """
    Check if this function call is a member call (preceded by . or ->)
    or namespace qualified call (preceded by ::).

    Returns True if it's a member/namespace call, False if it's a global call.
    """
    # Look backwards from the function name
    i = func_name_start - 1

    # Skip whitespace backwards
    while i >= 0 and content[i] in ' \t\n\r':
        i -= 1

    if i < 0:
        return False

    # Check for member access operators . or -> or namespace qualifier ::
    if content[i] == '.':
        return True
    if content[i] == '>' and i > 0 and content[i - 1] == '-':
        return True
    if content[i] == ':' and i > 0 and content[i - 1] == ':':
        return True

    return False


def is_function_declaration(content: str, func_name_start: int) -> bool:
    """
    Check if this function is a declaration/definition rather than a call.

    A function declaration/definition has the pattern:
        [return_type] functionName(

    Where return_type is typically void, VkResult, etc.
    A function call would be:
        functionName(
    where the preceding text is not a return type.
    """
    # Look backwards from the function name to find what precedes it
    i = func_name_start - 1

    # Skip whitespace, * and & backwards (treat * and & same as whitespace for return types)
    while i >= 0 and content[i] in ' \t\n\r*&':
        i -= 1

    if i < 0:
        return False

    # If preceded by certain characters, it's likely a call
    # e.g., (vkFunc(), =vkFunc(), ;vkFunc(), {vkFunc()
    if content[i] in '(;={},<>|!+-/%':
        return False

    # Extract the type/identifier before the function name
    # This handles namespace-qualified types like "angle::Result"
    id_end = i + 1
    while i >= 0 and (content[i].isalnum() or content[i] in '_:'):
        i -= 1
    id_start = i + 1

    if id_start == id_end:
        return False

    preceding_id = content[id_start:id_end]

    # Normalize the identifier by removing any leading/trailing colons
    # (handles cases like "::namespace::Type" or partial matches)
    preceding_id = preceding_id.strip(':')

    return preceding_id in FUNCTION_DECLARATION_KEYWORDS


def is_wrapped_in_macro(content: str, call_start: int) -> bool:
    """
    Check if the Vulkan call at call_start is wrapped in one of the WRAPPER_MACROS.
    Uses parenthesis matching to determine this.

    Returns True if wrapped, False otherwise.
    Note: Assumes call_start points to a valid function call.
    """
    # Look backwards from the call to see if we're inside a wrapping macro call
    # The structure would be: MACRO_NAME( ... functionName( ... ) ... )

    paren_depth = 0  # Track depth of nested parens we're inside
    brace_depth = 0  # Track depth of nested braces (for temporary objects like Type{})
    i = call_start - 1

    while i >= 0:
        ch = content[i]

        if ch == ')':
            paren_depth += 1
        elif ch == '(':
            if paren_depth > 0:
                paren_depth -= 1
            else:
                # Found an enclosing opening paren at depth 0
                # Check if this belongs to a wrapping macro
                paren_pos = i

                # Extract macro name before this paren (skip whitespace, then extract backwards)
                macro_name_end = paren_pos
                while macro_name_end > 0 and content[macro_name_end - 1] in ' \t\n\r':
                    macro_name_end -= 1
                macro_name_start = macro_name_end
                while macro_name_start > 0 and (content[macro_name_start - 1].isalnum() or
                                                content[macro_name_start - 1] == '_'):
                    macro_name_start -= 1

                if macro_name_start != macro_name_end:
                    macro_name = content[macro_name_start:macro_name_end]
                    if macro_name in WRAPPER_MACROS:
                        return True

                # Not a wrapping macro, continue looking for outer parens
        elif ch == '}':
            brace_depth += 1
        elif ch == '{':
            if brace_depth > 0:
                brace_depth -= 1
            elif paren_depth == 0:
                # Statement boundary (not inside parens) - stop looking
                break
        elif ch == ';' and paren_depth == 0 and brace_depth == 0:
            # Statement boundary - stop looking
            break

        i -= 1

    return False


def find_unwrapped_vk_calls_in_file(file_path: str,
                                    vk_functions: Set[str]) -> List[VkCallLocation]:
    """Find all Vulkan API calls in a file that are not wrapped with the required macros."""

    try:
        with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
            original_content = f.read()
    except Exception as e:
        print(f"Error reading {file_path}: {e}", file=sys.stderr)
        return []

    # Remove comments and neutralize string literals (keep macro definitions)
    cleaned_content = remove_comments_and_string_contents(original_content)

    vk_calls = []
    n = len(cleaned_content)
    i = 0

    # Track if we're inside a timer class scope (see TIMER_CLASS_NAME)
    timer_scope_depth = 0  # 0 means not inside timer scope
    brace_depth = 0

    while i < n:
        # Track brace depth for scope tracking
        if cleaned_content[i] == '{':
            brace_depth += 1
            i += 1
            continue
        elif cleaned_content[i] == '}':
            # Exit timer scope when leaving the brace level where timer was declared
            if timer_scope_depth > 0 and brace_depth == timer_scope_depth:
                timer_scope_depth = 0
            brace_depth -= 1
            i += 1
            continue

        # Check for timer class declaration (only if not already in a timer scope)
        if timer_scope_depth == 0:
            if cleaned_content[i:i + len(TIMER_CLASS_NAME)] == TIMER_CLASS_NAME:
                # Found a timer declaration - track this scope
                # The timer is at the current brace depth
                timer_scope_depth = brace_depth
                i += len(TIMER_CLASS_NAME)
                continue

        # Look for identifier start (letter or underscore)
        if cleaned_content[i].isalpha() or cleaned_content[i] == '_':
            # Extract the full identifier name
            func_name_start = i
            func_name_end = i
            while func_name_end < n and (cleaned_content[func_name_end].isalnum() or
                                         cleaned_content[func_name_end] == '_'):
                func_name_end += 1

            func_name = cleaned_content[func_name_start:func_name_end]

            # Check if this is a known vk function name or variation
            if func_name in vk_functions:
                # Skip whitespace to find opening paren
                open_paren_pos = func_name_end
                while open_paren_pos < n and cleaned_content[open_paren_pos] in ' \t\n\r':
                    open_paren_pos += 1

                if open_paren_pos < n and cleaned_content[open_paren_pos] == '(':
                    # Check if this function is in the denylist of non-Vulkan functions
                    if func_name in NON_VULKAN_FUNCTIONS:
                        # Skip - this is a known non-Vulkan function
                        i = func_name_end
                        continue

                    # Check if this is a call without arguments - Vulkan API calls always have arguments
                    # Skip whitespace after opening paren
                    close_paren_pos = open_paren_pos + 1
                    while close_paren_pos < n and cleaned_content[close_paren_pos] in ' \t\n\r':
                        close_paren_pos += 1

                    if close_paren_pos < n and cleaned_content[close_paren_pos] == ')':
                        # This is a call without arguments - not a Vulkan API call
                        i = func_name_end
                        continue

                    # This is a vk*() call or declaration
                    # First check if this is a function declaration/definition
                    if is_function_declaration(cleaned_content, func_name_start):
                        # Skip function declarations/definitions
                        i = func_name_end
                        continue

                    # Check if this is a member or namespace call (not a global Vulkan call)
                    if is_member_or_namespace_call(cleaned_content, func_name_start):
                        # Skip member/namespace calls
                        i = func_name_end
                        continue

                    # Check if it's wrapped (by macro or inside timer scope)
                    inside_timer_scope = (timer_scope_depth > 0)
                    if not inside_timer_scope and not is_wrapped_in_macro(
                            cleaned_content, func_name_start):
                        # Unwrapped call found
                        line_num = get_line_number(cleaned_content, func_name_start)

                        # Get the single line containing the function name
                        line_start = cleaned_content.rfind('\n', 0, func_name_start) + 1
                        line_end = cleaned_content.find('\n', func_name_start)
                        if line_end == -1:
                            line_end = n

                        # Get context chars before and after the function name, limited to the line
                        context_start = max(line_start, func_name_start - CONTEXT_CHAR_LIMIT)
                        context_end = min(line_end, func_name_end + CONTEXT_CHAR_LIMIT)
                        context = cleaned_content[context_start:context_end].strip()

                        vk_calls.append(
                            VkCallLocation(
                                file_path=file_path,
                                line_number=line_num,
                                call_text=func_name,
                                context=context))

            i = func_name_end
        else:
            i += 1

    return vk_calls


def find_source_files(directory: str, exclude_dir: str = None) -> List[str]:
    """Find all source files in the directory with configured extensions (see SOURCE_FILE_EXTENSIONS).
    If exclude_dir is specified, skip that directory during traversal.
    Comparison is done by full path (not just directory name) to avoid accidentally
    excluding directories that happen to share the same name at different locations."""
    source_files = []

    exclude_dir_abs = os.path.abspath(exclude_dir) if exclude_dir else None

    for root, dirs, files in os.walk(directory):
        # Modify dirs in-place to control which subdirectories os.walk descends into.
        if exclude_dir_abs:
            dirs[:] = [
                d for d in dirs if os.path.abspath(os.path.join(root, d)) != exclude_dir_abs
            ]

        for file in files:
            if file.endswith(SOURCE_FILE_EXTENSIONS):
                full_path = os.path.join(root, file)
                source_files.append(full_path)

    return source_files


def _process_file_wrapper(args):
    """Wrapper function for parallel processing of files."""
    file_path, vk_functions = args
    return find_unwrapped_vk_calls_in_file(file_path, vk_functions)


def main():
    volk_h_path = os.path.relpath(VOLK_H_PATH)
    default_search_dir = os.path.relpath(DEFAULT_SEARCH_DIR)
    test_dir = os.path.relpath(TEST_DIR)

    parser = argparse.ArgumentParser(
        description='Find Vulkan API calls that are NOT wrapped with wrapper macros.')
    parser.add_argument(
        '--search-dir',
        default=default_search_dir,
        help=f'Directory to search for source files (default: "{default_search_dir}")')
    parser.add_argument(
        '--exclude-dir',
        default=None,
        help=f'Directory to exclude from search (default: "{test_dir}" '
        f'when --search-dir is also not specified)')
    args = parser.parse_args()

    # Default exclude directory when using default search directory
    exclude_dir = args.exclude_dir
    if exclude_dir is None and args.search_dir == default_search_dir:
        exclude_dir = test_dir

    # Check if volk.h exists
    if not os.path.isfile(volk_h_path):
        print(f"Error: volk.h file not found: {volk_h_path}", file=sys.stderr)
        sys.exit(1)

    # Check if search directory exists
    if not os.path.isdir(args.search_dir):
        print(f"Error: Search directory not found: {args.search_dir}", file=sys.stderr)
        sys.exit(1)

    # Extract Vulkan function names from volk.h
    print(f"Extracting Vulkan function names from: {volk_h_path}")
    vk_base_functions = extract_vk_function_names_from_volk(volk_h_path)
    print(f"Found {len(vk_base_functions)} Vulkan function names in volk.h")

    if not vk_base_functions:
        print("Error: No Vulkan functions found in volk.h", file=sys.stderr)
        sys.exit(1)

    # Generate all naming variations
    print("Generating naming variations...")
    vk_functions = generate_all_function_variations(vk_base_functions)
    print(f"Total function names and variations: {len(vk_functions)}")
    print()

    # Find all source files
    print(f"Searching for source files in: {args.search_dir}")
    source_files = find_source_files(args.search_dir, exclude_dir)
    print(f"Found {len(source_files)} source files to analyze")
    print()

    # Find all unwrapped Vulkan calls using parallel processing
    all_unwrapped_calls = []

    # Prepare arguments for parallel processing
    process_args = [(fp, vk_functions) for fp in sorted(source_files)]

    # Use ProcessPoolExecutor for parallel file processing
    with ProcessPoolExecutor() as executor:
        results = executor.map(_process_file_wrapper, process_args)
        for unwrapped in results:
            all_unwrapped_calls.extend(unwrapped)

    # Print results
    print("=" * 80)
    if all_unwrapped_calls:
        print(f"Found {len(all_unwrapped_calls)} unwrapped Vulkan API calls")
    else:
        print("All Vulkan API calls are properly wrapped.")
    print("=" * 80)
    print()

    if not all_unwrapped_calls:
        return

    # Group by file
    calls_by_file = {}
    for call in all_unwrapped_calls:
        if call.file_path not in calls_by_file:
            calls_by_file[call.file_path] = []
        calls_by_file[call.file_path].append(call)

    # Print summary by file
    for file_path in sorted(calls_by_file.keys()):
        calls = calls_by_file[file_path]
        print(f"\n{file_path}: {len(calls)} unwrapped call(s)")
        print("-" * 60)
        for call in calls:
            print(f"  Line {call.line_number}: {call.context}")

    # Print summary
    print("\n" + "=" * 80)
    print("SUMMARY")
    print("=" * 80)
    print(f"Total files with unwrapped calls: {len(calls_by_file)}")
    print(f"Total unwrapped Vulkan API calls: {len(all_unwrapped_calls)}")

    # Print unique function names
    unique_calls = sorted(set(call.call_text for call in all_unwrapped_calls))
    print(f"\nUnique Vulkan functions called without wrapping: {len(unique_calls)}")
    for name in unique_calls:
        count = sum(1 for call in all_unwrapped_calls if call.call_text == name)
        print(f"  {name}: {count} occurrence(s)")

    # Exit with code 2 to indicate unwrapped calls were found
    # (code 0 = all wrapped, code 1 = error, code 2 = unwrapped calls found)
    sys.exit(2)


if __name__ == "__main__":
    main()
