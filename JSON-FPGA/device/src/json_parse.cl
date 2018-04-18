__kernel
void parseJson(__global char *restrict jsonstr, int linesize, __global char *restrict output) {
  unsigned thread_id = get_group_id(0);
  printf("Group #%u:", thread_id);
  // each kernel should process one json line
  // {"a":"1","b":"1"}
  int cursor = thread_id*linesize;
  __local char vals[2];
  int fieldIndex = 0;
  while(!(jsonstr[cursor] == '}')) {
    switch (jsonstr[cursor]) {
      case ':':
        ++cursor;//skip the '"'
        vals[fieldIndex] = jsonstr[cursor];
        continue;
      case ',':
        fieldIndex++;
        ++cursor;
        continue;
      default:
        ++cursor;
    }
  }
  __local char unsafeRow[40];
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
