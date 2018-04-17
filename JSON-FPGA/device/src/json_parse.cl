__kernel
void parseJson(__global char *restrict jsonstr, int linesize, __global char *restrict output) {
  unsigned thread_id = get_group_id(0);
  printf("Group #%u: Hello JSON from Altera's OpenCL Compiler!\n", thread_id);
  // each kernel should process one json line
  // {"a":"1","b":"1"}
  char *cursor = jsonstr + thread_id*linesize;
  //char fieldName1 = 'a';
  //char filedName2 = 'b';
  char *vals = new char[2];
  int fieldIndex = 0;
  while(!(*cursor == '}')) {
    switch (*cursor) {
      case ':':
        ++cursor;//skip the '"'
        vals[fieldIndex] = *cursor;
        continue;
      case ',':
        fieldIndex++;
        ++cursor;
        continue;
      default:
        ++cursor;
    }
  }
  char *unsafeRow = new char[40];
  memset(unsafeRow, 0, 40);
  int currentUnsafeRowPosition = thread_id*linesize;
  int index = 0;
  output[currentUnsafeRowPosition] |= 0;
  *(uint32_t*)( output + currentUnsafeRowPosition + 8 + 8 * index) = 1;//string length
  *(uint32_t*)( output + currentUnsafeRowPosition + 8 + 8 * index + 4) = 16;//string offset
  index++;
  *(uint32_t*)( output + currentUnsafeRowPosition + 8 + 8 * index) = 1;
  *(uint32_t*)( output + currentUnsafeRowPosition + 8 + 8 * index + 4) = 32;
  memcpy(output + currentUnsafeRowPosition + 16 + 8, vals[0], 1);// set string value
  memcpy(output + currentUnsafeRowPosition + 16 + 8 + 8, vals[1], 1);// set string value
}
