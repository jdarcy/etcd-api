# Copyright (c) 2013, Red Hat
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are met:
#
# Redistributions of source code must retain the above copyright notice, this
# list of conditions and the following disclaimer.  Redistributions in binary
# form must reproduce the above copyright notice, this list of conditions and
# the following disclaimer in the documentation and/or other materials provided
# with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
# POSSIBILITY OF SUCH DAMAGE.

CFLAGS	= -fPIC -g -O0 -Wall

SHLIB	= libetcd.so
S_OBJS	= etcd-api.o

TESTER	= etcd-test
T_OBJS	= etcd-test.o

LEADER	= leader
L_OBJS	= leader.o

TARGETS	= $(SHLIB) $(TESTER)
OBJECTS	= $(S_OBJS) $(T_OBJS) $(L_OBJS)

all: $(TARGETS)

$(SHLIB): $(S_OBJS)
	$(CC) -shared -nostartfiles $(S_OBJS) -lcurl -lyajl -o $@

$(TESTER): $(T_OBJS) $(SHLIB)
	$(CC) $(T_OBJS) -L. -letcd -o $@

$(LEADER): $(L_OBJS) $(SHLIB)
	$(CC) $(L_OBJS) -L. -letcd -o $@

clean:
	rm -f $(OBJECTS)

clobber distclean realclean spotless: clean
	rm -f $(TARGETS)
