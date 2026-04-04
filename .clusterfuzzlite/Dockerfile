FROM gcr.io/oss-fuzz-base/base-builder:v1
RUN apt-get update && \
    apt-get install -y software-properties-common && \
    add-apt-repository ppa:xmake-io/xmake &&\
    apt-get update &&\
    apt-get install -y xmake clang-tools-18
RUN ln -s /usr/lib/llvm-18/bin/clang-scan-deps /usr/local/bin/clang-scan-deps
COPY . $SRC/conv
WORKDIR conv
COPY .clusterfuzzlite/build.sh $SRC/
