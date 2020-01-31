#!/bin/bash
SCRIPT_PATH=$(dirname "$(realpath -s "$BASH_SOURCE")")
SRC_PATH=$(dirname "$SCRIPT_PATH")
DOCKER_IMAGE="flir/fam-ubuntu-18.0.4-gtest-1.8.0"

docker build "$SCRIPT_PATH" -t $DOCKER_IMAGE

docker run \
    --volume="${SRC_PATH}:/src" \
    -w "/src" \
    --rm \
    ${DOCKER_IMAGE} \
    /bin/bash -c "rm -rf test_run && mkdir test_run && cd test_run && cmake .. && make && tests/maf_test"


