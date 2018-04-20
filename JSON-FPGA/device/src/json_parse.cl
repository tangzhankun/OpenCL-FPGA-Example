__kernel
void parseJson(__global char *restrict jsonstr, int linesize, __global char *restrict output) {
  //printf("[kernel]json content:\n%s\n",jsonstr);
  //printf("[kernel]linesize: %d\n",linesize);
  //printf("[kernel]group id: %ld\n",get_group_id(0));
  //printf("[kernel]local size: %ld\n",get_local_size(0));
  unsigned thread_id = get_local_id(0);
  // each kernel should process one json line
  // {"a":"1","b":"1"}
  int cursor = thread_id*linesize;
  __local char vals[2];
  int fieldIndex = 0;
  while(!(jsonstr[cursor] == '}')) {
    switch (jsonstr[cursor]) {
      case ':':
        ++cursor;//skip the ':'
        ++cursor;//skip the '"'
        vals[fieldIndex] = jsonstr[cursor];
        //printf("[kernel]Group #%u:,got value:%c\n", thread_id, vals[fieldIndex]);
        continue;
      case ',':
        fieldIndex++;
        ++cursor;
        continue;
      default:
        ++cursor;
    }
  }
  int currentUnsafeRowPosition = thread_id*linesize;
  int index = 0;
  output[currentUnsafeRowPosition] |= 0;
  output[currentUnsafeRowPosition + 8 + 8 * index] = 1;//string length
  output[currentUnsafeRowPosition + 8 + 8 * index + 4] = 16;//string offset
  index++;
  output[currentUnsafeRowPosition + 8 + 8 * index] = 1;
  output[currentUnsafeRowPosition + 8 + 8 * index + 4] = 32;
  output[currentUnsafeRowPosition + 16 + 8] = vals[0];// set string value
  output[currentUnsafeRowPosition + 16 + 8 + 8] = vals[1];// set string value
}
