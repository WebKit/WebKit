# For MSVC_RUNTIME_LIBRARY
cmake_minimum_required(VERSION 3.15)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
    message(FATAL_ERROR "In-tree builds are not supported. Run CMake from a separate directory: cmake -B build")
endif()

# Detect whether capstone is compiled as top-level or a subdirectory
set(PROJECT_IS_TOP_LEVEL OFF)
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    set(PROJECT_IS_TOP_LEVEL ON)

    # Enable folder support
    set_property(GLOBAL PROPERTY USE_FOLDERS ON)
endif()

# https://cmake.org/cmake/help/latest/policy/CMP0042.html
cmake_policy(SET CMP0042 NEW)

# https://cmake.org/cmake/help/latest/policy/CMP0091.html
# Enable support for MSVC_RUNTIME_LIBRARY
cmake_policy(SET CMP0091 NEW)

# Check if VERSION is provided externally, otherwise default to 5.0.3
if(NOT DEFINED PROJECT_VERSION)
    set(PROJECT_VERSION "5.0.6")
endif()

# Use PROJECT_VERSION directly for CPack
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})

# Remove the 'v' prefix if it exists and extract the major, minor, and patch versions
string(REGEX MATCH "^[vV]?([0-9]+\\.[0-9]+\\.[0-9]+)" _ ${PROJECT_VERSION})
set(PROJECT_VERSION_BASE ${CMAKE_MATCH_1})

# Print the values of PROJECT_VERSION and PROJECT_VERSION_BASE
message(STATUS "PROJECT_VERSION: ${CPACK_PACKAGE_VERSION} CAPSTONE_VERSION: ${PROJECT_VERSION_BASE}")

# Set the project version without the pre-release identifier
project(capstone VERSION ${PROJECT_VERSION_BASE})

if (MSVC)
    add_compile_options(/W1 /w14189)
else()
    add_compile_options(-Wmissing-braces -Wunused-function -Warray-bounds -Wunused-variable -Wparentheses -Wint-in-bool-context)
endif()


# to configure the options specify them in in the command line or change them in the cmake UI.
# Don't edit the makefile!
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_STATIC_LIBS "Build static library" ON)
option(BUILD_STATIC_RUNTIME "Embed static MSVC runtime (Windows only). Always set if BUILD_SHARED_LIBS=ON" ${BUILD_SHARED_LIBS})
option(CAPSTONE_BUILD_MACOS_THIN "Disable universal2 builds on macOS" OFF)
option(CAPSTONE_BUILD_DIET "Build diet library" OFF)
option(CAPSTONE_BUILD_TESTS "Build tests" ${PROJECT_IS_TOP_LEVEL})
option(CAPSTONE_BUILD_CSTOOL "Build cstool" ${PROJECT_IS_TOP_LEVEL})
option(CAPSTONE_BUILD_CSTEST "Build cstest" OFF)
option(CAPSTONE_USE_DEFAULT_ALLOC "Use default memory allocation functions" ON)
option(CAPSTONE_ARCHITECTURE_DEFAULT "Whether architectures are enabled by default" ON)
option(CAPSTONE_DEBUG "Whether to enable extra debug assertions" OFF)
option(CAPSTONE_INSTALL "Generate install target" ${PROJECT_IS_TOP_LEVEL})

if (NOT BUILD_SHARED_LIBS AND NOT BUILD_STATIC_LIBS)
    FATAL_ERROR("BUILD_SHARED_LIBS and BUILD_STATIC_LIBS are both unset. Nothing to build.")
endif()

set(SUPPORTED_ARCHITECTURES ARM ARM64 M68K MIPS PPC SPARC SYSZ XCORE X86 TMS320C64X M680X EVM MOS65XX WASM BPF RISCV SH TRICORE)
set(SUPPORTED_ARCHITECTURE_LABELS ARM ARM64 M68K MIPS PowerPC Sparc SystemZ XCore x86 TMS320C64x M680x EVM MOS65XX WASM BPF RISCV SH TriCore)

# If building for OSX it's best to allow CMake to handle building both architectures
if(APPLE AND NOT CAPSTONE_BUILD_MACOS_THIN)
    # The cibuildwheel on Github Actions sets this env variable
    # with the architecture flags it wants to build for.
    if(DEFINED ENV{ARCHFLAGS})
        if("$ENV{ARCHFLAGS}" STREQUAL "-arch arm64 -arch x86_64")
            set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
        elseif("$ENV{ARCHFLAGS}" STREQUAL "-arch arm64")
            set(CMAKE_OSX_ARCHITECTURES "arm64")
        elseif("$ENV{ARCHFLAGS}" STREQUAL "-arch x86_64")
            set(CMAKE_OSX_ARCHITECTURES "x86_64")
        endif()
    else()
        set(CMAKE_OSX_ARCHITECTURES "x86_64;arm64")
    endif()
