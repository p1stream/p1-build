## P1stream build environment

This repository contains the build environment for [P1stream].

It is used to build P1stream itself and all native modules targetting it. It
contains the public headers for P1stream, as well as build environment settings
used by io.js, npm and node-gyp.

 [P1stream]: https://github.com/p1stream/p1stream

### Usage

Install this tool as follows:

    $ npm -g install p1stream/p1-build

Add the following to your target in `binding.gyp`:

    'include_dirs': [
        '$(p1stream_include_dir)'
    ]

Now build as usual, but prefix all commands with `p1-build`.
Probably start with something like:

    $ p1-build npm install

For incremental builds, you can also do something like:

    $ p1-build node-gyp build

### License

[GPLv3](LICENSE)

© 2015 — Stéphan Kochen
