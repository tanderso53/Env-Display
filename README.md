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
Documents/env-display/env-display
Documents/env-display/env-display -f <filename>
Documents/env-display/env-display -u <host> | -t <host> -p <port>
Documents/env-display/env-display -s <serial> [-b <baud>]
Documents/env-display/env-display -h
Documents/env-display/env-display -V

Calling with no options will read from stdin. Alternatively -f, -u, or -t
can be supplied to read from a file or from a UDP socket. To read from a
serial port, specify the port with -s and the speed with -b.

Options:
-f <filename>	A filename to read json env data from
-u <host>	A hostname or ip address to connect to via UDP. Cannot
		be used with the -f option
-t <host>	A hostname or ip address to connect to via TCP. Cannot
		be used with the -f option
-p <port>	A port number to connect to on the remote port. Must be
		used with the -u or -t option
-s <serial>	Special file path for a serial device
-b <baud>	Baud rate for serial connection (default: 9600)
-h		Print usage message, then exit
-V		Print version information, then exit
~~~~

## Example

~~~~
┌──────────────────────────────────────────────────────────────────────────────┐
│ Environmental Data Display Program, Version: v2.1.0-28-ge638f55              │
│ ┌──────────────────────────────────────────────────────────────────────────┐ │
│ │  V Batt                    3.70  V                                       │ │
│ │                                                                          │ │
│ │  temperature              21.88  degC                                    │ │
│ │                                                                          │ │
│ │  pressure              99402.24  Pa                                      │ │
│ │                                                                          │ │
│ │  humidity                100.00  %                                       │ │
│ │                                                                          │ │
│ │  gas resistance     12946861.00  ul                                      │ │
│ │                                                                          │ │
│ │  temperature              20.48  degC                                    │ │
│ │                                                                          │ │
│ │  pressure              99409.22  Pa                                      │ │
│ │                                                                          │ │
│ │  humidity                 35.98  %                                       │ │
│ │                                                                          │ │
│ │  PM1.0 Std                 2.00  ug/m^3                                  │ │
│ │                                                                          │ │
│ │  PM2.5 Std                 2.00  ug/m^3                                  │ │
│ │                                                                          │ │
│ │  pm10_std                  2.00  ug/m^3                                  │ │
│ │                                                                          │ │
│ │  NP > 0.3um              510.00  num/0.1L a                              │ │
│ │                                                                          │ │
│ └──────────────────────────────────────────────────────────────────────────┘ │
│ Last update: 03/07/2022 15:29:12 CST                                         │
└──────────────────────────────────────────────────────────────────────────────┘
~~~~

## License

The source code and compiled binaries are released under the terms of
the BSD 3-Clause "New" or "Revised" License. A copy of the license
should be included with the source or binary distribution.