endif()

list(LENGTH SUPPORTED_ARCHITECTURES count)
math(EXPR count "${count}-1")
# create options controlling whether support for a particular architecture is needed
foreach(i RANGE ${count})
    list(GET SUPPORTED_ARCHITECTURES ${i} supported_architecture)
    list(GET SUPPORTED_ARCHITECTURE_LABELS ${i} supported_architecture_label)
    option("CAPSTONE_${supported_architecture}_SUPPORT" "${supported_architecture_label} support" ${CAPSTONE_ARCHITECTURE_DEFAULT})
endforeach()

# propagate architecture support variables to preprocessor
foreach(supported_architecture ${SUPPORTED_ARCHITECTURES})
    set(option_name "CAPSTONE_${supported_architecture}_SUPPORT")
    if(${option_name})
        message("Enabling ${option_name}")
        add_definitions("-D${option_name}")
    endif()
endforeach()

option(CAPSTONE_X86_REDUCE "x86 with reduce instruction sets to minimize library" OFF)
option(CAPSTONE_X86_ATT_DISABLE "Disable x86 AT&T syntax" OFF)
option(CAPSTONE_OSXKERNEL_SUPPORT "Support to embed Capstone into OS X Kernel extensions" OFF)

if(CAPSTONE_BUILD_DIET)
    add_definitions(-DCAPSTONE_DIET)
endif()

if(CAPSTONE_USE_DEFAULT_ALLOC)
    add_definitions(-DCAPSTONE_USE_SYS_DYN_MEM)
endif()

if(CAPSTONE_X86_REDUCE)
    add_definitions(-DCAPSTONE_X86_REDUCE)
endif()

if(CAPSTONE_X86_ATT_DISABLE)
    add_definitions(-DCAPSTONE_X86_ATT_DISABLE)
endif()

if(CAPSTONE_DEBUG)
    add_definitions(-DCAPSTONE_DEBUG)
endif()

# Force static runtime libraries
if(BUILD_STATIC_RUNTIME)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

## sources
set(SOURCES_ENGINE
    cs.c
    Mapping.c
    MCInst.c
    MCInstrDesc.c
    MCRegisterInfo.c
    SStream.c
    utils.c
)
set(HEADERS_ENGINE
    cs_simple_types.h
    cs_priv.h
    LEB128.h
    Mapping.h
    MathExtras.h
    MCDisassembler.h
    MCFixedLenDisassembler.h
    MCInst.h
    MCInstrDesc.h
    MCRegisterInfo.h
    SStream.h
    utils.h
)

set(HEADERS_COMMON
    include/capstone/arm64.h
    include/capstone/arm.h
    include/capstone/capstone.h
    include/capstone/evm.h
    include/capstone/wasm.h
    include/capstone/mips.h
    include/capstone/ppc.h
    include/capstone/x86.h
    include/capstone/sparc.h
    include/capstone/systemz.h
    include/capstone/xcore.h
    include/capstone/m68k.h
    include/capstone/tms320c64x.h
    include/capstone/m680x.h
    include/capstone/mos65xx.h
    include/capstone/bpf.h
    include/capstone/riscv.h
    include/capstone/sh.h
    include/capstone/tricore.h
    include/capstone/platform.h
)

set(TEST_SOURCES test_basic.c test_detail.c test_skipdata.c test_iter.c)

## architecture support
if(CAPSTONE_ARM_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_ARM)
    set(SOURCES_ARM
        arch/ARM/ARMDisassembler.c
        arch/ARM/ARMInstPrinter.c
        arch/ARM/ARMMapping.c
        arch/ARM/ARMModule.c
    )
    set(HEADERS_ARM
        arch/ARM/ARMAddressingModes.h
        arch/ARM/ARMBaseInfo.h
        arch/ARM/ARMDisassembler.h
        arch/ARM/ARMInstPrinter.h
        arch/ARM/ARMMapping.h
        arch/ARM/ARMGenAsmWriter.inc
        arch/ARM/ARMGenDisassemblerTables.inc
        arch/ARM/ARMGenInstrInfo.inc
        arch/ARM/ARMGenRegisterInfo.inc
        arch/ARM/ARMGenSubtargetInfo.inc
        arch/ARM/ARMMappingInsn.inc
        arch/ARM/ARMMappingInsnOp.inc
        arch/ARM/ARMGenRegisterName.inc
        arch/ARM/ARMGenRegisterName_digit.inc
        arch/ARM/ARMGenSystemRegister.inc
        arch/ARM/ARMMappingInsnName.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_arm.c)
