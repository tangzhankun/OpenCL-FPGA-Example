#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string>
#include "CL/opencl.h"
#include "AOCLUtils/aocl_utils.h"

using namespace aocl_utils;

// OpenCL runtime configuration
cl_platform_id platform = NULL;
unsigned num_devices = 0;
scoped_array<cl_device_id> device; // num_devices elements
cl_context context = NULL;
scoped_array<cl_command_queue> queue; // num_devices elements
cl_program program = NULL;
scoped_array<cl_kernel> kernel; // num_devices elements

cl_mem input_json_buf;
cl_mem output_unsaferow_buf;

unsigned json_lines_count = 10;
// {"a":"a","b":"b"}, maximum value size to 8 bytes
unsigned json_line_column_count = 2;
unsigned unsafe_row_size = 8 + (8 + 8) * json_line_column_count;
unsigned json_file_size = 0;
unsigned json_line_size = 0;
scoped_aligned_ptr<char> input_json_str;
scoped_aligned_ptr<char> output_unsafe_row_binary;
// Function prototypes
float rand_float();
bool init_opencl();
void run();
void cleanup();

// Entry point.
int main(int argc, char **argv) {
  std::string jsonFilePath;
  Options options(argc, argv);
  if(options.has("jsonline")) {
    json_lines_count = options.get<unsigned>("jsonline");
    printf("json lines count: %d.\n", json_lines_count);
  }
  if(options.has("jsonfile")) {
    jsonFilePath = options.get<std::string>("jsonfile");
    printf("json file path: %s. \n", jsonFilePath.c_str());
    FILE *fp = fopen(jsonFilePath.c_str(), "r");
    if (!fp) {
      fprintf(stderr, "Cannot open file %s, Exiting...\n", jsonFilePath.c_str());
      exit(-1);
    }
    fseek(fp, 0L, SEEK_END);
    json_file_size = ftell(fp);
    rewind(fp);
    input_json_str.reset(json_file_size);
    printf("json file size: %d. \n", json_file_size);
    unsigned read_size = fread(input_json_str.get(), sizeof(char), json_file_size, fp); 
    input_json_str[read_size] = '\0';
    if (json_file_size != read_size) {
      // Something went wrong, throw away the memory and set
      // the buffer to NULL
      fprintf(stderr, "file size doesn't match read size %d vs %d, Exiting...\n", json_file_size, read_size);
      exit(-2);
    }
    fclose(fp);
    json_line_size = json_file_size/json_lines_count;
  }
  // Initialize OpenCL.
  if(!init_opencl()) {
    return -1;
  }

  // Run the kernel.
  run();

  // Free the resources allocated
  cleanup();
  return 0;
}

/////// HELPER FUNCTIONS ///////

// Randomly generate a floating-point number between -10 and 10.
float rand_float() {
  return float(rand()) / float(RAND_MAX) * 20.0f - 10.0f;
}

// Initializes the OpenCL objects.
bool init_opencl() {
  cl_int status;

  printf("Initializing OpenCL\n");

  if(!setCwdToExeDir()) {
    return false;
  }

  // Get the OpenCL platform.
  platform = findPlatform("Intel(R) FPGA SDK for OpenCL(TM)");
  if(platform == NULL) {
    printf("ERROR: Unable to find Intel(R) FPGA OpenCL platform.\n");
    return false;
  }

  // Query the available OpenCL device.
  device.reset(getDevices(platform, CL_DEVICE_TYPE_ALL, &num_devices));
  printf("Platform: %s\n", getPlatformName(platform).c_str());
  printf("Using %d device(s)\n", num_devices);
  for(unsigned i = 0; i < num_devices; ++i) {
    printf("  %s\n", getDeviceName(device[i]).c_str());
  }

  // Create the context.
  context = clCreateContext(NULL, num_devices, device, &oclContextCallback, NULL, &status);
  checkError(status, "Failed to create context");

  // Create the program for all device. Use the first device as the
  // representative device (assuming all device are of the same type).
  std::string binary_file = getBoardBinaryFile("json_parse", device[0]);
  printf("Using AOCX: %s\n", binary_file.c_str());
  program = createProgramFromBinary(context, binary_file.c_str(), device, num_devices);

  // Build the program that was just created.
  const unsigned MAX_INFO_SIZE = 0x10000;
  char info_buf[MAX_INFO_SIZE];
  status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
  if (err != CL_SUCCESS)
  {
    fprintf(stderr, "clBuild failed:%d\n", err);
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, MAX_INFO_SIZE, info_buf, NULL);
    fprintf(stderr, "\n%s\n", info_buf);
    exit(1);
  }
  else{
    clGetProgramBuildInfo(program, device_id, CL_PROGRAM_BUILD_LOG, MAX_INFO_SIZE, info_buf, NULL);
    printf("Kernel Build Success\n%s\n", info_buf);
  }
  checkError(status, "Failed to build program");

  // Create per-device objects.
  queue.reset(num_devices);
  kernel.reset(num_devices);

  // Command queue.
  queue[0] = clCreateCommandQueue(context, device[0], CL_QUEUE_PROFILING_ENABLE, &status);
  checkError(status, "Failed to create command queue");

  // Kernel.
  const char *kernel_name = "parseJson";
  kernel[0] = clCreateKernel(program, kernel_name, &status);
  checkError(status, "Failed to create kernel");

  // Input buffers.
  //We specifically assign this buffer to the first bank of global memory.
  input_json_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_MEM_BANK_1_ALTERA,
      json_file_size * sizeof(char), NULL, &status);
  checkError(status, "Failed to create buffer for json string");

  // Output buffer. This is unsaferow buffer, We assign this buffer to the first bank of global memory,
  output_unsaferow_buf = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_MEM_BANK_2_ALTERA,
      json_lines_count*unsafe_row_size, NULL, &status);
  checkError(status, "Failed to create buffer for unsaferow output");

  return true;
}


