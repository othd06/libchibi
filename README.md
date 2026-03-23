# libchibi: A compiler backend library based on https://github.com/rui314/chibicc

This is is my attempt to refactor chibicc to remove the lexer and parser, keeping only the code used to compile AST's into working ELF executables and compiling into a static library for use by various compiler frontends.

