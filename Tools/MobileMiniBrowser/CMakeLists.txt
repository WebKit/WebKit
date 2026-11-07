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

set(EXECUTABLE_NAME MobileMiniBrowser)
set(PRODUCT_NAME MobileMiniBrowser)
set(PRODUCT_BUNDLE_IDENTIFIER org.webkit.MobileMiniBrowser)

if (WEBKIT_SDK_IS_MACCATALYST)
    set(_resources_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MobileMiniBrowser.app/Contents/Resources)
    set(_device_family 2 6)
    set(_target_devices --target-device ipad)
else ()
    set(_resources_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/MobileMiniBrowser.app)
    set(_device_family 1 2 7)
    set(_target_devices --target-device iphone --target-device ipad)
endif ()

WEBKIT_PROCESS_INFO_PLIST(${MOBILEMINIBROWSER_DIR}/MobileMiniBrowser/Info.plist ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
    TARGETED_DEVICE_FAMILY ${_device_family})
unset(_device_family)

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
            ${_target_devices} --minimum-deployment-target ${CMAKE_OSX_DEPLOYMENT_TARGET}
            --compile ${_output} ${_storyboard}
        DEPENDS ${_storyboard}
        VERBATIM)
    list(APPEND _compiled_storyboards ${_output})
endforeach ()
add_custom_target(MobileMiniBrowserStoryboards DEPENDS ${_compiled_storyboards})
unset(_storyboards)
unset(_target_devices)

WEBKIT_COPY_FILES(MobileMiniBrowserResources
    DESTINATION ${_resources_dir}
    FILES ${MobileMiniBrowser_RESOURCES}
    FLATTENED NO_SYMLINK)

set(MobileMiniBrowser_COMPILE_OPTIONS -fobjc-arc)
set(MobileMiniBrowser_PRIVATE_LIBRARIES
    "-framework Foundation"
    "-framework UIKit"
    WebKit::WebKit
)
set(MobileMiniBrowser_DEPENDENCIES MobileMiniBrowserStoryboards MobileMiniBrowserResources)

WEBKIT_EXECUTABLE_DECLARE(MobileMiniBrowser)
set_target_properties(MobileMiniBrowser PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_BINARY_DIR}/Info.plist
)
# The bundle's signature covers its Info.plist and resources.
set(MobileMiniBrowser_CODE_SIGN_INPUTS ${_compiled_storyboards} ${CMAKE_CURRENT_BINARY_DIR}/Info.plist)
foreach (_resource IN LISTS MobileMiniBrowser_RESOURCES)
    get_filename_component(_name ${_resource} NAME)
    list(APPEND MobileMiniBrowser_CODE_SIGN_INPUTS ${_resources_dir}/${_name})
endforeach ()
if (WEBKIT_SDK_IS_MACCATALYST)
    foreach (_service IN ITEMS WebProcess NetworkProcess GPUProcess)
        set_property(TARGET MobileMiniBrowser APPEND PROPERTY LINK_DEPENDS $<TARGET_FILE:${_service}>)
    endforeach ()
else ()
    set_target_properties(MobileMiniBrowser PROPERTIES
        CODE_SIGN_BUNDLE "$<TARGET_BUNDLE_DIR:MobileMiniBrowser>")

    set(_entitlements ${MOBILEMINIBROWSER_DIR}/MobileMiniBrowser/MobileMiniBrowser.entitlements)
    if (WEBKIT_SDK_IS_SIMULATOR)
        WEBKIT_EMBED_ENTITLEMENTS(MobileMiniBrowser ${_entitlements})
    else ()
        set_target_properties(MobileMiniBrowser PROPERTIES CODE_SIGN_ENTITLEMENTS ${_entitlements})
    endif ()
    unset(_entitlements)

    # Embeds the same extensions, with the same Info.plist changes, as
    # TestRunnerShared/Scripts/install-extensions-rule.sh does for the Xcode build.
    if (USE_EXTENSIONKIT)
        add_dependencies(MobileMiniBrowser WebContentExtension WebContentEnhancedSecurityExtension NetworkingExtension)
        if (ENABLE_GPU_PROCESS)
            add_dependencies(MobileMiniBrowser GPUExtension)
        endif ()

        WEBKIT_EMBED_EXTENSION(MobileMiniBrowser WebContentExtension ${PRODUCT_BUNDLE_IDENTIFIER}
            CHANGE_EXTENSION_POINT ADD_ATS)
        WEBKIT_EMBED_EXTENSION(MobileMiniBrowser WebContentEnhancedSecurityExtension ${PRODUCT_BUNDLE_IDENTIFIER}
            CHANGE_EXTENSION_POINT)
        WEBKIT_EMBED_EXTENSION(MobileMiniBrowser NetworkingExtension ${PRODUCT_BUNDLE_IDENTIFIER}
            ADD_ATS)
        if (ENABLE_GPU_PROCESS)
            WEBKIT_EMBED_EXTENSION(MobileMiniBrowser GPUExtension ${PRODUCT_BUNDLE_IDENTIFIER})
        endif ()
    endif ()
endif ()
# Signs the app after the extensions are embedded.
WEBKIT_EXECUTABLE(MobileMiniBrowser)

unset(_compiled_storyboards)
unset(_resources_dir)
