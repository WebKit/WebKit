kind: autotools

build-depends:
- freedesktop-sdk.bst:public-stacks/buildsystem-autotools.bst
depends:
- freedesktop-sdk.bst:components/libgudev.bst

sources:
- kind: git_tag
  url: github_com:libusb/libusb.git
  track: v1.0.23
  ref: v1.0.23-0-ge782eeb2514266f6738e242cdcb18e3ae1ed06fa
