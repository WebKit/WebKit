# QtWebKit - qmake build info
CONFIG += building-libs
CONFIG += depend_includepath

TARGET = QtWebKit
TEMPLATE = lib

DEFINES += BUILDING_WEBKIT

CONFIG(debug, debug|release) : CONFIG_DIR = debug
else: CONFIG_DIR = release

SOURCE_DIR = $$replace(PWD, /WebKit/qt, "")

CONFIG(standalone_package) {
    isEmpty(WEBKIT2_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = $$PWD/../../JavaScriptCore/generated
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = $$PWD/../../WebCore/generated
    isEmpty(WC_GENERATED_SOURCES_DIR):WEBKIT2_GENERATED_SOURCES_DIR = $$PWD/../../WebKit2/generated
} else {
    isEmpty(WEBKIT2_GENERATED_SOURCES_DIR):JSC_GENERATED_SOURCES_DIR = ../../JavaScriptCore/generated
    isEmpty(WC_GENERATED_SOURCES_DIR):WC_GENERATED_SOURCES_DIR = ../../WebCore/generated
    isEmpty(WC_GENERATED_SOURCES_DIR):WEBKIT2_GENERATED_SOURCES_DIR = ../../WebKit2/generated

    !CONFIG(release, debug|release) {
        OBJECTS_DIR = obj/debug
    } else { # Release
        OBJECTS_DIR = obj/release
    }
}

include($$PWD/Api/headers.pri)
include($$SOURCE_DIR/WebKit.pri)
include($$SOURCE_DIR/JavaScriptCore/JavaScriptCore.pri)
webkit2:include($$SOURCE_DIR/WebKit2/WebKit2.pri)
include($$SOURCE_DIR/WebCore/WebCore.pri)

webkit2:addWebKit2Lib(../../WebKit2)

addWebCoreLib(../../WebCore)

!v8:addJavaScriptCoreLib(../../JavaScriptCore)

isEmpty(OUTPUT_DIR): OUTPUT_DIR = ../..

contains(QT_CONFIG, embedded):CONFIG += embedded

moduleFile=$$PWD/qt_webkit_version.pri
isEmpty(QT_BUILD_TREE):include($$moduleFile)
VERSION = $${QT_WEBKIT_MAJOR_VERSION}.$${QT_WEBKIT_MINOR_VERSION}.$${QT_WEBKIT_PATCH_VERSION}

include_webinspector: RESOURCES += $$SOURCE_DIR/WebCore/inspector/front-end/WebKit.qrc $$WC_GENERATED_SOURCES_DIR/InspectorBackendStub.qrc

# Extract sources to build from the generator definitions
defineTest(addExtraCompiler) {
    isEqual($${1}.wkAddOutputToSources, false): return(true)

    outputRule = $$eval($${1}.output)
    input = $$eval($${1}.input)
    input = $$eval($$input)

    for(file,input) {
        base = $$basename(file)
        base ~= s/\\..+//
        newfile=$$replace(outputRule,\\$\\{QMAKE_FILE_BASE\\},$$base)
        SOURCES += $$newfile
    }
    SOURCES += $$eval($${1}.wkExtraSources)
    export(SOURCES)

    return(true)
}

include($$SOURCE_DIR/WebCore/CodeGenerators.pri)

CONFIG(release):!CONFIG(standalone_package) {
    contains(QT_CONFIG, reduce_exports):CONFIG += hide_symbols
    unix:contains(QT_CONFIG, reduce_relocations):CONFIG += bsymbolic_functions
}

CONFIG(QTDIR_build) {
    include($$QT_SOURCE_TREE/src/qbase.pri)
} else {
    DESTDIR = $$OUTPUT_DIR/lib
    symbian: TARGET =$$TARGET$${QT_LIBINFIX}
}

symbian {
    TARGET.EPOCALLOWDLLDATA=1
    # DRM and Allfiles capabilites need to be audited to be signed on Symbian
    # For regular users that is not possible, so use the CONFIG(production) flag is added
    # To use all capabilies add CONFIG+=production
    # If building from QT source tree, also add CONFIG-=QTDIR_build as qbase.pri defaults capabilities to All -Tcb.    
    CONFIG(production) {
        TARGET.CAPABILITY = All -Tcb
    } else {
        TARGET.CAPABILITY = All -Tcb -DRM -AllFiles
    }
    isEmpty(QT_LIBINFIX) {
        TARGET.UID3 = 0x200267C2
    } else {
        TARGET.UID3 = 0xE00267C2
    }
    webkitlibs.sources = QtWebKit$${QT_LIBINFIX}.dll
    v8:webkitlibs.sources += v8.dll

    CONFIG(QTDIR_build): webkitlibs.sources = $$QMAKE_LIBDIR_QT/$$webkitlibs.sources
    webkitlibs.path = /sys/bin
    vendorinfo = \
        "; Localised Vendor name" \
        "%{\"Nokia\"}" \
        " " \
        "; Unique Vendor name" \
        ":\"Nokia, Qt\"" \
        " "
    webkitlibs.pkg_prerules = vendorinfo

    webkitbackup.sources = symbian/backup_registration.xml
    webkitbackup.path = /private/10202D56/import/packages/$$replace(TARGET.UID3, 0x,)

    contains(QT_CONFIG, declarative) {
         declarativeImport.sources = $$QT_BUILD_TREE/imports/QtWebKit/qmlwebkitplugin$${QT_LIBINFIX}.dll
         declarativeImport.sources += declarative/qmldir
         declarativeImport.path = c:$$QT_IMPORTS_BASE_DIR/QtWebKit
         DEPLOYMENT += declarativeImport
    }

    DEPLOYMENT += webkitlibs webkitbackup
    !CONFIG(production):CONFIG-=def_files

    # Need to build these sources here because of exported symbols
    SOURCES += \
        $$SOURCE_DIR/WebCore/plugins/symbian/PluginViewSymbian.cpp \
        $$SOURCE_DIR/WebCore/plugins/symbian/PluginContainerSymbian.cpp

    HEADERS += \
        $$SOURCE_DIR/WebCore/plugins/symbian/PluginContainerSymbian.h \
        $$SOURCE_DIR/WebCore/plugins/symbian/npinterface.h
}

!static: DEFINES += QT_MAKEDLL

SOURCES += \
    $$PWD/Api/qwebframe.cpp \
    $$PWD/Api/qgraphicswebview.cpp \
    $$PWD/Api/qwebpage.cpp \
    $$PWD/Api/qwebview.cpp \
    $$PWD/Api/qwebelement.cpp \
    $$PWD/Api/qwebhistory.cpp \
    $$PWD/Api/qwebsettings.cpp \
    $$PWD/Api/qwebhistoryinterface.cpp \
    $$PWD/Api/qwebplugindatabase.cpp \
    $$PWD/Api/qwebpluginfactory.cpp \
    $$PWD/Api/qwebsecurityorigin.cpp \
    $$PWD/Api/qwebscriptworld.cpp \
    $$PWD/Api/qwebdatabase.cpp \
    $$PWD/Api/qwebinspector.cpp \
    $$PWD/Api/qwebkitversion.cpp \
    \
    $$PWD/WebCoreSupport/QtFallbackWebPopup.cpp \
    $$PWD/WebCoreSupport/ChromeClientQt.cpp \
    $$PWD/WebCoreSupport/ContextMenuClientQt.cpp \
    $$PWD/WebCoreSupport/DragClientQt.cpp \
    $$PWD/WebCoreSupport/DumpRenderTreeSupportQt.cpp \
    $$PWD/WebCoreSupport/EditorClientQt.cpp \
    $$PWD/WebCoreSupport/EditCommandQt.cpp \
    $$PWD/WebCoreSupport/FrameLoaderClientQt.cpp \
    $$PWD/WebCoreSupport/FrameNetworkingContextQt.cpp \
    $$PWD/WebCoreSupport/GeolocationPermissionClientQt.cpp \
    $$PWD/WebCoreSupport/InspectorClientQt.cpp \
    $$PWD/WebCoreSupport/InspectorServerQt.cpp \
    $$PWD/WebCoreSupport/NotificationPresenterClientQt.cpp \
    $$PWD/WebCoreSupport/PageClientQt.cpp \
    $$PWD/WebCoreSupport/PopupMenuQt.cpp \
    $$PWD/WebCoreSupport/QtPlatformPlugin.cpp \
    $$PWD/WebCoreSupport/SearchPopupMenuQt.cpp \
    $$PWD/WebCoreSupport/WebPlatformStrategies.cpp

webkit2 {
    SOURCES += \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKArray.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKCertificateInfo.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKContextMenuItem.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKGraphicsContext.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKImage.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKNumber.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKSecurityOrigin.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKSerializedScriptValue.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKString.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKType.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKURL.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKURLRequest.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKURLResponse.cpp \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKUserContentURLPattern.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKAuthenticationChallenge.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKAuthenticationDecisionListener.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKBackForwardList.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKBackForwardListItem.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKContext.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKCredential.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKDatabaseManager.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKDownload.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKFrame.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKFramePolicyListener.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKGeolocationManager.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKGeolocationPermissionRequest.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKGeolocationPosition.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKInspector.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKOpenPanelParameters.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKOpenPanelResultListener.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKNavigationData.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPage.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPageGroup.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPluginSiteDataManager.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPreferences.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKProtectionSpace.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/cpp/qt/WKStringQt.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/cpp/qt/WKURLQt.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/ClientImpl.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qgraphicswkview.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkcontext.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkhistory.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkpage.cpp \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkpreferences.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundle.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleFrame.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleInspector.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePage.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePageGroup.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleScriptWorld.cpp \
        $$SOURCE_DIR/WebKit2/WebProcess/qt/WebProcessMainQt.cpp
}

HEADERS += \
    $$WEBKIT_API_HEADERS \
    $$PWD/Api/qwebplugindatabase_p.h \
    \
    $$PWD/WebCoreSupport/InspectorServerQt.h \
    $$PWD/WebCoreSupport/QtFallbackWebPopup.h \
    $$PWD/WebCoreSupport/FrameLoaderClientQt.h \
    $$PWD/WebCoreSupport/FrameNetworkingContextQt.h \
    $$PWD/WebCoreSupport/GeolocationPermissionClientQt.h \
    $$PWD/WebCoreSupport/NotificationPresenterClientQt.h \
    $$PWD/WebCoreSupport/PageClientQt.h \
    $$PWD/WebCoreSupport/QtPlatformPlugin.h \
    $$PWD/WebCoreSupport/PopupMenuQt.h \
    $$PWD/WebCoreSupport/SearchPopupMenuQt.h \
    $$PWD/WebCoreSupport/WebPlatformStrategies.h

webkit2 {
    HEADERS += \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKBase.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKCertificateInfo.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKContextMenuItem.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKContextMenuItemTypes.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKGeometry.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKGraphicsContext.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKImage.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKNumber.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKPageLoadTypes.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKSecurityOrigin.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKSerializedScriptValue.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKSharedAPICast.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKString.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKStringPrivate.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKType.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKURL.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKURLRequest.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKURLResponse.h \
        $$SOURCE_DIR/WebKit2/Shared/API/c/WKUserContentURLPattern.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKAPICast.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKAuthenticationChallenge.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKAuthenticationDecisionListener.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKBackForwardList.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKBackForwardListItem.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKContext.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKContextPrivate.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKCredential.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKCredentialTypes.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKDatabaseManager.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKDownload.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKFrame.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKFramePolicyListener.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKGeolocationManager.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKGeolocationPermissionRequest.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKGeolocationPosition.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKInspector.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKOpenPanelParameters.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKOpenPanelResultListener.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKNavigationData.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPage.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPageGroup.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPagePrivate.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPluginSiteDataManager.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPreferences.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKPreferencesPrivate.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKProtectionSpace.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WKProtectionSpaceTypes.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/WebKit2.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/C/qt/WKNativeEvent.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/cpp/WKRetainPtr.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/cpp/qt/WKStringQt.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/cpp/qt/WKURLQt.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/ClientImpl.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qgraphicswkview.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkcontext.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkcontext_p.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkhistory.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkhistory_p.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkpage.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkpage_p.h \
        $$SOURCE_DIR/WebKit2/UIProcess/API/qt/qwkpreferences.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleBackForwardList.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleBackForwardListItem.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleHitTestResult.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleNavigationAction.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleNodeHandle.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundleNodeHandlePrivate.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePage.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePageGroup.h \
        $$SOURCE_DIR/WebKit2/WebProcess/InjectedBundle/API/c/WKBundlePageOverlay.h
}

contains(DEFINES, ENABLE_NETSCAPE_PLUGIN_API=1) {
    unix:!symbian {
        maemo5 {
            HEADERS += $$PWD/WebCoreSupport/QtMaemoWebPopup.h
            SOURCES += $$PWD/WebCoreSupport/QtMaemoWebPopup.cpp
        }
    }
}

contains(DEFINES, ENABLE_VIDEO=1) {
    !contains(DEFINES, USE_GSTREAMER=1):contains(MOBILITY_CONFIG, multimedia) {
        HEADERS += \
            $$PWD/WebCoreSupport/FullScreenVideoQt.h \
            $$PWD/WebCoreSupport/FullScreenVideoWidget.h

        SOURCES += \
            $$PWD/WebCoreSupport/FullScreenVideoQt.cpp \
            $$PWD/WebCoreSupport/FullScreenVideoWidget.cpp
    }
}

contains(DEFINES, ENABLE_DEVICE_ORIENTATION=1) {
    HEADERS += \
        $$PWD/WebCoreSupport/DeviceMotionClientQt.h \
        $$PWD/WebCoreSupport/DeviceMotionProviderQt.h \
        $$PWD/WebCoreSupport/DeviceOrientationClientQt.h \
        $$PWD/WebCoreSupport/DeviceOrientationClientMockQt.h \
        $$PWD/WebCoreSupport/DeviceOrientationProviderQt.h

    SOURCES += \
        $$PWD/WebCoreSupport/DeviceMotionClientQt.cpp \
        $$PWD/WebCoreSupport/DeviceMotionProviderQt.cpp \
        $$PWD/WebCoreSupport/DeviceOrientationClientQt.cpp \
        $$PWD/WebCoreSupport/DeviceOrientationClientMockQt.cpp \
        $$PWD/WebCoreSupport/DeviceOrientationProviderQt.cpp
}

contains(DEFINES, ENABLE_GEOLOCATION=1) {
     HEADERS += \
        $$PWD/WebCoreSupport/GeolocationClientQt.h
     SOURCES += \
        $$PWD/WebCoreSupport/GeolocationClientQt.cpp
}

!symbian-abld:!symbian-sbsv2 {
    modfile.files = $$moduleFile
    modfile.path = $$[QMAKE_MKSPECS]/modules

    INSTALLS += modfile
} else {
    # INSTALLS is not implemented in qmake's mmp generators, copy headers manually

    inst_modfile.commands = $$QMAKE_COPY ${QMAKE_FILE_NAME} ${QMAKE_FILE_OUT}
    inst_modfile.input = moduleFile
    inst_modfile.output = $$[QMAKE_MKSPECS]/modules
    inst_modfile.CONFIG = no_clean

    QMAKE_EXTRA_COMPILERS += inst_modfile

    install.depends += compiler_inst_modfile_make_all
    QMAKE_EXTRA_TARGETS += install
}

!CONFIG(QTDIR_build) {
    exists($$OUTPUT_DIR/include/QtWebKit/classheaders.pri): include($$OUTPUT_DIR/include/QtWebKit/classheaders.pri)
    WEBKIT_INSTALL_HEADERS = $$WEBKIT_API_HEADERS $$WEBKIT_CLASS_HEADERS

    !symbian-abld:!symbian-sbsv2 {
        headers.files = $$WEBKIT_INSTALL_HEADERS

        !isEmpty(INSTALL_HEADERS): headers.path = $$INSTALL_HEADERS/QtWebKit
        else: headers.path = $$[QT_INSTALL_HEADERS]/QtWebKit

        !isEmpty(INSTALL_LIBS): target.path = $$INSTALL_LIBS
        else: target.path = $$[QT_INSTALL_LIBS]

        INSTALLS += target headers
    } else {
        # INSTALLS is not implemented in qmake's mmp generators, copy headers manually
        inst_headers.commands = $$QMAKE_COPY ${QMAKE_FILE_NAME} ${QMAKE_FILE_OUT}
        inst_headers.input = WEBKIT_INSTALL_HEADERS
        inst_headers.CONFIG = no_clean

        !isEmpty(INSTALL_HEADERS): inst_headers.output = $$INSTALL_HEADERS/QtWebKit/${QMAKE_FILE_BASE}${QMAKE_FILE_EXT}
        else: inst_headers.output = $$[QT_INSTALL_HEADERS]/QtWebKit/${QMAKE_FILE_BASE}${QMAKE_FILE_EXT}

        QMAKE_EXTRA_COMPILERS += inst_headers

        install.depends += compiler_inst_headers_make_all
    }

    unix {
        CONFIG += create_pc create_prl
        QMAKE_PKGCONFIG_LIBDIR = $$target.path
        QMAKE_PKGCONFIG_INCDIR = $$headers.path
        QMAKE_PKGCONFIG_DESTDIR = pkgconfig
        lib_replace.match = $$re_escape($$DESTDIR)
        lib_replace.replace = $$[QT_INSTALL_LIBS]
        QMAKE_PKGCONFIG_INSTALL_REPLACE += lib_replace
    }

    mac {
        !static:contains(QT_CONFIG, qt_framework):!CONFIG(webkit_no_framework) {
            !build_pass {
                message("Building QtWebKit as a framework, as that's how Qt was built. You can")
                message("override this by passing CONFIG+=webkit_no_framework to build-webkit.")

                CONFIG += build_all
            } else {
                debug_and_release:TARGET = $$qtLibraryTarget($$TARGET)
            }

            CONFIG += lib_bundle qt_no_framework_direct_includes qt_framework
            FRAMEWORK_HEADERS.version = Versions
            FRAMEWORK_HEADERS.files = $${headers.files}
            FRAMEWORK_HEADERS.path = Headers
            QMAKE_BUNDLE_DATA += FRAMEWORK_HEADERS
        }

        QMAKE_LFLAGS_SONAME = "$${QMAKE_LFLAGS_SONAME}$${DESTDIR}$${QMAKE_DIR_SEP}"
    }
}

symbian {
    shared {
        contains(CONFIG, def_files) {
            DEF_FILE=symbian
            # defFilePath is for Qt4.6 compatibility
            defFilePath=symbian
        } else {
            MMP_RULES += EXPORTUNFROZEN
        }
    }
}
