FROM pquic-ns3-dce-base

# PQUIC dependencies
USER root
COPY clang.list /etc/apt/sources.list.d/clang.list

RUN wget -O - https://apt.llvm.org/llvm-snapshot.gpg.key | apt-key add -

RUN apt-get update && \
    apt-get install --no-install-recommends -y pkg-config cmake llvm-6.0 lld-6.0 clang-6.0 gdb strace python3 python3-yaml && \
    rm -rf /var/lib/apt/lists/*

RUN update-alternatives --install /usr/bin/cc cc /usr/bin/clang-6.0 100 && \
    update-alternatives --set cc /usr/bin/clang-6.0 && \
    update-alternatives --install /usr/bin/c++ c++ /usr/bin/clang++-6.0 100 && \
    update-alternatives --set c++ /usr/bin/clang++-6.0 && \
    ln -s /usr/bin/clang-6.0 /usr/bin/clang && \
    ln -s /usr/bin/llc-6.0 /usr/bin/llc

USER ns3dce

# NS3 DCE patch
WORKDIR /home/ns3dce/dce-linux-dev/source/dce-linux-dev
COPY ns3-dce.patch .
RUN git apply < ns3-dce.patch && \
    ./waf

# OpenSSL
WORKDIR /home/ns3dce
RUN wget --no-check-certificate https://www.openssl.org/source/openssl-1.1.1d.tar.gz && \
    tar xf openssl-1.1.1d.tar.gz
WORKDIR openssl-1.1.1d
RUN ./config && \
    make -j$(nproc) && \
    make test && \
    sudo make install
ENV LD_LIBRARY_PATH /usr/local/lib

# LibArchive
WORKDIR /home/ns3dce
RUN wget https://www.libarchive.org/downloads/libarchive-3.4.0.tar.gz && \
    tar xf libarchive-3.4.0.tar.gz
WORKDIR libarchive-3.4.0
RUN ./configure --disable-bsdtar --disable-bsdcpio --without-openssl && \
    make -j$(nproc) && \
    make check -j$(nproc) && \
    sudo make install

# PicoTLS
WORKDIR /home/ns3dce
RUN git clone https://github.com/p-quic/picotls.git
WORKDIR picotls
RUN git submodule init && \
    git submodule update
RUN sed -i 's/${CMAKE_C_FLAGS}/${CMAKE_C_FLAGS} -fPIC/g' CMakeLists.txt
RUN cmake . && \
    make -j$(nproc)

WORKDIR /home/ns3dce/

ENV DCE_PATH /home/ns3dce/pquic/
ENV NS3_PATH /home/ns3dce/dce-linux-dev/source/dce-linux-dev
ENV LD_LIBRARY_PATH /home/ns3dce/dce-linux-dev/source/dce-linux-dev/build/lib:/home/ns3dce/dce-linux-dev/source/dce-linux-dev/build_opt/lib:/home/ns3dce/dce-linux-dev/build/lib:/home/ns3dce/dce-linux-dev/build_opt/lib:/usr/local/lib/
