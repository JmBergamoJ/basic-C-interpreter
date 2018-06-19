# basic-C-interpreter
very basic c-based interpreter, using a recursive descent parser. Made for studying purposes.

When compiling, link both PARSER.o and CLIB.o into CINTERP

Using GCC:
```
gcc PARSER.c -c
gcc CILIB.c -c
gcc CINTERP.c PARSER.o CILIB.o -o interp.exe
```

It uses command line arguments to run a source code:

```interp mySourceCode.c```