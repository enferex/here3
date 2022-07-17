here3: Statically insert a breakpoint interrupt into an executable x86 file.
============================================================================
Have you ever been curious to know if a particular program reaches a function
during runtime? Well, here3 is here to help.

here3 takes an object file and list of functions to instrument. here3 will scan
the debug information present in the object file and insert the x86 software
interrupt (INT3) into the start of each function requested.  During runtime, a
SIGTRAP will trigger if that function is reached.  A debugger can also be used
to catch the trap and generate a stack trace.

This replacement of the initial instruction will likely corrupt the stack so
once the trap is hit, do not expect the program to operate in typical fashion.


Usage
-----
`Usage: here3 <input object file> [list of functions to instrument]`

The output will be a new instrumented file with the ".here3" prefixed to the 
original extension.

Caveats
-------
This project is in alpha.

Dependencies
------------
* [llvm](https://llvm.org)
* [cmake](https://cmake.org)

Building
--------
1. Create a build directory: (mkdir build)
2. Run cmake to build this:  (cd build && cmake .. && make)

References
----------
1. [int3](https://en.wikipedia.org/wiki/INT_(x86_instruction)
2. [llvm tools](https://github.com/llvm/llvm-project/tree/main/llvm/tools)

Contact
-------
Matt Davis: https://github.com/enferex
