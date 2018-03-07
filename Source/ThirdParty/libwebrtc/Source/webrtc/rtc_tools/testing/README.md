This directory contains prebuilt tools used during end-to-end tests.
They will be downloaded by their SHA1 hash, and are not meant to be checked in.

Updating prebuilt_apprtc.zip:

- Follow AppRTC instructions:
    - `git clone https://github.com/webrtc/apprtc`
    - Install NodeJS:
        - Download <https://nodejs.org/> and extract it
        - `export PATH="$(pwd)/node-v6.10.3-linux-x64/bin:$PATH"`
    - `cd apprtc`
    - `npm install`
    - `export PATH="$(pwd)/node_modules/.bin:$PATH"`
    - `pip install --user --upgrade pip setuptools` - needed only on old systems
    - `grunt`
- Vendor collider's dependencies:
    - `ln -s "$(pwd)/src/collider" src/src`
    - `GOPATH="$(pwd)/src" go get -d collidermain`
    - `rm src/src`
- Remove unneeded files:
    - `rm -rf .git node_modules browsers`
- `zip -r prebuilt_apprtc.zip apprtc/`
- `mv prebuilt_apprtc.zip webrtc/src/rtc_tools/testing/prebuilt_apprtc.zip`

Updating golang/*:

- Go to <https://golang.org/dl/>
- Download these files:
    - go*.linux-amd64.tar.gz -> golang/linux/go.tar.gz
    - go*.darwin-amd64.tar.gz -> golang/mac/go.tar.gz
    - go*.windows-amd64.zip -> golang/windows/go.zip

After updating the archives:

- `cd webrtc/src/rtc_tools/testing`
- For each updated archive:
    - `upload_to_google_storage.py file.zip --bucket=chromium-webrtc-resources`
- `git commit -a && git cl upload`
