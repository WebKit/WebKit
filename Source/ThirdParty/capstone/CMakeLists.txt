if(POLICY CMP0042)
  # http://www.cmake.org/cmake/help/v3.0/policy/CMP0042.html
  cmake_policy(SET CMP0042 NEW)
endif(POLICY CMP0042)

if (POLICY CMP0048)
  # use old policy to honor version set using VERSION_* variables to preserve backwards
  # compatibility. change OLD to NEW when minimum cmake version is updated to 3.* and
  # set VERSION using project(capstone VERSION 4.0.0).
  # http://www.cmake.org/cmake/help/v3.0/policy/CMP0048.html
  cmake_policy (SET CMP0048 OLD)
endif()

add_definitions(-DCAPSTONE_USE_SYS_DYN_MEM)

## sources
set(SOURCES_ENGINE
    Source/cs.c
    Source/MCInst.c
    Source/MCInstrDesc.c
    Source/MCRegisterInfo.c
    Source/SStream.c
    Source/utils.c
)
set(HEADERS_ENGINE
    Source/cs_priv.h
    Source/LEB128.h
    Source/MathExtras.h
    Source/MCDisassembler.h
    Source/MCFixedLenDisassembler.h
    Source/MCInst.h
    Source/MCInstrDesc.h
    Source/MCRegisterInfo.h
    Source/SStream.h
    Source/utils.h
    )

set(HEADERS_COMMON
    Source/include/capstone/arm64.h
    Source/include/capstone/arm.h
    Source/include/capstone/capstone.h
    Source/include/capstone/evm.h
    Source/include/capstone/mips.h
    Source/include/capstone/ppc.h
    Source/include/capstone/x86.h
    Source/include/capstone/sparc.h
    Source/include/capstone/systemz.h
    Source/include/capstone/xcore.h
    Source/include/capstone/m68k.h
    Source/include/capstone/tms320c64x.h
    Source/include/capstone/m680x.h
    Source/include/capstone/mos65xx.h
    Source/include/capstone/platform.h
    )

## architecture support
if (WTF_CPU_ARM)
    add_definitions(-DCAPSTONE_HAS_ARM)
    set(SOURCES_ARM
        Source/arch/ARM/ARMDisassembler.c
        Source/arch/ARM/ARMInstPrinter.c
        Source/arch/ARM/ARMMapping.c
        Source/arch/ARM/ARMModule.c
    )
    set(HEADERS_ARM
        Source/arch/ARM/ARMAddressingModes.h
        Source/arch/ARM/ARMBaseInfo.h
        Source/arch/ARM/ARMDisassembler.h
        Source/arch/ARM/ARMGenAsmWriter.inc
        Source/arch/ARM/ARMGenDisassemblerTables.inc
        Source/arch/ARM/ARMGenInstrInfo.inc
        Source/arch/ARM/ARMGenRegisterInfo.inc
        Source/arch/ARM/ARMGenSubtargetInfo.inc
        Source/arch/ARM/ARMInstPrinter.h
        Source/arch/ARM/ARMMapping.h
        Source/arch/ARM/ARMMappingInsn.inc
        Source/arch/ARM/ARMMappingInsnOp.inc
        )
    set(HEADERS_ARM
        Source/arch/ARM/ARMAddressingModes.h
        Source/arch/ARM/ARMBaseInfo.h
        Source/arch/ARM/ARMDisassembler.h
        Source/arch/ARM/ARMGenAsmWriter.inc
        Source/arch/ARM/ARMGenDisassemblerTables.inc
        Source/arch/ARM/ARMGenInstrInfo.inc
        Source/arch/ARM/ARMGenRegisterInfo.inc
        Source/arch/ARM/ARMGenSubtargetInfo.inc
        Source/arch/ARM/ARMInstPrinter.h
        Source/arch/ARM/ARMMapping.h
        )
endif ()

if (WTF_CPU_MIPS)
    add_definitions(-DCAPSTONE_HAS_MIPS)
    set(SOURCES_MIPS
        Source/arch/Mips/MipsDisassembler.c
        Source/arch/Mips/MipsInstPrinter.c
        Source/arch/Mips/MipsMapping.c
        Source/arch/Mips/MipsModule.c
    )
    set(HEADERS_MIPS
        Source/arch/Mips/MipsDisassembler.h
        Source/arch/Mips/MipsGenAsmWriter.inc
        Source/arch/Mips/MipsGenDisassemblerTables.inc
        Source/arch/Mips/MipsGenInstrInfo.inc
        Source/arch/Mips/MipsGenRegisterInfo.inc
        Source/arch/Mips/MipsGenSubtargetInfo.inc
        Source/arch/Mips/MipsInstPrinter.h
        Source/arch/Mips/MipsMapping.h
        Source/arch/Mips/MipsMappingInsn.inc
        )
    set(HEADERS_MIPS
        Source/arch/Mips/MipsDisassembler.h
        Source/arch/Mips/MipsGenAsmWriter.inc
        Source/arch/Mips/MipsGenDisassemblerTables.inc
        Source/arch/Mips/MipsGenInstrInfo.inc
        Source/arch/Mips/MipsGenRegisterInfo.inc
        Source/arch/Mips/MipsGenSubtargetInfo.inc
        Source/arch/Mips/MipsInstPrinter.h
        Source/arch/Mips/MipsMapping.h
        )
endif ()

set(capstone_SOURCES
    ${SOURCES_ENGINE}
    ${SOURCES_ARM}
    ${SOURCES_MIPS}
    )

set(capstone_HEADERS
    ${HEADERS_COMMON}
    ${HEADERS_ENGINE}
    ${HEADERS_ARM}
    ${HEADERS_MIPS}
    )

set(capstone_INCLUDE_DIRECTORIES "${THIRDPARTY_DIR}/capstone/Source/include")

## targets
add_library(capstone STATIC ${capstone_SOURCES} ${capstone_HEADERS})
set_property(TARGET capstone PROPERTY OUTPUT_NAME capstone)
target_include_directories(capstone PRIVATE ${capstone_INCLUDE_DIRECTORIES})

if (COMPILER_IS_GCC_OR_CLANG)
    WEBKIT_ADD_TARGET_C_FLAGS(capstone
        -Wno-sign-compare
        -Wno-unused-parameter
        -Wno-implicit-fallthrough
        -Wno-missing-field-initializers
        -Wno-missing-format-attribute
        -Wno-discarded-qualifiers
        -Wno-cast-align
    )
endif ()
