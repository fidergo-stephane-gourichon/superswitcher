#!/bin/bash

TMPDIR=$( mktemp -d ) && echo "TMPDIR is $TMPDIR"

set -eu

PKGDIR="$PWD"

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)

echo "On branch $CURRENT_BRANCH"

if output=$(git status --porcelain) && [ -z "$output" ]; then
    echo "Working directory clean"
else 
    echo >&2 "Uncommitted changes. ABORT."
    git status
    echo >&2 "Uncommitted changes. ABORT."
    exit 1
fi


cd "$TMPDIR"
git clone "$PKGDIR"
tar zcvf superswitcher_0.9.orig.tar.gz superswitcher
cd superswitcher

dpkg-checkbuilddeps
debuild -us -uc

OUTDIR="$PKGDIR/../build_output_$( date +%Yy%mm%dd_%Hh%Mm%Ss )"

mkdir "$OUTDIR"

cd ..

cp -v *.* "$OUTDIR"

echo
echo ================================================================
echo "Artifacts available in $OUTDIR:"
echo ================================================================

cd "$OUTDIR"

ls -al
