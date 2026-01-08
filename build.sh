#!/bin/sh

## build script for JSRF-like engine for available platforms. By quantumde1.

case "$1" in
    "dreamcast")
        ## lets try to source default kos toolchain placement
        source /opt/toolchains/dc/kos/environ.sh
        ## if working, go next, otherwise idk
        make -f makefiles/dreamcastMakefile
	mkdcdisc -e jsrf.elf -o JSRF.cdi -d assets
        ;;
    "desktop")
        ## going without anything, lmao
        gmake -f makefiles/desktopMakefile
        ;;
    "clean")
        make -f makefiles/desktopMakefile clean
        ;;
    *)
        echo "Unknown or empty second argument: $1
available commands is $0 {dreamcast,desktop}"
        ;;
esac
