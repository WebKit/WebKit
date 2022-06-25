kind: manual

(@): flatpak-version.yml

build-depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/m4.bst
- freedesktop-sdk.bst:components/appstream-glib.bst

config:
  build-commands:
  - |
    for i in *.in; do
      m4 \
        -D__SDK_BRANCH__=%{sdk-branch} \
        -D__RUNTIME_VERSION__=%{runtime-version} \
        -D__RUNTIME_VERSION_DATE__=%{runtime-version-date} \
        "${i}" >"$(basename "${i}" .in)"
    done

  install-commands:
  - |
    mkdir -p "%{install-root}%{indep-libdir}"
    install os-release "%{install-root}%{indep-libdir}"
    mkdir -p "%{install-root}%{sysconfdir}"
    ln -s "$(realpath --relative-to="%{install-root}%{sysconfdir}" "%{install-root}%{indep-libdir}/os-release")" %{install-root}%{sysconfdir}/os-release

    mkdir -p "%{install-root}%{sysconfdir}"
    install issue "%{install-root}%{sysconfdir}"
    install issue.net "%{install-root}%{sysconfdir}"

    mkdir -p "%{install-root}%{datadir}/appdata"
    install org.webkit.Platform.appdata.xml "%{install-root}%{datadir}/appdata"
    install org.webkit.Sdk.appdata.xml "%{install-root}%{datadir}/appdata"

    appstream-compose --basename=org.webkit.Platform --prefix=%{install-root}%{prefix} --origin=flatpak org.webkit.Platform
    appstream-compose --basename=org.webkit.Sdk --prefix=%{install-root}%{prefix} --origin=flatpak org.webkit.Sdk

sources:
- kind: local
  path: files/os-release
