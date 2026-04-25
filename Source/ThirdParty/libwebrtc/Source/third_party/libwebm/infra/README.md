# Infra Build Scripts

This directory contains scripts to build libwebm in various configurations.
These scripts were created to support Jenkins integration pipelines but these
scripts can also be run locally.

## Environment

Most of these scripts were ported from Jenkins, so in order to be run locally
some environment variables must be set prior to invocation.

**WORKSPACE** Traditionally, the Jenkins `WORKSPACE` path. If not defined, a
temporary directory will be used.

## LUCI Integration

[Builder Dashboard](https://ci.chromium/p/open-codecs) \
The new builders run these scripts on each CL. The current configuration
supports the `refs/head/main` branch.

## Scripts

**compile.sh** Builds libwebm with supported configuration and host toolchains.
