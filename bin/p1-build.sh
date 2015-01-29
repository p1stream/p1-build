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
$(cd "${BASEDIR}" && node -e 'require("p1-build").shell()')

# Swap home directories for npm and node-gyp.
export REALHOME="${HOME}"
export HOME="${REALHOME}/.p1stream/.node"

# Execute command.
exec $@
