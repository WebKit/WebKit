# libavif docker image

To build and use libavif (with avifenc/avifdec) in an Ubuntu Docker container with all dependencies,
change into this directory and run:

    docker-compose run libavif

This should install all necessary dependencies, including manually building and installing all
supported codecs as shared libraries, and finally building and installing
`libavif`/`avifenc`/`avifdec`. It will then drop you a shell running in the container.

If you just want to force a (re)build and skip the interactive shell, run:

    docker-compose up --build --force-recreate libavif

*Note: This build process will take a while (15-30m or more depending on your machine).*
