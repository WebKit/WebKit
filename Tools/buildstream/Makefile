ARCH ?= $(shell flatpak --default-arch)
BOOTSTRAP_ARCH ?= $(shell flatpak --default-arch)

GIT ?= git

BST ?= bst
BST_ARGS ?=
_BST_ARGS ?= --no-interactive -o arch $(ARCH) -o bootstrap_build_arch $(BOOTSTRAP_ARCH)

OSTREE ?= ostree

CACHEDIR ?= cache

REPO ?= repo
GPG_KEY = 9A0495AF96828F9D5E032C46A9A60744BCE3F878

# EXPORT_ARGS ?= --gpg-sign=${GPG_KEY} --gpg-homedir=gpg
EXPORT_ARGS ?=

RSYNC_HOST ?= software.igalia.com
RSYNC_REMOTE_DIR ?= /var/www/software/webkit-sdk-repo
RSYNC_ARGS ?=

FLATPAK_RUNTIMES_REPO = $(CACHEDIR)/flatpak-runtimes-repo
FLATPAK_PLATFORM_EXTENSIONS_REPO = $(CACHEDIR)/flatpak-platform-extensions-repo
EXPORT_REPO = $(REPO)

EXPORT_REFS = $(shell [ -d "$(EXPORT_REPO)" ] && $(OSTREE) refs --repo $(EXPORT_REPO))

all: export

clean:
	if [ -d "$(CACHEDIR)" ]; then rmdir $(CACHEDIR); fi
	if [ -d "$(EXPORT_REPO)" ]; then rm -r $(EXPORT_REPO); fi
.PHONY: clean

export: | $(EXPORT_REPO)
ifneq ($(EXPORT_ARGS),)
	flatpak build-sign $(EXPORT_ARGS) $|
endif
	flatpak build-update-repo --generate-static-deltas $(EXPORT_ARGS) $|
.PHONY: export


fetch-junctions:
	$(BST) $(BST_ARGS) $(_BST_ARGS) fetch freedesktop-sdk.bst
.PHONY: fetch-junctions

track:
	$(BST) $(BST_ARGS) $(_BST_ARGS) track freedesktop-sdk.bst
	$(BST) $(BST_ARGS) $(_BST_ARGS) track flatpak-runtimes.bst flatpak-platform-extensions.bst --deps=all
.PHONY: track


$(CACHEDIR):
	mkdir -p $@

$(EXPORT_REPO):
	$(OSTREE) init --repo=$@ --mode=archive


.git/hooks/pre-commit: utils/git-pre-commit
	ln -frs $< .git/hooks/pre-commit


flatpak-version.yml: $(GIT_HOOKS) | CLEAN-flatpak-version.yml
	./utils/generate-version $@
.PHONY: flatpak-version.yml

CLEAN-flatpak-version.yml:
	rm -f flatpak-version.yml
.PHONY: CLEAN-flatpak-version.yml
clean: CLEAN-flatpak-version.yml


BUILD-flatpak-runtimes: flatpak-version.yml elements/**/*.bst
	$(BST) $(BST_ARGS) $(_BST_ARGS) build flatpak-runtimes.bst
.PHONY: BUILD-flatpak-runtimes

CHECK-flatpak-runtimes: flatpak-version.yml | fetch-junctions
	$(BST) $(BST_ARGS) $(_BST_ARGS) show flatpak-runtimes.bst
.PHONY: CHECK-flatpak-runtimes
check: CHECK-flatpak-runtimes

$(FLATPAK_RUNTIMES_REPO): BUILD-flatpak-runtimes | $(CACHEDIR)
	$(BST) $(BST_ARGS) $(_BST_ARGS) checkout --hardlinks --force flatpak-runtimes.bst $@

EXPORT-$(FLATPAK_RUNTIMES_REPO): $(FLATPAK_RUNTIMES_REPO) | $(EXPORT_REPO)
	$(OSTREE) pull-local --repo=$| $<
.PHONY: EXPORT-$(FLATPAK_RUNTIMES_REPO)
export: EXPORT-$(FLATPAK_RUNTIMES_REPO)

CLEAN-$(FLATPAK_RUNTIMES_REPO):
	rm -rf $(FLATPAK_RUNTIMES_REPO)
.PHONY: CLEAN-$(FLATPAK_RUNTIMES_REPO)
clean: CLEAN-$(FLATPAK_RUNTIMES_REPO)


BUILD-flatpak-platform-extensions: elements/**/*.bst
	$(BST) $(BST_ARGS) $(_BST_ARGS) build flatpak-platform-extensions.bst
.PHONY: BUILD-flatpak-platform-extensions

CHECK-flatpak-platform-extensions: | fetch-junctions
	$(BST) $(BST_ARGS) $(_BST_ARGS) show flatpak-platform-extensions.bst
.PHONY: CHECK-flatpak-platform-extensions
check: CHECK-flatpak-platform-extensions

$(FLATPAK_PLATFORM_EXTENSIONS_REPO): BUILD-flatpak-platform-extensions | $(CACHEDIR)
	$(BST) $(BST_ARGS) $(_BST_ARGS) checkout --hardlinks --force flatpak-platform-extensions.bst $@

EXPORT-$(FLATPAK_PLATFORM_EXTENSIONS_REPO): $(FLATPAK_PLATFORM_EXTENSIONS_REPO) | $(EXPORT_REPO)
	$(OSTREE) pull-local --repo=$| $<
.PHONY: EXPORT-$(FLATPAK_PLATFORM_EXTENSIONS_REPO)
export: EXPORT-$(FLATPAK_PLATFORM_EXTENSIONS_REPO)

CLEAN-$(FLATPAK_PLATFORM_EXTENSIONS_REPO):
	rm -rf $(FLATPAK_PLATFORM_EXTENSIONS_REPO)
.PHONY: CLEAN-$(FLATPAK_PLATFORM_EXTENSIONS_REPO)
clean: CLEAN-$(FLATPAK_PLATFORM_EXTENSIONS_REPO)

pull-repo:
	./ostree-releng-scripts/rsync-repos --rsync-opts="-vz ${RSYNC_ARGS}" --src ${RSYNC_HOST}:${RSYNC_REMOTE_DIR}/ --dest ${REPO}

push-repo:
	./ostree-releng-scripts/rsync-repos --rsync-opts="-vz ${RSYNC_ARGS}" --src ${REPO}/ --dest ${RSYNC_HOST}:${RSYNC_REMOTE_DIR}

dry-push-repo:
	./ostree-releng-scripts/rsync-repos --rsync-opts="-vzn ${RSYNC_ARGS}" --src ${REPO}/ --dest ${RSYNC_HOST}:${RSYNC_REMOTE_DIR}
