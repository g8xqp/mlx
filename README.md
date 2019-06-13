# mlx
Raspberry Pi melexis read experiments

Makefile

LIBS=-lm -lbcm2835
#mlxd: mlxd.c
#	cc -o mlxd mlxd.c $(LIBS)

mlxd: mlxd.cpp
	cc -o mlxd mlxd.cpp $(LIBS)