endif()

if(CAPSTONE_ARM64_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_ARM64)
    set(SOURCES_ARM64
        arch/AArch64/AArch64BaseInfo.c
        arch/AArch64/AArch64Disassembler.c
        arch/AArch64/AArch64InstPrinter.c
        arch/AArch64/AArch64Mapping.c
        arch/AArch64/AArch64Module.c
    )
    set(HEADERS_ARM64
        arch/AArch64/AArch64AddressingModes.h
        arch/AArch64/AArch64BaseInfo.h
        arch/AArch64/AArch64Disassembler.h
        arch/AArch64/AArch64InstPrinter.h
        arch/AArch64/AArch64Mapping.h
        arch/AArch64/AArch64GenAsmWriter.inc
        arch/AArch64/AArch64GenDisassemblerTables.inc
        arch/AArch64/AArch64GenInstrInfo.inc
        arch/AArch64/AArch64GenRegisterInfo.inc
        arch/AArch64/AArch64GenRegisterName.inc
        arch/AArch64/AArch64GenRegisterV.inc
        arch/AArch64/AArch64GenSubtargetInfo.inc
        arch/AArch64/AArch64GenSystemOperands.inc
        arch/AArch64/AArch64GenSystemOperands_enum.inc
        arch/AArch64/AArch64MappingInsn.inc
        arch/AArch64/AArch64MappingInsnName.inc
        arch/AArch64/AArch64MappingInsnOp.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_arm64.c)
endif()

if(CAPSTONE_MIPS_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_MIPS)
    set(SOURCES_MIPS
        arch/Mips/MipsDisassembler.c
        arch/Mips/MipsInstPrinter.c
        arch/Mips/MipsMapping.c
        arch/Mips/MipsModule.c
    )
    set(HEADERS_MIPS
        arch/Mips/MipsDisassembler.h
        arch/Mips/MipsGenAsmWriter.inc
        arch/Mips/MipsGenDisassemblerTables.inc
        arch/Mips/MipsGenInstrInfo.inc
        arch/Mips/MipsGenRegisterInfo.inc
        arch/Mips/MipsGenSubtargetInfo.inc
        arch/Mips/MipsInstPrinter.h
        arch/Mips/MipsMapping.h
        arch/Mips/MipsMappingInsn.inc
    )
    set(HEADERS_MIPS
        arch/Mips/MipsDisassembler.h
        arch/Mips/MipsGenAsmWriter.inc
        arch/Mips/MipsGenDisassemblerTables.inc
        arch/Mips/MipsGenInstrInfo.inc
        arch/Mips/MipsGenRegisterInfo.inc
        arch/Mips/MipsGenSubtargetInfo.inc
        arch/Mips/MipsInstPrinter.h
        arch/Mips/MipsMapping.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_mips.c)
endif()

if(CAPSTONE_PPC_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_POWERPC)
    set(SOURCES_PPC
        arch/PowerPC/PPCDisassembler.c
        arch/PowerPC/PPCInstPrinter.c
        arch/PowerPC/PPCMapping.c
        arch/PowerPC/PPCModule.c
    )
    set(HEADERS_PPC
        arch/PowerPC/PPCDisassembler.h
        arch/PowerPC/PPCGenAsmWriter.inc
        arch/PowerPC/PPCInstPrinter.h
        arch/PowerPC/PPCMapping.h
        arch/PowerPC/PPCPredicates.h
        arch/PowerPC/PPCGenAsmWriter.inc
        arch/PowerPC/PPCGenRegisterName.inc
        arch/PowerPC/PPCGenDisassemblerTables.inc
        arch/PowerPC/PPCMappingInsn.inc
        arch/PowerPC/PPCMappingInsnName.inc
        arch/PowerPC/PPCGenSubtargetInfo.inc
        arch/PowerPC/PPCGenRegisterInfo.inc
        arch/PowerPC/PPCGenInstrInfo.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_ppc.c)
endif()

