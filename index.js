var path = require('path');
var util = require('util');

exports.includeDir = path.join(__dirname, 'include');

exports.platform = process.platform;
exports.arch = 'x64';

exports.nodeVersion = '0.11.13';
exports.atomShellVersion = '0.15.9';

exports.atomShellPackageUrl = util.format(
    'https://github.com/atom/atom-shell/releases/download/v%s/atom-shell-v%s-%s-%s.zip',
    exports.atomShellVersion, exports.atomShellVersion, exports.platform, exports.arch);

exports.atomDistUrl = 'https://gh-contractor-zcbenz.s3.amazonaws.com/atom-shell/dist';

exports.env = {
    p1stream_include_dir: exports.includeDir,
    node_platform: exports.platform,
    node_arch: exports.arch,
    node_version: exports.version,
    atom_shell_version: exports.atomShellVersion,
    atom_shell_package_url: exports.atomShellPackageUrl,
    atom_dist_url: exports.atomDistUrl,
    npm_config_dist_url: exports.atomDistUrl,
    npm_config_target: exports.nodeVersion,
    npm_config_arch: exports.arch
};

exports.shell = function() {
    var env = exports.env;
    Object.keys(env).forEach(function(k) {
        console.log('export %s=%s', k, env[k]);
    });
};
