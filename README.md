
# libchibi: A compiler backend library based on [chibicc](https://github.com/rui314/chibicc)

Libchibi is a minimal non-optimising compiler backend that aims to be **the fastest way to go from a parser to working compiler** by removing the need to generate IR entirely. Libchibi is based on the AST used in [chibicc](https://github.com/rui314/chibicc) and generates either assembly or an assembled object file directly from the AST.

The backend is intended to be useful for a few different use cases:
  * hobbyist language development where writing a language easily without getting bogged down writing a lowering stage from the AST to IR or assembly
  * beginners to programming language development who want to learn about designing a language and writing a parser with as little friction and complexity as possible
  * programming language researchers for whom being able to try out different features quickly and easily is more important than producing optimised binaries.

Since the AST is based on chibicc and therefore reflects C's structure, it has many of the same advantages of using C as a compile backend in terms of C compatibility and being a pretty good baseline for a wide array of different frontend languages, but at the cost of portability since only Linux-x86_64 is supported and optimisation typically producing compiled code that is probably twice or more slower than the equivalent code that would be produced by emitting raw C and compiling with GCC [source: chibicc's readme]

I am also writing a book [Write Your First Compiler With Libchibi](https://github.com/othd06/Write-Your-First-Compiler-With-Libchibi) where I intend to teach many of the basic principles of writing a compiler to guide a beginner through the process of writing a lisp-like language using the libchibi library as a backend.

## Usage:
  * Include libchibi.h
  * Link with libchibi.a (both available in the releases section).
I currently do not have any documentation other than the header file and whatever is currently completed of my [book](https://github.com/othd06/Write-Your-First-Compiler-With-Libchibi) but please do submit an issue requesting documentation if there is anything you are struggling to understand.
### Important Note About Memory
One point of note is that, in line with chibicc, the library uses calloc to allocate memory (a variant of malloc that zero-initialises all allocated memory) but, since it is designed for relatively simple programs that will run once and close, never attempts to free any memory leaking all allocated memory and letting the operating system clean up on exit. This means you should not attempt to free any memory provided by libchibi and that it is recommended you code in a similar style or at least remain aware that this is going on under the hood.

## Building:
To build, clone the repository and run make. This will produce the libchibi.a static library allowing you to copy both that and the libchibi.h header into your own project. Running make clean will remove the compiled files.

## Status:
Since libchibi is based on the codebase of chibicc it inherits much of the status of that codebase in terms of theoretical functionality. This means it can theoretically be used to produce code that achieves all of the mandatory and most of the optional features of C11 as well as many gcc extensions (such as labels-as-values) as well as anything that can be reduced to that (which is pretty much all code since C is a fairly universal baseline).
The main limitation is that libchibi **only supports Linux-x86_64** however it is compatible, out of the box, with any C code for that platform (or any non-C code that exposes a sysV C API) and outputs useful debug symbols.
I have made attempts to provide somewhat nice error messages and maintain the error messages provided by chibicc's codegen.
Much like chibicc there is no optimisation pass currently (however I will likely add some basic constant folding at some point).
I am using Fedora 43 to develop libchibi and I suspect that it will work just as well on Ubuntu 20.04 (the platform that the codegen backend code was developed on) however I have not extensively bug-tested the library as of yet so please do report any bugs so that I (or someone else, if this project gathers contributors) can fix it.

## Internals:
libchibi primarily consists of the codegen from chibicc as well as my own API glue that defines opaque handles to all of the constructs required to build a program (objects, AST nodes, etc...) as well as comprehensive functions to build each of these.

## Contributing:
I'm very happy to take pull requests. Honestly, I'm not an expert in a lot of the backend code inherited from chibicc so I very much welcome anything that anyone has to add to the project to make it better or more useful. The style of my code also isn't particularly consistent with the style used by [Rui Ueyama](https://github.com/rui314) so there isn't exactly a strict styleguide or anything. Just try not to break stuff.


