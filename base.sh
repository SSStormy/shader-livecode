#!/bin/bash

function cd_into_build() {
    mkdir build -p
    cd build
}

function compile() {
    echo compiling

    cd ..
    premake5 gmake2
    cd build

    make config=debug -j7 2>&1 >/dev/null | tee build_log_colored

    build_status=${PIPESTATUS[0]}

    # strip color information from build_log while keeping it in less
    cat build_log_colored | sed -r "s/\x1B\[([0-9]{1,2}(;[0-9]{1,2})?)?[mGK]//g" > build_log

    if [ `wc -c < build_log_colored` -gt 0 ] ; then
        less build_log_colored
    fi
}

function run_on_success() {
    if [ ${build_status} -eq 0 ] ; then
        $@
    fi
}

function debug() {
    cgdb -ex "run" $@
}
