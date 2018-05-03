__kernel
void parseJson(__global char4 *restrict jsonstr, int linesize, __global char4 *restrict output) {
  //printf("[kernel]json content:\n%s\n",jsonstr);
  //printf("[kernel]linesize: %d\n",linesize);
  //printf("[kernel]group id: %ld\n",get_group_id(0));
  //printf("[kernel]local size: %ld\n",get_local_size(0));
  unsigned thread_id = get_global_id(0);
  // each kernel should process one json line
  // {"a":"1","b":"1"}
  int cursor = thread_id*18;
  __local char4 line_content[64];
  __local char4 vals[2];
  __local char4 temp;
  int fieldIndex = 0;
  #pragma unroll
  for (int i = cursor; i < (cursor + 18)/4; i++) {
      line_content[i] = jsonstr[i];
  }
  for (int j = 0; j < 18/4; j++) {
    temp = line_content[j];
    if (temp.x == ':') {
      vals[fieldIndex].x = temp.z;
    }
    if (temp.y == ':') {
      vals[fieldIndex].x = temp.w;
    }
    if (temp.z == ':' && (j+1)<18/4) {
      vals[fieldIndex].x = line_content[j+1].x;
    }
    if (temp.w == ':' && (j+1)<18/4) {
      vals[fieldIndex].x = line_content[j+1].y;
    }
    
    if (temp.x == ',' || temp.y == ',' || temp.z == ',' || temp.w == ',') {
      fieldIndex++;
    }
  
  }
  int currentUnsafeRowPosition = thread_id*linesize;
  int index = 0;
  output[currentUnsafeRowPosition/4].x |= 0;
  output[(currentUnsafeRowPosition + 8 + 8 * index)/4].x = 1;//string length
  output[(currentUnsafeRowPosition + 8 + 8 * index + 4)/4].x = 16;//string offset
  index++;
  output[(currentUnsafeRowPosition + 8 + 8 * index)/4].x = 1;
  output[(currentUnsafeRowPosition + 8 + 8 * index + 4)/4].x = 32;
  output[(currentUnsafeRowPosition + 16 + 8)/4] = vals[0];// set string value
  output[(currentUnsafeRowPosition + 16 + 8 + 8)/4] = vals[1];// set string value
}
