FROM ubuntu:latest AS build

ARG REPO="https://github.com/DmitryTyuphyakin/openocd.git"
ARG BRANCH="stm32f7x_spi_flash"

RUN apt-get update &&\
    apt-get install -y \
        checkinstall \
        make \
        libtool \
        pkg-config \
        autoconf \
        automake \
        zip \
        unzip \
        git \
        libjaylink-dev \
        libftdi-dev \
        libusb-1.0-0-dev

WORKDIR /work

RUN git config --global --add safe.directory /work
RUN git clone -b ${BRANCH} ${REPO} .

RUN ./bootstrap &&\
    ./configure && \
    make -j &&\
    checkinstall -y --install=no --fstrans=no\
        --nodoc \
        --pkgversion=`git describe --tags --match v* | sed -e 's/v//'` \
        --pkgname=openocd


FROM ubuntu:latest AS openocd

RUN apt-get update &&\
    apt-get install -y \
        libjaylink0 \
        libftdi1 \
        libusb-1.0-0 &&\
    apt-get clean autoclean &&\
    apt-get autoremove --yes &&\
    rm -rf /var/lib/{apt,dpkg,cache,log}/

WORKDIR /work

COPY --from=build /work/openocd*.deb .

RUN dpkg -i ./*.deb &&\
    rm ./*.deb

ENV OPENOCD_CFG="target/stm32f7x.cfg"

CMD openocd -s . -f ${OPENOCD_CFG}
