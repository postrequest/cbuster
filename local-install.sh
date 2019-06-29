#!/bin/sh 

# Retrieve and compile libdill
if wget http://libdill.org/libdill-2.14.tar.gz
then
	echo "Downloaded libdill"
else curl http://libdill.org/libdill-2.14.tar.gz -o libdill-2.14.tar.gz
	echo "Downloaded libdill"
fi
tar -xzf libdill-2.14.tar.gz
cd libdill-2.14
./configure --enable-tls
make
cd ..

# statically compile the binary
cc cbuster.c libdill-2.14/libdill.h libdill-2.14/.lib/libdill.a -static -lssl -ltls -lcrypto -pthread -o cbuster
