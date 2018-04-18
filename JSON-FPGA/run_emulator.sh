#!/bin/bash
recompile=${1:-true}
if [ $recompile = true ]
then
  cd device/src
  aoc -v -march=emulator ./json_parse.cl && cp -f ./json_parse.aocx ../../bin/
  cd -
fi
lines=${2:-2}
env CL_CONTEXT_EMULATOR_DEVICE_ALTERA=1 ./bin/host -jsonline=$lines -jsonfile="./jsonfiles/$lines.json"
#for gdb debug do the following
#1. env CL_CONTEXT_EMULATOR_DEVICE_ALTERA=1 gdb ./bin/host 
#2. when get into gdb run below to set args
#   set args -jsonline=$lines -jsonfile="./jsonfiles/$lines.json"
#   break parseJson
