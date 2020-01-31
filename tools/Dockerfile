FROM ubuntu:18.04

RUN apt-get update \
 && apt-get install -y \
    pkg-config \
    libevdev-dev \
    cmake \
    make \
    gcc \
    g++ \
    wget \
    git \
 && apt-get clean \
 && rm -rf /var/lib/apt/lists/*

 WORKDIR /tmp

RUN wget https://github.com/google/googletest/archive/release-1.8.0.tar.gz \
 && mkdir -p /usr/src/googletest \
 && tar xf release-1.8.0.tar.gz -C /usr/src/googletest \
 && rm release-1.8.0.tar.gz

WORKDIR /

ENV GTEST_ROOT /usr/src/googletest/googletest-release-1.8.0
