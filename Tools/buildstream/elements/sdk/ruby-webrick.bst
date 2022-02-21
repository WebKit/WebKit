kind: manual
sources:
- kind: remote
  url: ruby_gems:webrick-1.7.0.gem
  ref: 87e9b8e39947b7925338a5eb55427b11ce1f2b25a3645770ec9f39d8ebdb8cb4
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
      ruby -e "require 'webrick/htmlutils'"
