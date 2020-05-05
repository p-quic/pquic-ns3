#!/usr/bin/env bash

set -e

if [[ ! -f /.dockerenv ]]; then
    echo "This script is meant to be run inside the docker container";
    exit -1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

cd $DCE_PATH
cd ubpf/vm
make clean
NS3=1 make -j$(nproc)
cd $DCE_PATH
if [[ -d picoquic/michelfralloc ]]; then
    cd picoquic/michelfralloc
    if [[ ! -d ptmalloc3 ]]; then
        wget http://www.malloc.de/malloc/ptmalloc3-current.tar.gz
        tar xf ptmalloc3-current.tar.gz
        patch -p0 < ptmalloc.patch
    fi
    make clean
    make -j$(nproc)
fi
cd $DCE_PATH
rm -rf CMakeCache.txt CMakeFiles
sed -i 's/#define PICOQUIC_FIRST_RESPONSE_MAX (1 << 25)/#define PICOQUIC_FIRST_RESPONSE_MAX (1 << 28)/g' picoquicfirst/picoquicdemo.c
NS3=1 DISABLE_DEBUG_PRINTF=1 DISABLE_QLOG=1 cmake .
make -j$(nproc) picoquicdemo picoquiciperf
cd plugins
make -j$(nproc)
cd $DIR

cd $NS3_PATH
mkdir -p files-0/dev files-1/dev
cp -rv /pquic-ns3-dce/certs files-1/
cp -rv $DCE_PATH/plugins files-0
cp -rv $DCE_PATH/plugins files-1
ln -s -f /dev/null files-0/dev/null
ln -s -f /dev/null files-1/dev/null

cd $DIR