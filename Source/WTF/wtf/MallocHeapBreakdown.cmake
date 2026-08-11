add_library(MallocHeapBreakdown
    STATIC
    malloc_heap_breakdown/main.cpp
)
add_library(WebKit::MallocHeapBreakdown
    ALIAS
    MallocHeapBreakdown
)

target_include_directories(MallocHeapBreakdown
    PUBLIC
    malloc_heap_breakdown
)

if (USE_SYSPROF_CAPTURE)
  target_link_libraries(MallocHeapBreakdown
    PRIVATE
    SysProfCapture::SysProfCapture
  )
  target_compile_definitions(MallocHeapBreakdown
    PRIVATE
    USE_SYSPROF_CAPTURE
  )
endif ()
