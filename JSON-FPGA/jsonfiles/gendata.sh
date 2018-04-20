#!/bin/bash
set -e
sample="{\"a\":\"1\",\"b\":\"2\"}"
echo "using sample json line: ${sample}"
#for lines in 10 100 1000 10000 100000 1000000 10000000
for lines in 100000000
do
  i=0
  filename="${lines}_gen.json"
  rm -f $filename
  echo "generating ${filename} ..."
  while [ $i -lt $lines ]
  do
    echo $sample >> $filename
    i=`expr $i + 1`
  done
done
