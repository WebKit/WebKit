FIND_STAGING_LIBRARY(SLOG2_LIBRARY slog2)
list(APPEND JSC_LIBRARIES
    ${ICUI18N_LIBRARY}
    ${ICUUC_LIBRARY}
    ${INTL_LIBRARY} # Required for x86 builds
    ${M_LIBRARY}
    ${SLOG2_LIBRARY}
    ${Screen_LIBRARY}
    ${WebKitPlatform_LIBRARY}
)

if (PROFILING)
    list(APPEND JSC_LIBRARIES
        ${PROFILING_LIBRARY}
    )
endif ()
