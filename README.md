terminator
==========

A program to run a command as though in a terminal.

OVERVIEW:
---------

Terminator runs a command as though it were on a terminal using a PTY. This is
useful to trick programs into diabling buffering, or otherwise acting as if
they are interactive, while still capturing their output.

COMPILING:
----------

Terminator is build using autoconf. Run the following to build and install
terminator:

    autoreconf --install
    ./configure
    make
    make install

RUNNING:
--------

    terminator command [arg...]

To run terminator, give the command to run and all its command line arguments
as arguments to terminator. Terminator uses the execvp system call to run the
command, so the path will be searched if the command name is not a path.

