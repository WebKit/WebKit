#!/usr/bin/python2
#
# Copyright 2018 The ANGLE Project Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# registry_xml.py:
#   Parses information from Khronos registry files..

# List of supported extensions. Add to this list to enable new extensions
# available in gl.xml.

import sys, os
import xml.etree.ElementTree as etree

angle_extensions = [
    # ANGLE extensions
    "GL_CHROMIUM_bind_uniform_location",
    "GL_CHROMIUM_framebuffer_mixed_samples",
    "GL_CHROMIUM_path_rendering",
    "GL_CHROMIUM_copy_texture",
    "GL_CHROMIUM_copy_compressed_texture",
    "GL_CHROMIUM_lose_context",
    "GL_ANGLE_request_extension",
    "GL_ANGLE_robust_client_memory",
    "GL_ANGLE_copy_texture_3d",
]

gles1_extensions = [
    # ES1 (Possibly the min set of extensions needed by Android)
    "GL_OES_draw_texture",
    "GL_OES_framebuffer_object",
    "GL_OES_matrix_palette",
    "GL_OES_point_size_array",
    "GL_OES_query_matrix",
    "GL_OES_texture_cube_map",
]

supported_extensions = sorted(angle_extensions + gles1_extensions + [
    # ES2+
    "GL_ANGLE_framebuffer_blit",
    "GL_ANGLE_framebuffer_multisample",
    "GL_ANGLE_instanced_arrays",
    "GL_ANGLE_provoking_vertex",
    "GL_ANGLE_texture_multisample",
    "GL_ANGLE_translated_shader_source",
    "GL_EXT_blend_func_extended",
    "GL_EXT_debug_marker",
    "GL_EXT_discard_framebuffer",
    "GL_EXT_disjoint_timer_query",
    "GL_EXT_draw_buffers",
    "GL_EXT_geometry_shader",
    "GL_EXT_instanced_arrays",
    "GL_EXT_map_buffer_range",
    "GL_EXT_memory_object",
    "GL_EXT_memory_object_fd",
    "GL_EXT_occlusion_query_boolean",
    "GL_EXT_robustness",
    "GL_EXT_semaphore",
    "GL_EXT_semaphore_fd",
    "GL_EXT_texture_storage",
    "GL_KHR_debug",
    "GL_NV_fence",
    "GL_OES_EGL_image",
    "GL_OES_get_program_binary",
    "GL_OES_mapbuffer",
    "GL_OES_texture_border_clamp",
    "GL_OES_texture_storage_multisample_2d_array",
    "GL_OES_vertex_array_object",
    "GL_OVR_multiview",
    "GL_OVR_multiview2",
    "GL_KHR_parallel_shader_compile",
    "GL_ANGLE_multi_draw",
])

supported_egl_extensions = [
    "EGL_ANDROID_blob_cache",
    "EGL_ANDROID_get_frame_timestamps",
    "EGL_ANDROID_presentation_time",
    "EGL_ANGLE_d3d_share_handle_client_buffer",
    "EGL_ANGLE_device_creation",
    "EGL_ANGLE_device_d3d",
    "EGL_ANGLE_program_cache_control",
    "EGL_ANGLE_query_surface_pointer",
    "EGL_ANGLE_stream_producer_d3d_texture",
    "EGL_ANGLE_surface_d3d_texture_2d_share_handle",
    "EGL_ANGLE_window_fixed_size",
    "EGL_CHROMIUM_get_sync_values",
    "EGL_EXT_create_context_robustness",
    "EGL_EXT_device_query",
    "EGL_EXT_platform_base",
    "EGL_EXT_platform_device",
    "EGL_KHR_debug",
    "EGL_KHR_fence_sync",
    "EGL_KHR_image",
    "EGL_KHR_stream",
    "EGL_KHR_stream_consumer_gltexture",
    "EGL_KHR_swap_buffers_with_damage",
    "EGL_KHR_wait_sync",
    "EGL_NV_post_sub_buffer",
    "EGL_NV_stream_consumer_gltexture_yuv",
]

# Strip these suffixes from Context entry point names. NV is excluded (for now).
strip_suffixes = ["ANGLE", "EXT", "KHR", "OES", "CHROMIUM"]

