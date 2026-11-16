# For WPE, JavaScriptCore is built as an OBJECT library whose object files
# are bundled directly into libWPEWebKit, rather than being a separate
# shared library like in GTK.
#
# We only list the shared WebKit library in the frameworks lists below, but
# not the other non-shared libs WTF/bmalloc/JavaScriptCore, because the
# _WEBKIT_TARGET_LINK_FRAMEWORK macro adds object files for OBJECT libraries
# via $<TARGET_OBJECTS:> and their WebKit:: alias interface libraries,
# regardless of whether those objects are already contained in a SHARED
# library in the list. Including both WebKit and the OBJECT frameworks
# would cause duplicate symbols and bloated binaries.

# The goal is to reduce binary size of the built-products. See bug 304213
# This only works with -DDEVELOPER_MODE=ON because without it a webkitglib-symbols.map
# filter is passed to the linker that makes the required symbols not visible.
if (DEVELOPER_MODE)
    set(jsc_FRAMEWORKS WebKit)
    set(testapi_FRAMEWORKS ${jsc_FRAMEWORKS})
    set(testmasm_FRAMEWORKS ${jsc_FRAMEWORKS})
    set(testRegExp_FRAMEWORKS ${jsc_FRAMEWORKS})
    set(testb3_FRAMEWORKS ${jsc_FRAMEWORKS})
    set(testair_FRAMEWORKS ${jsc_FRAMEWORKS})
    set(testdfg_FRAMEWORKS ${jsc_FRAMEWORKS})
    set(testwasmdebugger_FRAMEWORKS ${jsc_FRAMEWORKS})
endif ()
