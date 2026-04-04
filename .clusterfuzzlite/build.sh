#!/bin/bash -eu
xmake f -c -v --ccache=n --root -m cifuzz --cxflags="$CXXFLAGS" --ldflags="$CXXFLAGS $LIB_FUZZING_ENGINE"
xmake -v --root cifuzz
cp build/linux/x86_64/cifuzz/cifuzz "$OUT"/
