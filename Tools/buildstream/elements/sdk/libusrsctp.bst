kind: meson
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-meson.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst

sources:
- kind: git_repo
  url: github_com:sctplab/usrsctp.git
  ref: 17983b032c3f0e33523972e534b18df832922800
variables:
  meson-local: >-
    -Dsctp_inet=false
    -Dsctp_inet6=false
