#!/bin/bash

function _error {
    echo "ERROR: $@"
    exit 1
}

XZIMAGE=""

for IMAGE in ${WEBKIT_CROSS_BUILT_IMAGES}; do
    if [[ ${IMAGE} == *.tar.xz ]]; then
        XZIMAGE=${IMAGE}
    fi
done

# Check config passed from cross-toolchain-helper
test -n "${XZIMAGE}" || _error "Can't find a image ending with .tar.xz in the env var WEBKIT_CROSS_BUILT_IMAGES"
test -f "${XZIMAGE}" || _error "Unable to access the file ${XZIMAGE} from the env var WEBKIT_CROSS_BUILT_IMAGES"
# Check user-defined config
test -n "${NFSIPSERVER}" || _error "Need to export variable NFSIPSERVER=ipaddr.of.nfs.server"
test -n "${NFSDEPLOYDIR}" || _error "Need to export variable NFSDEPLOYDIR=/path/to/where/to/deploy"

NFSIPSUBNET="$(echo ${NFSIPSERVER}|rev|cut -d\. -f2-|rev)"

# main
set -eux -o pipefail

test -d "${NFSDEPLOYDIR}" && sudo rm -fr "${NFSDEPLOYDIR}"
sudo mkdir -p "${NFSDEPLOYDIR}"
# This will unpack the tarball and preserve all the UIDs and extra permissions
sudo tar xfap "${XZIMAGE}" -C "${NFSDEPLOYDIR}" --numeric-owner

# set configs for boot
echo "dwc_otg.lpm_enable=0 video=HDMI-A-1:1920x1080@60D root=/dev/nfs nfsroot=${NFSIPSERVER}:${NFSDEPLOYDIR},vers=4 rw ip=dhcp rootwait" | sudo tee "${NFSDEPLOYDIR}/boot/cmdline.txt"
# reading /boot/cmdline.txt via TFTP is broken, remove comments as only  few lines at the top work
# This config forces a 1080p monitor to be always plugged even when there is not, is useful for testing.
sudo tee "${NFSDEPLOYDIR}/boot/config.txt" << EOF
dtoverlay=vc4-kms-v3d
hdmi_group:0=2
hdmi_mode:0=82
hdmi_force_hotplug=1
hdmi_force_hotplug:0=1
framebuffer_width=1920
framebuffer_height=1080
EOF


# For using the hostname set via DHCP we need to delete /etc/hostname. See https://systemd.network/hostname.html
sudo rm -f "${NFSDEPLOYDIR}/etc/hostname"

set +x
echo "Deployed directory for NFS booting at ${NFSDEPLOYDIR}"
echo "Please ensure this config line is at file /etc/exports :"
echo "  ${NFSDEPLOYDIR} *(rw,async,no_subtree_check,no_root_squash)"
echo "Then reload the NFS server with command: sudo exportfs -ra"
echo "Then configure the DHCP and TFTP server. Example: run the following command:"
echo "  sudo dnsmasq -d -i eth0 -z  -k  -a ${NFSIPSERVER} -p 786 --log-dhcp --log-queries --log-facility=/dev/stdout --dhcp-range=${NFSIPSUBNET}.32,${NFSIPSUBNET}.48,12h --pxe-service=0,\"Raspberry Pi Boot\" --tftp-root=${NFSDEPLOYDIR}/boot --enable-tftp"
echo "Note: replace eth0 above with the name of your LAN interface"
