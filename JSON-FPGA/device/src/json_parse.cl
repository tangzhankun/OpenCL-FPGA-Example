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
  __local char vals[16][16];
  int fieldIndex = 1;
  //temp = jsonstr[id];
  
  int data_mode=0;
  int data_size[64]={0};
  int offset[3]={0,8,16};
  char temp[N]={0};
  //bool it[514]={0};
  int it_cal[10]={0};
  int end_cal[10]={0};
  //char data[64];
  int i = 0;
  //int index=0;
  int out_index=0;
  int currentUnsafeRowPosition=0;
  int m=0;
  int mm=0;
  int n=0;
  int nn=0;
for (int lo=0;lo<word_num;lo++){
  line_content = jsonstr[lo];
  
  #pragma unroll 16
  for (int j = 0 ;j<N;j++) {
	  temp[j]=line_content.x[j];
          
      if (temp[j]==':') { it_cal[m]=j+2;m++; }
      if (temp[j]==',') { end_cal[mm]=j-1;mm++;}
        
  }
  //#pragma unroll
  
  
  #pragma unroll
  for (int k=0; k<3;k++){
        i=0;
        n=it_cal[k];
        data_size[k]=end_cal[k]-n;
        for (int w=0;w<8;w++){
          vals[k][w]= temp[n+w];
        }
       
  }
  //memset(it,0,sizeof(it));
  m=0;
  mm=0;
  currentUnsafeRowPosition = lo*56;

  output[currentUnsafeRowPosition]=0;
  output[currentUnsafeRowPosition+1]=0;
  output[currentUnsafeRowPosition+2]=0;
  output[currentUnsafeRowPosition+3]=0;
  output[currentUnsafeRowPosition+4]=0;
  output[currentUnsafeRowPosition+5]=0;
  output[currentUnsafeRowPosition+6]=0;
  output[currentUnsafeRowPosition+7]=0;
  #pragma unroll
for (int index=0;index < 3;index++){ 
                         		
			output[currentUnsafeRowPosition + 8 + 8 * index ] = data_size[index];
		        output[currentUnsafeRowPosition + 8 + 8 * index + 1] = data_size[index]>>8;
                        output[currentUnsafeRowPosition + 8 + 8 * index + 2] = data_size[index]>>16;
                        output[currentUnsafeRowPosition + 8 + 8 * index + 3] = data_size[index]>>24;	
                        output[currentUnsafeRowPosition + 8 + 8 * index + 4] = (offset[index]+24);
                        output[currentUnsafeRowPosition + 8 + 8 * index + 5] = (offset[index]+24)>>8;
                        output[currentUnsafeRowPosition + 8 + 8 * index + 6] = (offset[index]+24)>>16;
                        output[currentUnsafeRowPosition + 8 + 8 * index + 7] = (offset[index]+24)>>24;			
                  
 
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 ] = vals[index][0];// set string value
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +1 ] = vals[index][1];
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +2] = vals[index][2];
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +3] = vals[index][3];
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +4] = vals[index][4];
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +5] = vals[index][5];
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +6] = vals[index][6];
			output[currentUnsafeRowPosition +8+ offset[index] + 3 * 8 +7] = vals[index][7];
			
		}  
  
 }
}

