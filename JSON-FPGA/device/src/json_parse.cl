__kernel
void parseJson(__global char *restrict jsonstr, int linesize, __global char *restrict output) {
  unsigned thread_id = get_global_id(0);

  if(thread_id == 2) {
    printf("Thread #%u: Hello JSON from Altera's OpenCL Compiler!\n", thread_id);
  } 
}
