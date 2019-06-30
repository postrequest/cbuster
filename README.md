# cbuster

Simple and quick (directory && file) fuzzer.

## Dependencies
* libssl-dev
* libdill - (http://libdill.org/download.html) (https://github.com/sustrik/libdill) --enable-tls
* Only tested with LibreSSL

### Local Install
```
$ git clone https://github.com/postrequest/cbuster.git && cd cbuster && cc cbuster.c -ldill -o cbuster
$ ./cbuster
```
