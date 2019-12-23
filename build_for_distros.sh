#!/bin/bash

# Quick and dirty test on another distro

declare -a DEFAULT_DISTROS DISTROS

DEFAULT_DISTROS=( ubuntu:eoan ubuntu:bionic debian:buster ubuntu:xenial )

if [[ $# == 0 ]]
then
    DISTROS=( "${DEFAULT_DISTROS[@]}" )
    echo "No specific distro names provided, using default distros set."
else
    DISTROS=( "$@" )
fi

echo "Will build for distros :"
printf "%s\n" "${DISTROS[@]}"

echo "Starting..."

for DISTRO in "${DISTROS[@]}"
do
    echo $DISTRO
    docker run --rm -it -v $PWD:/up/superswitcher "$DISTRO" bash -c 'set -euxv ; apt-get update ; apt-get install -y --no-install-recommends debhelper gnome-common libwnck-dev git build-essential devscripts fakeroot ; cd /up/superswitcher ; bash recompile_local_debian_package.sh ; . /etc/lsb-release ; OUT=/up/superswitcher/build_artifacts_${DISTRIB_ID}_${DISTRIB_RELEASE}_${DISTRIB_CODENAME} ; mkdir -p "$OUT" ; cp -av /tmp/*/superswitcher* "$OUT"'
done

echo "...end"
