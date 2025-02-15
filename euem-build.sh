#!/bin/bash

set -e

WORKSPACE=/home/abeltran/gitorious
PHANTOMJS_DIR=${WORKSPACE}/phantomjs
QCA_DIR=${PHANTOMJS_DIR}/src/qca
QT_DIR=${PHANTOMJS_DIR}/src/qt
EMF_DIR=/home/abeltran/Geneos/Trunk/Development/Source
PLATFORM=linux_64

QT_CFG=''

COMPILE_JOBS=1
MAKEFLAGS_JOBS=''

if [[ "$MAKEFLAGS" != "" ]]; then
  MAKEFLAGS_JOBS=$(echo $MAKEFLAGS | egrep -o '\-j[0-9]+' | egrep -o '[0-9]+')
fi

if [[ "$MAKEFLAGS_JOBS" != "" ]]; then
  # user defined number of jobs in MAKEFLAGS, re-use that number
  COMPILE_JOBS=$MAKEFLAGS_JOBS
elif [[ $OSTYPE = darwin* ]]; then
   # We only support modern Mac machines, they are at least using
   # hyperthreaded dual-core CPU.
   COMPILE_JOBS=4
else
   CPU_CORES=`grep -c ^processor /proc/cpuinfo`
   if [[ "$CPU_CORES" -gt 1 ]]; then
       COMPILE_JOBS=$CPU_CORES
       if [[ "$COMPILE_JOBS" -gt 8 ]]; then
           # Safety net.
           COMPILE_JOBS=8
       fi
   fi
fi

until [ -z "$1" ]; do
    case $1 in
        "--qt-config")
            shift
            QT_CFG=" $1"
            shift;;
        "--qmake-args")
            shift
            QMAKE_ARGS=$1
            shift;;
        "--jobs")
            shift
            COMPILE_JOBS=$1
            shift;;
        "--help")
            echo "Usage: $0 [--qt-config CONFIG] [--jobs NUM]"
            echo
            echo "  --qt-config CONFIG          Specify extra config options to be used when configuring Qt"
            echo "  --jobs NUM                  How many parallel compile jobs to use. Defaults to 4."
            echo
            exit 0
            ;;
        *)
            echo "Unrecognised option: $1"
            exit 1;;
    esac
done

cd src/qt && ./preconfig.sh --jobs $COMPILE_JOBS --qt-config "$QT_CFG" && cd ../..

# Build QCA
cd ${QCA_DIR}/qca-2.0.3
./configure --qtdir=${QT_DIR}
make sub-src-make_default sub-tools-make_default

cd ${QCA_DIR}/qca-ossl-2.0.0
LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${QCA_DIR}/qca-2.0.3/lib:${EMF_DIR}/ThirdParty/openssl/${PLATFORM}/runtime ./configure --with-qca=${QCA_DIR}/qca-2.0.3 --qtdir=${QT_DIR}
make

# Build PhantomJS
cd ${PHANTOMJS_DIR}
src/qt/bin/qmake $QMAKE_ARGS
make -j$COMPILE_JOBS
