#include "TestSpecsDeclarations.hpp"

#include "../../src/DataPipeline.hpp"
#include "../../src/LayerData.hpp"

using namespace cnn_sr;

///
/// NOTE: generating expected output is just checking if all inputs, deltas
/// are read correctly. Change following line:
///   'scratch_w[idx] = delta * layer_input[prev_layer_idx + k];'
/// to:
///   'scratch_w[idx] = layer_input[prev_layer_idx + k];'
///   OR
///   'scratch_w[idx] = delta;'
/// Also just use BackpropagationTest_script.py to calc the values.
///
/// NOTE: data set 1 checks if kernel works, data set 2 checks if it does not
/// crash when used with big number of data
///

namespace test {
namespace specs {

///
/// PIMPL
///
struct BackpropagationTestImpl {
// INPUT_SIZE = input_dim*n(l-1)
#define INPUT_SIZE 50
  float input[INPUT_SIZE] = {-0.083, -0.064,  //
                             0.075,  -0.055,  //
                             -0.058, -0.138,  //
                             -0.068, -0.144,  //
                             -0.013, 0.176,   //

                             0.169,  0.049,   //
                             0.181,  -0.051,  //
                             0.136,  -0.062,  //
                             -0.165, -0.176,  //
                             0.159,  -0.060,  //

                             -0.112, 0.228,   //
                             0.003,  -0.138,  //
                             -0.123, -0.027,  //
                             -0.102, -0.061,  //
                             0.242,  -0.069,  //

                             0.406,  0.419,   //
                             -0.442, 0.685,   //
                             -0.627, -0.489,  //
                             0.376,  0.563,   //
                             0.680,  -0.371,  //

                             0.121,  -0.075,   //
                             -0.103, 0.031,    //
                             0.106,  0.033,    //
                             -0.036, -0.052,   //
                             0.052,  -0.035};  //

// DELTAS_SIZE = output_dim * n(l)
#define DELTAS_SIZE 27
  float deltas[DELTAS_SIZE] = {0.122f, 0.083f, 0.064f,  // row 1, col 1
                               0.057f, 0.075f, 0.055f,  // row 1, col 2
                               0.025f, 0.058f, 0.138f,  // row 1, col 3

                               0.170f, 0.068f, 0.144f,  // row 2, col 1
                               0.121f, 0.013f, 0.176f,  // row 2, col 2
                               0.065f, 0.169f, 0.049f,  // row 2, col 3

                               0.003f, 0.181f, 0.051f,   // row 3, col 1
                               0.021f, 0.136f, 0.062f,   // row 3, col 2
                               0.066f, 0.165f, 0.176f};  // row 3, col 3
#define WEIGHTS_SIZE 54
  /* clang-format off */
  const std::vector<float> expected_weights = {
     0.0438, -0.008,   0.0265,           -0.0203, -0.0072, -0.0328,
     0.0313, -0.049,   0.00872,          -0.0508, -0.096,  -0.0773,
     0.0157,  0.027,   0.0191,           -0.0623, -0.0533, -0.0526,
    -0.0418, -0.083,  -0.0991,            0.0052,  0.0941, -0.0232,
     0.0150, -0.106,  -0.0252,           -0.0159,  0.111,   0.0451,
     0.0445,  0.089,   0.109,            -0.0497, -0.109,  -0.0953,
    -0.0366, -0.075,  -0.0556,            0.144,  -0.0422,  0.164,
    -0.1360,  0.00027,-0.181,             0.0713,  0.12,    0.0159,
    -0.0287,  0.0962,  0.0414,           -0.0509, -0.106,  -0.0118
  };
  /* clang-format on */

  const std::vector<float> expected_bias = {0.650f, 0.948f, 0.915f};
};

///
/// BackpropagationTest
///

TEST_SPEC_PIMPL(BackpropagationTest)

void BackpropagationTest::init() {}

std::string BackpropagationTest::name(size_t data_set_id) {
  return data_set_id == 0 ?                              //
             "Backpropagation test - value correctness"  //
                          : "Backpropagation test - big data";
}

size_t BackpropagationTest::data_set_count() { return 2; }

void execute(DataPipeline *pipeline, LayerData &data,     //
             cnn_sr::CnnLayerGpuAllocationPool &gpu_buf,  //
             float *deltas, float *input, size_t input_w, size_t input_h) {
  auto context = pipeline->context();
  size_t output_dim[2];
  data.get_output_dimensions(output_dim, input_w, input_h);
  size_t deltas_size =
             output_dim[0] * output_dim[1] * data.current_filter_count,
         input_size = data.input_size(input_w, input_h);

  // gpu memory alloc
  opencl::MemoryHandle gpu_buf_layer_input;
  /* clang-format off */
  gpu_buf.deltas = context->allocate(CL_MEM_READ_ONLY, sizeof(cl_float) * deltas_size);
  gpu_buf_layer_input = context->allocate(CL_MEM_READ_ONLY, sizeof(cl_float) * input_size);
  /* clang-format on */
  context->write_buffer(gpu_buf.deltas, (void *)deltas, true);
  context->write_buffer(gpu_buf_layer_input, (void *)input, true);

  // run
  pipeline->backpropagate2(data, gpu_buf_layer_input, gpu_buf,  //
                           output_dim[0], output_dim[1]);
}

bool BackpropagationTest::operator()(size_t data_set_id,
                                     cnn_sr::DataPipeline *const pipeline) {
  assert_not_null(pipeline);
  auto context = pipeline->context();
  cnn_sr::CnnLayerGpuAllocationPool gpu_buf;

  if (data_set_id == 0) {
    // data for layer, needs filled up weights&bias to pass validation
    LayerData data(2, 3, 3);  // n_prev_filter_cnt/FILTER_CNT/f_spatial_size
    float w[WEIGHTS_SIZE], bias[10];
    data.set_bias(bias);
    data.set_weights(w);
    execute(pipeline, data, gpu_buf, _impl->deltas, _impl->input, 5, 5);
    // check results
    std::cout << "checking weights" << std::endl;
    assert_equals(pipeline, _impl->expected_weights, gpu_buf.grad_w);
    std::cout << "checking bias" << std::endl;
    assert_equals(pipeline, _impl->expected_bias, gpu_buf.grad_b);
  } else {
    LayerData data(32, 16, 3);
    float w[4608], bias[16];
    data.set_bias(bias);
    data.set_weights(w);
    // (we dont care about values and sizes must only be at least enough)
    const size_t input_w = 1024, input_h = 1024;
    std::vector<float> deltas(input_w * input_h * data.current_filter_count),
        input(input_w * input_h * data.n_prev_filter_cnt);
    execute(pipeline, data, gpu_buf,  //
            &deltas[0], &input[0], input_w, input_h);
    context->block();
    // didn't crash? then it's ok
  }

  return true;
}

//
//
}  // namespace specs
}  // namespace test