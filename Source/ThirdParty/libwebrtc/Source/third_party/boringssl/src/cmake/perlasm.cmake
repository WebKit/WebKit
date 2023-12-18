macro(append_to_parent_scope var)
  list(APPEND ${var} ${ARGN})
  set(${var} "${${var}}" PARENT_SCOPE)
endmacro()

function(add_perlasm_target dest src)
  get_filename_component(dir ${dest} DIRECTORY)
  if(dir STREQUAL "")
    set(dir ".")
  endif()

  add_custom_command(
    OUTPUT ${dest}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${dir}
    COMMAND ${PERL_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/${src} ${ARGN}
            ${dest}
    DEPENDS
    ${src}
    ${PROJECT_SOURCE_DIR}/crypto/perlasm/arm-xlate.pl
    ${PROJECT_SOURCE_DIR}/crypto/perlasm/x86_64-xlate.pl
    ${PROJECT_SOURCE_DIR}/crypto/perlasm/x86asm.pl
    ${PROJECT_SOURCE_DIR}/crypto/perlasm/x86gas.pl
    ${PROJECT_SOURCE_DIR}/crypto/perlasm/x86masm.pl
    ${PROJECT_SOURCE_DIR}/crypto/perlasm/x86nasm.pl
    WORKING_DIRECTORY .
  )
endfunction()

# perlasm generates perlasm output from a given file. arch specifies the
# architecture. dest specifies the basename of the output file. The list of
# generated files will be appended to ${var}_ASM and ${var}_NASM depending on
# the assembler used. Extra arguments are passed to the perlasm script.
function(perlasm var arch dest src)
  if(arch STREQUAL "aarch64")
    add_perlasm_target("${dest}-apple.S" ${src} ios64 ${ARGN})
    add_perlasm_target("${dest}-linux.S" ${src} linux64 ${ARGN})
    add_perlasm_target("${dest}-win.S" ${src} win64 ${ARGN})
    append_to_parent_scope("${var}_ASM" "${dest}-apple.S" "${dest}-linux.S" "${dest}-win.S")
  elseif(arch STREQUAL "arm")
    add_perlasm_target("${dest}-apple.S" ${src} ios32 ${ARGN})
    add_perlasm_target("${dest}-linux.S" ${src} linux32 ${ARGN})
    append_to_parent_scope("${var}_ASM" "${dest}-apple.S" "${dest}-linux.S")
  elseif(arch STREQUAL "x86")
    add_perlasm_target("${dest}-apple.S" ${src} macosx -fPIC -DOPENSSL_IA32_SSE2 ${ARGN})
    add_perlasm_target("${dest}-linux.S" ${src} elf -fPIC -DOPENSSL_IA32_SSE2 ${ARGN})
    add_perlasm_target("${dest}-win.asm" ${src} win32n -DOPENSSL_IA32_SSE2 ${ARGN})
    append_to_parent_scope("${var}_ASM" "${dest}-apple.S" "${dest}-linux.S")
    append_to_parent_scope("${var}_NASM" "${dest}-win.asm")
  elseif(arch STREQUAL "x86_64")
    add_perlasm_target("${dest}-apple.S" ${src} macosx ${ARGN})
    add_perlasm_target("${dest}-linux.S" ${src} elf ${ARGN})
    add_perlasm_target("${dest}-win.asm" ${src} nasm ${ARGN})
    append_to_parent_scope("${var}_ASM" "${dest}-apple.S" "${dest}-linux.S")
    append_to_parent_scope("${var}_NASM" "${dest}-win.asm")
  else()
    message(FATAL_ERROR "Unknown perlasm architecture: $arch")
  endif()
endfunction()
