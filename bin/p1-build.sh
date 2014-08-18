#!/bin/bash

# Setup environment.
BINDIR="$(dirname "${BASH_SOURCE}")"
$(cd "${BINDIR}" && node -e 'require("p1-build").shell()')

# Swap home directories for npm and node-gyp.
export REALHOME="${HOME}"
export HOME="${REALHOME}/.p1stream/.node"

# Execute command.
exec $@
