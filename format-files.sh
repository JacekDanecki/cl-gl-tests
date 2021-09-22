#!/bin/bash

for i in *.c *.cpp
do
	clang-format -i $i
done

