#define N 512
typedef struct{
	
	char x[N];
	
} charN;

__kernel
void parseJson(__global const charN *restrict jsonstr, int word_num, __global char *restrict output) {
  //printf("[kernel]json content:\n%s\n",jsonstr);
  //printf("[kernel]linesize: %d\n",linesize);
  //printf("[kernel]group id: %ld\n",get_group_id(0));
  //printf("[kernel]local size: %ld\n",get_local_size(0));
  //unsigned thread_id = get_local_id(0);
  // each kernel should process one json line
  // {"a":"1","b":"1"}
  //int cursor = thread_id*linesize;
  __local charN line_content;
  __local char vals[16][8];
  int fieldIndex = 1;
  //temp = jsonstr[id];
  
  int data_mode=0;
  int data_size[64]={0};
  int offset[64]={0};
  int temp[N]={0};
  //char data[64];
  int i = 0;
  int index=0;
  int out_index=0;
  for (int lo=0;lo<word_num;lo++){
  line_content = jsonstr[lo];
  #pragma unroll 16
  for (int j = 0 ;j<N;j++) {
	  temp[j]=line_content.x[j];fieldIndex=1;i=0;offset[0]=0;
      if (temp[j]==':') { ++data_mode; }
      else if (temp[j]==','){ 
	    offset[fieldIndex] =offset[fieldIndex-1] + 8*((data_size[fieldIndex-1]-1)/8)+8;
	    
		i = 0;
        fieldIndex++; }
	  else { 
	    if (data_mode==1){ data_mode=2; }
		else if (data_mode==2 && temp[j] != '"'){ vals[fieldIndex-1][i]= temp[j] ; i++; }
		else if (data_mode==2 && temp[j] == '"'){data_mode=0;data_size[fieldIndex-1]=i;} 
		
		}
        



  }
  
  
  
  int currentUnsafeRowPosition = lo*1024;
  
for (index=0;index < fieldIndex;index++){ 		
			output[currentUnsafeRowPosition + 8 + 8 * index] = offset[index]>>24;//string length
			output[currentUnsafeRowPosition + 8 + 8 * index + 1] = offset[index]>>16;
			output[currentUnsafeRowPosition + 8 + 8 * index + 2] = offset[index]>>8;
			output[currentUnsafeRowPosition + 8 + 8 * index + 3] = offset[index];
			output[currentUnsafeRowPosition + 8 + 8 * index + 4] = data_size[index]>>24;
			output[currentUnsafeRowPosition + 8 + 8 * index + 5] = data_size[index]>>16;
			output[currentUnsafeRowPosition + 8 + 8 * index + 6] = data_size[index]>>8;
			output[currentUnsafeRowPosition + 8 + 8 * index + 7] = data_size[index];
			
			
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 ] = vals[index][0];// set string value
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +1 ] = vals[index][1];
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +2] = vals[index][2];
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +3] = vals[index][3];
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +4] = vals[index][4];
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +5] = vals[index][5];
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +6] = vals[index][6];
			output[currentUnsafeRowPosition +8+ offset[index] + fieldIndex * 8 +7] = vals[index][7];
			
		}  
  }
 
}

