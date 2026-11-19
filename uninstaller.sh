# SetupDB writes registry_bin, vendor_uninstall, product and manifest above.
if [ -z "${SETUPDB_ROOT:-}" ] && command -v "$vendor_uninstall" >/dev/null 2>&1 &&
   "$vendor_uninstall" -v >/dev/null 2>&1; then
    exec "$vendor_uninstall" -L "$product" "$manifest" "$@"
fi

os=$(uname -s)
[ "$os" != OpenUNIX ] || os=SCO_SV
case $(uname -m) in
    arm64 | aarch64) arch=aarch64 ;;
    amd64 | x86_64) arch=x86_64 ;;
    i?86 | i86*) arch=x86 ;;
    90*/*) arch=hppa ;;
    *)
        case $os in
            IRIX*) arch=mips ;;
            AIX*) arch=ppc ;;
            *)
                arch=$(uname -p 2>/dev/null || uname -m)
                [ "$arch" != powerpc ] || arch=ppc
                ;;
        esac
        ;;
esac

uninstaller=$registry_bin/$os/$arch/uninstall
if [ "$arch" = x86_64 ] && [ ! -x "$uninstaller" ]; then
    uninstaller=$registry_bin/$os/amd64/uninstall
    [ -x "$uninstaller" ] || uninstaller=$registry_bin/$os/x86/uninstall
fi

if [ ! -x "$uninstaller" ]; then
    echo 'Could not find a usable uninstall program. Aborting.' >&2
    exit 1
fi

exec "$uninstaller" -L "$product" "$manifest" "$@"
