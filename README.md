# hillis-steele-prefix

A C++ implementation of the Hillis & Steele parallel prefix-sum algorithm using Unix `fork()`, shared memory (`shmget`), and a custom reusable process barrier.

## Team Members

- Ethan Varghese
- Ajay Alluri

## Build & Run

```
make        # compile
make run    # compile and run with default args (5 elements, 5 workers, A.txt → B.txt)
```

To run with custom arguments after building:

```
./my-sum <n> <m> <input_file> <output_file>
```

| Argument | Description |
|---|---|
| `n` | Number of elements to read from the input file |
| `m` | Number of worker processes (cores); must satisfy `1 ≤ m ≤ n` |
| `input_file` | Path to a file containing space-separated integers (at least `n`) |
| `output_file` | Path where the prefix-sum result will be written |

## Example

Given `A.txt` containing `3 -2 4 9 5`:

```
make run
```

Or with custom args: `./my-sum 5 4 A.txt B.txt` → `B.txt` will contain `3 1 5 14 19`