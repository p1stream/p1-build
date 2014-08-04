#!/bin/bash

# Setup environment.
$(node -e 'require("p1-build").shell()')

# Swap home directories for npm and node-gyp.
export REALHOME="${HOME}"
export HOME="${REALHOME}/.p1stream/.node"

# Execute command.
exec $@
