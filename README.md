MD5 Implementation
==================

An MD5 implementation in C, using the **SSEx** instruction set, to parallelize and speed up the algorithm.

To benchmark the speed, one program tries to crack a short MD5 hash using a brute force attack.
The other program calculates the MD5sum of a memory mapped file, given as an input.

The MD5 functions (with and without SSE instructions) can be easily isolated and used in different projects.

Compile
-------

To compile the files:
```
gcc -o md5 -O3 -lm md5.c
gcc -o md5_sse -msse -mmmx -msse2 -O3 md5_sse.c
```
