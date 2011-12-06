#! /bin/sh
set -e

if [ -e "${rootdir}/src/libdbdnull.so" ]; then
        libdir="${rootdir}/src"
else
        libdir="${rootdir}/src/.libs"
fi

$1 "${libdir}"
