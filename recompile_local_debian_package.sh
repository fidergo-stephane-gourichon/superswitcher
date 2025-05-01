#!/bin/bash

cat <<EOF
========================================================================
One-step Debian package generation script.
========================================================================

This script takes a git-version-controlled tree hierarchy containing
a proper debian/ directory for package generation, does a temporary
clone and builds debian packages from it.

Added value :
* automatically generated a proper *.orig.tar.gz as required by debuild.
* make sure the build it not polluted by any local non-commited files
* keep the original tree hierarchy clean

This script should be generic enough to be used in other programe.  It
might be confused by stray version names with strange or worse, evil,
characters, but if you name your package "little bobby tables" you
deserve to do all this by hand.

Written by Stéphane Gourichon <stephane_dpkg@gourichon.org>

========================================================================

Let's go!


EOF

set -euo pipefail

while [[ "$#" -gt 0 ]]
do
        case "$1" in
        --allow-unreleased)
                ALLOW_UNRELEASED="true"
                ;;

        --allow-uncommitted)
                ALLOW_UNCOMMITTED="true"
                ;;

        --)
                shift
                break
                ;;

        *)
                echo >&2 "Unknown argument: $1"
                ERRORS=$(( ${ERRORS:-0} +1 ))
                ;;

        esac
        shift
done

if [[ -n "${ERRORS:-}" ]]
then
    echo "Error. Usage scriptname [--allow-unreleased] [--allow-uncommitted]"
    exit 1
fi


cd "$(dirname "$(readlink -f "$0")" )"

dpkg-checkbuilddeps
echo -e "* dpkg-checkbuilddeps\tPASSED"

PKGDIR="$PWD"


# CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD) && echo "* Current git branch $CURRENT_BRANCH"

DEBIAN_PACKAGE_NAME="$( head -n 1 debian/changelog | sed -n 's/^\([^ ]*\)* (\([^-]*\)-[0-9]*).*$/\1/p' )"
DEBIAN_PACKAGE_VERSION_WITH_DEBIAN="$( head -n 1 debian/changelog | sed -n 's/^\([^ ]*\)* (\([^-]*-[0-9]*\)).*$/\2/p' )"
DEBIAN_PACKAGE_VERSION_BARE="$( head -n 1 debian/changelog | sed -n 's/^\([^ ]*\)* (\([^-]*\)-[0-9]*).*$/\2/p' )"

if [[ -z "${DEBIAN_PACKAGE_NAME}" || -z "${DEBIAN_PACKAGE_VERSION_BARE}" ]]
then
    echo >&2 "Cannot figure name (${DEBIAN_PACKAGE_NAME:-empty}) or package version (${DEBIAN_PACKAGE_VERSION_BARE:-empty}) from first line of debian/changelog. Aborting."
    head -n 1 debian/changelog
    exit 1
fi

GIT_TAG="$( git describe --tags --dirty )"

if output=$(git status --porcelain) && [ -z "$output" ]; then
    echo "Working directory clean"
else
    if [[ "${ALLOW_UNCOMMITTED:-}" != "true" ]]
    then
	echo >&2 "ERROR: uncommitted changes."
	echo >&2 "Use the --allow-uncommitted command-line argument to allow building from a dirty working directory. This is only useful to test the build script, because what is built is still a git checkout of the current hash."
	exit 1
    fi
    IS_CUSTOM_TAG=true
fi

if [[ "$GIT_TAG" != "${DEBIAN_PACKAGE_VERSION_BARE}" ]]
then
    echo >&2 "Git version ${GIT_TAG} is not exactly a tag matching Debian version ${DEBIAN_PACKAGE_VERSION_BARE}."
    if [[ "${ALLOW_UNRELEASED:-}" != "true" ]]
    then
	echo >&2 "Use the --allow-unreleased command-line argument to allow building an unreleased package."
	exit 1
    fi
    IS_CUSTOM_TAG=true
fi

if [[ "${IS_CUSTOM_TAG:-}" == true ]]
then
    DEBIAN_PACKAGE_VERSION_BARE=${DEBIAN_PACKAGE_VERSION_BARE}-${GIT_TAG}
    DEBIAN_PACKAGE_VERSION_WITH_DEBIAN=${DEBIAN_PACKAGE_VERSION_BARE}-1
fi


TMPDIR=$( mktemp -d ) && echo "* Will work in temp dir $TMPDIR"
cd "$TMPDIR"

NAMEFORTAR="${DEBIAN_PACKAGE_NAME}_${DEBIAN_PACKAGE_VERSION_BARE}"
DIRNAMEFORDEB="${DEBIAN_PACKAGE_NAME}-${DEBIAN_PACKAGE_VERSION_BARE}"
git clone "$PKGDIR" "${DIRNAMEFORDEB}"

sed -E "1s/\([^)]+\)/(${DEBIAN_PACKAGE_VERSION_WITH_DEBIAN})/" -i "${DIRNAMEFORDEB}"/debian/changelog

tar zcf ${NAMEFORTAR}.orig.tar.gz "${DIRNAMEFORDEB}"
cd "${DIRNAMEFORDEB}"

dpkg-checkbuilddeps
debuild -us -uc

if [[ -n "${1:-}" ]]
then
    OUTDIR="${1}"
else
    OUTDIR="$PKGDIR/../build_output_$( date +%Yy%mm%dd_%Hh%Mm%Ss )"
fi

mkdir -p "$OUTDIR"

cd ..

cp -v *.* "$OUTDIR"

echo
echo ================================================================
echo "Artifacts available in $OUTDIR:"
echo ================================================================

cd "$OUTDIR"

ls -al
