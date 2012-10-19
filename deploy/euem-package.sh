#!/bin/bash
set -e

WORKSPACE=/home/abeltran/gitorious
PHANTOMJS_DIR=${WORKSPACE}/phantomjs
QCA_DIR=${PHANTOMJS_DIR}/src/qca
QT_DIR=${PHANTOMJS_DIR}/src/qt
EMF_DIR=/home/abeltran/Geneos/Trunk/Development/Source
PLATFORM=linux_64

# Package PhantomJS
version=$(LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${QCA_DIR}/qca-2.0.3/lib:${EMF_DIR}/ThirdParty/openssl/${PLATFORM}/runtime ${PHANTOMJS_DIR}/bin/phantomjs --version | sed 's/ /-/' | sed 's/[()]//g')
DEST_DIR=${WORKSPACE}/phantomjs/deploy/phantomjs-$version-linux-$(uname -m)
rm -rf ${DEST_DIR}

mkdir -p ${DEST_DIR}/{bin,lib}

if [[ ! -f ${PHANTOMJS_DIR}/bin/phantomjs ]]; then
    exit 1
fi

cp ${PHANTOMJS_DIR}/bin/phantomjs ${DEST_DIR}/bin/phantomjs.bin
cp -r ${PHANTOMJS_DIR}/{ChangeLog,examples,LICENSE.BSD,README.md} ${DEST_DIR}

PHANTOMJS_BIN=${DEST_DIR}/bin/phantomjs.bin

LIBS=$(LD_LIBRARY_PATH=$LD_LIBRARY_PATH:${QCA_DIR}/qca-2.0.3/lib:${EMF_DIR}/ThirdParty/openssl/${PLATFORM}/runtime ldd ${PHANTOMJS_BIN} | egrep -o "/[^ ]+ ")

LIBLD=
for l in $LIBS; do
    ll=$(basename $l)
    cp $l ${DEST_DIR}/lib/$ll
    if [[ "$l" == *"ld-linux"* ]]; then
        LIBLD=$ll
    fi
done

RUN_SCRIPT=${DEST_DIR}/bin/phantomjs
echo '#!/bin/sh' >> ${RUN_SCRIPT}
echo 'path=$(dirname $(dirname $(readlink -f $0)))' >> ${RUN_SCRIPT}
echo 'export LD_LIBRARY_PATH=$path/lib:$path/../../..' >> ${RUN_SCRIPT}
echo 'exec $path/lib/'${LIBLD}' $path/bin/phantomjs.bin "$@"' >> ${RUN_SCRIPT}
chmod +x ${RUN_SCRIPT}

strip -s ${PHANTOMJS_BIN}
[[ -d ${DEST_DIR}/lib ]] && strip -s ${DEST_DIR}/lib/*
