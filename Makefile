#####################################################################
#                                                                   #
#                            Env Display                            #
#                 A program to display environmental                #
#                data from Kitty Comfort-like devices               #
#                                                                   #
#####################################################################

# Copyright 2021 Tyler J. Anderson

# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:

# 1. Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.

# 2. Redistributions in binary form must reproduce the above copyright
# notice, this list of conditions and the following disclaimer in the
# documentation and/or other materials provided with the distribution.

# 3. Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
# INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
# LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
# CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
# ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

# env-display Makefile

srcdir		=	./src
OBJDIR		:=	./objdir

# BUILD SECTION
CC		=	clang
CXX		=	clang++
VC		=	git
PROFILER	=	llvm-profdata merge
COV		=	llvm-cov

CFLAGS		=	-Wall -g -I/usr/local/include
CXXFLAGS	=	-std=c++17
LDLIBS		=	-ljsoncpp -lncurses -lform -lc
LDFLAGS		=	-L/usr/local/lib

APP		=	env-display
C_SRCS		=	main.c display-driver.c
CXX_SRCS	=	jsonparse.cpp
C_OBJS		=	$(addprefix $(OBJDIR)/,$(C_SRCS:.c=.o))
CXX_OBJS	=	$(addprefix $(OBJDIR)/,$(CXX_SRCS:.cpp=.o))
OBJS		:=	$(C_OBJS) $(CXX_OBJS)
INSTROBJ	:=	$(addprefix $(OBJDIR)/,display-driver.oi jsonparse.oi tests.o)
H		=	jsonparse.h display-driver.h
LICENSE		=	./LICENSE

IS_REPO		:=	$(shell if [ -d ./.git ]; then echo "1"; else echo "0"; fi)

ifeq ($(IS_REPO), 1)
CFLAGS		+=	-DVM_VERSION="\"$(shell $(VC) describe --always)\""
endif

.PHONY: all clean install coverage

all: $(APP)

$(OBJDIR)/%.o: $(srcdir)/%.c $(addprefix $(srcdir)/,$(H))
	@echo "*** BUILDING $@ ***"
	$(CC) -c ${CFLAGS} -o $@ $<

$(OBJDIR)/%.oi: $(srcdir)/%.c $(addprefix $(srcdir)/,$(H))
	@echo "*** BUILDING $@ ***"
	$(CC) -c ${CFLAGS} -fprofile-instr-generate -fcoverage-mapping \
		-o $@ $<

$(OBJDIR)/%.o: $(srcdir)/%.cpp $(addprefix $(srcdir)/,$(H))
	@echo "*** BUILDING $@ ***"
	$(CXX) -c ${CFLAGS} ${CXXFLAGS} -o $@ $<

$(OBJDIR)/%.oi: $(srcdir)/%.cpp $(addprefix $(srcdir)/,$(H))
	@echo "*** BUILDING $@ ***"
	$(CXX) -c ${CFLAGS} ${CXXFLAGS} -fprofile-instr-generate \
		-fcoverage-mapping -o $@ $<

%.profraw: %
	@echo "**** RUNNING TESTS $< ****"
	LLVM_PROFILE_FILE="$@" ./$<

%.profdata: %.profraw
	$(PROFILER) -sparse $< -o $@

coverage.json: test-suite.profdata test-suite.profraw
	$(COV) export ./test-suite -instr-profile=$< \
		-ignore-filename-regex="/usr/local/include/.*" > $@

coverage.report: test-suite.profdata test-suite.profraw
	$(COV) report ./test-suite -instr-profile=$< \
		-ignore-filename-regex="/usr/local/include/.*" > $@

$(APP): $(OBJS)
	@echo "*** BUILDING $@ ***"
	$(CXX) ${CFLAGS} ${LDFLAGS} ${LDLIBS} -o $@ $(OBJS)
	@echo "Complete! Install with \"make install\""

clean:
	$(RM) $(APP) test-suite coverage.json test-suite.profraw \
		test-suite.profdata coverage.report
	$(RM) -R $(OBJDIR)

test-suite: $(INSTROBJ)
	@echo "*** BUILDING $@ ***"
	$(CXX) ${CFLAGS} ${LDFLAGS} ${LDLIBS} \
		-fprofile-instr-generate -fcoverage-mapping -o $@ $^

coverage: coverage.json coverage.report
	cat coverage.report

$(OBJS): | $(OBJDIR)

$(INSTROBJ): | $(OBJDIR)

$(OBJDIR):
	mkdir $(OBJDIR)

# INSTALL SECTION
INSTALL=install
INSTALL_PROGRAM=$(INSTALL)
INSTALL_DATA=$(INSTALL) -m 644
PREFIX=/usr/local
BINDIR=$(PREFIX)/bin
DATAROOTDIR=$(PREFIX)/share
DATADIR=$(DATAROOTDIR)
SYSCONFDIR=$(PREFIX)/etc
DOCDIR=$(DATAROOTDIR)/doc/$(APP)

install: all
	mkdir -p $(DESTDIR)$(BINDIR)
	mkdir -p $(DESTDIR)$(DATADIR)/$(APP)
	$(INSTALL_PROGRAM) $(APP) $(DESTDIR)$(BINDIR)/$(APP)
	$(INSTALL_DATA) $(LICENSE) $(DESTDIR)$(DATADIR)/$(APP)/LICENSE
