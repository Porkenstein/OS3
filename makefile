# USE the GNU C/C++ compler:
CPP = g++

# COMPOLER OPTIONS:
CXXFLAGS= -std=c++0x

#OBJECT FILES
OBJS = dsh.o

dsh: dsh.o
	${CPP} ${CXXFLAGS} -lm ${OBJS} -o dsh
dsh.o: dsh.cpp

