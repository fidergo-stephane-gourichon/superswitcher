#!/bin/bash
set -eu

declare -a DEFAULT_DISTROS DISTROS

DEFAULT_DISTROS=(debian:stable debian:testing debian:oldstable debian:oldoldstable)

# Defaults
ENGINE="docker"
ENGINE_RUN_EXTRA_ARGS="-u $(id -u):$(id -g)"

while [[ "$#" -gt 0 ]]
do
    case "$1" in
        --engine=docker)
            ENGINE="docker"
            ENGINE_RUN_EXTRA_ARGS="-u $(id -u):$(id -g)"
            ;;

        --engine=podman)
            ENGINE="podman"
            ENGINE_RUN_EXTRA_ARGS=""
            ;;

        --distros=*)
	    DISTROS_STRING=${1#--distros=}
	    DISTROS_STRING=${DISTROS_STRING#\"}
	    DISTROS_STRING=${DISTROS_STRING%\"}
	    read -ra DISTROS <<< "$DISTROS_STRING"
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

ARGS_FOR_RECOMPILE_SCRIPT="$@"

if [[ -n "${ERRORS:-}" ]]
then
    echo >&2 "Error. Usage scriptname [--engine=docker] [--engine=podman] -- [arguments for recompile_local_debian_package.sh ...]"
    echo >&2 "Default set of distros: ${DEFAULT_DISTROS[@]}"
    exit 1
fi

# Handle distros
if [[ -z "${DISTROS:-}" ]]; then
    DISTROS=( "${DEFAULT_DISTROS[@]}" )
    echo "No specific distro names provided, using default distros set."
fi

echo "Using engine: $ENGINE"
echo "ENGINE_RUN_EXTRA_ARGS: $ENGINE_RUN_EXTRA_ARGS"

echo "Will build for distros:"
echo
printf "%s\n" "${DISTROS[@]}"
echo

echo "Starting..."

for DISTRO in "${DISTROS[@]}"
do
    echo "Start for distro ${DISTRO}..."

    DISTRO_IMAGE="${DISTRO}-slim"
    if ! ${ENGINE} manifest inspect "${DISTRO_IMAGE}" > /dev/null 2>&1
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
    ${ENGINE} build -t "$IMG" - <<EOF
FROM $DISTRO_IMAGE
RUN apt-get update && \
    apt-get install -y --no-install-recommends debhelper gnome-common git build-essential devscripts fakeroot lsb-release
RUN set -euxv ; for X in libwnck-dev libwnck-3-dev ; do apt-get install -y --no-install-recommends "\$X" && exit 0 ; done ; echo >&2 "Could not install dev package for libwnck." ; exit 1
EOF

    echo "Running build for $DISTRO with log to $LOGFILE"
    ${ENGINE} run ${ENGINE_RUN_EXTRA_ARGS} --rm \
    -v "$PWD":/up/superswitcher:ro \
    -v "$(realpath "$ARTIFACT_BASE")":/up/artifacts \
    "$IMG" bash -euxc "
cd /up/superswitcher
export HOME=/tmp
git config --global --add safe.directory /up/superswitcher
git config --global --add safe.directory /up/superswitcher/.git

if [ -f /etc/lsb-release ]; then
    . /etc/lsb-release
elif [ -f /etc/os-release ]; then
    . /etc/os-release
    # Map os-release fields to expected variable names for compatibility
    DISTRIB_ID=\"\${ID:-unknown}\"
    DISTRIB_RELEASE=\"\${VERSION_ID:-unknown}\"
    DISTRIB_CODENAME=\"\${VERSION_CODENAME:-unknown}\"
else
    DISTRIB_ID=\"unknown\"
    DISTRIB_RELEASE=\"unknown\"
    DISTRIB_CODENAME=\"unknown\"
fi

OUT=/up/artifacts/build_artifacts_\${DISTRIB_ID}_\${DISTRIB_RELEASE}_\${DISTRIB_CODENAME}

mkdir -p \"\$OUT\"
bash -xv recompile_local_debian_package.sh $ARGS_FOR_RECOMPILE_SCRIPT -- \"\$OUT\"
" 2>&1 | tee "$LOGFILE"

echo "...end for distro $DISTRO"

done

echo "...end"
