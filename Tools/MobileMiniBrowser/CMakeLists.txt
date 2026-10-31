set(MOBILEMINIBROWSER_DIR "${TOOLS_DIR}/MobileMiniBrowser")

set(MobileMiniBrowser_SOURCES
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowser/main.m
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/SceneDelegate.m
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/SettingsViewController.m
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/TabViewController.m
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/WebViewController.m
)

set(MobileMiniBrowser_RESOURCES
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/Resources/index.html
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/Resources/looping.html
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/Resources/looping2s.html
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/Resources/test.mp4
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/Resources/test2s.mp4
)

set(EXECUTABLE_NAME MiniBrowser)
set(PRODUCT_NAME MiniBrowser)
set(PRODUCT_BUNDLE_IDENTIFIER org.webkit.MobileMiniBrowser)

set(_contents_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MiniBrowser.app/Contents)
set(_resources_dir ${_contents_dir}/Resources)

# Xcode leaves the iOS-only keys out of a Mac Catalyst build.
file(READ ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowser/Info.plist _info_plist)
string(REGEX REPLACE "\\$\\(([A-Za-z_]+)\\)" "\${\\1}" _info_plist "${_info_plist}")
set(_info_plist_input ${CMAKE_CURRENT_BINARY_DIR}/MiniBrowser-Info.plist.in)
file(WRITE ${_info_plist_input} "${_info_plist}")
execute_process(COMMAND plutil -remove LSRequiresIPhoneOS ${_info_plist_input} OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
execute_process(COMMAND plutil -remove UIRequiredDeviceCapabilities ${_info_plist_input} OUTPUT_QUIET COMMAND_ERROR_IS_FATAL ANY)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowser/Info.plist)
WEBKIT_PROCESS_INFO_PLIST(${_info_plist_input} ${CMAKE_CURRENT_BINARY_DIR}/MiniBrowser-Info.plist
    TARGETED_DEVICE_FAMILY 2 6)
unset(_info_plist)
unset(_info_plist_input)

set(_storyboards
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowserFramework/Base.lproj/Main.storyboard
    ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowser/Base.lproj/LaunchScreen.storyboard
)
set(_compiled_storyboards)
foreach (_storyboard IN LISTS _storyboards)
    get_filename_component(_name ${_storyboard} NAME_WE)
    set(_output ${_resources_dir}/Base.lproj/${_name}.storyboardc)
    add_custom_command(OUTPUT ${_output}
        COMMAND ${CMAKE_COMMAND} -E make_directory ${_resources_dir}/Base.lproj
        COMMAND ${CMAKE_COMMAND} -E rm -rf ${_output}
        COMMAND ibtool --errors --output-format human-readable-text
            --target-device ipad --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
            --compile ${_output} ${_storyboard}
        DEPENDS ${_storyboard}
        VERBATIM)
    list(APPEND _compiled_storyboards ${_output})
endforeach ()
add_custom_target(MobileMiniBrowserStoryboards DEPENDS ${_compiled_storyboards})
unset(_storyboards)
unset(_compiled_storyboards)

WEBKIT_COPY_FILES(MobileMiniBrowserResources
    DESTINATION ${_resources_dir}
    FILES ${MobileMiniBrowser_RESOURCES}
    FLATTENED NO_SYMLINK)

add_executable(MobileMiniBrowser MACOSX_BUNDLE ${MobileMiniBrowser_SOURCES})
set_target_properties(MobileMiniBrowser PROPERTIES
    OUTPUT_NAME MiniBrowser
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/MiniBrowser-Info.plist
)
target_compile_options(MobileMiniBrowser PRIVATE -fobjc-arc)
target_link_libraries(MobileMiniBrowser PRIVATE
    "-framework Foundation"
    "-framework UIKit"
    WebKit::WebKit
)
add_dependencies(MobileMiniBrowser MobileMiniBrowserStoryboards MobileMiniBrowserResources)
foreach (_service IN ITEMS WebProcess NetworkProcess GPUProcess)
    set_property(TARGET MobileMiniBrowser APPEND PROPERTY LINK_DEPENDS $<TARGET_FILE:${_service}>)
endforeach ()

_WEBKIT_ADD_DSYM(MobileMiniBrowser)
_WEBKIT_ADD_CODE_SIGN(MobileMiniBrowser)

unset(_contents_dir)
unset(_resources_dir)
