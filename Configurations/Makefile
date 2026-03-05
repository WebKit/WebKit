
# The major *host* versions of macOS that WebKit supports at-desk development
# on. Update this list when new major versions are released, to support
# building downlevels targeting those OS versions. Remove from this list when
# WebKit drops downlevel support for an OS version.
SUPPORTED_MAC_OS_VERSIONS = 14 15 26

DeploymentTargets.xcconfig: Scripts/generate-deployment-target-xcconfig.py Makefile
	./$< $(SUPPORTED_MAC_OS_VERSIONS) > $@