void run() {
  cl_int status;

  // Transfer inputs to each device. Each of the host buffers supplied to
  // clEnqueueWriteBuffer here is already aligned to ensure that DMA is used
  // for the host-to-device transfer.
  status = clEnqueueWriteBuffer(queue[0], input_json_buf, CL_FALSE,
      0, json_file_size, input_json_str, 0, NULL, NULL);
  checkError(status, "Failed to transfer json_str to FPGA");

  // Wait for all queues to finish.
  clFinish(queue[0]);

  // Launch kernels.
  // This is the portion of time that we'll be measuring for throughput
  // benchmarking.
  scoped_array<cl_event> kernel_event(num_devices);

  const double start_time = getCurrentTimestamp();
  for(unsigned i = 0; i < num_devices; ++i) {
    // Set kernel arguments.
    unsigned argi = 0;

    status = clSetKernelArg(kernel[i], argi++, sizeof(cl_mem), &input_json_buf);
    checkError(status, "Failed to set argument(input json buffer) %d", argi - 1);

    status = clSetKernelArg(kernel[i], argi++, sizeof(json_line_size), &json_line_size);
    checkError(status, "Failed to set argument(json line size) %d", argi - 1);

    status = clSetKernelArg(kernel[i], argi++, sizeof(cl_mem), &output_unsaferow_buf);
    checkError(status, "Failed to set argument(output unsafe row buffer) %d", argi - 1);

    // Enqueue kernel.
    // Use a global work size corresponding to the size of the output matrix.
    // Each work-item computes the result for one value of the output matrix,
    // so the global work size has the same dimensions as the output matrix.
    //
    // The local work size is one block, so BLOCK_SIZE x BLOCK_SIZE.
    //
    // Events are used to ensure that the kernel is not launched until
    // the writes to the input buffers have completed.
    const size_t global_work_size[1] = {json_lines_count};
    const size_t local_work_size[1]  = {1};
    printf("Launching for device %d (global size: %zd, %zd)\n", i, global_work_size[0], global_work_size[1]);

    status = clEnqueueNDRangeKernel(queue[0], kernel[0], 1, NULL,
        global_work_size, local_work_size, 0, NULL, &kernel_event[i]);
    checkError(status, "Failed to launch kernel");
  }

  // Wait for all kernels to finish.
  clWaitForEvents(num_devices, kernel_event);

  const double end_time = getCurrentTimestamp();
  const double total_time = end_time - start_time;

  // Wall-clock time taken.
  printf("\nTime: %0.3f ms\n", total_time * 1e3);

  // Get kernel times using the OpenCL event profiling API.
  for(unsigned i = 0; i < num_devices; ++i) {
    cl_ulong time_ns = getStartEndTime(kernel_event[i]);
    printf("Kernel time (device %d): %0.3f ms\n", i, double(time_ns) * 1e-6);
  }

  // Release kernel events.
  for(unsigned i = 0; i < num_devices; ++i) {
    clReleaseEvent(kernel_event[i]);
  }

  // Read the result.
  for(unsigned i = 0; i < num_devices; ++i) {
    status = clEnqueueReadBuffer(queue[0], output_unsaferow_buf, CL_TRUE,
        0, json_lines_count * unsafe_row_size, output_unsafe_row_binary, 0, NULL, NULL);
    checkError(status, "Failed to read output matrix");
  }

}

// Free the resources allocated during initialization
void cleanup() {
  for(unsigned i = 0; i < num_devices; ++i) {
    if(kernel && kernel[i]) {
      clReleaseKernel(kernel[i]);
    }
    if(queue && queue[0]) {
      clReleaseCommandQueue(queue[0]);
    }
    if (input_json_buf) {
      clReleaseMemObject(input_json_buf);
    }
    if (output_unsaferow_buf) {
      clReleaseMemObject(output_unsaferow_buf);
    }
  }

  if(program) {
    clReleaseProgram(program);
  }
  if(context) {
    clReleaseContext(context);
  }
}

