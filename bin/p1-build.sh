#!/bin/bash

set -e

# Figure out if we're a global or local install.
BINDIR="$(dirname "${BASH_SOURCE}")"
if [ -d "${BINDIR}/../.bin" ]; then
    BASEDIR="${BINDIR}/../.."
else
    BASEDIR="${BINDIR}/../lib"
fi

# Setup environment.
export p1stream_include_dir="$(cd "${BASEDIR}" && iojs -pe 'require("p1-build").includeDir')"

# Execute command.
exec $@
