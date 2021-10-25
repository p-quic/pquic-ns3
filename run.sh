#!/usr/bin/env bash

if [[ $# -lt 1 ]]; then
    echo "Usage: $0 pquic_dir [command]"
    exit -1
fi

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

if [[ $# -lt 2 ]]; then
    docker run -i -t -v $DIR/myscripts:/home/ns3dce/dce-linux-dev/source/dce-linux-dev/myscripts -v $DIR:/pquic-ns3-dce -v $1:/home/ns3dce/pquic pquic-ns3-dce
else
    docker run --rm -v $DIR/myscripts:/home/ns3dce/dce-linux-dev/source/dce-linux-dev/myscripts -v $DIR:/pquic-ns3-dce -v $1:/home/ns3dce/pquic pquic-ns3-dce $2
fi