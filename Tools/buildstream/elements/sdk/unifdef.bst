kind: make
depends:
- freedesktop-sdk.bst:bootstrap-import.bst

variables:
  make-args: >-
    prefix="%{prefix}"

sources:
- kind: tar
  url: https://dotat.at/prog/unifdef/unifdef-2.12.tar.xz
  ref: 43ce0f02ecdcdc723b2475575563ddb192e988c886d368260bc0a63aee3ac400

public:
  bst:
    integration-commands:
    - unifdef -V
