#!/bin/bash
set -eu

declare -a DEFAULT_DISTROS DISTROS

DEFAULT_DISTROS=(debian:stable debian:testing debian:oldstable debian:oldoldstable)

if [[ $# == 0 ]]
then
    DISTROS=( "${DEFAULT_DISTROS[@]}" )
    echo "No specific distro names provided, using default distros set."
else
    DISTROS=( "$@" )
fi

echo "Will build for distros:"
printf "%s\n" "${DISTROS[@]}"

echo "Starting..."

for DISTRO in "${DISTROS[@]}"
do
    echo "Start for distro ${DISTRO}..."

    DISTRO_IMAGE="${DISTRO}-slim"
    if ! docker manifest inspect "${DISTRO_IMAGE}" > /dev/null 2>&1
    then
	DISTRO_IMAGE="${DISTRO}"
    fi

    echo "Using image $DISTRO_IMAGE"

    IMG="superswitcher-build:${DISTRO//:/_}"
    LOGFILE="../build_log_${DISTRO//:/_}.txt"
    ARTIFACT_BASE="../build_artifacts_${DISTRO//:/_}"

    # Ensure a clean artifacts directory for this build
    rm -rf "$ARTIFACT_BASE"
    mkdir -p "$ARTIFACT_BASE"

    echo "Building image ${IMG} for $DISTRO (for caching)..."
    docker build -t "$IMG" - <<EOF
FROM $DISTRO_IMAGE
RUN apt-get update && \
    apt-get install -y --no-install-recommends debhelper gnome-common git build-essential devscripts fakeroot lsb-release
RUN set -euxv ; for X in libwnck-dev libwnck-3-dev ; do apt-get install -y --no-install-recommends "\$X" && exit 0 ; done ; echo >&2 "Could not install dev package for libwnck." ; exit 1
EOF

    echo "Running build for $DISTRO with log to $LOGFILE"
    docker run -u $(id -u):$(id -g) --rm \
    -v "$PWD":/up/superswitcher:ro \
    -v "$(realpath "$ARTIFACT_BASE")":/up/artifacts \
    "$IMG" bash -euxc '
cd /up/superswitcher
export HOME=/tmp
git config --global --add safe.directory /up/superswitcher
git config --global --add safe.directory /up/superswitcher/.git

if [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
elif [ -f /etc/os-release ]; then
    . /etc/os-release
    # Map os-release fields to expected variable names for compatibility
    DISTRIB_ID="${ID:-unknown}"
    DISTRIB_RELEASE="${VERSION_ID:-unknown}"
    DISTRIB_CODENAME="${VERSION_CODENAME:-unknown}"
else
    DISTRIB_ID="unknown"
    DISTRIB_RELEASE="unknown"
    DISTRIB_CODENAME="unknown"
fi

OUT=/up/artifacts/build_artifacts_${DISTRIB_ID}_${DISTRIB_RELEASE}_${DISTRIB_CODENAME}

mkdir -p "$OUT"
bash -xv recompile_local_debian_package.sh "$OUT"
' 2>&1 | tee "$LOGFILE"

echo "...end for distro $DISTRO"

done

echo "...end"
