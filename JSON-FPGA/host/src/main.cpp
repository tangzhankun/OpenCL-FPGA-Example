#include <stdio.h>
#include <stdlib.h>
#include <math.h>
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
void compute_reference();
void verify();
void cleanup();

// Entry point.
int main(int argc, char **argv) {
  char* jsonFilePath = NULL;
  Options options(argc, argv);
  if(options.has("jsonline")) {
    json_lines_count = options.get<unsigned>("jsonline");
  }
  if(options.has("jsonfile")) {
    jsonFilePath = options.get<char*>("jsonfile");
    FILE *fp = fopen(jsonFilePath, "r");
    if (!fp) {
      fprintf(stderr, "Cannot open file %s, Exiting...\n", jsonFilePath);
      exit(-1);
    }
    fseek(fp, 0L, SEEK_END);
    json_file_size = ftell(fp);
    rewind(fp);
    input_json_str.reset(json_file_size);
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
  printf("Json file path:\n  %s", jsonFilePath);

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
  status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
  checkError(status, "Failed to build program");

  // Create per-device objects.
  queue.reset(num_devices);
  kernel.reset(num_devices);

  // Command queue.
  queue[i] = clCreateCommandQueue(context, device[i], CL_QUEUE_PROFILING_ENABLE, &status);
  checkError(status, "Failed to create command queue");

  // Kernel.
  const char *kernel_name = "parseJson";
  kernel[i] = clCreateKernel(program, kernel_name, &status);
  checkError(status, "Failed to create kernel");

  // Input buffers.
  //We specifically assign this buffer to the first bank of global memory.
  input_json_buf = clCreateBuffer(context, CL_MEM_READ_ONLY | CL_CHANNEL_1_INTELFPGA,
      json_file_size * sizeof(char), NULL, &status);
  checkError(status, "Failed to create buffer for json string");

  // Output buffer. This is unsaferow buffer, We assign this buffer to the first bank of global memory,
  output_unsaferow_buf = clCreateBuffer(context, CL_MEM_WRITE_ONLY | CL_CHANNEL_1_INTELFPGA,
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
    const size_t global_work_size[2] = {json_lines_count * unsafe_row_size, 1};
    const size_t local_work_size[2]  = {unsafe_row_size, 1};
    printf("Launching for device %d (global size: %zd, %zd)\n", i, global_work_size[0], global_work_size[1]);

    status = clEnqueueNDRangeKernel(queue[i], kernel[i], 2, NULL,
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
    status = clEnqueueReadBuffer(queue[i], output_unsaferow_buf, CL_TRUE,
        0, json_lines_count * unsafe_row_size, output_unsafe_row_binary, 0, NULL, NULL);
    checkError(status, "Failed to read output matrix");
  }

  // Verify results.
  //compute_reference();
  //verify();
}

void compute_reference() {
  // Compute the reference output.
  printf("Computing reference output\n");
  ref_output.reset(C_height * C_width);

  for(unsigned y = 0, dev_index = 0; y < C_height; ++dev_index) {
    for(unsigned yy = 0; yy < rows_per_device[dev_index]; ++yy, ++y) {
      for(unsigned x = 0; x < C_width; ++x) {
        // Compute result for C(y, x)
        float sum = 0.0f;
        for(unsigned k = 0; k < A_width; ++k) {
          sum += input_a[dev_index][yy * A_width + k] * input_b[k * B_width + x];
        }
        ref_output[y * C_width + x] = sum;
      }
    }
  }
}

void verify() {
  printf("Verifying\n");

  // Compute the L^2-Norm of the difference between the output and reference
  // output matrices and compare it against the L^2-Norm of the reference.
  float diff = 0.0f;
  float ref = 0.0f;
  for(unsigned y = 0, dev_index = 0; y < C_height; ++dev_index) {
    for(unsigned yy = 0; yy < rows_per_device[dev_index]; ++yy, ++y) {
      for(unsigned x = 0; x < C_width; ++x) {
        const float o = output[dev_index][yy * C_width + x];
        const float r = ref_output[y * C_width + x];
        const float d = o - r;
        diff += d * d;
        ref += r * r;
      }
    }
  }

  const float diff_l2norm = sqrtf(diff);
  const float ref_l2norm = sqrtf(ref);
  const float error = diff_l2norm / ref_l2norm;
  const bool pass = error < 1e-6;
  printf("Verification: %s\n", pass ? "PASS" : "FAIL");
  if(!pass) {
    printf("Error (L^2-Norm): %0.3g\n", error);
  }
}

// Free the resources allocated during initialization
void cleanup() {
  for(unsigned i = 0; i < num_devices; ++i) {
    if(kernel && kernel[i]) {
      clReleaseKernel(kernel[i]);
    }
    if(queue && queue[i]) {
      clReleaseCommandQueue(queue[i]);
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
