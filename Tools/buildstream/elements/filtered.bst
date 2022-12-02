kind: stack

(?):
- arch == "x86_64":
    depends:
    - sdk/rr.bst
    - sdk/clangd.bst

- arch == "aarch64":
    depends:
    - sdk/clangd-arduino.bst