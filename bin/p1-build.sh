#!/bin/bash

# Swap home directories for npm and node-gyp.
export REALHOME="${HOME}"
export HOME="${REALHOME}/.p1stream/.node"

# Current atom-shell version.
export atom_shell_version="0.15.2"

# Current atom-shell node-gyp settings.
export npm_config_dist_url="https://gh-contractor-zcbenz.s3.amazonaws.com/atom-shell/dist"
export npm_config_target="0.11.13"
export npm_config_arch="x64"

# Execute command.
exec $@
