kind: manual
sources:
- kind: git_tag
  ref: 3.0.4
  url: github_com:SCons/scons.git

build-depends:
- freedesktop-sdk.bst:components/python3.bst

depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/python3.bst

config:
  build-commands:
    - |
      python3 bootstrap.py build/scons

  install-commands:
    - |
      cd build/scons
      python3 setup.py install --prefix "%{prefix}" --root "%{install-root}"
      sed -i 's,env python,env python3,' %{install-root}/usr/bin/scons

public:
  bst:
    integration-commands:
    - |
      scons --help > /dev/null
