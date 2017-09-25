### AVS File Decoder (Qt version)

![Preview](https://raw.github.com/grandchild/AVS-File-Decoder-Qt/master/etc/preview.png)

This is the Qt port of the [AVS File Decoder](https://github.com/grandchild/AVS-File-Decoder).
It exists because batch conversion turned out a bit slow over Javascript and HTTP POST requests the way I was doing it in the [multiple](https://github.com/grandchild/AVS-File-Decoder/tree/multiple) branch over there.


#### Build
Import into QtCreator and build there or from the (Unix) commandline:
```sh
$ make
# or
$ make release
```

#### Run
```sh
$ make run
# or
$ cd build/<type>/ && ./AvsDecoder
```