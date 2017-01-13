# routerboot2art
The routerboot2art extracts an ART from RouterBoot image of Mikrotik router.

## Build
To build an executable simply type:
```sh
g++ routerboot2art.cpp -o routerboot2art
```

## RouterBoot image
Before you can generate the ART, you must obtain a RouterBoot image (mtdblock0 partition).
```sh
ssh root@192.168.1.1 "dd if=/dev/mtdblock0 of=/tmp/mtdblock0"
scp root@192.168.1.1:/tmp/mtdblock0 routerboot2art/mtdblock0
```

## ART extraction
Finally you can generate the art.bin file.
```sh
g++ routerboot2art/routerboot2art.cpp -o routerboot2art/routerboot2art
routerboot2art/routerboot2art mtdblock0 art.bin
```
