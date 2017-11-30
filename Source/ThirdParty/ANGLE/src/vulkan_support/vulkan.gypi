# Copyright 2016 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
    'variables':
    {
        'glslang_path': '../../third_party/glslang-angle/src',
        'spirv_headers_path': '../../third_party/spirv-headers/src',
        'spirv_tools_path': '../../third_party/spirv-tools-angle/src',
        'vulkan_layers_path': '../../third_party/vulkan-validation-layers/src',
        'vulkan_json': 'angledata',
        'vulkan_loader_sources':
        [
            '<(vulkan_layers_path)/loader/cJSON.c',
            '<(vulkan_layers_path)/loader/cJSON.h',
            '<(vulkan_layers_path)/loader/debug_report.c',
            '<(vulkan_layers_path)/loader/debug_report.h',
            '<(vulkan_layers_path)/loader/dev_ext_trampoline.c',
            '<(vulkan_layers_path)/loader/extension_manual.c',
            '<(vulkan_layers_path)/loader/extension_manual.h',
            '<(vulkan_layers_path)/loader/gpa_helper.h',
            '<(vulkan_layers_path)/loader/loader.c',
            '<(vulkan_layers_path)/loader/loader.h',
            '<(vulkan_layers_path)/loader/murmurhash.c',
            '<(vulkan_layers_path)/loader/murmurhash.h',
            '<(vulkan_layers_path)/loader/phys_dev_ext.c',
            '<(vulkan_layers_path)/loader/trampoline.c',
            '<(vulkan_layers_path)/loader/vk_loader_platform.h',
            '<(vulkan_layers_path)/loader/wsi.c',
            '<(vulkan_layers_path)/loader/wsi.h',
        ],
        'vulkan_loader_win_sources':
        [
            '<(vulkan_layers_path)/loader/dirent_on_windows.c',
            '<(vulkan_layers_path)/loader/dirent_on_windows.h',
        ],
        'vulkan_loader_include_dirs':
        [
            '<(vulkan_layers_path)/include',
            '<(vulkan_layers_path)/loader',
        ],
        'vulkan_loader_cflags_win':
        [
            '/wd4054', # Type cast from function pointer
            '/wd4055', # Type cast from data pointer
            '/wd4100', # Unreferenced formal parameter
            '/wd4152', # Nonstandard extension used (pointer conversion)
            '/wd4201', # Nonstandard extension used: nameless struct/union
            '/wd4214', # Nonstandard extension used: bit field types other than int
            '/wd4232', # Nonstandard extension used: address of dllimport is not static
            '/wd4305', # Type cast truncation
            '/wd4706', # Assignment within conditional expression
            '/wd4996', # Unsafe stdlib function
        ],
        'vulkan_layer_generated_files':
        [
            '<(angle_gen_path)/vulkan/vk_enum_string_helper.h',
            '<(angle_gen_path)/vulkan/vk_struct_size_helper.h',
            '<(angle_gen_path)/vulkan/vk_struct_size_helper.c',
            '<(angle_gen_path)/vulkan/vk_safe_struct.h',
            '<(angle_gen_path)/vulkan/vk_safe_struct.cpp',
            '<(angle_gen_path)/vulkan/vk_layer_dispatch_table.h',
            '<(angle_gen_path)/vulkan/vk_dispatch_table_helper.h',
            '<(angle_gen_path)/vulkan/vk_loader_extensions.h',
            '<(angle_gen_path)/vulkan/vk_loader_extensions.c',
            '<@(vulkan_gen_json_files_outputs)',
        ],
        'glslang_sources':
        [
            '<(glslang_path)/glslang/GenericCodeGen/CodeGen.cpp',
            '<(glslang_path)/glslang/GenericCodeGen/Link.cpp',
            '<(glslang_path)/glslang/Include/arrays.h',
            '<(glslang_path)/glslang/Include/BaseTypes.h',
            '<(glslang_path)/glslang/Include/Common.h',
            '<(glslang_path)/glslang/Include/ConstantUnion.h',
            '<(glslang_path)/glslang/Include/InfoSink.h',
            '<(glslang_path)/glslang/Include/InitializeGlobals.h',
            '<(glslang_path)/glslang/Include/intermediate.h',
            '<(glslang_path)/glslang/Include/PoolAlloc.h',
            '<(glslang_path)/glslang/Include/ResourceLimits.h',
            '<(glslang_path)/glslang/Include/revision.h',
            '<(glslang_path)/glslang/Include/ShHandle.h',
            '<(glslang_path)/glslang/Include/Types.h',
            '<(glslang_path)/glslang/MachineIndependent/Constant.cpp',
            '<(glslang_path)/glslang/MachineIndependent/gl_types.h',
            '<(glslang_path)/glslang/MachineIndependent/glslang.y',
            '<(glslang_path)/glslang/MachineIndependent/glslang_tab.cpp',
            '<(glslang_path)/glslang/MachineIndependent/glslang_tab.cpp.h',
            '<(glslang_path)/glslang/MachineIndependent/InfoSink.cpp',
            '<(glslang_path)/glslang/MachineIndependent/Initialize.cpp',
            '<(glslang_path)/glslang/MachineIndependent/Initialize.h',
            '<(glslang_path)/glslang/MachineIndependent/Intermediate.cpp',
            '<(glslang_path)/glslang/MachineIndependent/intermOut.cpp',
            '<(glslang_path)/glslang/MachineIndependent/IntermTraverse.cpp',
            '<(glslang_path)/glslang/MachineIndependent/iomapper.cpp',
            '<(glslang_path)/glslang/MachineIndependent/iomapper.h',
            '<(glslang_path)/glslang/MachineIndependent/limits.cpp',
            '<(glslang_path)/glslang/MachineIndependent/linkValidate.cpp',
            '<(glslang_path)/glslang/MachineIndependent/LiveTraverser.h',
            '<(glslang_path)/glslang/MachineIndependent/localintermediate.h',
            '<(glslang_path)/glslang/MachineIndependent/parseConst.cpp',
            '<(glslang_path)/glslang/MachineIndependent/ParseContextBase.cpp',
            '<(glslang_path)/glslang/MachineIndependent/ParseHelper.cpp',
            '<(glslang_path)/glslang/MachineIndependent/ParseHelper.h',
            '<(glslang_path)/glslang/MachineIndependent/parseVersions.h',
            '<(glslang_path)/glslang/MachineIndependent/PoolAlloc.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/Pp.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpAtom.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpContext.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpContext.h',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpMemory.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpScanner.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpSymbols.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpTokens.cpp',
            '<(glslang_path)/glslang/MachineIndependent/preprocessor/PpTokens.h',
            '<(glslang_path)/glslang/MachineIndependent/propagateNoContraction.cpp',
            '<(glslang_path)/glslang/MachineIndependent/propagateNoContraction.h',
            '<(glslang_path)/glslang/MachineIndependent/reflection.cpp',
            '<(glslang_path)/glslang/MachineIndependent/reflection.h',
            '<(glslang_path)/glslang/MachineIndependent/RemoveTree.cpp',
            '<(glslang_path)/glslang/MachineIndependent/RemoveTree.h',
            '<(glslang_path)/glslang/MachineIndependent/Scan.cpp',
            '<(glslang_path)/glslang/MachineIndependent/Scan.h',
            '<(glslang_path)/glslang/MachineIndependent/ScanContext.h',
            '<(glslang_path)/glslang/MachineIndependent/ShaderLang.cpp',
            '<(glslang_path)/glslang/MachineIndependent/SymbolTable.cpp',
            '<(glslang_path)/glslang/MachineIndependent/SymbolTable.h',
            '<(glslang_path)/glslang/MachineIndependent/Versions.cpp',
            '<(glslang_path)/glslang/MachineIndependent/Versions.h',
            '<(glslang_path)/glslang/OSDependent/osinclude.h',
            '<(glslang_path)/glslang/Public/ShaderLang.h',
            '<(glslang_path)/hlsl/hlslAttributes.cpp',
            '<(glslang_path)/hlsl/hlslAttributes.h',
            '<(glslang_path)/hlsl/hlslGrammar.cpp',
            '<(glslang_path)/hlsl/hlslGrammar.h',
            '<(glslang_path)/hlsl/hlslOpMap.cpp',
            '<(glslang_path)/hlsl/hlslOpMap.h',
            '<(glslang_path)/hlsl/hlslParseables.cpp',
            '<(glslang_path)/hlsl/hlslParseables.h',
            '<(glslang_path)/hlsl/hlslParseHelper.cpp',
            '<(glslang_path)/hlsl/hlslParseHelper.h',
            '<(glslang_path)/hlsl/hlslScanContext.cpp',
            '<(glslang_path)/hlsl/hlslScanContext.h',
            '<(glslang_path)/hlsl/hlslTokens.h',
            '<(glslang_path)/hlsl/hlslTokenStream.cpp',
            '<(glslang_path)/hlsl/hlslTokenStream.h',
            '<(glslang_path)/OGLCompilersDLL/InitializeDll.cpp',
            '<(glslang_path)/OGLCompilersDLL/InitializeDll.h',
            '<(glslang_path)/SPIRV/bitutils.h',
            '<(glslang_path)/SPIRV/disassemble.cpp',
            '<(glslang_path)/SPIRV/disassemble.h',
            '<(glslang_path)/SPIRV/doc.cpp',
            '<(glslang_path)/SPIRV/doc.h',
            '<(glslang_path)/SPIRV/GLSL.ext.KHR.h',
            '<(glslang_path)/SPIRV/GLSL.std.450.h',
            '<(glslang_path)/SPIRV/GlslangToSpv.cpp',
            '<(glslang_path)/SPIRV/GlslangToSpv.h',
            '<(glslang_path)/SPIRV/hex_float.h',
            '<(glslang_path)/SPIRV/InReadableOrder.cpp',
            '<(glslang_path)/SPIRV/Logger.cpp',
            '<(glslang_path)/SPIRV/Logger.h',
            '<(glslang_path)/SPIRV/spirv.hpp',
            '<(glslang_path)/SPIRV/SpvBuilder.cpp',
            '<(glslang_path)/SPIRV/SpvBuilder.h',
            '<(glslang_path)/SPIRV/spvIR.h',
            '<(glslang_path)/StandAlone/ResourceLimits.cpp',
            '<(glslang_path)/StandAlone/ResourceLimits.h',
        ],
        'glslang_win_sources':
        [
            '<(glslang_path)/glslang/OSDependent/Windows/ossource.cpp',
        ],
        'glslang_unix_sources':
        [
            '<(glslang_path)/glslang/OSDependent/Unix/ossource.cpp',
        ],
        'spirv_tools_sources':
        [
            '<(angle_gen_path)/vulkan/core.insts-1.0.inc',
            '<(angle_gen_path)/vulkan/core.insts-1.1.inc',
            '<(angle_gen_path)/vulkan/generators.inc',
            '<(angle_gen_path)/vulkan/glsl.std.450.insts-1.0.inc',
            '<(angle_gen_path)/vulkan/opencl.std.insts-1.0.inc',
            '<(angle_gen_path)/vulkan/operand.kinds-1.0.inc',
            '<(angle_gen_path)/vulkan/operand.kinds-1.1.inc',
            '<(spirv_tools_path)/source/assembly_grammar.cpp',
            '<(spirv_tools_path)/source/assembly_grammar.h',
            '<(spirv_tools_path)/source/binary.cpp',
            '<(spirv_tools_path)/source/binary.h',
            '<(spirv_tools_path)/source/diagnostic.cpp',
            '<(spirv_tools_path)/source/diagnostic.h',
            '<(spirv_tools_path)/source/disassemble.cpp',
            '<(spirv_tools_path)/source/enum_set.h',
            '<(spirv_tools_path)/source/ext_inst.cpp',
            '<(spirv_tools_path)/source/ext_inst.h',
            '<(spirv_tools_path)/source/instruction.h',
            '<(spirv_tools_path)/source/libspirv.cpp',
            '<(spirv_tools_path)/source/macro.h',
            '<(spirv_tools_path)/source/message.cpp',
            '<(spirv_tools_path)/source/name_mapper.cpp',
            '<(spirv_tools_path)/source/name_mapper.h',
            '<(spirv_tools_path)/source/opcode.cpp',
            '<(spirv_tools_path)/source/opcode.h',
            '<(spirv_tools_path)/source/operand.cpp',
            '<(spirv_tools_path)/source/operand.h',
            '<(spirv_tools_path)/source/parsed_operand.cpp',
            '<(spirv_tools_path)/source/parsed_operand.h',
            '<(spirv_tools_path)/source/print.cpp',
            '<(spirv_tools_path)/source/print.h',
            # TODO(jmadill): Determine if this is ever needed.
            #'<(spirv_tools_path)/source/software_version.cpp',
            '<(spirv_tools_path)/source/spirv_constant.h',
            '<(spirv_tools_path)/source/spirv_definition.h',
            '<(spirv_tools_path)/source/spirv_endian.cpp',
            '<(spirv_tools_path)/source/spirv_endian.h',
            '<(spirv_tools_path)/source/spirv_target_env.cpp',
            '<(spirv_tools_path)/source/spirv_target_env.h',
            '<(spirv_tools_path)/source/table.cpp',
            '<(spirv_tools_path)/source/table.h',
            '<(spirv_tools_path)/source/text.cpp',
            '<(spirv_tools_path)/source/text.h',
            '<(spirv_tools_path)/source/text_handler.cpp',
            '<(spirv_tools_path)/source/text_handler.h',
            '<(spirv_tools_path)/source/util/bitutils.h',
            '<(spirv_tools_path)/source/util/hex_float.h',
            '<(spirv_tools_path)/source/util/parse_number.cpp',
            '<(spirv_tools_path)/source/util/parse_number.h',
            '<(spirv_tools_path)/source/val/basic_block.cpp',
            '<(spirv_tools_path)/source/val/construct.cpp',
            '<(spirv_tools_path)/source/val/function.cpp',
            '<(spirv_tools_path)/source/val/instruction.cpp',
            '<(spirv_tools_path)/source/val/validation_state.cpp',
            '<(spirv_tools_path)/source/validate.cpp',
            '<(spirv_tools_path)/source/validate.h',
            '<(spirv_tools_path)/source/validate_cfg.cpp',
            '<(spirv_tools_path)/source/validate_datarules.cpp',
            '<(spirv_tools_path)/source/validate_id.cpp',
            '<(spirv_tools_path)/source/validate_instruction.cpp',
            '<(spirv_tools_path)/source/validate_layout.cpp',
        ],
        'vulkan_layer_utils_sources':
        [
            '<(vulkan_layers_path)/layers/vk_layer_config.cpp',
            '<(vulkan_layers_path)/layers/vk_layer_config.h',
            '<(vulkan_layers_path)/layers/vk_layer_extension_utils.cpp',
            '<(vulkan_layers_path)/layers/vk_layer_extension_utils.h',
            '<(vulkan_layers_path)/layers/vk_layer_utils.cpp',
            '<(vulkan_layers_path)/layers/vk_layer_utils.h',
        ],
        'vulkan_struct_wrappers_outputs':
        [
            '<(angle_gen_path)/vulkan/vk_safe_struct.cpp',
            '<(angle_gen_path)/vulkan/vk_safe_struct.h',
            '<(angle_gen_path)/vulkan/vk_struct_size_helper.c',
            '<(angle_gen_path)/vulkan/vk_struct_size_helper.h',
            '<(angle_gen_path)/vulkan/vk_struct_string_helper.h',
            '<(angle_gen_path)/vulkan/vk_struct_string_helper_cpp.h',
        ],
        'VkLayer_core_validation_sources':
        [
            # This file is manually included in the layer
            # '<(angle_gen_path)/vulkan/vk_safe_struct.cpp',
            '<(angle_gen_path)/vulkan/vk_safe_struct.h',
            '<(vulkan_layers_path)/layers/buffer_validation.cpp',
            '<(vulkan_layers_path)/layers/buffer_validation.h',
            '<(vulkan_layers_path)/layers/core_validation.cpp',
            '<(vulkan_layers_path)/layers/core_validation.h',
            '<(vulkan_layers_path)/layers/descriptor_sets.cpp',
            '<(vulkan_layers_path)/layers/descriptor_sets.h',
        ],
        'VkLayer_swapchain_sources':
        [
            '<(vulkan_layers_path)/layers/swapchain.cpp',
            '<(vulkan_layers_path)/layers/swapchain.h',
        ],
        'VkLayer_object_tracker_sources':
        [
            '<(vulkan_layers_path)/layers/object_tracker.cpp',
            '<(vulkan_layers_path)/layers/object_tracker.h',
        ],
        'VkLayer_unique_objects_sources':
        [
            '<(angle_gen_path)/vulkan/unique_objects_wrappers.h',
            # This file is manually included in the layer
            # '<(angle_gen_path)/vulkan/vk_safe_struct.cpp',
            '<(angle_gen_path)/vulkan/vk_safe_struct.h',
            '<(vulkan_layers_path)/layers/unique_objects.cpp',
            '<(vulkan_layers_path)/layers/unique_objects.h',
        ],
        'VkLayer_threading_sources':
        [
            '<(angle_gen_path)/vulkan/thread_check.h',
            '<(vulkan_layers_path)/layers/threading.cpp',
            '<(vulkan_layers_path)/layers/threading.h',
        ],
        'VkLayer_parameter_validation_sources':
        [
            '<(angle_gen_path)/vulkan/parameter_validation.h',
            '<(vulkan_layers_path)/layers/parameter_validation.cpp',
        ],
        'vulkan_gen_json_files_sources_win':
        [
            '<(vulkan_layers_path)/layers/windows/VkLayer_core_validation.json',
            '<(vulkan_layers_path)/layers/windows/VkLayer_object_tracker.json',
            '<(vulkan_layers_path)/layers/windows/VkLayer_parameter_validation.json',
            '<(vulkan_layers_path)/layers/windows/VkLayer_swapchain.json',
            '<(vulkan_layers_path)/layers/windows/VkLayer_threading.json',
            '<(vulkan_layers_path)/layers/windows/VkLayer_unique_objects.json',
        ],
        'vulkan_gen_json_files_sources_linux':
        [
            '<(vulkan_layers_path)/layers/linux/VkLayer_core_validation.json',
            '<(vulkan_layers_path)/layers/linux/VkLayer_object_tracker.json',
            '<(vulkan_layers_path)/layers/linux/VkLayer_parameter_validation.json',
            '<(vulkan_layers_path)/layers/linux/VkLayer_swapchain.json',
            '<(vulkan_layers_path)/layers/linux/VkLayer_threading.json',
            '<(vulkan_layers_path)/layers/linux/VkLayer_unique_objects.json',
        ],
        'vulkan_gen_json_files_outputs':
        [
            '<(PRODUCT_DIR)/<(vulkan_json)/VkLayer_core_validation.json',
            '<(PRODUCT_DIR)/<(vulkan_json)/VkLayer_object_tracker.json',
            '<(PRODUCT_DIR)/<(vulkan_json)/VkLayer_parameter_validation.json',
            '<(PRODUCT_DIR)/<(vulkan_json)/VkLayer_swapchain.json',
            '<(PRODUCT_DIR)/<(vulkan_json)/VkLayer_threading.json',
            '<(PRODUCT_DIR)/<(vulkan_json)/VkLayer_unique_objects.json',
        ],
    },
    'conditions':
    [
        ['angle_enable_vulkan==1',
        {
            'targets':
            [
                {
                    'target_name': 'glslang',
                    'type': 'static_library',
                    'sources':
                    [
                        '<@(glslang_sources)',
                    ],
                    'include_dirs':
                    [
                        '<(glslang_path)',
                    ],
                    'msvs_settings':
                    {
                        'VCCLCompilerTool':
                        {
                            'PreprocessorDefinitions':
                            [
                                '_HAS_EXCEPTIONS=0',
                            ],
                            'AdditionalOptions':
                            [
                                '/wd4100', # Unreferenced formal parameter
                                '/wd4244', # Conversion from 'int' to 'char', possible loss of data
                                '/wd4456', # Declaration hides previous local declaration
                                '/wd4457', # Declaration hides function parameter
                                '/wd4458', # Declaration hides class member
                                '/wd4702', # Unreachable code (from glslang_tab.cpp)
                                '/wd4718', # Recursive call has no side effects (from PpContext.cpp)
                            ],
                        },
                    },
                    'direct_dependent_settings':
                    {
                        'include_dirs':
                        [
                            '<(glslang_path)/glslang/Public',
                            '<(glslang_path)',
                        ],
                    },
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<@(glslang_win_sources)',
                            ],
                        }],
                        ['OS=="linux"',
                        {
                            'sources':
                            [
                                '<@(glslang_unix_sources)',
                            ],
                        }],
                    ],
                },

                {
                    'target_name': 'spirv_tools',
                    'type': 'static_library',
                    'sources':
                    [
                        '<@(spirv_tools_sources)',
                    ],
                    'include_dirs':
                    [
                        '<(angle_gen_path)/vulkan',
                        '<(spirv_headers_path)/include',
                        '<(spirv_tools_path)/include',
                        '<(spirv_tools_path)/source',
                    ],
                    'msvs_settings':
                    {
                        'VCCLCompilerTool':
                        {
                            'PreprocessorDefinitions':
                            [
                                '_HAS_EXCEPTIONS=0',
                            ],
                            'AdditionalOptions':
                            [
                                '/wd4127', # Conditional expression is constant, happens in a template in hex_float.h.
                                '/wd4706', # Assignment within conditional expression
                                '/wd4996', # Unsafe stdlib function
                            ],
                        },
                    },
                    'actions':
                    [
                        {
                            'action_name': 'spirv_tools_gen_build_tables_1_0',
                            'message': 'generating info tables for SPIR-V 1.0 instructions and operands',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(spirv_headers_path)/include/spirv/1.0/extinst.glsl.std.450.grammar.json',
                                '<(spirv_headers_path)/include/spirv/1.0/spirv.core.grammar.json',
                                '<(spirv_tools_path)/source/extinst-1.0.opencl.std.grammar.json',
                                '<(spirv_tools_path)/utils/generate_grammar_tables.py',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/core.insts-1.0.inc',
                                '<(angle_gen_path)/vulkan/glsl.std.450.insts-1.0.inc',
                                '<(angle_gen_path)/vulkan/opencl.std.insts-1.0.inc',
                                '<(angle_gen_path)/vulkan/operand.kinds-1.0.inc',
                            ],
                            'action':
                            [
                                'python', '<(spirv_tools_path)/utils/generate_grammar_tables.py',
                                '--spirv-core-grammar=<(spirv_headers_path)/include/spirv/1.0/spirv.core.grammar.json',
                                '--extinst-glsl-grammar=<(spirv_headers_path)/include/spirv/1.0/extinst.glsl.std.450.grammar.json',
                                '--extinst-opencl-grammar=<(spirv_tools_path)/source/extinst-1.0.opencl.std.grammar.json',
                                '--core-insts-output=<(angle_gen_path)/vulkan/core.insts-1.0.inc',
                                '--glsl-insts-output=<(angle_gen_path)/vulkan/glsl.std.450.insts-1.0.inc',
                                '--opencl-insts-output=<(angle_gen_path)/vulkan/opencl.std.insts-1.0.inc',
                                '--operand-kinds-output=<(angle_gen_path)/vulkan/operand.kinds-1.0.inc',
                            ],
                        },

                        {
                            'action_name': 'spirv_tools_gen_build_tables_1_1',
                            'message': 'generating info tables for SPIR-V 1.1 instructions and operands',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(spirv_tools_path)/utils/generate_grammar_tables.py',
                                '<(spirv_headers_path)/include/spirv/1.1/spirv.core.grammar.json',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/core.insts-1.1.inc',
                                '<(angle_gen_path)/vulkan/operand.kinds-1.1.inc',
                            ],
                            'action':
                            [
                                'python', '<(spirv_tools_path)/utils/generate_grammar_tables.py',
                                '--spirv-core-grammar=<(spirv_headers_path)/include/spirv/1.1/spirv.core.grammar.json',
                                '--core-insts-output=<(angle_gen_path)/vulkan/core.insts-1.1.inc',
                                '--operand-kinds-output=<(angle_gen_path)/vulkan/operand.kinds-1.1.inc',
                            ],
                        },

                        {
                            'action_name': 'spirv_tools_gen_generators_inc',
                            'message': 'generating generators.inc for SPIRV-Tools',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(spirv_tools_path)/utils/generate_registry_tables.py',
                                '<(spirv_headers_path)/include/spirv/spir-v.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/generators.inc',
                            ],
                            'action':
                            [
                                'python', '<(spirv_tools_path)/utils/generate_registry_tables.py',
                                '--xml=<(spirv_headers_path)/include/spirv/spir-v.xml',
                                '--generator-output=<(angle_gen_path)/vulkan/generators.inc',
                            ],
                        },
                    ],

                    'direct_dependent_settings':
                    {
                        'include_dirs':
                        [
                            '<(spirv_headers_path)/include',
                            '<(spirv_tools_path)/include',
                        ],
                    },
                },

                {
                    'target_name': 'vulkan_layer_utils_static',
                    'type': 'static_library',
                    'msvs_cygwin_shell': 0,
                    'sources':
                    [
                        '<@(vulkan_layer_utils_sources)',
                    ],
                    'include_dirs':
                    [
                        '<(angle_gen_path)/vulkan',
                        '<@(vulkan_loader_include_dirs)',
                    ],
                    'msvs_settings':
                    {
                        'VCCLCompilerTool':
                        {
                            'PreprocessorDefinitions':
                            [
                                '_HAS_EXCEPTIONS=0',
                            ],
                            'AdditionalOptions':
                            [
                                '/wd4100', # Unreferenced formal parameter
                                '/wd4309', # Truncation of constant value
                                '/wd4505', # Unreferenced local function has been removed
                                '/wd4996', # Unsafe stdlib function
                            ],
                        },
                    },
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'defines':
                            [
                                'WIN32',
                                'WIN32_LEAN_AND_MEAN',
                                'VK_USE_PLATFORM_WIN32_KHR',
                                'VK_USE_PLATFORM_WIN32_KHX',
                            ],
                        }],
                        ['OS=="linux"',
                        {
                            'defines':
                            [
                                'VK_USE_PLATFORM_XCB_KHR',
                                'VK_USE_PLATFORM_XCB_KHX',
                            ],
                        }],
                    ],
                    'direct_dependent_settings':
                    {
                        'msvs_cygwin_shell': 0,
                        'sources':
                        [
                            '<(vulkan_layers_path)/layers/vk_layer_table.cpp',
                            '<(vulkan_layers_path)/layers/vk_layer_table.h',
                        ],
                        'include_dirs':
                        [
                            '<(angle_gen_path)/vulkan',
                            '<(glslang_path)',
                            '<(vulkan_layers_path)/layers',
                            '<@(vulkan_loader_include_dirs)',
                        ],
                        'msvs_settings':
                        {
                            'VCCLCompilerTool':
                            {
                                'PreprocessorDefinitions':
                                [
                                    '_HAS_EXCEPTIONS=0',
                                ],
                                'AdditionalOptions':
                                [
                                    '/wd4100', # Unreferenced local parameter
                                    '/wd4201', # Nonstandard extension used: nameless struct/union
                                    '/wd4456', # declaration hides previous local declaration
                                    '/wd4505', # Unreferenced local function has been removed
                                    '/wd4996', # Unsafe stdlib function
                                ],
                            }
                        },
                        'conditions':
                        [
                            ['OS=="win"',
                            {
                                'defines':
                                [
                                    'WIN32_LEAN_AND_MEAN',
                                    'VK_USE_PLATFORM_WIN32_KHR',
                                    'VK_USE_PLATFORM_WIN32_KHX',
                                ],
                                'configurations':
                                {
                                    'Debug_Base':
                                    {
                                        'msvs_settings':
                                        {
                                            'VCCLCompilerTool':
                                            {
                                                'AdditionalOptions':
                                                [
                                                    '/bigobj',
                                                ],
                                            },
                                        },
                                    },
                                },
                            }],
                            ['OS=="linux"',
                            {
                                'defines':
                                [
                                    'VK_USE_PLATFORM_XCB_KHR',
                                    'VK_USE_PLATFORM_XCB_KHX',
                                ],
                            }],
                        ],
                    },

                    'actions':
                    [
                        # Duplicate everything because of GYP limitations.
                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_enum_string_helper_h',
                            'message': 'generating vk_enum_string_helper.h',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/helper_file_generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_enum_string_helper.h'
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                 '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml',
                                'vk_enum_string_helper.h', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_struct_size_helper_h',
                            'message': 'generating vk_struct_size_helper.h',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/helper_file_generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_struct_size_helper.h'
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                 '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml',
                                'vk_struct_size_helper.h', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_struct_size_helper_c',
                            'message': 'generating vk_struct_size_helper.c',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/helper_file_generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_struct_size_helper.c'
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                 '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml',
                                'vk_struct_size_helper.c', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_safe_struct_h',
                            'message': 'generating vk_safe_struct.h',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/helper_file_generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_safe_struct.h'
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                 '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml',
                                'vk_safe_struct.h', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_safe_struct_cpp',
                            'message': 'generating vk_safe_struct.cpp',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/helper_file_generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_safe_struct.cpp'
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                 '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml',
                                'vk_safe_struct.cpp', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_layer_dispatch_table_h',
                            'message': 'generating vk_layer_dispatch_table.h',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/loader_extension_generator.py',
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_layer_dispatch_table.h',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml', 'vk_layer_dispatch_table.h', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_dispatch_table_helper_h',
                            'message': 'generating vk_dispatch_table_helper.h',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/dispatch_table_helper_generator.py',
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_dispatch_table_helper.h',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml', 'vk_dispatch_table_helper.h', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_loader_extensions_h',
                            'message': 'generating vk_loader_extensions.h',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/loader_extension_generator.py',
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_loader_extensions.h',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml', 'vk_loader_extensions.h', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_run_vk_xml_generate_vk_loader_extensions_c',
                            'message': 'generating vk_loader_extensions.c',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/loader_extension_generator.py',
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/vk_loader_extensions.c',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml', 'vk_loader_extensions.c', '-quiet',
                            ],
                        },

                        {
                            'action_name': 'vulkan_generate_json_files',
                            'message': 'generating Vulkan json files',
                            'inputs':
                            [
                                '<(angle_path)/scripts/generate_vulkan_layers_json.py',
                            ],
                            'outputs':
                            [
                                '<@(vulkan_gen_json_files_outputs)',
                            ],
                            'conditions':
                            [
                                ['OS=="win"',
                                {
                                    'inputs':
                                    [
                                        '<@(vulkan_gen_json_files_sources_win)',
                                    ],
                                    'action':
                                    [
                                        'python', '<(angle_path)/scripts/generate_vulkan_layers_json.py',
                                        '<(vulkan_layers_path)/layers/windows', '<(PRODUCT_DIR)/<(vulkan_json)',
                                    ],
                                }],
                                ['OS=="linux"',
                                {
                                    'inputs':
                                    [
                                        '<@(vulkan_gen_json_files_sources_linux)',
                                    ],
                                    'action':
                                    [
                                        'python', '<(angle_path)/scripts/generate_vulkan_layers_json.py',
                                        '<(vulkan_layers_path)/layers/linux', '<(PRODUCT_DIR)/<(vulkan_json)',
                                    ],
                                }],
                            ],
                        },
                    ],
                },

                {
                    'target_name': 'vulkan_loader',
                    'type': 'static_library',
                    'sources':
                    [
                        '<@(vulkan_loader_sources)',
                    ],
                    'include_dirs':
                    [
                        '<@(vulkan_loader_include_dirs)',
                        '<(angle_gen_path)/vulkan',
                    ],
                    'defines':
                    [
                        'API_NAME="Vulkan"',
                        'VULKAN_NON_CMAKE_BUILD',
                    ],
                    'msvs_settings':
                    {
                        'VCCLCompilerTool':
                        {
                            'AdditionalOptions':
                            [
                                '<@(vulkan_loader_cflags_win)',
                            ],
                        },
                        'VCLinkerTool':
                        {
                            'AdditionalDependencies':
                            [
                                'shlwapi.lib',
                            ],
                        },
                    },
                    'direct_dependent_settings':
                    {
                        'include_dirs':
                        [
                            '<@(vulkan_loader_include_dirs)',
                        ],
                        'msvs_settings':
                        {
                            'VCLinkerTool':
                            {
                                'AdditionalDependencies':
                                [
                                    'shlwapi.lib',
                                ],
                            },
                        },
                        'defines':
                        [
                            'ANGLE_VK_LAYERS_DIR="<(vulkan_json)"',
                        ],
                        'conditions':
                        [
                            ['OS=="win"',
                            {
                                'defines':
                                [
                                    'VK_USE_PLATFORM_WIN32_KHR',
                                    'VK_USE_PLATFORM_WIN32_KHX',
                                ],
                            }],
                            ['OS=="linux"',
                            {
                                'defines':
                                [
                                    'VK_USE_PLATFORM_XCB_KHR',
                                    'VK_USE_PLATFORM_XCB_KHX',
                                ],
                            }],
                        ],
                    },
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<@(vulkan_loader_win_sources)',
                            ],
                            'defines':
                            [
                                'VK_USE_PLATFORM_WIN32_KHR',
                                'VK_USE_PLATFORM_WIN32_KHX',
                            ],
                        }],
                        ['OS=="linux"',
                        {
                            'defines':
                            [
                                'HAVE_SECURE_GETENV',
                                'VK_USE_PLATFORM_XCB_KHR',
                                'VK_USE_PLATFORM_XCB_KHX',
                            ],
                        }],
                    ],
                },

                {
                    'target_name': 'VkLayer_core_validation',
                    'type': 'shared_library',
                    'dependencies':
                    [
                        'spirv_tools',
                        'vulkan_layer_utils_static',
                    ],
                    'sources':
                    [
                        '<@(VkLayer_core_validation_sources)',
                    ],
                    'actions':
                    [
                        {
                            'action_name': 'layer_core_validation_order_deps',
                            'message': 'stamping for layer_core_validation_order_deps',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<@(vulkan_layer_generated_files)' ],
                            'outputs': [ '<(angle_gen_path)/vulkan/layer_core_validation_order_deps.stamp' ],
                            'action':
                            [
                                'python', '<(angle_path)/gyp/touch_stamp.py',
                                '<(angle_gen_path)/vulkan/layer_core_validation_order_deps.stamp',
                            ]
                        },
                    ],
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<(vulkan_layers_path)/layers/VkLayer_core_validation.def',
                            ]
                        }],
                    ],
                },

                {
                    'target_name': 'VkLayer_swapchain',
                    'type': 'shared_library',
                    'dependencies':
                    [
                        'vulkan_layer_utils_static',
                    ],
                    'sources':
                    [
                        '<@(VkLayer_swapchain_sources)',
                    ],
                    'actions':
                    [
                        {
                            'action_name': 'layer_swapchain_order_deps',
                            'message': 'stamping for layer_swapchain_order_deps',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<@(vulkan_layer_generated_files)' ],
                            'outputs': [ '<(angle_gen_path)/vulkan/layer_swapchain_order_deps.stamp' ],
                            'action':
                            [
                                'python', '<(angle_path)/gyp/touch_stamp.py',
                                '<(angle_gen_path)/vulkan/layer_swapchain_order_deps.stamp',
                            ]
                        },
                    ],
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<(vulkan_layers_path)/layers/VkLayer_swapchain.def',
                            ]
                        }],
                    ],
                },

                {
                    'target_name': 'VkLayer_object_tracker',
                    'type': 'shared_library',
                    'dependencies':
                    [
                        'vulkan_layer_utils_static',
                    ],
                    'sources':
                    [
                        '<@(VkLayer_object_tracker_sources)',
                    ],
                    'actions':
                    [
                        {
                            'action_name': 'layer_object_tracker_order_deps',
                            'message': 'stamping for layer_object_tracker_order_deps',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<@(vulkan_layer_generated_files)' ],
                            'outputs': [ '<(angle_gen_path)/vulkan/layer_object_tracker_order_deps.stamp' ],
                            'action':
                            [
                                'python', '<(angle_path)/gyp/touch_stamp.py',
                                '<(angle_gen_path)/vulkan/layer_object_tracker_order_deps.stamp',
                            ]
                        },
                    ],
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<(vulkan_layers_path)/layers/VkLayer_object_tracker.def',
                            ]
                        }],
                    ],
                },

                {
                    'target_name': 'VkLayer_unique_objects',
                    'type': 'shared_library',
                    'dependencies':
                    [
                        'vulkan_layer_utils_static',
                    ],
                    'sources':
                    [
                        '<@(VkLayer_unique_objects_sources)',
                    ],
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<(vulkan_layers_path)/layers/VkLayer_unique_objects.def',
                            ]
                        }],
                    ],
                    'actions':
                    [
                        {
                            'action_name': 'layer_unique_objects_order_deps',
                            'message': 'stamping for layer_unique_objects_order_deps',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<@(vulkan_layer_generated_files)' ],
                            'outputs': [ '<(angle_gen_path)/vulkan/layer_unique_objects_order_deps.stamp' ],
                            'action':
                            [
                                'python', '<(angle_path)/gyp/touch_stamp.py',
                                '<(angle_gen_path)/vulkan/layer_unique_objects_order_deps.stamp',
                            ]
                        },
                        {
                            'action_name': 'vulkan_layer_unique_objects_generate',
                            'message': 'generating Vulkan unique_objects helpers',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/unique_objects_generator.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/unique_objects_wrappers.h',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml', 'unique_objects_wrappers.h', '-quiet',
                            ],
                        },
                    ],
                },

                {
                    'target_name': 'VkLayer_threading',
                    'type': 'shared_library',
                    'dependencies':
                    [
                        'vulkan_layer_utils_static',
                    ],
                    'sources':
                    [
                        '<@(VkLayer_threading_sources)',
                    ],
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<(vulkan_layers_path)/layers/VkLayer_threading.def',
                            ]
                        }],
                    ],
                    'actions':
                    [
                        {
                            'action_name': 'layer_threading_order_deps',
                            'message': 'stamping for layer_threading_order_deps',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<@(vulkan_layer_generated_files)' ],
                            'outputs': [ '<(angle_gen_path)/vulkan/layer_threading_order_deps.stamp' ],
                            'action':
                            [
                                'python', '<(angle_path)/gyp/touch_stamp.py',
                                '<(angle_gen_path)/vulkan/layer_threading_order_deps.stamp',
                            ]
                        },
                        {
                            'action_name': 'vulkan_layer_threading_generate',
                            'message': 'generating Vulkan threading header',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/thread_check.h',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o',
                                '<(angle_gen_path)/vulkan', '-registry', '<(vulkan_layers_path)/scripts/vk.xml',
                                'thread_check.h', '-quiet',
                            ],
                        },
                    ],
                },

                {
                    'target_name': 'VkLayer_parameter_validation',
                    'type': 'shared_library',
                    'dependencies':
                    [
                        'vulkan_layer_utils_static',
                    ],
                    'sources':
                    [
                        '<@(VkLayer_parameter_validation_sources)',
                    ],
                    'conditions':
                    [
                        ['OS=="win"',
                        {
                            'sources':
                            [
                                '<(vulkan_layers_path)/layers/VkLayer_parameter_validation.def',
                            ]
                        }],
                    ],
                    'actions':
                    [
                        {
                            'action_name': 'layer_parameter_validation_order_deps',
                            'message': 'stamping for layer_parameter_validation_order_deps',
                            'msvs_cygwin_shell': 0,
                            'inputs': [ '<@(vulkan_layer_generated_files)' ],
                            'outputs': [ '<(angle_gen_path)/vulkan/layer_parameter_validation_order_deps.stamp' ],
                            'action':
                            [
                                'python', '<(angle_path)/gyp/touch_stamp.py',
                                '<(angle_gen_path)/vulkan/layer_parameter_validation_order_deps.stamp',
                            ]
                        },
                        {
                            'action_name': 'vulkan_layer_parameter_validation_generate',
                            'message': 'generating Vulkan parameter_validation header',
                            'msvs_cygwin_shell': 0,
                            'inputs':
                            [
                                '<(vulkan_layers_path)/scripts/generator.py',
                                '<(vulkan_layers_path)/scripts/lvl_genvk.py',
                                '<(vulkan_layers_path)/scripts/reg.py',
                                '<(vulkan_layers_path)/scripts/vk.xml',
                            ],
                            'outputs':
                            [
                                '<(angle_gen_path)/vulkan/parameter_validation.h',
                            ],
                            'action':
                            [
                                'python', '<(vulkan_layers_path)/scripts/lvl_genvk.py', '-o', '<(angle_gen_path)/vulkan',
                                '-registry', '<(vulkan_layers_path)/scripts/vk.xml', 'parameter_validation.h', '-quiet',
                            ],
                        },
                    ],
                },

                {
                    'target_name': 'angle_vulkan',
                    'type': 'none',
                    'dependencies':
                    [
                        'glslang',
                        # Need to disable these to prevent multiply defined symbols with ninja.
                        # TODO(jmadill): Figure out how to implement data_deps in gyp.
                        # 'VkLayer_core_validation',
                        # 'VkLayer_object_tracker',
                        # 'VkLayer_parameter_validation',
                        # 'VkLayer_swapchain',
                        # 'VkLayer_threading',
                        # 'VkLayer_unique_objects',
                        'vulkan_loader',
                    ],
                    'export_dependent_settings':
                    [
                        'glslang',
                        'vulkan_loader',
                    ],
                }
            ],
        }],
    ],
}