if(CAPSTONE_X86_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_X86)
    set(SOURCES_X86
        arch/X86/X86Disassembler.c
        arch/X86/X86DisassemblerDecoder.c
        arch/X86/X86IntelInstPrinter.c
        arch/X86/X86InstPrinterCommon.c
        arch/X86/X86Mapping.c
        arch/X86/X86Module.c
    )
    set(HEADERS_X86
        arch/X86/X86BaseInfo.h
        arch/X86/X86Disassembler.h
        arch/X86/X86DisassemblerDecoder.h
        arch/X86/X86DisassemblerDecoderCommon.h
        arch/X86/X86GenAsmWriter.inc
        arch/X86/X86GenAsmWriter1.inc
        arch/X86/X86GenAsmWriter1_reduce.inc
        arch/X86/X86GenAsmWriter_reduce.inc
        arch/X86/X86GenDisassemblerTables.inc
        arch/X86/X86GenDisassemblerTables_reduce.inc
        arch/X86/X86GenInstrInfo.inc
        arch/X86/X86GenInstrInfo_reduce.inc
        arch/X86/X86GenRegisterInfo.inc
        arch/X86/X86InstPrinter.h
        arch/X86/X86Mapping.h
        arch/X86/X86MappingInsn.inc
        arch/X86/X86MappingInsnOp.inc
        arch/X86/X86MappingInsnOp_reduce.inc
        arch/X86/X86MappingInsn_reduce.inc
    )
    set(HEADERS_X86
        arch/X86/X86BaseInfo.h
        arch/X86/X86Disassembler.h
        arch/X86/X86DisassemblerDecoder.h
        arch/X86/X86DisassemblerDecoderCommon.h
        arch/X86/X86GenAsmWriter.inc
        arch/X86/X86GenAsmWriter1.inc
        arch/X86/X86GenAsmWriter1_reduce.inc
        arch/X86/X86GenAsmWriter_reduce.inc
        arch/X86/X86GenDisassemblerTables.inc
        arch/X86/X86GenDisassemblerTables_reduce.inc
        arch/X86/X86GenInstrInfo.inc
        arch/X86/X86GenInstrInfo_reduce.inc
        arch/X86/X86GenRegisterInfo.inc
        arch/X86/X86InstPrinter.h
        arch/X86/X86Mapping.h
    )
    if(NOT CAPSTONE_BUILD_DIET)
        set(SOURCES_X86 ${SOURCES_X86} arch/X86/X86ATTInstPrinter.c)
    endif()
    set(TEST_SOURCES ${TEST_SOURCES} test_x86.c test_customized_mnem.c)
endif()

if(CAPSTONE_SPARC_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_SPARC)
    set(SOURCES_SPARC
        arch/Sparc/SparcDisassembler.c
        arch/Sparc/SparcInstPrinter.c
        arch/Sparc/SparcMapping.c
        arch/Sparc/SparcModule.c
    )
    set(HEADERS_SPARC
        arch/Sparc/Sparc.h
        arch/Sparc/SparcDisassembler.h
        arch/Sparc/SparcGenAsmWriter.inc
        arch/Sparc/SparcGenDisassemblerTables.inc
        arch/Sparc/SparcGenInstrInfo.inc
        arch/Sparc/SparcGenRegisterInfo.inc
        arch/Sparc/SparcGenSubtargetInfo.inc
        arch/Sparc/SparcInstPrinter.h
        arch/Sparc/SparcMapping.h
        arch/Sparc/SparcMappingInsn.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_sparc.c)
endif()

if(CAPSTONE_SYSZ_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_SYSZ)
    set(SOURCES_SYSZ
        arch/SystemZ/SystemZDisassembler.c
        arch/SystemZ/SystemZInstPrinter.c
        arch/SystemZ/SystemZMapping.c
        arch/SystemZ/SystemZModule.c
        arch/SystemZ/SystemZMCTargetDesc.c
    )
    set(HEADERS_SYSZ
        arch/SystemZ/SystemZDisassembler.h
        arch/SystemZ/SystemZGenAsmWriter.inc
        arch/SystemZ/SystemZGenDisassemblerTables.inc
        arch/SystemZ/SystemZGenInsnNameMaps.inc
        arch/SystemZ/SystemZGenInstrInfo.inc
        arch/SystemZ/SystemZGenRegisterInfo.inc
        arch/SystemZ/SystemZGenSubtargetInfo.inc
        arch/SystemZ/SystemZInstPrinter.h
        arch/SystemZ/SystemZMapping.h
        arch/SystemZ/SystemZMappingInsn.inc
        arch/SystemZ/SystemZMCTargetDesc.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_systemz.c)
endif()

