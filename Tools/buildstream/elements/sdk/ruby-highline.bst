kind: manual
sources:
- kind: remote
  url: ruby_gems:highline-2.0.3.gem
  ref: 2ddd5c127d4692721486f91737307236fe005352d12a4202e26c48614f719479
build-depends:
- freedesktop-sdk.bst:components/ruby.bst
depends:
- freedesktop-sdk.bst:bootstrap-import.bst
- freedesktop-sdk.bst:components/ruby.bst

config:
  install-commands:
    (>):
    - |
      gem install --local --ignore-dependencies --no-user-install --no-document --install-dir "$(ruby -e'puts Gem.default_dir')" --bindir %{install-root}%{prefix}/bin --build-root '%{install-root}' *.gem

public:
  bst:
    integration-commands:
    - |
      ruby -e "require 'highline'"
