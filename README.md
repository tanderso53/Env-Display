# Env Display

Ncurses program to display environmental data from a Kitty Comfort
device

## Dependencies

The following libraries and utilities are required:

- jsoncpp
- ncurses

## Building

In addition to the above dependencies, the following is required to
build from source:

- A compiler (Clang or GCC recommended)
- GNUmake (Default make system on linux, may need to install on BSDs)
- Development headers for libraries (Pkgs may be called jsoncpp-dev or
  jsoncpp-devel)

On Linux:

~~~~ bash
make
make install
~~~~

On FreeBSD and similar:

~~~~ bash
gmake
gmake install
~~~~

By default, the build system will use clang as the compiler. If you
would like to use GCC, add the following to the make command:

~~~~ bash
gmake CC=gcc CXX=g++
~~~~

To install in a home directory instead of a system directory, try the
following:

~~~~ bash
gmake install PREFIX="$HOME/.local"
~~~~

## Usage

~~~~
Usage:
env-display
env-display -f <filename>
env-display -u <host> -p <port>

Calling with no options will read from stdin. Alternatively -f or -u
can be supplied to read from a file or from a UDP socket.

Options:
-f <filename>	A filename to read json env data from
-u <host>	A hostname or ip address to connect to via UDP. Cannot
		be used with the -f option
-p <port>	A port number to connect to on the remote port. Must be
		used with the -u option
-h		Print usage message, then exit
-V		Print version information, then exit
~~~~

## License

The source code and compiled binaries are released under the terms of
the BSD 3-Clause "New" or "Revised" License. A copy of the license
should be included with the source or binary distribution.
