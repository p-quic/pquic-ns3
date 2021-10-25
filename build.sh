#!/usr/bin/env bash

set -e
docker build -f Dockerfile-base.14.04 -t picoquic-ns3-dce-base .
docker build -t picoquic-ns3-dce .
