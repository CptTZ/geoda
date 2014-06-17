#!/bin/bash


if ! [ -d build ]; then
    echo "Please run build.sh to build GeoDa executable file first."
    exit
fi

if ! [ -f build/GeoDa ]; then
    echo "Please run build.sh to build GeoDa executable file first."
    exit
fi

rm -rf product
cp -rf package product

chmod +x product/DEBIAN/postinst
cp -rf build/* product/usr/local/geoda/
cp run_geoda.sh product/usr/bin/

cd product 
find . -name .svn |xargs rm -rf
cd ..

MACHINE_TYPE=`uname -m`
if [ $MACHINE_TYPE == 'x86_64' ]; then
    # 64-bit platform
    rm product/DEBIAN/control
    mv product/DEBIAN/control64 product/DEBIAN/control
fi

rm -f *.deb
dpkg -b product/ geoda_1.5.deb

