#!/usr/bin/env bash

set -e

if [[ ! -f /.dockerenv ]]; then
    echo "This script is meant to be run inside the docker container";
    exit -1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd $DCE_PATH
rm -rf CMakeCache.txt CMakeFiles
sed -i 's/#define PICOQUIC_FIRST_RESPONSE_MAX (1 << 25)/#define PICOQUIC_FIRST_RESPONSE_MAX (1 << 28)/g' picoquicfirst/picoquicdemo.c
NS3=1 DISABLE_DEBUG_PRINTF=1 DISABLE_QLOG=1 cmake .
make -j$(nproc) picoquicdemomp picolog_t
cd $DIR

cd $NS3_PATH
mkdir -p files-0/dev files-1/dev
cp -rv /pquic-ns3-dce/certs files-1/
ln -s -f /dev/null files-0/dev/null
ln -s -f /dev/null files-1/dev/null

cd $DIR