# The EGL_ANGLE_explicit_context extension is generated differently from other extensions.
# Toggle generation here.
support_EGL_ANGLE_explicit_context = True

def script_relative(path):
    return os.path.join(os.path.dirname(sys.argv[0]), path)

def path_to(folder, file):
    return os.path.join(script_relative(".."), "src", folder, file)

class GLCommandNames:
    def __init__(self):
        self.command_names = {}

    def get_commands(self, version):
        return self.command_names[version]

    def get_all_commands(self):
        cmd_names = []
        # Combine all the version lists into a single list
        for version, version_cmd_names in sorted(self.command_names.iteritems()):
            cmd_names += version_cmd_names

        return cmd_names

    def add_commands(self, version, commands):
        # Add key if it doesn't exist
        if version not in self.command_names:
            self.command_names[version] = []
        # Add the commands that aren't duplicates
        self.command_names[version] += commands

class RegistryXML:
    def __init__(self, xml_file, ext_file = None):
        tree = etree.parse(script_relative(xml_file))
        self.root = tree.getroot()
        if (ext_file):
            self._AppendANGLEExts(ext_file)
        self.all_commands = self.root.findall('commands/command')
        self.all_cmd_names = GLCommandNames()
        self.commands = {}

    def _AppendANGLEExts(self, ext_file):
        angle_ext_tree = etree.parse(script_relative(ext_file))
        angle_ext_root = angle_ext_tree.getroot()

        insertion_point = self.root.findall("./commands")[0]
        for command in angle_ext_root.iter('commands'):
            insertion_point.extend(command)

        insertion_point = self.root.findall("./extensions")[0]
        for extension in angle_ext_root.iter('extensions'):
            insertion_point.extend(extension)

    def AddCommands(self, feature_name, annotation):
        xpath = ".//feature[@name='%s']//command" % feature_name
        commands = [cmd.attrib['name'] for cmd in self.root.findall(xpath)]

        # Remove commands that have already been processed
        current_cmds = self.all_cmd_names.get_all_commands()
        commands = [cmd for cmd in commands if cmd not in current_cmds]

        self.all_cmd_names.add_commands(annotation, commands)
        self.commands[annotation] = commands

    def _ClassifySupport(self, supported):
        if 'gles2' in supported:
            return 'gl2ext'
        elif 'gles1' in supported:
            return 'glext'
        elif 'egl' in supported:
            return 'eglext'
        elif 'wgl' in supported:
            return 'wglext'
        else:
            assert False
            return 'unknown'

    def AddExtensionCommands(self, supported_extensions, apis):
        # Use a first step to run through the extensions so we can generate them
        # in sorted order.
        self.ext_data = {}
        self.ext_dupes = {}
        ext_annotations = {}

        for extension in self.root.findall("extensions/extension"):
            extension_name = extension.attrib['name']
            if not extension_name in supported_extensions:
                continue

            ext_annotations[extension_name] = self._ClassifySupport(extension.attrib['supported'])

            ext_cmd_names = []

            # There's an extra step here to filter out 'api=gl' extensions. This
            # is necessary for handling KHR extensions, which have separate entry
            # point signatures (without the suffix) for desktop GL. Note that this
            # extra step is necessary because of Etree's limited Xpath support.
            for require in extension.findall('require'):
                if 'api' in require.attrib and require.attrib['api'] not in apis:
                    continue

                # A special case for EXT_texture_storage
                filter_out_comment = "Supported only if GL_EXT_direct_state_access is supported"
                if 'comment' in require.attrib and require.attrib['comment'] == filter_out_comment:
                    continue

                extension_commands = require.findall('command')
                ext_cmd_names += [command.attrib['name'] for command in extension_commands]

            self.ext_data[extension_name] = sorted(ext_cmd_names)

        for extension_name, ext_cmd_names in sorted(self.ext_data.iteritems()):

            # Detect and filter duplicate extensions.
            dupes = []
            for ext_cmd in ext_cmd_names:
                if ext_cmd in self.all_cmd_names.get_all_commands():
                    dupes.append(ext_cmd)

            for dupe in dupes:
                ext_cmd_names.remove(dupe)

            self.ext_data[extension_name] = sorted(ext_cmd_names)
            self.ext_dupes[extension_name] = dupes
            self.all_cmd_names.add_commands(ext_annotations[extension_name], ext_cmd_names)
