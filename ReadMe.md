# WebKit

WebKit is a cross-platform web browser engine. On iOS and macOS, it powers Safari, Mail, Apple Books, and many other applications. For more information about WebKit, see the [WebKit project website](https://webkit.org/).

---

## Table of Contents

- [Trying the Latest](#trying-the-latest)
- [Reporting Bugs](#reporting-bugs)
- [Getting the Code](#getting-the-code)
- [Building WebKit](#building-webkit)
  - [Apple Platforms](#building-for-apple-platforms)
  - [GTK Port](#building-the-gtk-port)
  - [WPE Port](#building-the-wpe-port)
  - [Windows Port](#building-windows-port)
- [Running WebKit](#running-webkit)
  - [macOS Applications](#with-safari-and-other-macos-applications)
  - [iOS Simulator](#ios-simulator)
  - [Linux Ports](#linux-ports)
- [Contribute](#contribute)

---

## Trying the Latest

| Platform | How to Test |
|----------|-------------|
| macOS | [Download Safari Technology Preview](https://webkit.org/downloads/) |
| Linux | [Download Epiphany Technology Preview](https://webkitgtk.org/epiphany-tech-preview) |
| Windows | Build from source (see [Building Windows Port](#building-windows-port)) |

---

## Reporting Bugs

1. 🔍 [Search WebKit Bugzilla](https://bugs.webkit.org/query.cgi?format=specific&product=WebKit) to check for existing reports
2. 📝 [Create a Bugzilla account](https://bugs.webkit.org/createaccount.cgi) if you don't have one
3. 🐛 File a bug following [our guidelines](https://webkit.org/bug-report-guidelines/)

Once filed, you will receive email updates at each stage of the [bug life cycle](https://webkit.org/bug-life-cycle). After the bug is considered fixed, you may be asked to download the [latest nightly](https://webkit.org/nightly) and confirm the fix works.

---

## Getting the Code

Clone WebKit's Git repository:

```bash
git clone https://github.com/WebKit/WebKit.git WebKit
