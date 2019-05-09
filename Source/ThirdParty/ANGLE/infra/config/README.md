## Configurations files for chrome-infra services.

There are two kinds of configs: **global** and **ref-specific**.


## Global configs.

Apply to the whole repo regardless of the ref (branch).

Located in [global](global) directory.

Currently active version can be checked at
https://luci-config.appspot.com/#/projects/angle .


## Ref-specific configs.

Apply only to a ref(branch) they are located in.

Located in [branch](branch) directory.

Currently active version can be checked at

    https://luci-config.appspot.com/#/projects/angle/<ref>
    # For example, for master branch:
    #   https://luci-config.appspot.com/#/projects/angle/refs/heads/master

