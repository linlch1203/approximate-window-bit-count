# Approximate Window Bit Count (DGIM Algorithm)

This repository contains an implementation of the DGIM algorithm for estimating the number of 1s in a sliding window over a data stream. 

The implementation is highly optimized for embedded IoT devices, featuring:
- **$O(1)$ Amortized Time Complexity**: Uses optimization arrays to achieve constant-time bucket merging, processing over 50 million items per second.
- **Strict Memory Constraints**: Calls `malloc` exactly once during initialization to pre-allocate a custom memory pool. No dynamic memory allocation occurs during stream processing.
- **Low Memory Footprint**: Uses only ~1.5 MB of memory to track a sliding window of 100,000,000 items with a relative error bound of 0.1%.

## Files
- `window-bit-count-apx.h`: The core implementation of the DGIM algorithm.
- `test.c`: Automated test suite verifying the approximation against an exact counting method.
- `bench.c`: Benchmarking script to measure throughput and memory usage.
- `Makefile`: Build instructions.

## Usage

To run the tests:
```bash
make test
```

To run the benchmark:
```bash
make bench
```

## Benchmark Results
```text
**** BENCHMARK: Bit counting over a sliding window (approximate) *****
stream length = 1,000,000,000
window size = 100,000,000
k = 1,000
last output = 99,994,113
number of merges = 999,969,746
duration = 19,695,236,000 nanoseconds
throughput = 50,773,699 items/sec
memory footprint = 1,537,536 bytes
```