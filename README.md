# TCB Tools
Tools for extracting TCB image files from Dance Dance Revolution CS games.

## Building
```
mkdir build
cd build
cmake ..
make
```

## Usage

### tcb-extract
Search for and extract compressed and uncompressed TCB files within a file.

```
./tcb-extract <mode> <file containing TCBs>
```
Modes:
* 1 - Bruteforce search for compressed and uncompressed TCBs.
* 2 - Extract TCBs from file beginning with table (compressed entries).
* 3 - Extract TCBs from file beginning with table (uncompressed entries).

### tcb-convert
Convert TCB files to PNG images or inject PNG images back into TCB files. The extracted image will be a standard RGBA PNG image converted from either a 16 or 256 color palletized source. When injecting a PNG back into a TCB, the image data will be updated and a new pallete will be generated to match the TCB's original format. The PNG you inject must be the same resolution as the TCB.

```
./tcb-convert <mode> <TCB file>
```
Modes:
* e - Convert TCB to PNG.
* i - Inject PNG into TCB.
