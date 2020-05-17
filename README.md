# DDR Tools
Tools for extracting and modifying files from Dance Dance Revolution CS games.

## TCB Tools
Tools for extracting and converting TCB image files.

### Building
```
mkdir build
cd build
cmake ..
make
```

### Usage

#### tcb-extract
Search for and extract compressed and uncompressed TCB files within a file.

```
./tcb-extract <mode> <file containing TCBs>
```
Modes:
* `1` - Bruteforce search for compressed and uncompressed TCBs.
* `2` - Extract TCBs from file beginning with table (compressed entries).
* `3` - Extract TCBs from file beginning with table (uncompressed entries).

In most cases `1` works best as many table variations exist that `2` and `3`
won't work with.

#### tcb-convert
Convert TCB files to PNG images or inject PNG images back into TCB files. The
extracted image will be a standard RGBA PNG image converted from either a 16 or
256 color palletized source. When injecting a PNG back into a TCB, the image
data will be updated and a new pallete will be generated to match the TCB's
original format. The PNG you inject must be the same resolution as the TCB.

> Convert TCB to PNG:
```
./tcb-convert e <TCB file>
```

> Inject PNG into TCB:
```
./tcb-convert i <PNG file>
```

## filedata-tool.py
Extract and create filedata.bin files.

```
python3 filedata-tool.py <mode> <elf file> <filedata.bin> <directory>
```
Modes:
* `extract` - Extract the contents of `filedata.bin` to `directory`. All files
  referenced in the file table located in the game's `elf file` will be
  extracted in addition to "hidden" data missing from the file table. A CSV file
  will be created named `directory\fieldata.csv` containing IDs, offsets, and
  lengths found in the game's ELF, hidden file offsets and lengths, the exported
  filename, and a guessed description of the file contents to aid in
  modification.
* `create` - Create `filedata.bin` using files in `directory`. The CSV created
  by the extraction mode is used to assemble a new file in the correct order and
  to update the file table in `elf file`.

### Tips
* Don't modify the ids, offsets, or lengths in the CSV file created by the
  extraction mode. The filenames can be changed if desired.
* Don't change the order of the rows in the CSV file. It matches the order of
  the file table found in the game's ELF.
* New entries can't be added to the filetable, although this wouldn't be useful
  anyway. Instead, existing entries can be modified.
