cmake_minimum_required(VERSION 3.13)
project(pquic-ns3)

set(CMAKE_CXX_STANDARD 14)

include_directories(/usr/local/include/ns3-dev)

add_library(quic-point-to-point-helper helper/quic-point-to-point-helper.cc)
add_library(quic-network-simulator-helper helper/quic-network-simulator-helper.cc)
add_library(quic-iperf-network-simulator-helper helper/quic-iperf-network-simulator-helper.cc)

add_executable(blackhole blackhole/blackhole.cc)
add_executable(droplist droplist/droplist.cc)
add_executable(ecn-burst ecn-burst/ecn-burst.cc)
add_executable(multipath-p2p multipath-p2p/multipath-p2p.cc)
add_executable(multipath-p2p-asym multipath-p2p-asym/multipath-p2p-asym.cc)
add_executable(multipath-p2p-asym-3p multipath-p2p-asym-3p/multipath-p2p-asym-3p.cc)
add_executable(multipath-p2p-faulty multipath-p2p-faulty/multipath-p2p-faulty.cc)
add_executable(pquic pquic/dce-pquic.cc)
add_executable(simple-ecn simple-ecn/simple-ecn.cc)
add_executable(simple-p2p simple-p2p/simple-p2p.cc)
add_executable(tcp-cross-traffic tcp-cross-traffic/tcp-cross-traffic.cc)
add_executable(udp-cross-traffic udp-cross-traffic/udp-cross-traffic.cc)
add_executable(cdn-multi-exit-single-dest cdn-multi-exit-single-dest/cdn-multi-exit-single-dest.cc)