if(CAPSTONE_XCORE_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_XCORE)
    set(SOURCES_XCORE
        arch/XCore/XCoreDisassembler.c
        arch/XCore/XCoreInstPrinter.c
        arch/XCore/XCoreMapping.c
        arch/XCore/XCoreModule.c
    )
    set(HEADERS_XCORE
        arch/XCore/XCoreDisassembler.h
        arch/XCore/XCoreGenAsmWriter.inc
        arch/XCore/XCoreGenDisassemblerTables.inc
        arch/XCore/XCoreGenInstrInfo.inc
        arch/XCore/XCoreGenRegisterInfo.inc
        arch/XCore/XCoreInstPrinter.h
        arch/XCore/XCoreMapping.h
        arch/XCore/XCoreMappingInsn.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_xcore.c)
endif()

if(CAPSTONE_M68K_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_M68K)
    set(SOURCES_M68K
        arch/M68K/M68KDisassembler.c
        arch/M68K/M68KInstPrinter.c
        arch/M68K/M68KModule.c
    )
    set(HEADERS_M68K
        arch/M68K/M68KDisassembler.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_m68k.c)
endif()

if(CAPSTONE_TMS320C64X_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_TMS320C64X)
    set(SOURCES_TMS320C64X
        arch/TMS320C64x/TMS320C64xDisassembler.c
        arch/TMS320C64x/TMS320C64xInstPrinter.c
        arch/TMS320C64x/TMS320C64xMapping.c
        arch/TMS320C64x/TMS320C64xModule.c
    )
    set(HEADERS_TMS320C64X
        arch/TMS320C64x/TMS320C64xDisassembler.h
        arch/TMS320C64x/TMS320C64xGenAsmWriter.inc
        arch/TMS320C64x/TMS320C64xGenDisassemblerTables.inc
        arch/TMS320C64x/TMS320C64xGenInstrInfo.inc
        arch/TMS320C64x/TMS320C64xGenRegisterInfo.inc
        arch/TMS320C64x/TMS320C64xInstPrinter.h
        arch/TMS320C64x/TMS320C64xMapping.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_tms320c64x.c)
endif()

if(CAPSTONE_M680X_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_M680X)
    set(SOURCES_M680X
        arch/M680X/M680XDisassembler.c
        arch/M680X/M680XInstPrinter.c
        arch/M680X/M680XModule.c
    )
    set(HEADERS_M680X
        arch/M680X/M680XInstPrinter.h
        arch/M680X/M680XDisassembler.h
        arch/M680X/M680XDisassemblerInternals.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_m680x.c)
endif()

if(CAPSTONE_EVM_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_EVM)
    set(SOURCES_EVM
        arch/EVM/EVMDisassembler.c
        arch/EVM/EVMInstPrinter.c
        arch/EVM/EVMMapping.c
        arch/EVM/EVMModule.c
    )
    set(HEADERS_EVM
        arch/EVM/EVMDisassembler.h
        arch/EVM/EVMInstPrinter.h
        arch/EVM/EVMMapping.h
        arch/EVM/EVMMappingInsn.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_evm.c)
endif()

if(CAPSTONE_WASM_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_WASM)
    set(SOURCES_WASM
        arch/WASM/WASMDisassembler.c
        arch/WASM/WASMInstPrinter.c
        arch/WASM/WASMMapping.c
        arch/WASM/WASMModule.c
    )
    set(HEADERS_WASM
        arch/WASM/WASMDisassembler.h
        arch/WASM/WASMInstPrinter.h
        arch/WASM/WASMMapping.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_wasm.c)
endif()

if(CAPSTONE_MOS65XX_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_MOS65XX)
    set(SOURCES_MOS65XX
            arch/MOS65XX/MOS65XXModule.c
            arch/MOS65XX/MOS65XXDisassembler.c)
    set(HEADERS_SOURCES_MOS65XX
            arch/MOS65XX/MOS65XXDisassembler.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_mos65xx.c)
endif()

if(CAPSTONE_BPF_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_BPF)
    set(SOURCES_BPF
        arch/BPF/BPFDisassembler.c
        arch/BPF/BPFInstPrinter.c
        arch/BPF/BPFMapping.c
        arch/BPF/BPFModule.c
    )
    set(HEADERS_BPF
        arch/BPF/BPFConstants.h
        arch/BPF/BPFDisassembler.h
        arch/BPF/BPFInstPrinter.h
        arch/BPF/BPFMapping.h
        arch/BPF/BPFModule.h
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_bpf.c)
endif()

