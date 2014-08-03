## P1stream build environment

This repository contains the build environment for [P1stream].

P1stream itself, and any module targetting it, depends on this repository. It
contains the public headers for P1stream, as well as build environment settings
used by atom-shell, node.js, npm and node-gyp.

 [P1stream]: https://github.com/p1stream/p1stream

### For module authors

If you are writing a module for P1stream, add this module to your
`dependencies` in `package.json`:

    "dependencies": {
        "p1-build": "p1stream/p1-build"
    }

Tags match versions of P1stream; pin your dependency to a tag if you want to
depend on a specific version.

Then add the `include` directory to your `include_dirs` in `binding.gyp`:

    'include_dirs': [
        'node_modules/p1-build/include'
    ]

Finally, install `p1-build` globally:

    $ npm -g install p1stream/p1-build

And build your module with the usual commands, prefixed with `p1-build`. For
example, one of the following:

    $ p1-build node-gyp configure build
    $ p1-build npm install .

### License

[GPLv3](LICENSE)

© 2014 — Stéphan Kochen
