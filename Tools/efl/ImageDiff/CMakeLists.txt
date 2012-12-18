set(ImageDiff_SOURCES
    ${TOOLS_DIR}/efl/ImageDiff/ImageDiff.cpp
)

set(ImageDiff_LIBRARIES
    ${ECORE_EVAS_LIBRARIES}
    ${ECORE_LIBRARIES}
    ${EINA_LIBRARIES}
    ${EVAS_LIBRARIES}
    ${WTF_LIBRARY_NAME}
)

set(ImageDiff_INCLUDE_DIRECTORIES
    ${CMAKE_BINARY_DIR}
    ${ECORE_EVAS_INCLUDE_DIRS}
    ${ECORE_INCLUDE_DIRS}
    ${EINA_INCLUDE_DIRS}
    ${EVAS_INCLUDE_DIRS}
    ${WTF_DIR}
)

include_directories(${ImageDiff_INCLUDE_DIRECTORIES})
add_executable(ImageDiff ${ImageDiff_SOURCES})
target_link_libraries(ImageDiff ${ImageDiff_LIBRARIES})
set_target_properties(ImageDiff PROPERTIES FOLDER "Tools")