if(CAPSTONE_RISCV_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_RISCV)
    set(SOURCES_RISCV
        arch/RISCV/RISCVDisassembler.c
        arch/RISCV/RISCVInstPrinter.c
        arch/RISCV/RISCVMapping.c
        arch/RISCV/RISCVModule.c
    )
    set(HEADERS_RISCV
        arch/RISCV/RISCVBaseInfo.h
        arch/RISCV/RISCVDisassembler.h
        arch/RISCV/RISCVInstPrinter.h
        arch/RISCV/RISCVMapping.h
        arch/RISCV/RISCVModule.h
        arch/RISCV/RISCVGenAsmWriter.inc
        arch/RISCV/RISCVGenDisassemblerTables.inc
        arch/RISCV/RISCVGenInsnNameMaps.inc
        arch/RISCV/RISCVGenInstrInfo.inc
        arch/RISCV/RISCVGenRegisterInfo.inc
        arch/RISCV/RISCVGenSubtargetInfo.inc
        arch/RISCV/RISCVMappingInsn.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_riscv.c)
endif()

if(CAPSTONE_SH_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_SH)
    set(SOURCES_SH
        arch/SH/SHDisassembler.c
        arch/SH/SHInstPrinter.c
        arch/SH/SHModule.c
    )
    set(HEADERS_SH
        arch/SH/SHDisassembler.h
        arch/SH/SHInstPrinter.h
        arch/SH/SHModule.h
        arch/SH/SHInsnTable.inc
    )
    set(TEST_SOURCES ${TEST_SOURCES} test_sh.c)
endif()

if (CAPSTONE_TRICORE_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_TRICORE)
    set(SOURCES_TRICORE
            arch/TriCore/TriCoreDisassembler.c
            arch/TriCore/TriCoreInstPrinter.c
            arch/TriCore/TriCoreMapping.c
            arch/TriCore/TriCoreModule.c
            )
    set(HEADERS_TRICORE
            arch/TriCore/TriCoreDisassembler.h
            arch/TriCore/TriCoreLinkage.h
            arch/TriCore/TriCoreGenAsmWriter.inc
            arch/TriCore/TriCoreGenDisassemblerTables.inc
            arch/TriCore/TriCoreGenInstrInfo.inc
            arch/TriCore/TriCoreGenRegisterInfo.inc
            arch/TriCore/TriCoreMapping.h
            arch/TriCore/TriCoreModule.h
            )
    set(TEST_SOURCES ${TEST_SOURCES} test_tricore.c)
endif ()

if (CAPSTONE_OSXKERNEL_SUPPORT)
    add_definitions(-DCAPSTONE_HAS_OSXKERNEL)
endif()

set(ALL_SOURCES
    ${SOURCES_ENGINE}
    ${SOURCES_ARM}
    ${SOURCES_ARM64}
    ${SOURCES_MIPS}
    ${SOURCES_PPC}
    ${SOURCES_X86}
    ${SOURCES_SPARC}
    ${SOURCES_SYSZ}
    ${SOURCES_XCORE}
    ${SOURCES_M68K}
    ${SOURCES_TMS320C64X}
    ${SOURCES_M680X}
    ${SOURCES_EVM}
    ${SOURCES_WASM}
    ${SOURCES_MOS65XX}
    ${SOURCES_BPF}
    ${SOURCES_RISCV}
    ${SOURCES_SH}
    ${SOURCES_TRICORE}
)

set(ALL_HEADERS
    ${HEADERS_COMMON}
    ${HEADERS_ENGINE}
    ${HEADERS_ARM}
    ${HEADERS_ARM64}
    ${HEADERS_MIPS}
    ${HEADERS_PPC}
    ${HEADERS_X86}
    ${HEADERS_SPARC}
    ${HEADERS_SYSZ}
    ${HEADERS_XCORE}
    ${HEADERS_M68K}
    ${HEADERS_TMS320C64X}
    ${HEADERS_M680X}
    ${HEADERS_EVM}
    ${HEADERS_WASM}
    ${HEADERS_MOS65XX}
    ${HEADERS_BPF}
    ${HEADERS_RISCV}
    ${HEADERS_SH}
    ${HEADERS_TRICORE}
)

## properties
# version info
set_property(GLOBAL PROPERTY VERSION ${PROJECT_VERSION})

## targets
add_library(capstone OBJECT ${ALL_SOURCES} ${ALL_HEADERS})
set_property(TARGET capstone PROPERTY C_STANDARD 99)
target_include_directories(capstone PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
)
if(BUILD_STATIC_LIBS)
    add_library(capstone_static STATIC $<TARGET_OBJECTS:capstone>)
    # Use normal capstone name. Otherwise we get libcapstone_static.a
    set_target_properties(capstone_static PROPERTIES OUTPUT_NAME "capstone")
    target_include_directories(capstone_static PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    )
endif()

