ARCH ?= $(shell flatpak --default-arch)
BOOTSTRAP_ARCH ?= $(shell flatpak --default-arch)

BST ?= bst
BST_ARGS ?=
_BST_ARGS ?= --no-interactive -o arch $(ARCH) -o bootstrap_build_arch $(BOOTSTRAP_ARCH)

REPO ?= repo
EXPORT_ARGS ?=

RSYNC_HOST ?= software.igalia.com
RSYNC_REMOTE_DIR ?= /var/www/software/webkit-sdk-repo
RSYNC_ARGS ?= -vz

flatpak-repo:
	test -e $(REPO) || ostree init --repo=$(REPO) --mode=archive
	./utils/generate-version flatpak-version.yml
	$(BST) $(BST_ARGS) $(_BST_ARGS) build flatpak-runtimes.bst
	$(BST) $(BST_ARGS) $(_BST_ARGS) build flatpak-platform-extensions.bst
	$(BST) $(BST_ARGS) $(_BST_ARGS) build flatpak-platform-extensions-extra.bst
	rm -rf checkout-repo/
	$(BST) $(BST_ARGS) $(_BST_ARGS) artifact checkout flatpak-runtimes.bst --directory checkout-repo/
	ostree pull-local --repo $(REPO) checkout-repo/
	rm -rf checkout-repo/
	$(BST) $(BST_ARGS) $(_BST_ARGS) artifact checkout flatpak-platform-extensions.bst --directory checkout-repo/
	ostree pull-local --repo $(REPO) checkout-repo/
	rm -rf checkout-repo/
	$(BST) $(BST_ARGS) $(_BST_ARGS) artifact checkout flatpak-platform-extensions-extra.bst --directory checkout-repo/
	ostree pull-local --repo $(REPO) checkout-repo/
	rm -rf checkout-repo/
ifneq ($(EXPORT_ARGS),)
	flatpak build-sign $(EXPORT_ARGS) $(REPO)
	flatpak build-update-repo --generate-static-deltas $(EXPORT_ARGS) $(REPO)
endif

pull-repo:
	./ostree-releng-scripts/rsync-repos --rsync-opts="${RSYNC_ARGS}" --src ${RSYNC_HOST}:${RSYNC_REMOTE_DIR}/ --dest ${REPO}

push-repo:
	./ostree-releng-scripts/rsync-repos --rsync-opts="${RSYNC_ARGS}" --src ${REPO}/ --dest ${RSYNC_HOST}:${RSYNC_REMOTE_DIR}

dry-push-repo:
	./ostree-releng-scripts/rsync-repos --rsync-opts="-n ${RSYNC_ARGS}" --src ${REPO}/ --dest ${RSYNC_HOST}:${RSYNC_REMOTE_DIR}
