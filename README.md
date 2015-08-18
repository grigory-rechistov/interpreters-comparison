# interpreters-comparison

Sample programs for comparison of different VM interpretation techniques.
See the Makefile for the full list of targets.

## Original List of Interpreters

* `switched` - switched interpreter
* `threaded` - threaded interpreter
* `predecoded` - switched interpreter with preliminary decoding phase
* `subroutined` - subroutined interpreter
* `threaded-cached` - threaded interpreter with pre-decoding.
* `tailrecursive` - subroutined interpreter with tail-call optimization
* `translated` - binary translator to Intel 64 machine code
* `native` - a static implementation of the test program in C

## More Binary Translators

Additional types of translators have been added as a follow up to the original work:
* `translator-retmanip` - a variant of translator which manipulates with a return stack to direct control flow.
* `translator-jmpmanip` - a variant of translator which uses indirect jumps instead of returns to direct control flow.
* `translator-inline` - a variant of translator which inlines code blocks for target instructions.

They are only tested to work when compiled with GCC.
In our experiments, ICC-generated code did not satisfy simplified assumptions on register usage; adapting code to it would require certain amount of corrections to prologue/epilogue.

## Benchmarking

Use `measure.sh` to measure run time of individual binaries or to perform a comparison of multiple variants (alternatively, run `make all measure`).

The suite was tested to compile and run with GCC 4.8.1, GCC 5.1.0 and ICC 15.0.3 on Ubuntu Linux 12.04.5. Limited testing is also done on Windows 8.1 Cygwin64 environment, GCC 4.8.

An article discussing structure, performance variations, and comparison of interpreters: http://habrahabr.ru/company/intel/blog/261665/ (in Russian).
