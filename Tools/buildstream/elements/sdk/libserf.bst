kind: manual
sources:
- kind: git_tag
  track: trunk
  url: github_com:apache/serf.git
  ref: 1cb2ab783fb7182a7d5db64d428be30b6d779a90
build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-cmake.bst
- sdk-build-depends/scons.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- test-infra/apr.bst
- test-infra/apr-util.bst
- freedesktop-sdk.bst:components/pkg-config.bst

config:
  build-commands:
    - |
      scons PREFIX=%{prefix} LIBDIR=/usr/lib/%{gcc_triplet}

  install-commands:
    - |
      scons install --install-sandbox=%{install-root}

public:
  bst:
    integration-commands:
    - |
      pkg-config --modversion serf-2
