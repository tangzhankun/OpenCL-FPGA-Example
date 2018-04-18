#!/bin/bash
lines=2
env CL_CONTEXT_EMULATOR_DEVICE_ALTERA=1 ./bin/host -jsonline=$lines -jsonfile="./bin/$lines.json"