if(BUILD_SHARED_LIBS)
    set_property(TARGET capstone PROPERTY POSITION_INDEPENDENT_CODE 1)
    add_library(capstone_shared SHARED $<TARGET_OBJECTS:capstone>)
    # Use normal capstone name. Otherwise we get libcapstone_shared.so
    set_target_properties(capstone_shared PROPERTIES OUTPUT_NAME "capstone")
    set_target_properties(capstone_shared PROPERTIES
        VERSION ${PROJECT_VERSION}
        SOVERSION ${PROJECT_VERSION_MAJOR}
    )
    target_include_directories(capstone_shared PUBLIC
        $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    )
    target_compile_definitions(capstone PUBLIC CAPSTONE_SHARED)
endif()

if(CAPSTONE_BUILD_TESTS)
    set(CMAKE_FOLDER "Tests")
    enable_testing()
    foreach(TSRC ${TEST_SOURCES})
        string(REGEX REPLACE ".c$" "" TBIN ${TSRC})
        add_executable(${TBIN} "tests/${TSRC}")
        target_link_libraries(${TBIN} PRIVATE capstone)
        add_test(NAME "capstone_${TBIN}" COMMAND ${TBIN})
    endforeach()
    if(CAPSTONE_ARM_SUPPORT)
        set(ARM_REGRESS_TEST test_arm_regression.c)
        string(REGEX REPLACE ".c$" "" ARM_REGRESS_BIN ${ARM_REGRESS_TEST})
        add_executable(${ARM_REGRESS_BIN} "suite/arm/${ARM_REGRESS_TEST}")
        target_link_libraries(${ARM_REGRESS_BIN} PRIVATE capstone)
        add_test(NAME "capstone_${ARM_REGRESS_BIN}" COMMAND ${ARM_REGRESS_BIN})
    endif()
    # fuzz target built with the tests
    add_executable(fuzz_disasm suite/fuzz/onefile.c suite/fuzz/fuzz_disasm.c suite/fuzz/platform.c)
    target_link_libraries(fuzz_disasm PRIVATE capstone)
    unset(CMAKE_FOLDER)
endif()

source_group("Source\\Engine" FILES ${SOURCES_ENGINE})
source_group("Source\\ARM" FILES ${SOURCES_ARM})
source_group("Source\\ARM64" FILES ${SOURCES_ARM64})
source_group("Source\\Mips" FILES ${SOURCES_MIPS})
source_group("Source\\PowerPC" FILES ${SOURCES_PPC})
source_group("Source\\Sparc" FILES ${SOURCES_SPARC})
source_group("Source\\SystemZ" FILES ${SOURCES_SYSZ})
source_group("Source\\X86" FILES ${SOURCES_X86})
source_group("Source\\XCore" FILES ${SOURCES_XCORE})
source_group("Source\\M68K" FILES ${SOURCES_M68K})
source_group("Source\\TMS320C64x" FILES ${SOURCES_TMS320C64X})
source_group("Source\\M680X" FILES ${SOURCES_M680X})
source_group("Source\\EVM" FILES ${SOURCES_EVM})
source_group("Source\\WASM" FILES ${SOURCES_WASM})
source_group("Source\\MOS65XX" FILES ${SOURCES_MOS65XX})
source_group("Source\\BPF" FILES ${SOURCES_BPF})
source_group("Source\\RISCV" FILES ${SOURCES_RISCV})
source_group("Source\\SH" FILES ${SOURCES_SH})
source_group("Source\\TriCore" FILES ${SOURCES_TRICORE})

source_group("Include\\Common" FILES ${HEADERS_COMMON})
source_group("Include\\Engine" FILES ${HEADERS_ENGINE})
source_group("Include\\ARM" FILES ${HEADERS_ARM})
source_group("Include\\ARM64" FILES ${HEADERS_ARM64})
source_group("Include\\Mips" FILES ${HEADERS_MIPS})
source_group("Include\\PowerPC" FILES ${HEADERS_PPC})
source_group("Include\\Sparc" FILES ${HEADERS_SPARC})
source_group("Include\\SystemZ" FILES ${HEADERS_SYSZ})
source_group("Include\\X86" FILES ${HEADERS_X86})
source_group("Include\\XCore" FILES ${HEADERS_XCORE})
source_group("Include\\M68K" FILES ${HEADERS_M68K})
source_group("Include\\TMS320C64x" FILES ${HEADERS_TMS320C64X})
source_group("Include\\M680X" FILES ${HEADERS_MC680X})
source_group("Include\\EVM" FILES ${HEADERS_EVM})
source_group("Include\\WASM" FILES ${HEADERS_WASM})
source_group("Include\\MOS65XX" FILES ${HEADERS_MOS65XX})
source_group("Include\\BPF" FILES ${HEADERS_BPF})
source_group("Include\\RISCV" FILES ${HEADERS_RISCV})
source_group("Include\\SH" FILES ${HEADERS_SH})
source_group("Include\\TriCore" FILES ${HEADERS_TRICORE})

