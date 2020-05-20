kind: manual
sources:
- kind: remote
  url: ruby_gems:json-1.8.5.gem
  ref: 70f7e21bc66da1dbf925968f9d88be811bcc5ace0bc614c7e838241a0b8c9fb7
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
      ruby -e "require 'json'"
