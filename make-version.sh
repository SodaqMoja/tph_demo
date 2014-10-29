#!/bin/sh
#
# Create a header file with a version number to be displayed
# at startup.

VERSION=$(git describe --tags 2> /dev/null)
[ -z "${VERSION}" ] && { echo "ERROR: Cannot determine version. Did you do a git-tag?"; exit 1; }

echo "#define VERSION \"$VERSION\"" > version.h.tmp
[ ! -s version.h ] && { mv version.h.tmp version.h; exit 0; }
cmp -s version.h.tmp version.h || { mv version.h.tmp version.h; exit 0; }
rm -f version.h.tmp
exit 0
