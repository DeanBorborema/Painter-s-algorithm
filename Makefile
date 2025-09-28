CC = gcc
CFLAGS = -Wall -O2
SRC = painter.c
BIN = painter

UNAME_S := $(shell uname -s)

# Detecção do SO
ifeq ($(UNAME_S),Linux)
    LIBS = -lGL -lGLU -lglut
endif