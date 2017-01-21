import json
import logging
import re

from webkitpy.common.host import Host
from webkitpy.common.system.filesystem import FileSystem

CHANGESET_NOT_AVAILABLE = 'Not Available'

_log = logging.getLogger(__name__)


# Notes:
# Generation of info.plist for the framework is not triggered, using a static version instead in Source/ThirdParty/libwebrtc/WebKit.
# Generation expects that ".." refers to the root of the libwebrtc repository folder
# - input json file should be generated from gn using the --ide=json argument on a direct child folder of the libwebrtc repository folder
# cd /Users/Alice/WebKit/Source/ThirdParty/libwebrtc/libwebrtc
# gn gen out '--args=is_component_build=false rtc_include_tests=true rtc_use_h264=true rtc_libvpx_build_vp9=false rtc_libvpx_build_vp8=false target_os="mac" target_cpu="x64"' --ide=json

# The CMakeLists.txt uses  two variables:
# - LIBWEBRTC_INPUT_DIR: absolute path to the root of the libwebrtc folder (default to ${CMAKE_SOURCE_DIR}/libwebrtc)
# - LIBWEBRTC_OUTPUT_DIR: absolute path to the root of the build folder (default to ${CMAKE_BINARY_DIR})
# cd /Users/Alice/WebKit/Source/ThirdParty/libwebrtc/libwebrtc
# Tools/Scripts/generate-libwebrtc-cmake

# Before calling make, apply Source/ThirdParty/libwebrtc/WebKit/patch_libwebrtc_headers patch

# cmake /Users/Alice/WebKit/Source/ThirdParty/libwebrtc -B/Users/Alice/WebKit/WebKitBuild/Debug/libwebrtc
# cd /Users/Alice/WebKit/Source/ThirdParty/libwebrtc/libwebrtc
# git apply ../WebKit/patch_libwebrtc_headers
# make -C /Users/Alice/WebKit/WebKitBuild/Debug/libwebrtc


def main(args, _stdout, _stderr):
    json_file = args[0] if len(args) >= 1 else "Source/ThirdParty/libwebrtc/WebKit/project.json"
    output_file = args[1] if len(args) >= 2 else "Source/ThirdParty/libwebrtc/CMakeLists.txt"

    configure_logging()

    generator = CMakeGenerator(json_file, output_file)
    generator.generate()


def configure_logging():
    class LogHandler(logging.StreamHandler):

        def format(self, record):
            if record.levelno > logging.INFO:
                return "%s: %s" % (record.levelname, record.getMessage())
            return record.getMessage()

    logger = logging.getLogger()
    logger.setLevel(logging.INFO)
    handler = LogHandler()
    handler.setLevel(logging.INFO)
    logger.addHandler(handler)
    return handler


