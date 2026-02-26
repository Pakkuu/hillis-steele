# hillis-steele-prefix

A C++ implementation of the Hillis & Steele parallel prefix-sum algorithm using Unix `fork()`, shared memory (`shmget`), and a custom process barrier.

## Build

```
make
```

## Usage

```
./my-sum <n> <m> <input_file> <output_file>
```

`n` = number of elements, `m` = number of worker processes