## installation
if(CAPSTONE_INSTALL)
    include(GNUInstallDirs)
    install(FILES ${HEADERS_COMMON} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/capstone)

    # Support absolute installation paths (discussion: https://github.com/NixOS/nixpkgs/issues/144170)
    if(IS_ABSOLUTE ${CMAKE_INSTALL_LIBDIR})
        set(CAPSTONE_PKGCONFIG_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
        set(CAPSTONE_CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})
    else()
        set(CAPSTONE_PKGCONFIG_INSTALL_LIBDIR "\${prefix}/${CMAKE_INSTALL_LIBDIR}")
        set(CAPSTONE_CMAKE_INSTALL_LIBDIR "\${PACKAGE_PREFIX_DIR}/${CMAKE_INSTALL_LIBDIR}")
    endif()
    if(IS_ABSOLUTE ${CMAKE_INSTALL_INCLUDEDIR})
        set(CAPSTONE_PKGCONFIG_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
        set(CAPSTONE_CMAKE_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR})
    else()
        set(CAPSTONE_PKGCONFIG_INSTALL_INCLUDEDIR "\${prefix}/${CMAKE_INSTALL_INCLUDEDIR}")
        set(CAPSTONE_CMAKE_INSTALL_INCLUDEDIR "\${PACKAGE_PREFIX_DIR}/${CMAKE_INSTALL_INCLUDEDIR}")
    endif()

    configure_file(capstone.pc.in ${CMAKE_BINARY_DIR}/capstone.pc @ONLY)
    install(FILES ${CMAKE_BINARY_DIR}/capstone.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

    include(CMakePackageConfigHelpers)
    set(CAPSTONE_CMAKE_CONFIG_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/capstone")
    configure_package_config_file(
        capstone-config.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/capstone-config.cmake
        INSTALL_DESTINATION ${CAPSTONE_CMAKE_CONFIG_INSTALL_DIR}
    )
    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/capstone-config-version.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/capstone-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/capstone-config-version.cmake"
        DESTINATION ${CAPSTONE_CMAKE_CONFIG_INSTALL_DIR}
    )

    if(BUILD_SHARED_LIBS)
        set(LIB_INSTALL_TARGETS capstone_shared)
    endif()

    if (BUILD_STATIC_LIBS)
        set(LIB_INSTALL_TARGETS ${LIB_INSTALL_TARGETS} capstone_static)
    endif()

    install(TARGETS ${LIB_INSTALL_TARGETS}
        EXPORT capstone-targets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    install(EXPORT capstone-targets
            NAMESPACE capstone::
            DESTINATION ${CAPSTONE_CMAKE_CONFIG_INSTALL_DIR}
    )

    # uninstall target
    if(NOT TARGET UNINSTALL)
        configure_file(
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake_uninstall.cmake.in"
            "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
            IMMEDIATE @ONLY
        )
        set(CMAKE_FOLDER)
        add_custom_target(UNINSTALL COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)
        set_target_properties(UNINSTALL PROPERTIES
            FOLDER CMakePredefinedTargets
        )
    endif()
endif()

if(CAPSTONE_BUILD_CSTOOL)
    file(GLOB CSTOOL_SRC cstool/*.c)
    add_executable(cstool ${CSTOOL_SRC})
    target_link_libraries(cstool PRIVATE capstone)

    if(CAPSTONE_INSTALL)
        install(TARGETS cstool EXPORT capstone-targets DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

if(CAPSTONE_BUILD_CSTEST)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(CMOCKA REQUIRED IMPORTED_TARGET cmocka)

    file(GLOB CSTEST_SRC suite/cstest/src/*.c)
    add_executable(cstest ${CSTEST_SRC})
    target_link_libraries(cstest PUBLIC capstone PkgConfig::CMOCKA)
    target_include_directories(cstest PRIVATE
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            ${PROJECT_SOURCE_DIR}/suite/cstest/include
            ${CMOCKA_INCLUDE_DIRS}
            )

    if(CAPSTONE_INSTALL)
        install(TARGETS cstest EXPORT capstone-targets DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
endif()

set(CPACK_PROJECT_CONFIG_FILE "${CMAKE_SOURCE_DIR}/CPackConfig.cmake")
include(CPackConfig.txt)