class CMakeGenerator(object):

    def __init__(self, inputFilename, outputFilename):
        self.host = Host()
        self.filesystem = FileSystem()
        self.project = json.loads(self.filesystem.read_text_file(inputFilename))

        self.enable_g711 = False
        self.enable_g722 = False
        # Current Openssl cannot really compile since they use deprecated openssl functions
        self.enable_boringssl = True
        self.enable_vpx = False
        self.enable_libjpeg = False

        self.targets = self.project["targets"]
        self.outputFilename = outputFilename

        self.skip_test_targets = True

        self.starting_lines = ["cmake_minimum_required(VERSION 3.5)",
                              "set(CMAKE_CXX_STANDARD 11)",
                              "enable_language(ASM)",
                              "",
                              "if (NOT LIBWEBRTC_INPUT_DIR)",
                              "    set(LIBWEBRTC_INPUT_DIR ${CMAKE_SOURCE_DIR}/Source)",
                              "endif ()",
                              "if (NOT LIBWEBRTC_OUTPUT_DIR)",
                              "    set(LIBWEBRTC_OUTPUT_DIR ${CMAKE_BINARY_DIR})",
                              "endif ()",
                              "",
                              "file(WRITE ${LIBWEBRTC_OUTPUT_DIR}/dummy.c \"\")",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/libjpeg_turbo/simd_asm)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/ffmpeg/ffmpeg_yasm)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/webrtc/sdk)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/third_party/yasm/include)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/audio_coding/neteq)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/logging/rtc_event_log)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_coding/audio_network_adaptor)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/modules/audio_processing)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/sdk)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/gen/webrtc/tools/event_log_visualizer)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/audio_coding/neteq)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/logging/rtc_event_log)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_coding/audio_network_adaptor)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/modules/audio_processing)",
                              "file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/pyproto/webrtc/tools/event_log_visualizer)",
                              "",
                              ]

        self.ending_lines = ["",
                            "set_target_properties(WebrtcBaseGtest_Prod PROPERTIES LINKER_LANGUAGE CXX)",
                            "set_target_properties(WebrtcLoggingRtc_Event_Log_Api PROPERTIES LINKER_LANGUAGE CXX)",
                            ]
        if self.enable_libjpeg:
            self.ending_lines.append("set_target_properties(Third_PartyLibjpeg_TurboSimd_Asm PROPERTIES LINKER_LANGUAGE CXX)")

        self.initialize_targets()

    def initialize_targets(self):
        # Simplifying generation
        self.targets["//webrtc/sdk:rtc_sdk_framework_objc"]["sources"][:] = []
        # Static_library requires as least one source file
        self.targets["//webrtc/sdk:rtc_sdk_objc"]["sources"] = ["//out/dummy.c"]
        # Executable target without any source file
        self.targets["//webrtc:webrtc_tests"]["type"] = "group"
        # Duplicate symbol issue with source_set
        self.targets["//webrtc/api:call_api"]["type"] = "static_library"
        # Simpler for linking WebCore
        self.targets["//third_party/boringssl:boringssl"]["type"] = "static_library"
        self.targets["//third_party/boringssl:boringssl"]["outputs"] = ["//out/libboringssl.a"]
        # We use a static info plist instead of a dynamic one
        del self.targets["//webrtc/sdk:rtc_sdk_framework_objc_info_plist"]
        self.targets["//webrtc/sdk:rtc_sdk_framework_objc_info_plist_bundle_data"]["deps"].remove("//webrtc/sdk:rtc_sdk_framework_objc_info_plist")

        # Macro to change specific things in LibWebRTC, only used in libjingle_peerconnection currently
        self.targets["//webrtc/api:libjingle_peerconnection"]["defines"].append("WEBRTC_WEBKIT_BUILD")

        if not self.enable_g711:
            self.remove_webrtc_g711()
        if not self.enable_g722:
            self.remove_g722()

        if self.enable_boringssl:
            self.ending_lines.append("set_target_properties(Third_PartyBoringsslBoringssl_Asm PROPERTIES LINKER_LANGUAGE CXX)")
        else:
            self.remove_boringssl()

        if self.enable_vpx:
            self.ending_lines.append("set_target_properties(Third_PartyLibvpxLibvpx_Yasm PROPERTIES LINKER_LANGUAGE CXX)")
            self.starting_lines.append("file(MAKE_DIRECTORY ${LIBWEBRTC_OUTPUT_DIR}/obj/third_party/libvpx/libvpx_yasm)")
        else:
            self.remove_libvpx()

        self.remove_openmax_dl()

        if not self.enable_libjpeg:
            self.remove_libjpeg()
            self.remove_yasm()

        self.remove_webrtc_base_sha1()
        self.targets.pop("//build/config/sanitizers:options_sources")

    def _remove_target(self, targetName):
        self.targets.pop(targetName)
        for name, target in self.targets.iteritems():
            if "deps" in target:
                deps = target["deps"]
                if targetName in deps:
                    deps.remove(targetName)

    def remove_webrtc_g711(self):
        self._remove_target("//webrtc/modules/audio_coding:g711_test")
        self._remove_target("//webrtc/modules/audio_coding:neteq_pcmu_quality_test")
        self._remove_target("//webrtc/modules/audio_coding:audio_decoder_unittests")

        self._remove_target("//webrtc/modules/audio_coding:g711")
        self.targets["//webrtc/modules/audio_coding:pcm16b"]["sources"].append("//webrtc/modules/audio_coding/codecs/g711/audio_encoder_pcm.cc")
        self.targets["//webrtc/modules/audio_coding:pcm16b"]["source_outputs"]["//webrtc/modules/audio_coding/codecs/g711/audio_encoder_pcm.cc"] = "obj/webrtc/modules/audio_coding/g711/audio_encoder_pcm.o"
        for name, target in self.targets.iteritems():
            if "include_dirs" in target:
                include_dirs = target["include_dirs"]
                if "//webrtc/modules/audio_coding/codecs/g711/include/" in include_dirs:
                    include_dirs.remove("//webrtc/modules/audio_coding/codecs/g711/include/")

            if "defines" in target:
                defines = target["defines"]
                if "CODEC_G711" in defines:
                    defines.remove("CODEC_G711")

    def remove_libjpeg(self):
        self.targets.pop("//third_party/libjpeg_turbo:libjpeg")
        self.targets.pop("//third_party:jpeg")
        self.targets.pop("//third_party/libjpeg_turbo:simd")
        self.targets.pop("//third_party/libjpeg_turbo:simd_asm")
        self.targets.pop("//third_party/libjpeg_turbo:simd_asm_action")

        libyuv = self.targets["//third_party/libyuv:libyuv"]
        libyuv["deps"].remove("//third_party:jpeg")
        libyuv["defines"].remove("HAVE_JPEG")
        libyuv["defines"].remove("USE_LIBJPEG_TURBO=1")

        self.targets["//third_party/libyuv:libyuv_unittest"]["defines"].remove("HAVE_JPEG")
        self.targets["//third_party/libyuv:psnr"]["defines"].remove("HAVE_JPEG")

        for name, target in self.targets.iteritems():
            if "include_dirs" in target:
                include_dirs = target["include_dirs"]
                if "//third_party/openmax_dl/" in include_dirs:
                    include_dirs.remove("//third_party/openmax_dl/")

            if "deps" in target:
                deps = target["deps"]
                if "//third_party:jpeg" in deps:
                    deps.remove("//third_party:jpeg")

            if "defines" in target:
                defines = target["defines"]
                if "RTC_USE_OPENMAX_DL" in defines:
                    defines.remove("RTC_USE_OPENMAX_DL")

    def remove_webrtc_base_sha1(self):
        base = self.targets["//webrtc/base:rtc_base"]
        base["source_outputs"].pop("//webrtc/base/sha1.cc")
        base["sources"].remove("//webrtc/base/sha1.cc")

    def remove_yasm(self):
        self.targets.pop("//third_party/yasm:yasm")
        self.targets.pop("//third_party/yasm:compile_gperf")
        self.targets.pop("//third_party/yasm:compile_gperf_for_include")
        self.targets.pop("//third_party/yasm:compile_nasm_macros")
        self.targets.pop("//third_party/yasm:compile_nasm_version")
        self.targets.pop("//third_party/yasm:compile_re2c")
        self.targets.pop("//third_party/yasm:compile_re2c_lc3b")
        self.targets.pop("//third_party/yasm:compile_win64_gas")
        self.targets.pop("//third_party/yasm:compile_win64_nasm")
        self.targets.pop("//third_party/yasm:generate_license")
        self.targets.pop("//third_party/yasm:generate_module")
        self.targets.pop("//third_party/yasm:generate_version")
        self.targets.pop("//third_party/yasm:yasm_utils")
        self.targets.pop("//third_party/yasm:genperf")
        self.targets.pop("//third_party/yasm:genmodule")
        self.targets.pop("//third_party/yasm:re2c")
        self.targets.pop("//third_party/yasm:genstring")
        self.targets.pop("//third_party/yasm:genversion")
        self.targets.pop("//third_party/yasm:genmacro")

    def remove_openmax_dl(self):
        self.targets.pop("//third_party/openmax_dl/dl:dl")
        for name, target in self.targets.iteritems():
            if "include_dirs" in target:
                include_dirs = target["include_dirs"]
                if "//third_party/openmax_dl/" in include_dirs:
                    include_dirs.remove("//third_party/openmax_dl/")

            if "deps" in target:
                deps = target["deps"]
                if "//third_party/openmax_dl/dl:dl" in deps:
                    deps.remove("//third_party/openmax_dl/dl:dl")

            if "defines" in target:
                defines = target["defines"]
                if "RTC_USE_OPENMAX_DL" in defines:
                    defines.remove("RTC_USE_OPENMAX_DL")

        common_audio = self.targets["//webrtc/common_audio:common_audio"]
        common_audio["source_outputs"].pop("//webrtc/common_audio/real_fourier_openmax.cc")
        common_audio["sources"].remove("//webrtc/common_audio/real_fourier_openmax.cc")

    def remove_libvpx(self):
        self.targets = {name: target for name, target in self.targets.iteritems() if not ("libvpx" in name or "vp9" in name or "vp8" in name)}
        for name, target in self.targets.iteritems():
            if "include_dirs" in target:
                include_dirs = target["include_dirs"]
                if "//third_party/libvpx/source/libvpx/" in include_dirs:
                    include_dirs.remove("//third_party/libvpx/source/libvpx/")

            if not "deps" in target:
                continue
            target["deps"] = [dep for dep in target["deps"] if not ("libvpx" in dep or "vp9" in dep or "vp8" in dep)]

        target = self.targets["//webrtc/modules/video_coding:video_coding"]
        target["defines"].append("RTC_DISABLE_VP8")
        target["defines"].append("RTC_DISABLE_VP9")
        target["sources"].append("//webrtc/modules/video_coding/codecs/vp9/vp9_noop.cc")
        target["source_outputs"]["//webrtc/modules/video_coding/codecs/vp9/vp9_noop.cc"] = ["obj/webrtc/modules/video_coding/webrtc_vp9/vp9_noop.o"]

        target = self.targets["//webrtc/media:rtc_media"]
        target["defines"].append("RTC_DISABLE_VP8")
        target["defines"].append("RTC_DISABLE_VP9")

    def remove_boringssl(self):
        self.targets.pop("//third_party/boringssl:boringssl")
        self.targets.pop("//third_party/boringssl:boringssl_asm")
        for name, target in self.targets.iteritems():
            if "include_dirs" in target:
                include_dirs = target["include_dirs"]
                if "//third_party/boringssl/src/include/" in include_dirs:
                    include_dirs.remove("//third_party/boringssl/src/include/")
                    #include_dirs.append("/usr/local/opt/openssl/include/")

            if not "deps" in target:
                continue
            deps = target["deps"]
            if "//third_party/boringssl:boringssl" in deps:
                deps.remove("//third_party/boringssl:boringssl")
                # Do we need this one?
                target["defines"].append("OPENSSL_NO_SSL_INTERN")
                # Do we need to set -L for access to the libs?
                target["ldflags"].extend(["-lcrypto", "-lssl"])
        self.targets["//webrtc/p2p:stun_prober"]["ldflags"].extend(["-lcrypto", "-lssl"])

    def remove_g722(self):
        self.targets.pop("//webrtc/modules/audio_coding:g722")
        self.targets.pop("//webrtc/modules/audio_coding:g722_test")
        for name, target in self.targets.iteritems():
            if "defines" in target:
                defines = target["defines"]
                if "WEBRTC_CODEC_G722" in defines:
                    defines.remove("WEBRTC_CODEC_G722")
                if "CODEC_G722" in defines:
                    defines.remove("CODEC_G722")

            if "include_dirs" in target:
                include_dirs = target["include_dirs"]
                if "//webrtc/modules/audio_coding/codecs/g722/include/" in include_dirs:
                    include_dirs.remove("//webrtc/modules/audio_coding/codecs/g722/include/")

            if not "deps" in target:
                continue
            deps = target["deps"]
            if "//webrtc/modules/audio_coding:g722" in deps:
                deps.remove("//webrtc/modules/audio_coding:g722")
            if "//webrtc/modules/audio_coding:g722_test" in target["deps"]:
                deps.remove("//webrtc/modules/audio_coding:g722_test")

    def generate(self):
        lines = self.starting_lines

        lines.extend(self._initialize_frameworks())

        for name, target in self.targets.iteritems():
            lines.append("\n".join(self.generate_target(self.sanitize_target_name(name), target)))

        lines.extend(self.generate_libwebrtc_target())
        lines.extend(self.ending_lines)
        self.write_lazily("\n".join(lines))

    def _initialize_frameworks(self):
        lines = []
        frameworks = []
        for name, target in self.targets.iteritems():
            if ('sdk' in name and not "peerconnection" in name):
                continue
            if "libs" in target:
                frameworks.extend(target["libs"])
        frameworks = list(set(frameworks))
        for framework in frameworks:
            framework = framework.replace(".framework", "")
            lines.append("find_library(" + framework.upper() + "_LIBRARY " + framework + ")")

        return lines

    def write_lazily(self, content):
        if (self.filesystem.exists(self.outputFilename)):
            old_content = self.filesystem.read_text_file(self.outputFilename)
            if old_content == content:
                return
        self.filesystem.write_text_file(self.outputFilename, content)

    def sanitize_target_name(self, name):
        return "".join([step.title() for step in re.split('/|:', name)])

    def convert_deps(self, names):
        return " ".join([self.sanitize_target_name(name) for name in names])

    def convert_source(self, source):
        return source.replace("//out", "${LIBWEBRTC_OUTPUT_DIR}").replace("//", "${LIBWEBRTC_INPUT_DIR}/")

    def convert_input(self, input):
        return input.replace("//out", "${LIBWEBRTC_OUTPUT_DIR}").replace("//", "${LIBWEBRTC_INPUT_DIR}/")

    def convert_inputs(self, inputs):
        return " ".join(inputs).replace("//out", "${LIBWEBRTC_OUTPUT_DIR}").replace("//", "${LIBWEBRTC_INPUT_DIR}/")

    def convert_output(self, output):
        return output.replace("//out", "${LIBWEBRTC_OUTPUT_DIR}")

    def convert_outputs(self, outputs):
        return " ".join(outputs).replace("//out", "${LIBWEBRTC_OUTPUT_DIR}")

    def generate_libwebrtc_target(self):
        skipped_sources = [
            "//webrtc/base/sha1.cc",
            "//webrtc/base/sha1digest.cc",
            "//webrtc/base/md5.cc",
            "//webrtc/base/md5digest.cc",

            "//webrtc/base/json.cc"
            "//third_party/jsoncpp/overrides/src/lib_json/json_reader.cpp",
            "//third_party/jsoncpp/overrides/src/lib_json/json_value.cpp",
            "//third_party/jsoncpp/source/src/lib_json/json_writer.cpp"]
        lines = []
        lines.append("# Start of target LIBWEBRTC")
        objects = []
        dependencies = []
        for name, target in self.targets.iteritems():
            if target["testonly"] or name.startswith("//webrtc/examples"):
                continue
            if "source_outputs" in target:
                for source, output in target["source_outputs"].iteritems():
                    if source in skipped_sources:
                        continue
                    if source.endswith(".o"):
                        continue
                    dependencies.append(self.sanitize_target_name(name))
                    if source.endswith(".asm"):
                        objects.append(output[0].replace("_action", ""))
                    elif output[0].endswith(".o"):
                        filename = source.replace("//out/", "").replace("//", "Source/")
                        if not filename.endswith(".o"):
                            filename += ".o"
                        objects.append(("CMakeFiles/" + self.sanitize_target_name(name) + ".dir/" + filename))
        dependencies = list(set(dependencies))

        lines.append("file(WRITE ${LIBWEBRTC_OUTPUT_DIR}/list_libwebrtc_objects \"" + "\n".join(objects) + "\")")
        lines.append("add_custom_command(OUTPUT ${LIBWEBRTC_OUTPUT_DIR}/../libwebrtc.a")
        lines.append("    COMMAND libtool -static -o ${LIBWEBRTC_OUTPUT_DIR}/../libwebrtc.a -filelist ${LIBWEBRTC_OUTPUT_DIR}/list_libwebrtc_objects")
        lines.append("    VERBATIM)")

        lines.append("add_custom_target(LIBWEBRTC DEPENDS " + " ".join(dependencies) + " ${LIBWEBRTC_OUTPUT_DIR}/../libwebrtc.a)")
        lines.append("# End of target LIBWEBRTC")
        return lines

    def generate_target(self, name, target):
        if (self.skip_test_targets and target["testonly"]) or name.startswith("WebrtcExamples"):
            return []

        lines = ["\n# Start of target " + name]
        if target["type"] == "action":
            lines.extend(self.generate_action_target(name, target))
        elif target["type"] == "action_foreach":
            lines.extend(self.generate_action_foreach_target(name, target))
        elif target["type"] == "copy":
            lines.extend(self.generate_copy_target(name, target))
        elif target["type"] == "executable":
            lines.extend(self.generate_executable_target(name, target))
        elif target["type"] == "shared_library":
            lines.extend(self.generate_shared_library_target(name, target))
        elif target["type"] == "static_library":
            lines.extend(self.generate_static_library_target(name, target))
        elif target["type"] == "create_bundle":
            lines.extend(self.generate_bundle_target(name, target))
        elif target["type"] == "bundle_data":
            lines.extend(self.generate_bundle_data_target(name, target))
        elif target["type"] == "group":
            lines.extend(self.generate_group_target(name, target))
        elif target["type"] == "source_set":
            lines.extend(self.generate_source_set_target(name, target))
        else:
            raise "unsupported target type: " + target["type"]
        lines.append("# End of target " + name)
        return lines

    def convert_arguments(self, arguments):
        value = ""
        is_first = True
        for argument in arguments:
            if not is_first:
                value += " "
            is_first = False

            if (argument.startswith("../")):
                value += "${LIBWEBRTC_INPUT_DIR}/" + argument[3:]
            elif (argument.startswith("gen/")):
                value += "${LIBWEBRTC_OUTPUT_DIR}/" + argument
            elif (argument.startswith("-I../")):
                value += "-I${LIBWEBRTC_INPUT_DIR}/" + argument[5:]
            elif (argument == "-I."):
                value += "-I${LIBWEBRTC_OUTPUT_DIR}"
            elif (argument == "-I.."):
                value += "-I${LIBWEBRTC_INPUT_DIR}"
            elif (argument == "-Igen"):
                value += "-I${LIBWEBRTC_OUTPUT_DIR}/gen"
            else:
                value += argument
        return value

    def _generate_add_dependencies(self, name, target):
        if not "deps" in target:
            return []
        dependencies = self.convert_deps([dep for dep in target["deps"] if self._is_active_dependency(dep)])
        return ["add_dependencies(" + name + " " + dependencies + ")"] if len(dependencies) else []

    def _is_active_dependency(self, name):
        return not((self.skip_test_targets and self.targets[name]["testonly"]) or name.startswith("//webrtc/examples"))

    def generate_action_target(self, name, target):
        lines = []
        outputs = self.convert_outputs(target["outputs"])
        deps = self.convert_deps(target["deps"])
        args = self.convert_arguments(target["args"])
        script = "${LIBWEBRTC_INPUT_DIR}/" + target["script"][2:]
        if (script.endswith(".py")):
            script = "python " + script

        lines.append("add_custom_command(OUTPUT " + outputs)
        if deps:
            lines.append("    DEPENDS " + deps)
        lines.append("    COMMAND " + script + " " + args)
        lines.append("    VERBATIM)")

        lines.append("add_custom_target(" + name + " DEPENDS " + self.convert_deps(target["deps"]) + " " + self.convert_outputs(target["outputs"]) + ")")

        return lines

    def generate_action_foreach_target(self, name, target):
        lines = []
        outputs = [self.convert_output(output) for output in target["outputs"]]
        deps = self.convert_deps(target["deps"])
        sources = [self.convert_source(source) for source in target["sources"]]
        script = "${LIBWEBRTC_INPUT_DIR}/" + target["script"][2:]
        if (script.endswith(".py")):
            script = "python " + script

        for output, source in zip(outputs, sources):
            args = self.convert_arguments(target["args"])
            args = args.replace("{{source}}", source).replace("{{source_name_part}}", self.filesystem.splitext(self.filesystem.basename(source))[0])
            lines.append("add_custom_command(OUTPUT " + output)
            lines.append("    MAIN_DEPENDENCY " + source)
            lines.append("    COMMAND " + script + " " + args)
            if deps:
                lines.append("    DEPENDS " + deps)
            lines.append("    VERBATIM)")

        lines.append("add_custom_target(" + name + " DEPENDS " + " ".join(outputs) + ")")

        return lines

    def generate_copy_target(self, name, target):
        lines = []
        outputs = self.convert_outputs(target["outputs"])
        sources = [self.convert_source(source) for source in target["sources"]]
        lines.append("list(APPEND " + name + " " + outputs + ")")

        for output, source in zip(target["outputs"], sources):
            lines.append("file(COPY " + source + " DESTINATION " + self.convert_output(output) + ")")
        lines.append("add_custom_target(" + name)
        lines.append("    COMMAND echo \"Generating copy target" + name + "\"")
        lines.append("    VERBATIM)")
        lines.extend(self._generate_add_dependencies(name, target))
        return lines

    def _compute_compile_target_objects(self, name):
        target = self.targets[name]
        if target["type"] == "source_set" and not "sources" in target:
            return []
        sources = ["$<TARGET_OBJECTS:" + self.sanitize_target_name(name) + ">"]
        for dep in self.targets[name]["deps"]:
            if not self.targets[dep]["type"] == "source_set":
                continue
            sources.extend(self._compute_compile_target_objects(dep))
        return sources

    def _compute_compile_target_sources(self, target):
        sources = [self.convert_source(source) for source in target["sources"] if not source.endswith(".h")] if "sources" in target else []
        if target["type"] == "source_set":
            return sources

        for dep in target["deps"]:
            if not self.targets[dep]["type"] == "source_set":
                continue
            sources.extend(self._compute_compile_target_objects(dep))

        return sources

    def _generate_compile_target_sources(self, name, target):
        lines = []
        sources = self._compute_compile_target_sources(target)
        if len(sources):
            lines.append("set(" + name + "_SOURCES " + "\n    ".join(sources) + ")")

        return lines

    def _compute_compile_flags(self, target):
        flags = []
        for flag in ["asmflags", "cflags", "cflags_c", "cflags_cc", "cflags_objc", "cflags_objcc"]:
            if flag in target:
                flags.extend(target[flag])

        self._remove_next_flag = False

        def keep_flag(flag):
            if self._remove_next_flag:
                self._remove_next_flag = False
                return False
            if flag == "-Xclang":
                self._remove_next_flag = True
                return False
            if flag == "-isysroot":
                self._remove_next_flag = True
                return False
            if flag == "-Wno-undefined-var-template":
                return False
            if flag == "-Wno-nonportable-include-path":
                return False
            if flag == "-Wno-address-of-packed-member":
                return False
            if flag == "-std=c++11":
                return False
            return True
        cleaned_flags = filter(keep_flag, flags)
        no_duplicate_flags = []
        [no_duplicate_flags.append(flag) for flag in cleaned_flags if not no_duplicate_flags.count(flag)]
        return no_duplicate_flags

    def compute_include_dirs(self, target):
        dirs = []
        if "include_dirs" in target:
            dirs.extend(target["include_dirs"])
        return dirs

    def _generate_compile_target_options(self, name, target):
        lines = []

        flags = self._compute_compile_flags(target)
        compilation_flags = "\" \"".join(flags)
        lines.append("target_compile_options(" + name + " PRIVATE \"" + compilation_flags + "\")")

        if "defines" in target:
            lines.append("target_compile_definitions(" + name + " PRIVATE " + " ".join(target["defines"]) + ")")

        dirs = list(set(self.compute_include_dirs(target)))
        if len(dirs):
            lines.append("target_include_directories(" + name + " PRIVATE " + self.convert_inputs(dirs) + ")")

        if "ldflags" in target:
            lines.append("set_target_properties(" + name + " PROPERTIES LINK_FLAGS \"" + " ".join(target["ldflags"]) + "\")")

        return lines

    def _compute_linked_libraries(self, target):
        libraries = []
        for dep in target["deps"]:
            dep_target = self.targets[dep]
            if dep_target["type"] == "static_library" or dep_target["type"] == "shared_library":
                libraries.append(self.sanitize_target_name(dep))
            elif dep_target["type"] == "group" or dep_target["type"] == "source_set":
                libraries.extend(self._compute_linked_libraries(dep_target))
        return libraries

    def _generate_linked_libraries(self, name, target):
        return [("target_link_libraries(" + name + " " + library + ")") for library in self._compute_linked_libraries(target)]

    def _handle_frameworks(self, name, target):
        if not "libs" in target:
            return []

        lines = []
        for framework in target["libs"]:
            framework = framework.replace(".framework", "").upper()
            lines.append("target_include_directories(" + name + " PRIVATE ${" + framework + "_INCLUDE_DIR})")
            lines.append("target_link_libraries(" + name + " ${" + framework + "_LIBRARY})")

        return lines

    def _set_output(self, name, target):
        if not "outputs" in target:
            return []

        lines = []
        output = target["outputs"][0]
        if not output.startswith("//out/"):
            raise "Output not in build directory"
        output_dir = "${LIBWEBRTC_OUTPUT_DIR}/" + self.filesystem.dirname(output[6:])
        output_name = self.filesystem.basename(output[6:])
        if output_name.startswith("lib") and output_name.endswith(".a"):
            output_name = output_name[3:-2]
        lines.append("set_target_properties(" + name + " PROPERTIES RUNTIME_OUTPUT_DIRECTORY " + output_dir + ")")
        lines.append("set_target_properties(" + name + " PROPERTIES OUTPUT_NAME " + output_name + ")")
        return lines

    def generate_executable_target(self, name, target):
        lines = self._generate_compile_target_sources(name, target)
        if len(lines):
            lines.append("add_executable(" + name + " ${" + name + "_SOURCES})")
        else:
            lines.append("add_executable(" + name + ")")
        lines.extend(self._generate_compile_target_options(name, target))

        lines.extend(self._set_output(name, target))
        lines.extend(self._generate_linked_libraries(name, target))
        lines.extend(self._handle_frameworks(name, target))

        lines.extend(self._generate_add_dependencies(name, target))
        return lines

    def generate_shared_library_target(self, name, target):
        lines = self._generate_compile_target_sources(name, target)
        if len(lines):
            lines.append("add_library(" + name + " SHARED ${" + name + "_SOURCES})")
        else:
            lines.append("add_library(" + name + " SHARED)")
        lines.extend(self._generate_compile_target_options(name, target))

        lines.extend(self._set_output(name, target))
        lines.extend(self._generate_linked_libraries(name, target))
        lines.extend(self._handle_frameworks(name, target))

        lines.extend(self._generate_add_dependencies(name, target))
        return lines

    def generate_static_library_target(self, name, target):
        lines = self._generate_compile_target_sources(name, target)
        lines.append("add_library(" + name + " STATIC" + ((" ${" + name + "_SOURCES}") if len(lines) else "") + ")")
        lines.extend(self._generate_compile_target_options(name, target))

        lines.extend(self._set_output(name, target))
        lines.extend(self._generate_linked_libraries(name, target))
        lines.extend(self._handle_frameworks(name, target))

        return lines

    def generate_bundle_data_target(self, name, target):
        lines = []
        lines.append("add_custom_target(" + name + ")")
        lines.extend(self._generate_add_dependencies(name, target))
        return lines

    def generate_bundle_target(self, name, target):
        # We replace dynamically Info.plist with a static one.
        info_plist = "${LIBWEBRTC_INPUT_DIR}/../WebKit/" + self.filesystem.basename(target["bundle_data"]["source_files"][-1])
        lines = self.generate_shared_library_target(name, target)
        lines.append("set_target_properties(" + name + """ PROPERTIES
            FRAMEWORK TRUE
            FRAMEWORK_VERSION C
            MACOSX_FRAMEWORK_INFO_PLIST """ + info_plist + ")")
        return lines

    def generate_group_target(self, name, target):
        lines = []
        lines.append("add_custom_target(" + name + ")")
        lines.extend(self._generate_add_dependencies(name, target))
        return lines

    def generate_source_set_target(self, name, target):
        if not "sources" in target or not len(target["sources"]):
            return []

        lines = self._generate_compile_target_sources(name, target)
        if len(lines):
            lines.append("add_library(" + name + " OBJECT ${" + name + "_SOURCES})")
        else:
            lines.append("add_library(" + name + " OBJECT)")
        lines.extend(self._generate_compile_target_options(name, target))

        return lines
