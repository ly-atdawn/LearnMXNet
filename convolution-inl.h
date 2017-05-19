/*!
 * Copyright (c) 2015 by Contributors
 * \file convolution-inl.h
 * \brief
 * \author
*/
#ifndef MXNET_OPERATOR_CONVOLUTION_INL_H_
#define MXNET_OPERATOR_CONVOLUTION_INL_H_ // �� 

#include <mxnet/io.h> // include/mxnet��, �����������������ݽṹ�����ݵ�����. 
#include <mxnet/base.h> // include/mxnet��, ����mxnet��������Ϣ�Լ��������ݽṹ.
/*���Ƿ�֧��OpenCV, CUDA, CUDNN, VS�Լ�������mxnet�����ռ��µı����Ķ���, ��cpu, gpu, index_t�Լ�Save, Load��. 
*/ 
#include <mxnet/ndarray.h> // include/mxnet��, 
#include <mxnet/operator.h> // ��include/mxnet��, �����������(operator), ����������, ������. ��OP��Prop�ĺ�����������. 
#include <dmlc/logging.h> // mxnet����־ͷ�ļ�. ��dmlc-core/include/dmlc��,
#include <dmlc/optional.h> // ��dmlc-core/include/dmlc��, ������class optional.  
#include <algorithm> // ��׼C++�㷨��. 
#include <map> // C++ map����ͷ�ļ�. 
#include <vector> // ��������. 
#include <string> // �ַ���. 
#include <utility> // utilityͷ�ļ��������صĹ�ϵ�����, �򻯹�ϵ�������д��, ��������pair����,
// pair������һ��ģ������, ���Դ洢һ��ֵ.
#include "./operator_common.h" // src/operator��, mxnet�Ĳ�һЩ���õ�����.

#include<iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

using namespace std;

namespace mxnet {
namespace op {

namespace conv {
enum ConvolutionOpInputs {kData, kWeight, kBias}; // ��������������, ����ǰһ����������data, �����Ȩ��wmat��ƫ��. 
enum ConvolutionOpOutputs {kOut}; // �������������. 
enum ConvolutionOpResource {kTempSpace}; // ��������Դ����, ����һ����ʱ�ռ�, ����ռ�����������С��. 
/*
��Щ������Ҫ������ڴ���Ϊ�����ռ���м���, ����˵cudnnConvolutionForward. ���������, 
ϵͳ��ÿ��Զ��ⲿ���ڴ���й���, ����ϵͳ������һЩ�Ż�, ����˵�ڴ���ظ�����.
struct ResourceRequest {
  enum Type {
    kRandom,  // get an mshadow::Random<xpu> object
    kTempSpace,  // request temporay space
  };
  Type type;
};
*/
enum ConvolutionOpCudnnTune {kOff, kLimited, kFastest}; // ����GPU��ɾ������ʱ, ����������. û��ʹ��, ���convolution-inl.h
// ��cpu���. 
}

/*
��������������ͼ�Ĵ�С, �������ͼ�ߴ� = [(��������ͼ�ߴ� + 2 * pad - ����˳ߴ�)/����](����ȡ��) + 1.

Matlab�е�Ӧ�ú���-conv2:��ά���, һά��Ӧconv. 0.8�汾��convolution-inl.hֻ����2D���, 0.9�汾��
convolution-inl.h������3D���, ���ҽ�2D�����3D��������һ����.

�����������, ���MATLAB�ľ������conv��conv2�����Ƶ�, conv��������ľ��, ��һά���; conv2����ά���, ��������. 
w = conv(A, B,'shape')�ؾ����һ����. ��һ������shape����ָ��:
full ����ȫ�����ֵ(ȱʡ). kFull,  Ϊ1.
same ���ؾ�������Ĳ���, ��A����ͬ�Ĵ�С. 
valid �����ؾ���е���Щ�������û�в���Ĳ���, ������.. kValid, Ϊ0. 

����ļ��㲽��:
(1)��������Լ��ĺ���Ԫ��˳ʱ����ת180��, ��˳ʱ����ת180, ������ת��! 
(2)�ƶ�����˵�����Ԫ�أ�ʹ��λ������ͼ����������ص����Ϸ�. ����shape�Ĳ�ͬ��ѡ��ͬ�ľ����ʽ.
(3)����ת��ľ�����У�������ͼ�������ֵ��ΪȨ�����. 
(4)������������ĺ���Ϊ���������ض�Ӧ���������.

A=[1 2 3;4 5 6;7 8 9];
B=[1 2;3 4];

conv2(A, B, 'full'), ����ȫ���, ���ȶ�B����180��˳ʱ����ת, Ȼ���A���в�0����. �����ʱ����A����Χ���в���. 
��������� = 2 *(Nh - 1), ��������� = 2 * (Nw - 1). �����B��СΪNh * Nw. 
����ʱ��AΪ:   ����ʱBΪ: . Ȼ���ٶ�ӦԪ���������Ӽ��Ǿ����Ľ��. 
0 0 0 0 0        4 3       
0 1 2 3 0        2 1 
0 4 5 6 0
0 7 8 9 0 
0 0 0 0 0

conv2(A, B, 'valid'), ���ؾ��������û�в��㲿�ֵļ�����, ����CNN�еľ������, �þ���˶���A����. ������. 

conv2(A,B, 'same'), ���غ�Aͬ����С�ľ����Ľ��, ����full�������Եõ�same�����Ľ��. �������� 
*/

struct ConvolutionParam : public dmlc::Parameter<ConvolutionParam> { // �����Ĳ����ṹConvolutionParam. 
  TShape kernel; // �����, TShape: һ��shape��, ������Ա�ʾһ��Tensor����״. ����TShape����ʾTensor�Ĵ�С. 
  /*������Ƕ�ά��(h, w)����ά��(d, h, w). ��ʵ�ֶ�ά�������ά���, ��Ӧ��ͼ���п����ǻҶ�ͼ��(2ά����), RGBͼ��(3ά����).
  ��������������2ά�Ļ���3ά���й�. (channel K��һ����ͼ���ͨ������, ����Ϊǰһ������ͼ������.) 
  TShape����: http://mxnet.io/doxygen/classmxnet_1_1TShape.html*/  
  TShape stride; // ����˻�������, ��ά��(h, w)����ά��(d, h, w).  
  TShape dilate;
  /*
  ����������ǽ���������ŵ����ͳ߶�Լ���ĳ߶���, ����ԭ�����û��ռ�õ����������. ����:
  1 2 3      1 0 2 0 3
  4 5 6 ---->0 0 0 0 0      
  7 8 9      4 0 5 0 6
             0 0 0 0 0
             7 0 8 0 9
  �������3*3���͵���5*5. ���ͺ�ľ�����������һЩ0. ����ϵ�����������͵Ĺ�ϵ, ���Ȼص���������͹�ʽ:
  ���͵ľ���˳ߴ� = ����ϵ�� * (ԭʼ����˳ߴ� - 1) + 1.
  ���ھ���Ĳ�������, ����˳ߴ�������, ����˵�����ϵ���̻��˾���˸ߺͿ�������ű���, 
  ����ϵ����֤�����͵ľ���˳ߴ�Ϊ����. 
  */
  TShape pad; // ԭʼ���ݵĸ߶Ȼ��ȷ����ϲ���0ֵ��Ȧ��, ��ά��(h, w)����ά��(d, h, w).
  uint32_t num_filter; // ����˵ĸ���. ���������һ���ж��ٸ�����ͼ. 
  /*
  uint8_t��uint16_t��uint32_t: ʹ��typedef��������ı���.
  typedef unsigned char uint8_t; 1�ֽ� 
  typedef unsigned int uint16_t; 2�ֽ� 
  typedef unsigned long uint32_t; 4�ֽ� 
  typedef unsigned long long uint64_t; 8�ֽ� 
  */
  uint32_t num_group;
  /*
  �����������и��num_group��partitions, Ȼ����ÿ��partition��ʹ�þ������, �ٽ�����Ľ����������. �е㲢�е���˼. 
  */
  uint64_t workspace; // �������������ʱ�����ռ�, ��MB����. 
  bool no_bias; // ������Ƿ�ʹ��ƫ��, Ĭ��ʹ��ƫ��. 
  dmlc::optional<int> cudnn_tune;
  /*
  template<typename T>
  class dmlc::optional< T >
  C++17�Ŀ�ѡ��, ʵ��������cudnn_tune, cudnn_tune��������: 
  cudnn_tune�����Ƿ�ͨ������������ѡ�����㷨, ����Ҫ��������, �������ʱ����ܻ�䳤, ���ǲ������ٶ�ȴ����. 
  cudnn_tune��������ѡ����:
  off: ������
  limited_workspace: �ڲ��������������Ƶ������, ���в�������, ��ѡ�������㷨.
  fastest: ���Թ���������, ѡ�������㷨. 
  */
  bool cudnn_off; // �þ�����Ƿ�ʹ��cudnn, Ĭ��ʹ�ã� 
  dmlc::optional<int> layout;
  /*
  layout��cudnn_tune����һ��. ��������, �����Ȩ�ص�layout(����).  
  */ 
  DMLC_DECLARE_PARAMETER(ConvolutionParam) { // #define DMLC_DECLARE_PARAMETER(PType), ʹ�ú������������Ĳ���. 
    
	/*
	��DMLC_DECLARE_PARAMETER, DMLC_DECLARE_FIELD�Ķ����Լ�����describe, set_default, set_range, add_enum����
	#include <dmlc/parameter.h>��. 
	*/
	
    DMLC_DECLARE_FIELD(kernel).describe("convolution kernel size: (h, w) or (d, h, w)");
    /*#define DMLC_DECLARE_FIELD(FieldName)ʹ�ú�#define DMLC_DECLARE_FIELD(FieldName)���Ծ����Ĳ�����������, ���ú���
	describe(const std::string &description)�Բ���FieldName��������. ����˵Ĵ�С, ��ά�������ά���.*/
    DMLC_DECLARE_FIELD(stride).set_default(TShape())
    .describe("convolution stride: (h, w) or (d, h, w)");
    /*
	����˻�������, ��ά��������ά����. ���ú���set_default(const DType &default_value)���ò���FieldName��Ĭ��ֵ, Ĭ��ֵ��
	TShape(), ��mxnet::TShape::TShape( ), ����TShape��ĬȻ���캯��, NONE; ���ú���describe(const std::string &description)
	�Բ���FieldName��������.  
	*/
	
	/*
	TShape a = TShape();
	cout<<"TShape()[0]: "<<a[0]<<endl; 0
	cout<<"TShape()[1]: "<<a[1]<<endl; 0
	cout<<"TShape()[2]: "<<a[2]<<endl; 0
	
	��TShape��ĬȻ���캯��TShape()����� shape����, ��ֵ����0. 
	
	����stride��Ĭ��ֵ��(1, 1), ��������(0, 0). dliate��pad��Ĭ��ֵ��ֵ1.
	conv1 = mx.symbol.Convolution(data=data, kernel=(5,5), stride=(2,2), num_filter=20), ָ��stride=(2, 2).
	conv1 = mx.symbol.Convolution(data=data, kernel=(5,5), num_filter=20), ��ָ��stride=(1, 1). 
	*/
	
    DMLC_DECLARE_FIELD(dilate).set_default(TShape())
    .describe("convolution dilate: (h, w) or (d, h, w)"); // ����ϵ��.  
    DMLC_DECLARE_FIELD(pad).set_default(TShape())
    .describe("pad for convolution: (h, w) or (d, h, w)"); // ʼ���ݵĸ߶Ȼ��ȷ����ϲ���0ֵ��Ȧ��.
    DMLC_DECLARE_FIELD(num_filter).set_range(1, 100000)
    .describe("convolution filter(channel) number");
    /*
	����˵ĸ���. ���ú���set_range(DType begin, DType end)����num_filter�ķ�Χ, 1-100000. 
	*/
    DMLC_DECLARE_FIELD(num_group).set_default(1)
    .describe("Number of group partitions. Equivalent to slicing input into num_group\n    "
              "partitions, apply convolution on each, then concatenate the results"); // �����������и��num_group��partitions
    // ĬȻ���и�����. 
    DMLC_DECLARE_FIELD(workspace).set_default(1024).set_range(0, 8192)
    .describe("Maximum tmp workspace allowed for convolution (MB).");
    /*�������������ʱ�����ռ�, Ĭ����1024M, ��Χ��0M - 8192M.*/
    DMLC_DECLARE_FIELD(no_bias).set_default(false)
    .describe("Whether to disable bias parameter."); // ��������Ƿ�ʹ��ƫ��. 
    DMLC_DECLARE_FIELD(cudnn_tune)
    .add_enum("off", conv::kOff)
    .add_enum("limited_workspace", conv::kLimited)
    .add_enum("fastest", conv::kFastest)
    .set_default(dmlc::optional<int>())
    .describe("Whether to pick convolution algo by running performance test.\n    "
              "Leads to higher startup time but may give faster speed. Options are:\n    "
              "\'off\': no tuning\n    "
              "\'limited_workspace\': run test and pick the fastest algorithm "
              "that doesn't exceed workspace limit.\n    "
              "\'fastest\': pick the fastest algorithm and ignore workspace limit.\n    "
              "If set to None (default), behavior is determined by environment\n    "
              "variable MXNET_CUDNN_AUTOTUNE_DEFAULT: 0 for off,\n    "
              "1 for limited workspace (default), 2 for fastest.");
    /*
	���ÿ�ѡ��cudnn_tune��ѡ�����㷨. ���ú���add_enum(const std::string &key, int value)��Ϊcudnn_tune��Ӳ���. ���һ��
	��ֵkey��value, conv::kOffΪ0, conv::kLimitedΪ1, conv::kFastestΪ2. Ĭ��ֵ��dmlc::optional<int>(), ����optional��Ĭ��
	���캯��, û��ֵΪ��.   
	*/
    DMLC_DECLARE_FIELD(cudnn_off).set_default(false)
    .describe("Turn off cudnn for this layer."); // �Ƿ�ʹ��cudnn, Ĭ����false. ��ʹ��. 
    DMLC_DECLARE_FIELD(layout)
    .add_enum("NCHW", mshadow::kNCHW)
    .add_enum("NHWC", mshadow::kNHWC)
    .add_enum("NCDHW", mshadow::kNCDHW)
    .add_enum("NDHWC", mshadow::kNDHWC)
    .set_default(dmlc::optional<int>())
    .describe("Set layout for input, output and weight. Empty for\n    "
              "default layout: NCHW for 2d and NCDHW for 3d.");
    /*
	��������, �����Ȩ�ص�layout(����), ���屾������data, �����Ȩ�صĲ���, ��Ϊ������������4D������, ���Ҫ�趨���ڴ�洢,
	������Ϊ��������Ϊ��. Blob memory is row-major in layout, so the last / rightmost dimension changes fastest. 
	Ĭ��2dʹ��NCHW, 3dʹ��NCDHW.   kNCHW = 0, kNHWC, kCHWN, kNCDHW = 1 << 5, kNDHWC,
    kCDHWN��ö�ٱ���. ������mshadow/mshadow/base.h��.  
	*/
  }
};

template<typename xpu, typename DType>
class ConvolutionOp : public Operator { // ���������, ǰ������ͷ������. 
 public:
  explicit ConvolutionOp(ConvolutionParam p) {
    // explicit�ؼ���ֻ����������ֻ��һ���������๹�캯��, ���������Ǳ����ù��캯������ʾ��, ������ʽ��. �����Ǿ������p. 
    this->param_ = p;
    // p��ConvolutionParam����������Ķ���, ��p��ֵ��param_. ��͵����ĸ�ֵ��һ��, param_����p, ���Կ�����ָ��p��ָ��. 
    
    // convert MBytes first to Bytes and then to elements.
    param_.workspace = (param_.workspace << 20) / sizeof(DType);
    // param_.workspace��ֵ, ������param_.workspaceΪ20, �ٳ���sizeof(DType). ����sizeof(float). 
    CHECK(param_.layout.value() == mshadow::kNCHW ||
          param_.layout.value() == mshadow::kNCDHW)
      << "Only support NCHW and NCDHW layout";
    // ������layoutֻ֧��NCHW and NCDHW, �жϾ�����layout.value()(layout��ֵ, ����һ��int����)�Ƿ���mshadow::kNCHW��
	// mshadow::kNCDHW. ��������, �������. 
  }

  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_args) {
    /*ǰ�򴫲�, �麯��. ������ʵ�������ж���. ����Ҫ����ֵ. ����Ϊ�� l ��. 
	in_data: ��������data, �����Ȩ��wmat��ƫ��(����������ƫ��, ����û��).
	req: ���ݲ���ģʽ. 
	out_data: �������, out. 
	*/
    using namespace mshadow;
    using namespace mshadow::expr;
    CHECK_EQ(req[conv::kOut], kWriteTo);
    /*
	�жϾ����ǰ�򴫲�������ģʽ�ǲ���kWriteTo, �������, ����. һ�������, ���е�out_data������Ӧ����kWriteTo, 
	��ʾout_data�����tensor�ǿ���ֱ��д���ԭʼ���ڴ�� .  
	*/
    size_t expected = param_.no_bias ? 2 : 3;
    /*
	size_t�Ǳ�׼C���ж����, ӦΪunsigned int, ��64λϵͳ��Ϊ long unsigned int. ����expected, param_.no_biasΪ��, ��expected
	Ϊ2, ����Ϊ3. ��expected����������ǰ�򴫲��еĲ����ĸ�����, ��ֻ��kData, KWeight, ����kData, KWeight,, kBias. 
	*/
    CHECK_EQ(in_data.size(), expected); // ������in_data(��������)�Ĵ�С�Ƿ��expected���, �����, ����. 
    CHECK_EQ(out_data.size(), 1); // ǰ�����ֻ��һ�����out, ���ǰ������е�out_data�Ĵ�СӦ����1.
	 
    Stream<xpu> *s = ctx.get_stream<xpu>(); // XPU��. CPU��GPU. 
    if (param_.kernel.ndim() > 2) {
      LOG(FATAL) << "Volume convolution is not implmented in mshadow";
    }
    /*
	param_�ʹ����˾����Ĳ���ʵ��, ����param_���Ե��þ���������һ������. kernel.ndim()��TShape�����ά��, mshadowֻ���˶�ά
	���. convolution-inl.h(0.9�汾��)����3D���, ����NNVM. 
	
	log�ļ���:
	error�Ǵ���; fatal������; silence: ��ʾ���������־��Ϣ; verbose����ʾ����ϸ����־��Ϣ.
	*/
    Tensor<xpu, 4, DType> data = in_data[conv::kData].get<xpu, 4, DType>(s);
    /*
	������(��l��)����������in_data[kData]����4ά������. ��ʹ��get����:
    mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get(mshadow::Stream<Device> *stream = NULL)const. 4ά������, ����ͺ�
	blob�Ƚ�������, ��number N x channel K x height H x width W, (N, K, H, W). Blob memory is row-major in layout. 
	
	���channel K��һ����ͼ���ͨ������, ����Ϊǰһ������ͼ������.  
	*/
    Shape<3> wmat_shape =
        Shape3(param_.num_group,
               param_.num_filter / param_.num_group,
               data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1]);
    /*
	��https://raw.githubusercontent.com/jdeng/gomxnet/master/mxnet.cc�����ҵ�Shape1, Shape2, Shape3, Shape4�Ķ���:
	Shape3��������:
	MSHADOW_XINLINE Shape<3> Shape3(index_t s0, index_t s1, index_t s2) {
        Shape<3> s;
        s[0] = s0; s[1] = s1; s[2] = s2;
        return s;
    }
    
    ����wmat_shape, Ϊ���屾��(��l��)��Ȩ��wmat��׼��. �������Ȩ�ؾ����shape. 
    s0 = param_.num_group, ��num_group(���������и��num_group��partitions), Ĭ��Ϊ1;
	s1 = param_.num_filter / param_.num_group. num_filterΪ����˵ĸ���, ��ʹ��COnvolutionʱָ��, num_filter�ķ�Χ, 1-100000.
	s2 = data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1];
	data.shape_[1]��data shape�ĵڶ���������ֵ, ��channel K. ����Ϊ�����ǰһ������ͼ������.  
    param_.kernel[0]��kernel[0], ֻ��ʹ��param_������kernel. ��ʹ�þ�����ʱ��: kernel = (5,5).  
    param_.kernel[1]��kernel[1]. 
	*/
    Tensor<xpu, 3, DType> wmat =
        in_data[conv::kWeight].get_with_shape<xpu, 3, DType>(wmat_shape, s);
    /*
	�������Ȩ��in_data[conv::kWeight]��in_data[1]����3ά������. ��������get_with_shape. ��������:
	mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get_with_shape(const mshadow::Shape<dim> & shape, 
	mshadow::Stream< Device > *stream = NULL)const, ����Shape<dim> & shape ����wmat_shape. 
	
	������Ȩ��wmat��3D������. Ȩ�صĴ�С��: wmat_shape. һ���, �����ľ����(weight)�ĸ���Ϊ:
	num_filter * (kernel[0] * kernel[1]). ����mxnet�����ʱ, �õ��� num_group ����, ��˽�wmat���ó�3D��, ���е�һ��ά����
	num_group����Ŀ. ����Ҫ����������������data�ֳ�num_group��partitions, ��ôÿ��partitions�ľ���˸�������:
	param_.num_filter / param_.num_group��.   
	*/
    Tensor<xpu, 4, DType> out = out_data[conv::kOut].get<xpu, 4, DType>(s);
    /*���屾��(��l��)���������out_data[kOut]��out_data[0], Ϊ4ά������.*/
    
#if defined(__CUDACC__)
    CHECK_EQ(s->blas_handle_ownership_, Stream<xpu>::OwnHandle)
        << "Must init CuBLAS handle in stream";
#endif // __CUDACC__ // �Ƿ�����__CUDA__��. 
    const index_t nbatch = data.size(0);
    /*
    index_t�Ķ�����mshadow/mshadow/base.h��.
	typedef unsigned index_t; 
	unsigned a; ��unsigned int a; ��ͬ����. ����ʡ����int, ֻ�ܺ�unsigned int�ȼ�. 
    
	Tensor<xpu, 4, DType> data; data��4ά������, �����Ĵ�С��(N, C, H, W), ��һά�������Ӽ��ĸ���, ���Կ�������ѵ����һ������
	����; C��ͼ���ͨ����, �������Ƕ�ά�Ļ�����ά������(���channel K��һ����ͼ���ͨ������, ����Ϊǰһ������ͼ������. ); 
	H�����ݵĸ߶�; W�����ݵĿ��. ���, data.size(0)��N, data.size(1)��C, data.size(2)��H, data.size(3)��W. 
	nbatch����������(unsigned int)�ı���, ������������С. 
	*/
	
    Tensor<xpu, 1, DType> workspace =
        ctx.requested[conv::kTempSpace].get_space_typed<xpu, 1, DType>(
            Shape1(this->InitTemp(data.shape_, out.shape_)), s);
	/*
	get requested temp space. ��ȡ�������ʱ�ռ�. 
	��Щ������Ҫ������ڴ���Ϊ�����ռ���м���, ����˵cudnnConvolutionForward. ���������, ϵͳ��ÿ��Զ��ⲿ���ڴ���й���, 
	����ϵͳ������һЩ�Ż�, ����˵�ڴ���ظ�����. ���BN��kTempSpace. ��BN�ķ������������һ����ʱ����Դ�ռ�, ����ռ�����. 
	*/            
    /*
	workspace��һά������(����). 
	OpContext: �ṹ��, ������include/mxnet/operator.h��, �ýṹ����Լ�¼������ǰ��ͺ��򴫲��е���Ϣ. ctx�ǽṹ��OpContext��
	��Ķ���, requested��OPContext�ṹ���µĺ���:
    // brief Resources requested by the operator
  	std::vector<Resource> requested; // �������ؾ�������������Դ. 
    ctx.requested���ص���һ����������, ������Ҫ��ֻ��kTempSpace����������Դ����, ��һ����ʱ�Ĳ����ռ�. 
	ctx.requested[conv::kTempSpace]����һ��Resource�Ķ���, �ٵ���get_space_typed����, ���������Ĳ����ռ�����һ��һά������.
	
	Resource�ṹ�嶨��: http://mxnet.io/doxygen/structmxnet_1_1Resource.html. ������mxnet�����������Դ.
    get_space_typed����:
    mshadow::Tensor<xpu, ndim, DType> mxnet::Resource::get_space_typed (mshadow::Shape<ndim> shape, mshadow::Stream<xpu>* 
	stream )const ����ָ�����͵�Tensor. 
	����, shape�Ƿ��ص�Tensor��Shape, stream���������. 
	
	MSHADOW_XINLINE Shape<1> Shape1(index_t s0) {
	    Shape<1> s; s[0] = s0;
        return s;
    } 
    
    �����һ�������Ķ����õ���InitTemp����, �ú���Ҳ�Ƕ�������COnvolutionOP��:
    inline index_t InitTemp(const mshadow::Shape<4> &ishape, const mshadow::Shape<4> &oshape) {...} 
    data.shape_, out.shape_ ��������������shape, ����data��out����4ά������, ����data.shape_, out.shape_����4ά��. ������
	Shape<4>, InitTemp��������ֵ������index_t, ����Shape1�Ĳ�������һ��. Shape1(this->InitTemp(data.shape_, out.shape_))����
	�ľ���Tensor��shape. 
	*/        
            
    for (index_t i = 0; i < nbatch; i += nstep_) {
      const index_t step = std::min(nstep_, nbatch - i);
      
      // cout<<"step: "<<step<<endl;  // 64, ji batch_size. 
      /*
	  step����������(unsigned int), index_t nstep_ ��nstep_����ConvolutionOp�µ�һ�����ݳ�Ա, ������ʱ��ָ��nsetp_�Ĵ�С. 
	  nstep_��batch_size. ���forѭ��ִֻ��һ��, �� i == 0.  
	  stepȡnstep_ �� nbatch_ - i ����Сֵ. ����nstep_��batch_size, ���stepΪbatch_size. 
	  */
	  
      Tensor<xpu, 2, DType> temp_col = Tensor<xpu, 2, DType>(workspace.dptr_,
                                               Shape2(shape_colunit_[0],
                                                      shape_colunit_[1] * step), s);
      Tensor<xpu, 3, DType> temp_dst = Tensor<xpu, 3, DType>(
                                               workspace.dptr_ + temp_col.shape_.Size(),
                                               Shape3(shape_dstunit_[0],
                                                      shape_dstunit_[1],
                                                      shape_dstunit_[2] * step), s);
	  /*
	  lenet��ִ��ʱ������ͼ��С:
	  28*28 --> 12*12(conv1) --> 6*6(pool1) --> 2*2(conv2) --> 1*1(pool2) -->... 
	  */
	  /*
	  ��������shape_colunit_, shape_dstunit_, nstep_. 
  	  ��������ʱָ����, ����ConvolutionOp���˽�к���InitTemp��ָ����, ���°汾��convolution-inl.h��Ҳ��һ��˽�к���
  	  LayerSetUp, ����ָ��һЩֵ. ����ishape(��������shape)��oshape(�������shape)��, data.shape_, out.shape_
	  ����ȷ��OP���õ���һЩ������ֵ. 
	  */
	  
	  // cout<<"forward_nstep_: "<<nstep_<<endl; // nstep_��batch_size.  
	  /*cout<<"shape_colunit_[0]: "<<shape_colunit_[0]<<endl; 
	  // conv1 = mx.symbol.Convolution(data=data, kernel=(5,5), stride=(2,2), num_filter=10)ʱ, Ϊ25. 
	  // shape_colunit_[0]��: �����ǰһ������ͼ���� * kernel[0] * kernel[1]. ishape[1] * ksize_y * ksize_x.
	  cout<<"shape_colunit_[1]: "<<shape_colunit_[1]<<endl;
	  // Ϊ144, 4. �� ������������ͼ�ĸ߶� * ��� ֮��. oshape[2] * oshape[3].
	  cout<<"shape_dstunit_[0]: "<<shape_dstunit_[0]<<endl;
	  //  shape_dstunit_[0]��1. param_.num_group.  
	  cout<<"shape_dstunit_[1]: "<<shape_dstunit_[1]<<endl;
	  // shape_dstunit_[1]�Ǿ�������˵ĸ���. Ϊ10, 50. param_.num_filter / param_.num_group.
	  cout<<"shape_dstunit_[2]: "<<shape_dstunit_[2]<<endl;
	  // �� ������������ͼ�ĸ߶� * ��� ֮��.  oshape[2] * oshape[3].*/ 
	  
	  /*
	  ����������Tensor��Ĺ��캯��������xpu, ndim == 2, 3, DType�µĶ���temp_col��temp_dst. ��ҪTensor��(�ṹ��)�Ķ���, ��:
	  mshadow/mshadow/tensor.h 363��. 
	  ����375��378��Tensor���캯������ʽ, ȷ��375��378��ʹ�õĹ��캯��Ϊ:
	  template<typename Device, int dimension, typename DType MSHADOW_DEFAULT_DTYPE>
	  MSHADOW_XINLINE Tensor(DType *dptr, const Shape<dimension> &shape, Stream<Device> *stream) : dptr_(dptr), shape_(shape),
	  stride_(shape[kSubdim]), stream_(stream) {}. ����data pointer and shape, without stride������Tensor�Ķ���. 
	  Device����make mxnet��ָ��, cpu��gpu. dimension���ڶ������������ʱ��ָ��, ��2, 3. 
	  DType *dptr: ��float* �͵�ָ��dptr;
	  Shape<dimension> &shape: ��һ��shape, ����Shape2����Shape3����(Shape2��Shape3��������). �������Tensor�����shape.  
	  Stream<Device> *stream: ��Stream����. 
	  
	  *dptr�����ʵ��һ����workspace.dptr_, һ����workspace.dptr_ + temp_col.shape_.Size().
	  workspace����: Tensor<xpu, 1, DType> workspace, ����1άTensor�Ķ���, ���� struct Tensor<Device, 1, DType> �ṹ���µ�
	  ��Ա: DType *dptr_; 514��. ��ָ�����ݵ�ָ��. 
	  temp_col.shape_.Size()�� temp_col.shape_���������ĳ˻�, �� shape_colunit_[0] * shape_colunit_[1] * step, ��һ����. 
	  
	  ����Tensor��Ĺ��캯�������� xpu, ndim == 2, 3, DType�µ�Tensor����temp_col��temp_dst, �ڴ�����ʱ���ָ��
	  temp_col��temp_dst��shape. temp_col��temp_dst��������ʱ��Tensor, ����tmp_col��tmp_dst���������վ��������Tensor
	  Tensor<xpu, 4, DType> out.  
	  
	  Shape3��������:
	  MSHADOW_XINLINE Shape<3> Shape3(index_t s0, index_t s1, index_t s2) {
        Shape<3> s;
        s[0] = s0; s[1] = s1; s[2] = s2;
        return s;
  	  }
  	  
	  Shape2��������:
	  MSHADOW_XINLINE Shape<2> Shape2(index_t s0, index_t s1) {
        Shape<2> s;
        s[0] = s0; s[1] = s1;
        return s;
      }
	  
	  // MSHADOW_XINLINE is used for inlining template code for both CUDA and CPU code. MSHADOW_XINLINE��һ����. 
	  #ifdef MSHADOW_XINLINE
	      #error "MSHADOW_XINLINE must not be defined"
	  #endif
	  */
 
      if (param_.pad[0] == 0 && param_.pad[1] == 0) { // 0.8�ľ������ֻ��ʵ��2D���, ���kernel, stride, dilate, pad����2D��.
	    // param_ָ��ṹ��ConvolutionParam����p��ָ��, �������ʳ�Աpad. ��pad[0], pad[1]. 
		// ��2D���ʱ, ���Ծ���˲���, temp_col��:
		
		/*// data�Ǿ�������������! data�� 64 * 1 * 28 * 28��. ��д������ʶ������.  			   
		cout<<"data.shape_[0]: "<<data.shape_[0]<<endl; // 64 
		cout<<"data.shape_[1]: "<<data.shape_[1]<<endl; // 1, ͨ����. channel K. ��ͨ��(�Ҷ�ͼ��), RGBͼ��(��ɫͼ��). 
		cout<<"data.shape_[2]: "<<data.shape_[2]<<endl; // 28 
		cout<<"data.shape_[3]: "<<data.shape_[3]<<endl; // 28 */ 
		
		/*
		// ���������������� data(4D tensor)������ָ��. 
		cout<<"data[0]: "<<data[0].dptr_<<endl; // data[0]: 0x7f98600008c0 
		cout<<"data[1]: "<<data[1].dptr_<<endl; // data[1]: 0x7f9860001500 
		cout<<"data[2]: "<<data[2].dptr_<<endl; // data[2]: 0x7f9860002140 
		cout<<"data[3]: "<<data[3].dptr_<<endl; // data[3]: 0x7f9860002d80 
		*/
		
		/*
 		// д������, ��data���ݱ��浽.txt��. ��Ϊcout���һ��tensor�����, 
		// ����ֻ��������ֵ. 
		// std::ofstream fdata("/home/ly/MXNet/fdata.txt");
		for(int i = 0; i < data.shape_[0]; ++i){
				for(int j = 0; j < data.shape_[2]; ++j){
						for(int k = 0; k < data.shape_[3]; ++k){ 
								fdata << data[i][0][j][k] <<" "; 
						}
				}
		}
		fdata<<flush;
	 	// fdata.close();
		// data��������mshadow::Tensor<mshadow::cpu, 4, float>&
		// data[0]��������mshadow::Tensor<mshadow::cpu, 3, float>
		// data[0][0]��������mshadow::Tensor<mshadow::cpu, 2, float>
		// data[0][0][0]��������mshadow::Tensor<mshadow::cpu, 1, float>
		// data[0][0][0][0]��������float&. 
		*/
		
        temp_col = unpack_patch2col(data.Slice(i, i + step),
                                    param_.kernel[0],
                                    param_.kernel[1],
                                    param_.stride[0],
                                    param_.stride[1],
                                    param_.dilate[0],
                                    param_.dilate[1]);
        /*
		data.Slice(i, i + step), �õ���Slice����, ��Ƭ����, ��һ��Tensor������Ƭ, ����data.Slice(i, i + step)�ĵ��ø�ʽ, Slice
		Ӧ����Tensor�ṹ��ĳ�Ա, slice������Slice�����ǲ�һ����, Slice�����:
		mshadow/mshadow/tensor.h 481��:
		MSHADOW_XINLINE Tensor<Device, dimension, DType> Slice(index_t begin, index_t end) const {...}. Slice�ǽṹ��Tensor��
		��Ա����, ���Tensor�Ķ���data���Ե���Slice����.
		slice the tensor in highest dimension [begin,end)
		param begin: begin position of slice
		param end: end position of slice
		return tensor after slice. ������Ƭ���Tensor. 
		*/
		
		/*
        // cout<<"i: "<<i<<endl; // 0, iһֱ��0, ѭ����ִ��1��. 
        // ����iamge������Ǿ����. 
		auto a = data.Slice(i, i + step); // c++11�����ƶ�.
        cout<<"a.shape_[0]: "<<a.shape_[0]<<endl; // step, ��Ϊstep��batch_size, ����Ϊ64. ����a.size(0). 
        cout<<"a.shape_[1]: "<<a.shape_[1]<<endl; // 1. 
        cout<<"a.shape_[2]: "<<a.shape_[2]<<endl; // 28.
		cout<<"a.shape_[3]: "<<a.shape_[3]<<endl; // 28.
		*/
		
		/*
		����Tensor�͵ı�������ֱ����cout���, ��������mshadow/mshadow/tensor.h 363��, Tensor�ṹ��ĳ�Ա������Tensor������:
		Tensor�ṹ��ĳ�Ա����:
	    pointer to the data 
  		DType *dptr_; // ָ��Tensor���ݵ�ָ�� dptr_.  
  		
		shape of the tensor
  		Shape<dimension> shape_; // ���� shape_ ������Tensor��shape. 
  		
		storing the stride information in x dimension
		this is used to deal with pitch allocation in gpu or sse(align x dimension to 64bit) for efficiency
		index_t stride_;
		
		stream where the computation lies
		stream is a device dependency concept where each computation
		Stream<Device> *stream_; 
		*/
		/*
		// ������tensorʱ��dptr_����.
		cout<<"a[0]: "<<a[0].dptr_<<endl; // a[0]: 0x7f98600008c0
		cout<<"a[1]: "<<a[1].dptr_<<endl; // a[1]: 0x7f9860001500 
		cout<<"a[2]: "<<a[2].dptr_<<endl; // a[2]: 0x7f9860002140 
		cout<<"a[3]: "<<a[3].dptr_<<endl; // a[3]: 0x7f9860002d80 
		*/
		/*
		ʵ����, ����step = batch_size, ���tensor��dptr_. һ����data, һ����data.Slice(i, i + step). �Ƚϲ���.
		���� data �� data.Slice(i, i + step)�Ĵ�С�Ƚ� + dptr_ �Ƚ�, ����step��batch_size, ��Ϊ��������������û�з����ı�. 
		
		data.Slice(i, i + step)�Ƕ�data���tensor������Ƭ, ȡdata��һ����(i, i + step). 
		*/
                                    
        /*����temp_col: 
		���ú���unpack_patch2col: �����mshadow/mshadow/extension/unpack_patch2col.h 91��104��. ���������ͬ, ����445ʹ�õ�
		unpack_patch2col, ��������Ϊ:
	    template<typename SrcExp, typename DType, int etype>
		inline UnpackPatchToColXExp<SrcExp, DType, ExpInfo<SrcExp>::kDim> unpack_patch2col(
		const Exp<SrcExp, DType, etype> &img, index_t psize_y, index_t psize_x, index_t pstride_y_, index_t pstride_x_,
		index_t pdilate_y_, index_t pdilate_x_) {...}. 
		��ͼ���(patches of image)unpack(��ѹ)��һ�������һ��, ������unpack_patch2col�õ�mat��, ����ʵ�־��.
	
		img: source image, img������3D Tensor����4D Tensor(���ͼ��). ������4DTensor, batch_size * 1 * 28 * 28. 
		psize_y: ÿ��patch�ĸ߶�. ������kernl[0], ������˵ĸ߶�. 
		psize_x: ÿ��patch�Ŀ��. ������kernel[1], ������˵Ŀ��. 
		pstride_y_: ÿ��patch��y�����ϵĻ�������. ������stride[0], ���������y�����ϵĻ�������. 
		pstride_x_: ÿ��patch��x�����ϵĻ�������.  ������stride[1], ���������x�����ϵĻ�������. 
		pdilate_y_: ÿ��patch��y�����ϵ�����ϵ��. ������dilate[0], ������˵�����ϵ��, y����. 
		pdilate_x_: ÿ��patch��x�����ϵ�����ϵ��. ������dilate[1], ������˵�����ϵ��, x����. 
		
		����unpack_patch2col�õ�mat, �õ�output: output = dot(weight, mat). output���Ǿ������������ͼ. 
		out_height = [(in_height - psize_y)] / pstride + 1,
		out_width  = [(in_width - psize_x)] / pstride + 1. 
		*/
		
		/*
		cout<<"temp_col.shape_[0]: "<<temp_col.shape_[0]<<endl; // 25, param_.kernel[0] * param_.kernel[1] 
		// �����С�� shape_colunit_[0]��: �����ǰһ������ͼ���� * kernel[0] * kernel[1]. 
		cout<<"temp_col.shape_[1]: "<<temp_col.shape_[1]<<endl; // 9216, batch_size * [ out.size(2) * out.size(3) ], out��
		// ������������ͼ.  �����С�� shape_colunit_[1] * step, �� step * ������������ͼ�ĸ߶� * ��� ֮��.
		// �ڶ�������temp_colʱ, �ѽ����С�����. 
		cout<<"temp_col.shape_.Size(): "<<temp_col.shape_.Size()<<endl; // 230400 = 25 * 9216.
		
		��ͼ���(patches of image) 28 * 28��patch, unpack(��ѹ)��һ�������һ��, 
		����unpack_patch2col�õ�mat��, ����ʵ�־��.
		*/
		
		
      } else {
        temp_col = unpack_patch2col(pad(data.Slice(i, i + step),
                                    param_.pad[0], param_.pad[1]),
                                    param_.kernel[0],
                                    param_.kernel[1],
                                    param_.stride[0],
                                    param_.stride[1],
                                    param_.dilate[0],
                                    param_.dilate[1]);
        /*
		���pad[0]��pad[1]����0, ������ pad���� �� data.Slice(i, i + step) ���в������, Ȼ������unpack_patch2col����.
		
		pad����Ҳ�Ƕ�����mshadow::expr�����ռ���, ���Ҫusing namespace mshadow::expr; �����mshadow/mshadow/extension/pad.h. 
		����241��ʹ�õ�pad����������, ���:
		template<typename SrcExp, typename DType, int etype>
		pad(const Exp<SrcExp, DType, etype> &src, index_t pad_y, index_t pad_x)
		��һ��ͼƬ���в������, ��ͼƬ�����ܲ���. srcԭͼ��; pad_y: padding size in y, ����y�����ϲ��������; pad_x: 
		padding size in x, ��x�����ϲ��������. ���ز���Ľ��, �����ز�����֮��ľ���.  
		*/                            
                                    
      } 
	  // temp_col ������ unpack_patch2col ���tensor, ��С�� shape_colunit_[0] * [shape_colunit_[1] * step]. 

      const index_t gstride = temp_col.size(0) / param_.num_group; 
      // gstride������index_t, ��unsigned int�͵�. ��ֵΪ temp_col.size(0) / param_.num_group.
	  // �� shape_colunit_[0](�����ǰһ������ͼ���� * kernel[0] * kernel[1].) / num_group(Ĭ��Ϊ1, 
	  // �����������и��num_group��partitions. num_groupò��ֻ����1). 
      
      for (uint32_t gid = 0; gid < param_.num_group; ++gid) { 
	    /*
		ʹ��typedef��������ı���. 1�ֽ�:uint8_t; 2�ֽ�: uint16_t; 4�ֽ�: uint32_t; 8�ֽ�: uint64_t.
		typedef unsigned char uint8_t;
		typedef unsigned int uint16_t; 
		typedef unsigned long uint32_t;
		typedef unsigned long long uint64_t;
		
		param_��ConvolutionParam�ṹ��Ķ���, ���ó�Աnum_group. ��Ϊnum_groupĬ��Ϊ1, ��� gid == 0. ��ֻ��һ��forѭ��.
		���forѭ�����ľ���: �����������и��num_group��partitions. �е㲢�е���˼. 
		*/

        mshadow::Tensor<xpu, 2, DType> tmpc = temp_col.Slice(gstride * gid,
                                       gstride * (gid + 1));
        /*
		temp_col������unpack_patch2col֮���Tensor, ��padĬ��Ϊ0�������, ���СΪ: 
		shape_colunit_[0] * [shape_colunit_[1] * step]. �ٴε���Slice, tensor��Ƭ����:
		Slice����, ���ø�ʽ: data.Slice(i, i + step)�ĵ��ø�ʽ. 
		
		gid�������� num_group - 1. ��gid������þ���, ��һ��tensor(���������и��num_group��partitions), gid�ʹ���ÿ��
		partitions������, ���� gid ��partitions. ����������Slice�����и�temp_col, �и�ķ�Χ�� gid-gid+1, ����temp_col�и��
		1��partitions. ÿ��partitions(Ҳ��һ��tensor)�Ĵ�С�� gstride.  
		 
		*/
		/*
		// ���tmpc�Ĵ�С, ��һ��2ά������.
		cout<<"tmpc.shape_[0]: "<<tmpc.shape_[0]<<endl; // 25 
		cout<<"tmpc.shape_[1]: "<<tmpc.shape_[1]<<endl; // 9216 
		
		��Ĭ�� num_group == 1�������, gid == 0. gstride == temp_col.size(0). 
		���, Ϊtemp_col.Slice(0, gstride). ��, tmpc �� temp_col��һ����. ������Ϊû���� ���������и��num_group��partitions
		�������, ��� tmpc �� temp_col��һ����. 
		*/
		
                                       
        temp_dst[gid] = dot(wmat[gid], tmpc);
        /*
		temp_dst��3D��tensor. gid == 0, ����Ǹ� temp_dst[0]��ֵ, ���� unpack_patch2col ������˵��, 
		output: output = dot(weight, mat)�����Կ����Ǿ����Ľ��(��ɢ�����, �����Ԫ�غͶ�Ӧλ�õ�Ԫ����������, �����dot
		. ����dot�����Ǿ���֮��ĵ����). 
		���, temp_dst�����������Ľ��. 
		
		1)num_group == 1�������, tmpc��tmp_col��һ����С��, ��������unpack_patch2col���tensor. ��tmpc�Ǿ�������������. 
		tmpc���������, pad[0] == pad[1] == 0 ��ȫΪ0. ��֮, ���Ǿ�������������. 
		
		2)wmat[0]���Ǿ����ȫ���ľ���˵�Ȩ��ֵ(����Ȩֵ����, ���һ������ͼ��Ҫһ�������).
		wmat�Ǿ�����Ȩ��, ������˵�Ȩ��. wmat��������:
  		Shape<3> wmat_shape =
        Shape3(param_.num_group,
               param_.num_filter / param_.num_group,
               data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1]);
	   Tensor<xpu, 3, DType> wmat;  
	   
	   wmat��3Dtensor, ���wmat[0]����2D��tensor. ���, Ĭ��num_group == 1, ���wmat�ĵ�һά������0. ��wmat[0]��ά����:
	   { param_.num_filter / param_.num_group } * { data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1] } 
	   ��: ����˵ĸ��� * channel K * kernel[0] * kernel[1]. channel K == 1����2D���. ���, num_group == 1�������, 
	   wmat[0]���Ǿ����ȫ���ľ���˵�Ȩ��ֵ(����Ȩֵ����, ���һ������ͼ��Ҫһ�������). 
	   ��������, ����һ��������˵, ������Ȩ������ͬ��. 
	   
	   3)temp_dst[0] ���ǶԾ������������ tmpc ������(��weight)��� �������(�������).
	   ����unpack_patch2col ������˵��, dot(weight, tmpc)������������Ľ��. 
	   temp_dst[0]������������Ľ��, ��������;���˷���������. ��û���漰ƫ��.
	   tem_dst��������:
	   Tensor<xpu, 3, DType> temp_dst; ��СΪ: shape_dstunit_[0] * shape_dstunit_[1] * { shape_dstunit_[2] * step), s) };
	   shape_dstunit_[0]��1, ��channel K; shape_dstunit_[1]�Ǿ�������˵ĸ���; shape_dstunit_[2]�� �����������ͼ��
	   �߶� * ��� ֮��. ���, temp_dst�ĵ�һά���ֵΪ0. 
	   ��num_group == 1�������, temp_dst[0]��2D��tensor, ��С��:
	   ����˵ĸ���(��������������ͼ����) * { ����ͼ��С * batch_size }. ��������, һ�����������������������ݴ�С��:
	   ����˵ĸ���(��������������ͼ����) * ����ͼ��С; ���ڲ�ͬ������, �����С��ͬ, ֻ��ֵ��ͬ. 
	   ���, temp_dst[0] ���ǶԾ������������ tmpc ������(��weight)��� �������(�������).   
		*/
      }
      // ���ϵ���Щ������ֵ�Ľ������ num_group == 1�� step == batch_size�������, �Ƴ�����. ��wmat�Ĵ�С, wmat[gid]�Ĵ�С, 
	  //tmpc�Ĵ�С, tmp_col�Ĵ�С, tmp_dst�Ĵ�С�ȵ�. �@��tensor�� num_group != 1�r, ��Ҫ�l���仯. ���ﲻ��ϸ�о���.   
      out.Slice(i, i + step) = swapaxis<1, 0>(reshape(temp_dst,
                                              mshadow::Shape4(param_.num_filter,
                                                  step,
                                                  out.size(2),
                                                  out.size(3))));
	  /*
	  num_group == 1�������, gid == 0. temp_dst[0] ���ǶԾ������������ tmpc ������(��weight)����������(�������). ��С��:
	  ����˵ĸ���(��������������ͼ����) * { ����ͼ��С * batch_size }.
	  
	  // cout<<"i: "<<i<<endl; // 0, iһֱ��0, ѭ����ִ��1��. 
	  // ����iamge������Ǿ����. 
	  auto a = data.Slice(i, i + step); // c++11�����ƶ�.
	  cout<<"a.shape_[0]: "<<a.shape_[0]<<endl; // step, ��Ϊstep��batch_size, ����Ϊ64. ����a.size(0). ��index_t nstep_;���. 
	  cout<<"a.shape_[1]: "<<a.shape_[1]<<endl; // 1. 
	  cout<<"a.shape_[2]: "<<a.shape_[2]<<endl; // 28.
	  cout<<"a.shape_[3]: "<<a.shape_[3]<<endl; // 28.
	  data�Ǿ�������������, ��δ�� unpack_patch2col����. data.Slice(i, i + step)����data��Ƭ�� [nbatcb/nstep_]��, ���д���.
	  
	  �����������������data���г��� [nbatcb/nstep_]��, �����д���, ��ô tmp_col Ҳ�� [nbatcb/nstep_]��, tmp_dstҲ��
	  [nbatcb/nstep_]��. ���, temp_dst[gid] = dot(wmat[gid], tmpc)(����gid==0)����������һ��data�ľ����Ľ��. 
	  out�Ǿ���������tensor, ��4D��. ��� tmp_dstҲ��[nbatcb/nstep_]�ݵ�, ��ôoutҲ���г��� [nbatcb/nstep_]��. ���Ҫһ�ݷ�
	  �ĸ�ֵ, ���յ������out. 
	  �� out.Slice(i, i + step), ���ǶԾ��������out(һ��, ����СΪstep)�ĸ�ֵ����. ��[nbatcb/nstep_]��out��ֵ��ϲ����ǵõ�
	  �˾������������. �ٽ�һ�� temp_dst[0] ����һ�ݵ� outʱ, Ҫ�ı��С. ��Ϊout��4D��, temp_dst[0]��2D��tensor.
	  
	  1)reshape����, ���¶�������Ĵ�С. ���reshape����python numpy�е�reshape, reshape������յĵ�һ��������tmp_dst, ������
	  Tensor. �����: mshadow/mshadow/extension/reshape.h 48��. ��mshadow::exprc�����ռ���. 
	  template<typename SrcExp, typename DType, int etype, int dimdst>
	  inline ReshapeExp<SrcExp, DType, dimdst, ExpInfo<SrcExp>::kDim> reshape(const Exp<SrcExp, DType, etype> &src, 
	  Shape<dimdst> oshape) {...}. reshape a tensor to another shape.
	  src: Tensor<Device, dimsrc>, dimsrc��src��ά��, ��3.
	  oshape: target shape. ��Shape<dimdst> oshape, dimdst�����tensor��ά��, ��4.
	  return a expresion with type Tensor<Device, dimdst>. 
	  ���reshape(temp_dst, *)��������temp_dst���3D Tensor��shape, �������ݵ��ܸ����ǲ����. temp_dst[0]��2D��tensor, ��С��:
	  ����˵ĸ���(��������������ͼ����) * { ����ͼ��С * batch_size }. ���, temp_dst�Ĵ�СΪ:
	  channel K * ����˵ĸ���(��������������ͼ����) * { ����ͼ��С * batch_size }.
	  reshape���������shapeΪ: num_filter * step * out.size(2) * out.size(3). ��:
	  ����˸��� * step(batch_size) * ������������ͼ�߶� * ���(4D). �������ö��Ͼ�������out tensor��ά��.
	  
	  reshape(temp_dst, *)�ͷ��� ����˸��� * step(batch_size) * ������������ͼ�߶� * ���(4D)��һ��4Dtensor.
	  
	  2)swapaxis����, ���������mshadow/mshadow/extension/swapaxis.h 52��. ��mshadow::exprc�����ռ���. 
	  template<int a1, int a2, typename SrcExp, typename DType, int etype>
	  inline SwapAxisExp<SrcExp, DType, ExpInfo<SrcExp>::kDim, ExpInfo<SrcExp>::kDim - a1, a2> swapaxis(
	  const Exp<SrcExp, DType, etype> &src) {...}. reshapes a tensor to another shape.
	  src: Tensor<Device, dimsrc>, dimsrc��src��ά��, ��4. 
	  return a expresion with type Tensor<Device,dimdst>. dimdst�����tensor��ά��, ��4.
	  ģ�����a1: higher dimension to be swapped, assert a1 > a2. assert���ԭ�Ͷ�����<assert.h>��, 
	  ����������������������ش���, ����ֹ����ִ��.
	  ģ�����a2: lower dimension to be swapped. 
	  
	  swapaxis<1, 0>(...)���Ƕ� reshape(temp_dst, *)�ķ��ؽ��������reshapeһ��(..). 1, 0��ָ��ģ�����:
	  int a1, int a2. ģ����, ģ�庯����ģ�������������ָ��: swapaxis<1, 0>(...).  
	  
	  ���reshape�Ľ��(һ��temp_dst)����һ�� out. һ��[nbatcb/nstep_]��. 
	  
	  */
	  
    }
    if (!param_.no_bias) { // no_biasĬ��Ϊfalse, ���if��ʹ��bias. �����Ĭ�ϲ�ʹ��bias(ƫ��). 
      // add bias, broadcast bias to dim 1: channel
      Tensor<xpu, 1, DType> bias = in_data[conv::kBias].get<xpu, 1, DType>(s);
      // ����get������in_data[2]����1ά������, ������. �������������bias, ��������. bias��һ��1D��tensor. 
      // cout<<"bias.size(0): "<<bias.size(0)<<endl; // ��������bias�Ĵ�С, 1D��tensor�Ĵ�С. Ϊ��������˵ĸ���.
	  // ��һ������˶�Ӧһ��bias, Ҳ�ǹ����. 
      out += broadcast<1>(bias, out.shape_);
      /*
	  broadcast��: mshadow/mshadow/extension/broadcast.h 69��:
	  template<int dimcast, typename SrcExp, typename DType, int etype, int dimdst>
	  inline Broadcast1DExp<SrcExp, DType, dimdst, dimdst - dimcast> broadcast(const expr::Exp<SrcExp, DType, etype> &src, 
	  Shape<dimdst> shape) {..}. 
	  src Tensor<Device,1>; shape: shape of output; ���� a expresion with type Tensor<Device, dimdst>, dimdstΪ4, 
	  ���ص�Tensor��ά��Ϊ4, ��shape�ĸ������йص�.
	  * input: Tensor<Device,1>: ishape[0]
	  * output: Tensor<Device,dimdst> : oshape[dimcast] = ishape[0].
	  ģ�����tparam dimcast: target dimension where the 1D tensor will be broadcasted 
	  ��һ��1ά�� Tensor ����� dimdst ά�� Tensor. Ϊ����ȷ����!! 
	  
	  out.shape_�Ǿ�������out��shape, ��һ��Shape<4>�ı���, ��СΪ ������� * batch_size * ����ͼ�߶� * ���.
	  broadcast<1>(bias, out.shape_)���ǽ�bias���1D������չ�ɺ;������� out, ����һ����С��tensor. �������ӷ�.
	  
	  ��Ϊout��ÿһ�����ݵ㶼����һ��bias, ����Ҫ��bias��չ�ɺ�outһ����С��tensor�ſ������ӷ�. bias�Ĵ�С�Ǿ���˵ĸ���, ��
	  һ������˶�Ӧһ��bias. Ҳ���Ǿ�����һ���������ͼ��Ӧ��һ��bias. 
	  */
    }
  }

  virtual void Backward(const OpContext &ctx,
                        const std::vector<TBlob> &out_grad,
                        const std::vector<TBlob> &in_data,
                        const std::vector<TBlob> &out_data,
                        const std::vector<OpReqType> &req,
                        const std::vector<TBlob> &in_grad,
                        const std::vector<TBlob> &aux_args) {
    /*�����(��l��)�в���weight��bias(����ʹ��ƫ��, ���ܲ���Ӧ. ���ʹ��ƫ��, ÿ��������䱸һ��), 
	���Ҫ���������ʧJ����BN��(��l��)�Ĳв�, weight���ݶȺ�bias���ݶ�. 
    !!!!!!!!!!!!!!!!�ݶȿ��Կ�������ʧJ���ڲ�����ĵ���, �в���Կ�������ʧJ���ڲ�����ĵ���!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
	 
    in_grad����в�/�ݶȲ���, ��������, ÿ��Ԫ�ص�������TBlob. ������(��l��)��.
	out_grad����в�/�ݶȲ���, ��������, ÿ��Ԫ�ص�������TBlob. ��һ��(��l + 1��)�Ĳв�/�ݶ�, ���㱾��Ĳв�/�ݶ�. 
	������һ��Ĳв����������ʧ���ڱ�������Ĳв�, �Ӷ��ټ������ʧ���ڱ���������ݶ�, �ٽ���sgd����. �õ���һ�������һ���
	�в�.
	 
	in_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)������.  
	out_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)�����. 	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType.
	aux_args: ��ʾ����Ϊ�˷���������Ҫ�ĸ��ӵ� tensor. ���ӵ�Tensor������: kMovingMean, kMovingVar. ��ǰ���Ĳ�����ûʹ��
	aux_args����������.
	*/
    
    using namespace mshadow;
    using namespace mshadow::expr; // һЩ���ʽ���ڵ������ռ�. mshadow::expr.  
    // TODO(bing): check the BLAS Handle, be careful
    if (param_.kernel.ndim() > 2) {
      LOG(FATAL) << "Volume convolution is not implmented in mshadow";
    } // 0.8�汾��mxnetֻ����2D���, ���kernel��ά����� > 2, �����һ��log��Ϣ; 0.9�汾��mxnet������3D�����.  
    CHECK_EQ(out_grad.size(), 1); // ��������һ��(��l + 1��)���ݸ�������ֻ�вв�, ���out_grad�����Ĵ�СΪ1. 
    size_t expected = param_.no_bias == 0 ? 3 : 2; //  ����expected, ��������û��ƫ�þ���2, ����Ϊ3. ����expected�����ƾ��
	// ��in_data��in_grad�����Ĵ�С. 
    CHECK(in_data.size() == expected && in_grad.size() == expected); // ���������in_data��in_grad�����Ĵ�СΪexpected.
	// ����������ƫ��, ��ôin_data������С����3, ��������kData, �����Ȩ��kWeight, ƫ��kBias.
	// in_grad�����Ĵ�СҲ��3, ������ʧ���ھ��������Ĳв�out, ��ʧ���ھ����Ȩ�ص��ݶ�, ��ʧ����ƫ�õ��ݶ�. 
	 
    CHECK_EQ(req.size(), expected); // rep�����Ĵ�С��expected. ��, ����:
	// ��ʧ���ھ��������Ĳв�out, ��ʧ���ھ����Ȩ�ص��ݶ�, ��ʧ����ƫ�õ��ݶ�, ���ò�ͬ�����ݲ���ģʽ. 
    CHECK_EQ(in_data[conv::kWeight].CheckContiguous(), true);
	/*
	in_data[1].CheckContiguous(), ����CheckContiguous����, �����: include/mxnet��tensor_blob.h 136��:
	inline bool CheckContiguous(void) const {
	    return shape_[shape_.ndim() - 1] == stride_;
	} CheckContiguous������TBlob���µĳ�Ա����, ��˿�������TBlob�Ķ������. return whether the tensor's memory is continuous
	�������һ��tensor���ڴ��ǲ���������. �����Ƿ���true, ���Ƿ���false.
	
	in_data[conv::kWeight]��in_data[1], ��TBlob�Ķ���, ��˿��Ե���CheckContiguous����. �������in_data[1]���tensor���ڴ�
	�ǲ���������. 
	*/
	 
    // get data
    Stream<xpu> *s = ctx.get_stream<xpu>();
    Tensor<xpu, 4, DType> data = in_data[conv::kData].get<xpu, 4, DType>(s);
    // ����get����������������in_data[0]����4D��tensor, data. �������������. batch_size * channel K * �߶� * ���. 
    Shape<3> wmat_shape =
        Shape3(param_.num_group,
               param_.num_filter / param_.num_group,
               data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1]);
    /*
    ����wmat_shape, Ϊ���屾��(��l��)��Ȩ��wmat��׼��. �������Ȩ�ؾ����shape. 
    s0 = param_.num_group, ��num_group(���������и��num_group��partitions), Ĭ��Ϊ1;
	s1 = param_.num_filter / param_.num_group. num_filterΪ����˵ĸ���, ��ʹ��COnvolutionʱָ��, num_filter�ķ�Χ, 1-100000.
	s2 = data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1];
	data.shape_[1]��data shape�ĵڶ���������ֵ, ��channel K. ����Ϊ�����ǰһ������ͼ������.  
    param_.kernel[0]��kernel[0], ֻ��ʹ��param_������kernel. ��ʹ�þ�����ʱ��: kernel = (5,5).  
    param_.kernel[1]��kernel[1]. 
	*/           
               
    Tensor<xpu, 3, DType> wmat =
        in_data[conv::kWeight].get_with_shape<xpu, 3, DType>(wmat_shape, s);
    /*
	�������Ȩ��in_data[conv::kWeight]��in_data[1]����3ά������. ��������get_with_shape. ��������:
	mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get_with_shape(const mshadow::Shape<dim> & shape, 
	mshadow::Stream< Device > *stream = NULL)const, ����Shape<dim> & shape ����wmat_shape. 
	
	������Ȩ��wmat��3D������. Ȩ�صĴ�С��: wmat_shape. һ���, �����ľ����(weight)�ĸ���Ϊ:
	num_filter * (kernel[0] * kernel[1]). ����mxnet�����ʱ, �õ��� num_group ����, ��˽�wmat���ó�3D��, ���е�һ��ά����
	num_group����Ŀ. ����Ҫ����������������data�ֳ�num_group��partitions, ��ôÿ��partitions�ľ���˸�������:
	param_.num_filter / param_.num_group��.   
	*/    
        
    Tensor<xpu, 4, DType> grad = out_grad[conv::kOut].get<xpu, 4, DType>(s);
    Tensor<xpu, 4, DType> gdata = in_grad[conv::kData].get<xpu, 4, DType>(s);
    Tensor<xpu, 3, DType> gwmat =
        in_grad[conv::kWeight].get_with_shape<xpu, 3, DType>(wmat_shape, s);
    /*
	����get������������:
	��һ��(��l + 1��)�Ĳв�out_grad[conv::kOut]��out_grad[0], ����4D����.  
	
	������ʧ���ھ��������Ĳв�gdata, ��in_grad[conv::kData]��in_grad[0]����4D������. mxnet�ڽ��в�ļ���ʱ, �����ȷ�����ڴ�, 
	��in_grad�Ǿ����Ĳв�/�ݶȵ�����, Ҫ���ȷ������Щ�ڴ�.
	
	������ʧ���ھ����Ȩ��weight���ݶ�gwmat, ����get_with_shape��in_grad[conv::kWeight]��in_grad[1]����3D����. 
	����gwmat��shape�;����ľ���˵�Ȩ��wmat��shape��һ����.  
	
	��ʹ�ú���get��get_with_shapeʱ, һ����ģ���������. 
	*/
	    
#if defined(__CUDACC__)
    CHECK_EQ(s->blas_handle_ownership_, Stream<xpu>::OwnHandle)
        << "Must init CuBLAS handle in stream";
#endif // __CUDACC__��. 
    const index_t nbatch = data.size(0); // nbatch��batch_size. data.size(0)��data�ĵ�һά�Ĵ�С. 
    Tensor<xpu, 1, DType> workspace =
        ctx.requested[conv::kTempSpace].get_space_typed<xpu, 1, DType>(
            Shape1(this->InitTemp(data.shape_, grad.shape_)), s);
    /*
	������ǰ�򴫲��еĴ�����һ����. ����һ����ʱ�ڴ�ռ�workspace, ��1D��tensor. ������Ҫ������ڴ���Ϊ�����ռ���м���.
	InitTemp�������, һ����data.shape_, һ����grad.shape_. 
	*/
            
    for (index_t i = 0; i < nbatch; i += nstep_) {
      const index_t step = std::min(nstep_, nbatch - i);
      // ��ʵ�����й�����, nbatch == batch_size, nstep_ == batch_size. ���step == batch_size. ����;����ǰ�򴫲���һ����
	  // ����, ����ǰ�򴫲��Ĺ�����, ������������data, weight���������out. ��һ��һ�ݵĽ��м����(��nstep_ < nbatchʱ). 
	  // ���, ���򴫲�ʱ, �ڼ���в���ݶȵ�ʱ��Ҳ��һ��һ�ݵļ����. Ҳ�е㲢�е���˼.   
      Tensor<xpu, 2, DType> temp_col = Tensor<xpu, 2, DType>(workspace.dptr_,
                                               Shape2(shape_colunit_[0],
                                                      shape_colunit_[1] * step), s);
      Tensor<xpu, 3, DType> temp_dst = Tensor<xpu, 3, DType>(
                                               workspace.dptr_ + temp_col.shape_.Size(),
                                               Shape3(shape_dstunit_[0],
                                                      shape_dstunit_[1],
                                                      shape_dstunit_[2] * step), s);
	  /*
	  ��������shape_colunit_, shape_dstunit_, nstep_. 
  	  ��������ʱָ����, ����ConvolutionOp���˽�к���InitTemp��ָ����, ���°汾��convolution-inl.h��Ҳ��һ��˽�к���
  	  LayerSetUp, ����ָ��һЩֵ. ����ishape(��������shape)��oshape(�������shape)��, data.shape_, out.shape_
	  ����ȷ��OP���õ���һЩ������ֵ. 
	  */
	  
	  // cout<<"forward_nstep_: "<<nstep_<<endl; // nstep_��batch_size.  
	  /*cout<<"shape_colunit_[0]: "<<shape_colunit_[0]<<endl; 
	  // conv1 = mx.symbol.Convolution(data=data, kernel=(5,5), stride=(2,2), num_filter=10)ʱ, Ϊ25. 
	  // shape_colunit_[0]��: �����ǰһ������ͼ���� * kernel[0] * kernel[1]. ishape[1] * ksize_y * ksize_x.
	  cout<<"shape_colunit_[1]: "<<shape_colunit_[1]<<endl;
	  // Ϊ144, 4. �� ������������ͼ�ĸ߶� * ��� ֮��. oshape[2] * oshape[3].
	  cout<<"shape_dstunit_[0]: "<<shape_dstunit_[0]<<endl;
	  //  shape_dstunit_[0]��1. param_.num_group.  
	  cout<<"shape_dstunit_[1]: "<<shape_dstunit_[1]<<endl;
	  // shape_dstunit_[1]�Ǿ�������˵ĸ���. Ϊ10, 50. param_.num_filter / param_.num_group.
	  cout<<"shape_dstunit_[2]: "<<shape_dstunit_[2]<<endl;
	  // �� ������������ͼ�ĸ߶� * ��� ֮��.  oshape[2] * oshape[3].*/ 
	  /*
	  �;����ǰ�д����ı���һ��, ����Tensor�ṹ��Ĺ��캯��: Tensor<xpu, 2/3, DType>(..)������Tensor����temp_col��temp_dst.
	  temp_col��2D������, temp_dst��3D������. 
	  
	  ��num_group == 1�������: 
	  temp_col�Ĵ�СΪ: shape_colunit_[0] * (shape_colunit_[1] * step), ��: 
	  { �����ǰһ������ͼ���� * kernel[0] * kernel[1] } * { ������������ͼ�ĸ߶� * ��� * step }. 
	  temp_dst��СΪ: shape_dstunit_[0] * shape_dstunit_[1] * (shape_dstunit_[2] * step), ��:
	  { channel K } * { ��������˵ĸ��� } * { ������������ͼ�ĸ߶� * ��� ֮�� }.
	  */
                                                      
      temp_dst = reshape(swapaxis<1, 0>(grad.Slice(i, i + step)), temp_dst.shape_);
      /*
	  temp_dst��3D��tensor, shape�����Ѿ�������. grad����һ��(��l + 1��)�Ĳв�, ��4D��tensor.  
	  ������һ��(��l + 1��)�Ĳв��temp_dst, ��temp_dst�ʹ�����һ��Ĳв�. ֻ���ڸ�ֵ��ʱ����Ҫreshape.
	  
	  1)reshape����. �����: mshadow/mshadow/extension/reshape.h 48��. ��mshadow::exprc�����ռ���. 
	  template<typename SrcExp, typename DType, int etype, int dimdst>
	  inline ReshapeExp<SrcExp, DType, dimdst, ExpInfo<SrcExp>::kDim> reshape(const Exp<SrcExp, DType, etype> &src, 
	  Shape<dimdst> oshape) {...}. reshape a tensor to another shape.
	  src: Tensor<Device, dimsrc>, dimsrc��src��ά��, ��4.
	  oshape: target shape. ��Shape<dimdst> oshape, dimdst�����tensor��ά��, ��3.
	  return a expresion with type Tensor<Device, dimdst>.  
	  
	  2)swapaxis����. ���������mshadow/mshadow/extension/swapaxis.h 52��. ��mshadow::exprc�����ռ���. 
	  template<int a1, int a2, typename SrcExp, typename DType, int etype>
	  inline SwapAxisExp<SrcExp, DType, ExpInfo<SrcExp>::kDim, ExpInfo<SrcExp>::kDim - a1, a2> swapaxis(
	  const Exp<SrcExp, DType, etype> &src) {...}. reshapes a tensor to another shape.
	  src: Tensor<Device, dimsrc>, dimsrc��src��ά��, ��4. 
	  return a expresion with type Tensor<Device,dimdst>. dimdst�����tensor��ά��, ��4.
	  ģ�����a1: higher dimension to be swapped, assert a1 > a2. assert���ԭ�Ͷ�����<assert.h>��, 
	  ����������������������ش���, ����ֹ����ִ��.
	  ģ�����a2: lower dimension to be swapped. 
	  
	  reshape�Ĳ���src�൱����: grad.Slice(i, i + step). ������һ��Ĳв������Ƭ, һ��һ�ݵ�����. һ��forѭ��, ȡһ��(һ��
	  [nbatch/nstep_]��)�ϲ�Ĳв�, ����������Ĳв�, Ȩ���ݶ�, ƫ���ݶ�.
	  ����nbatch == nstep_, ��� grad.Slice(i, i + step) �� grad����ͬ��. iʼ��Ϊ0. ������һ��ȫ���Ĳв�grad�����������
	  ��ʧ���ھ��������Ĳв�, ��ʧ���ھ��������weight���ݶ�, ��ʧ���ھ����ƫ�õ��ݶ�. 
	  */
      
      if (param_.pad[0] == 0 && param_.pad[1] == 0) { // pad[0] == pad[1] == 0, ���������ݽ��в������. 
        temp_col = unpack_patch2col(data.Slice(i, i + step),
                                     param_.kernel[0],
                                     param_.kernel[1],
                                     param_.stride[0],
                                     param_.stride[1],
                                     param_.dilate[0],
                                     param_.dilate[1]);
        // ����temp_col, temp_col��2D��tensor. ��������unpack_patch2col�������. 
		// ����i == 0, step == batch_size, ���data�� data.Slice(i, i + step)����ͬ��. 
		// ��ͼ���(patches of image)unpack(��ѹ)��һ�������һ��, ������unpack_patch2col�õ�mat��, ����ʵ�־��ǰ��ͷ���. 
      } else {
        temp_col = unpack_patch2col(pad(data.Slice(i, i + step), param_.pad[0], param_.pad[1]),
                                     param_.kernel[0],
                                     param_.kernel[1],
                                     param_.stride[0],
                                     param_.stride[1],
                                     param_.dilate[0],
                                     param_.dilate[1]);
        // pad[0]��pad[1]��һ������0, ��������pad������data.Slice(i, i + step)���в������, Ȼ����ʹ��unpack_patch2col����. 
      } // ���ǰ������Ĵ�����һ����. data�Ǿ�������������, ����unpack_patch2col�������������ݽ���unpack, �õ�temp_col,
	  // ����ʵ�־�����ǰ��ͷ������.
	    
      const index_t gstride = temp_col.size(0) / param_.num_group;
      // gstride������index_t, ��unsigned int�͵�. ��ֵΪ temp_col.size(0) / param_.num_group.
   	  // �� shape_colunit_[0](�����ǰһ������ͼ���� * kernel[0] * kernel[1].) / num_group(Ĭ��Ϊ1, 
   	  // �����������и��num_group��partitions. num_groupò��ֻ����1).
		  
      for (uint32_t gid = 0; gid < param_.num_group; ++gid) { // ��ǰ��Ĳ�����һ����. gid == 0, ��ʼѭ��. һֱ��num_group.
	  	// һ��һ��partitions�Ĳ���. num_groupĬ��Ϊ1, �����и�����. 
        Tensor<xpu, 2, DType> tmpc = temp_col.Slice(gstride * gid, gstride * (gid + 1));
        /*
        ����tmpc, ��ǰ�򴫲�һ��. ��һ����ʱ��tensor, ���num_group != 1, ��ô�ͽ�temp_col�и�. 
		��Ĭ�� num_group == 1�������, gid == 0. gstride == temp_col.size(0). 
		���, Ϊtemp_col.Slice(0, gstride). ��, tmpc �� temp_col��һ����. ������Ϊû���� ���������и��num_group��partitions
		�������, ��� tmpc �� temp_col��һ����. 
		
		gid������þ���, ��һ��tensor(���������и��num_group��partitions), gid�ʹ���ÿ��
		partitions������, ���� gid ��partitions. ����������Slice�����и�temp_col, �и�ķ�Χ�� gid-gid+1, ����temp_col�и��
		1��partitions. ÿ��partitions(Ҳ��һ��tensor)�Ĵ�С�� gstride.
		*/
        
        if (i == 0) { // ��Ϊ nbatch == nstep_, ��� i == 0, ʼ��Ϊ0. 
          // cout<<"backward_i: "<<i<<endl; // 0 
          Tensor<xpu, 2, DType> tmp_gwmat = gwmat[gid]; // ���Կ��������. 
          /*
		  ��Ĭ��num_group == 1�������, gid��0. gwmat[gid]��gwmat[0], gid����gwmat���tensor��һά.
		  gwmat����ʧ���ھ����Ȩ��weight���ݶ�, ��3D��tensor, ��shape��wmat(�����ľ���˵�Ȩ��)shapeһ��:
		  { param_.num_group } * { param_.num_filter / param_.num_group } 
		  * { data.shape_[1] / param_.num_group * param_.kernel[0] * param_.kernel[1] }.
		  
		  ��Ĭ��num_group == 1�������, gwmat���3D��tensor, ���һά�����ֵ��0. gwmat[gid]��һ��2D��tensor, ��0.8�汾��mxnet
		  ��, ֻ��ʵ��2D���, �������ͼ��channel K == 1. �� tmp_gwmat���2D tensor�Ĵ�С����:
		  num_filter * { kernel[0] * kernel[1] }. �������Ǿ�������˵Ĳ����ĸ���. Ҳ����ʧ���ھ�������˲���weight���ݶ�
		  �ĸ���.
		  
		  �����Ƕ���tmp_gwmat, ����ʧ���ھ�������˲���weight���ݶ�. 
		  */
          
          Assign(tmp_gwmat, req[conv::kWeight], dot(temp_dst[gid], tmpc.T()));
          /*
		  Assign��ֵ����. �����tmp_gwmat, ����tmp_gwmat��ֵ; ���ݲ���ģʽ��req[conv::kWeight], �������Ȩ�ص����ݲ���ģʽ;
		  exp�� dot(temp_dst[gid], tmpc.T()).
		  ���� unpack_patch2col ������˵��, output: output = dot(weight, mat)�����Կ����Ǿ����Ľ��
		  (��ɢ�����, �����Ԫ�غͶ�Ӧλ�õ�Ԫ����������, �����dot. ����dot�����Ǿ���֮��ĵ����). 
		  
		  dot(temp_dst[gid], tmpc.T())��ȡ��ת��(��: A'*B �� B'*A���ò���һ��ת��). 
		  
		  ����temp_dst[gid], ��Ĭ��num_group == 1�������, ��Ϊtemp_dst[0]. temp_dst��3D��tensor, ��ֵ����һ��Ĳв�(������
		  reshape����). temp_dst���3D��tensor, ���һά�Ĵ�С��channel K. ��0.8�汾��Ϊ1, ������temp_dst[0]. 
		  temp_dst[0]��һ��2D��tensor, ���� i == 0, grad.Slice(i, i + step) �� grad����ͬ��, 
		  ������2D��tensor������һ��ȫ���Ĳв�.
		  
		  tmpc: ��Ĭ�� num_group == 1�������, gid == 0. gstride == temp_col.size(0). 
		  ���, Ϊtemp_col.Slice(0, gstride). ��, tmpc �� temp_col��һ����. ������Ϊû���� ���������и��num_group��partitions
		  �������, ��� tmpc �� temp_col��һ����. tmpc�������������data����unpack_patch2col������Ľ��, �����Կ����Ǿ��
		  ���������������!
		  
		  tmp_gwmat������ʧ���ھ�������˵Ĳ���weight���ݶ�:
		  ��ֵΪ: ���������� * ��һ��Ĳв�.    
		  */
          
        } else { // ���i != 0, ��ô���ۼӼ���. ÿ���ۼӵ�ֵ��: dot(temp_dst[gid], tmpc.T()). 
          gwmat[gid] += dot(temp_dst[gid], tmpc.T());
        }
      }

      for (uint32_t gid = 0; gid < param_.num_group; ++gid) { // gidѭ��, gid == 0. ֻ��һ��forѭ��. 
        Tensor<xpu, 2, DType> tmpc = temp_col.Slice(gstride * gid, gstride * (gid + 1));
        tmpc = dot(wmat[gid].T(), temp_dst[gid]);
        /*
		����һ��2D��tensor, tmpc. �����ʱ��������tmpc����һ��, ������ֵ�����ı�.
		tmpc���2D��tensor��ֵΪ: dot(wmat[gid].T(), temp_dst[gid]). �����������. tmpc�����涨��ʱ, �ǿ������������������. 
		
		num_group == 1�������, gid == 0. 
		1)wmat[0]���Ǿ����ȫ���ľ���˵�Ȩ��ֵ(����Ȩֵ����, ���һ������ͼ��Ҫһ�������). 
		wmat��3Dtensor, ���wmat[0]����2D��tensor. ���, Ĭ��num_group == 1, ���wmat�ĵ�һά������0. num_group == 1�������, 
	 	wmat[0]���� �����ȫ���ľ���˵�Ȩ��ֵ(����Ȩֵ����, ���һ������ͼ��Ҫһ�������). 
	   	��������, ����һ��������˵, ������Ȩ������ͬ��. 
		wmat[gid].T()ȡת��, T��Tensor�ṹ���µĳ�Ա����, ����ȡһ��tensor��ת��.   
		
		2)����temp_dst[gid], ��Ĭ��num_group == 1�������, ��Ϊtemp_dst[0]. temp_dst��3D��tensor, ��ֵ����һ��Ĳв�(������
		reshape����). temp_dst���3D��tensor, ���һά�Ĵ�С��channel K. ��0.8�汾��Ϊ1, ������temp_dst[0]. 
		temp_dst[0]��һ��2D��tensor, ���� i == 0, grad.Slice(i, i + step) �� grad����ͬ��, 
		������2D��tensor������һ��ȫ���Ĳв�. 
		
		tmpc����: ��������˵�ȫ��weight * ��һ��Ĳв�. 
		*/
      }
      
      // ������ʧ���ھ���������Ĳв�, gdata. ���������, pad[0]==pad[1]==0�Ͳ�ȫΪ0. 
      if (param_.pad[0] == 0 && param_.pad[1] == 0) {
        Assign(gdata.Slice(i, i + step), req[conv::kData],
               pack_col2patch(temp_col,
                              data.Slice(i, i + step).shape_,
                              param_.kernel[0],
                              param_.kernel[1],
                              param_.stride[0],
                              param_.stride[1],
                              param_.dilate[0],
                              param_.dilate[1]));
        /*
		������ʧ���ھ���������Ĳв�, gdata. pad[0]==pad[1]==0�������.
		Assign��ֵ����, ��ֵ�Ķ�����: gdata.Slice(i, i + step), ��;����ǰ����������Ƶ�. �����������������data���г���
	    [nbatcb/nstep_]��. ���Ҫһ�ݷݵĸ�ֵ; ���ݲ���ģʽ��req[conv::kData], ��������������ݵĲ���ģʽ; expΪ:
		pack_col2patch(...)�ķ���ֵ. unpack_patch2col�����ǽ�����������data����unpack, �Ա���о������; ��pack_col2patch��
		���෴, Ϊ�˵õ��в����.
		
		pack_col2patch������: mshadow/mshadow/extension/pack_col2patch.h 72�к�88��. �в�ͬ�Ĳ���. ������ѡ����:
		template<typename SrcExp, typename DType, int dstdim, int etype>
		inline PackColToPatchXExp<SrcExp, DType, dstdim> pack_col2patch(const expr::Exp<SrcExp, DType, etype> &src,
		Shape<dstdim> imshape, index_t psize_y, index_t psize_x, index_t pstride_y, index_t pstride_x,
		index_t pdilate_y, index_t pdilate_x) {..}. pack_col2patch�Ƿ������, ���������������(deconvolution)(mxnetҲ�������
		deconvolution�����˵�����һ��). Deconvolution�ǽ�Convolution�ķ��򷴹���--ǰ������, �����ǰ��. 
		����pack ��ͼ��. ������������, unpack_patch2col�����ǽ�����ͼ���tensor; pack_col2patch�ǽ�tensor�������ͼ.  
	
		mat: source matrix; Դ����Ϊ temp_col, ���Ծ���������data����unpack_patch2col��Ľ��, ���Կ����Ǿ��������������.  
		imshape: Ŀ��img��shape; data.Slice(i, i + step).shape_, ��һ��data��shape, ��Ϊi == 0, step == batch_size, ���Ծ���
		data.shape_, �����������data��shape. 
		psize_y: ÿ��patch�ĸ߶�;
		psize_x: ÿ��patch�Ŀ��;
		pstride_y: ÿ��patch��y�����ϵĻ�������;
		pstride_x: ÿ��patch��x�����ϵĻ�������;	 
		pdilate_y_: ÿ��patch��y�����ϵ�����ϵ��. ������dilate[0], ������˵�����ϵ��, y����. 
		pdilate_x_: ÿ��patch��x�����ϵ�����ϵ��. ������dilate[1], ������˵�����ϵ��, x����. 
	  	
	  	!!!!!!!��֪���ڸ�ʲô!!!!!!! 
		*/
		
		// cout<<"gdata[0][0].shape_[0]: "<<gdata[0][0].shape_[0]<<endl; // ��l - 1������ͼ�Ĵ�С. 
		// cout<<"gdata[0][0].shape_[1]: "<<gdata[0][0].shape_[1]<<endl;
                              
      } else {
        Shape<4> pshape = data.Slice(i, i + step).shape_;
        pshape[2] += 2 * param_.pad[0];
        pshape[3] += 2 * param_.pad[1];
        Assign(gdata.Slice(i, i + step), req[conv::kData],
               crop(pack_col2patch(temp_col,
                                   pshape,
                                   param_.kernel[0],
                                   param_.kernel[1],
                                   param_.stride[0],
                                   param_.stride[1],
                                   param_.dilate[0],
                                   param_.dilate[1]),
                    gdata[i][0].shape_));
		/*
		pad[0]��pad[1]��һ������0. Ҫ�ȶ��� imshape: Ŀ��img��shape. 
		���ȶ���pshape, ��Ϊ: data.Slice(i, i + step).shape_. ����pad[0]��pad[1]��Ϊ0, ��˶�pshape[2]��pshape[3]���мӺ�:
		pshape[2] += 2 * param_.pad[0];
        pshape[3] += 2 * param_.pad[1]; // ����pshape[2]���� 2 * pad[0]. �����������pshape���� imshape: Ŀ��img��shape.
		
		Ȼ���ٴ�����pack_col2patch����, ����temp_col, imshape: Ŀ��img��shapeΪpshape(�Ѿ���pad��Ϣ������������).
		
		�ڶ�pack_col2patch(...)ʹ��crop����, ���вü�. �ü��ɺ� gdata[i][0].shape_��gdata[0][0].shape_һ����С��tensor.
		
		gdata��4D��tensor, ���gdata[0][0]����2D��tensor, gdata[0][0].shape_������ʧ���ھ��������Ĳв��shape. 
		�ǵ�l - 1������ͼ�Ĵ�С.  
		*/
		// cout<<"gdata[0][0].shape_[0]: "<<gdata[0][0].shape_[0]<<endl; // ��l - 1������ͼ�Ĵ�С. 
		// cout<<"gdata[0][0].shape_[1]: "<<gdata[0][0].shape_[1]<<endl; 
      }
    }
    
    /*
    cout<<"grad.size(0): "<<grad.size(0)<<endl; // 64, batch_size 
	cout<<"grad.size(1): "<<grad.size(1)<<endl; // 10, ����˸���. 
	cout<<"grad.size(2): "<<grad.size(2)<<endl; // 12, �������������ͼ�߶�. 
	cout<<"grad.size(3): "<<grad.size(3)<<endl; // 12, ������������ͼ���.
	
	������ĳ��Ĳв�Ҫ�͸ò�������Сһ��!! ��ǰҲ����ô����. grad����һ��(��l + 1��)�Ĳв�. 
	*/ 
	
    if (!param_.no_bias) { // �����ʹ��ƫ��, Ҫ����ʧ���ھ����ƫ�õ��ݶ�.    
      Tensor<xpu, 1, DType> gbias = in_grad[conv::kBias].get<xpu, 1, DType>(s);
      // ��Ϊƫ����1D��tensor, �������ݶ�Ҳ��1D��. ����get������������in_grad[2]����1D��tensor, gbias.
      Assign(gbias, req[conv::kBias], sumall_except_dim<1>(grad));
      /*
	  Assign��ֵ����, ��ֵ����ʱgbias, ����ʧ���ھ�����ƫ��bias���ݶ�; ��Ŀ����ģʽ��kBias��; exp��sumall_except_dim<1>(grad).
	  �����˵�1ά�ȵ�, ����һ��(��l + 1��)�Ĳв�������. 
	  grad��4D��tensor. ��һ��(��l + 1��)����ʧ��������Ĳв�!  
	  
	  ���gbias�������ǿ��Զ��ϵ�. 
	  */
    }
  }

 private:
  inline index_t InitTemp(const mshadow::Shape<4> &ishape,
                          const mshadow::Shape<4> &oshape) {
    const int ksize_y = param_.kernel[0];
    const int ksize_x = param_.kernel[1]; // ����ksize_y��ksize_x, �ֱ���kernel[0]��kernel[1]. 
    shape_colunit_ = mshadow::Shape2(ishape[1] * ksize_y * ksize_x,
                                     oshape[2] * oshape[3]); // ����ishape��oshape, ��shape_colunit_��ֵ. ����temp_col. 
    shape_dstunit_ = mshadow::Shape3(param_.num_group,
                                     param_.num_filter / param_.num_group,
                                     oshape[2] * oshape[3]); // ����oshape, ��shape_dstunit_��ֵ. ����temp_dst. 
    // param_.workspace is in elements of sizeof(DType)
    // if param_.workspace is set to zero the nstep_ equals ishape[0] (batch)
    nstep_ = std::max(
        std::min(
            static_cast<index_t>(
                param_.workspace / (shape_colunit_.Size() + shape_dstunit_.Size())),
            ishape[0]),
        1U); // ��nstep_��ֵ. ��forѭ��. 
    // cout<<"nstep_: "<<nstep_<<endl; // batch_size. 

    mshadow::Shape<2> scol = mshadow::Shape2(shape_colunit_[0],
                                             shape_colunit_[1] * nstep_); // ����һ��2D��shape, ���С��:
	// { shape_colunit_[0] } * { shape_colunit_[1] * nstep_ }. 
    mshadow::Shape<3> sdst = mshadow::Shape3(shape_dstunit_[0],
                                             shape_dstunit_[1],
                                             shape_dstunit_[2] * nstep_); // ����һ��3D��shape, ���СΪ:
	// { shape_dstunit_[0] } * { shape_dstunit_[1] } * {shape_dstunit_[2] * nstep_} 
    index_t required_size = scol.Size() + sdst.Size();
    CHECK_GE(param_.workspace, required_size)
      << "\nMinimum workspace size: " << required_size * sizeof(DType) << " Bytes\n"
      << "Given: " << param_.workspace * sizeof(DType) << " Bytes";
    return required_size; // required_size���� scol.Size() + sdst.Size(); ������shape2�Ĵ�С(����ά���Ĵ�С���) + shape3��
	// ��С. 
  }

  ConvolutionParam param_;
  mshadow::Shape<2> shape_colunit_;
  mshadow::Shape<3> shape_dstunit_;
  index_t nstep_;
  /*
  shape_colunit_ , shape_dstunit_, nstep_�Ƕ�������ConvolutionOp�е���������, ����ʱ���һ��. 
  shape_colunit_��Shape<2>�����, ���ֻ��shape_colunit_[0]��shape_colunit_[1], ����һ����ά��shape;
  shape_dstunit_��Shape<3>�����, �����shape_dstunit_[0], shape_dstunit_[1], shape_dstunit_[2].
  
  ��������shape_colunit_, shape_dstunit_, nstep_. 
  ��������ʱָ����, ����ConvolutionOp���˽�к���InitTemp��ָ����, ���°汾��convolution-inl.h��Ҳ��һ��˽�к���
  LayerSetUp, ����ָ��һЩֵ. ����ishape(��������shape)��oshape(�������shape)����ȷ��OP���õ���һЩ������ֵ. 
  */
};  // class ConvolutionOp

template<typename xpu>
Operator* CreateOp(ConvolutionParam param, int dtype,
                   std::vector<TShape> *in_shape,
                   std::vector<TShape> *out_shape,
                   Context ctx);

#if DMLC_USE_CXX11
class ConvolutionProp : public OperatorProperty {
 public:
  std::vector<std::string> ListArguments() const override {
    if (!param_.no_bias) {
      return {"data", "weight", "bias"};
    } else {
      return {"data", "weight"};
    }
  }

  void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) override {
    using namespace mshadow;
    param_.Init(kwargs);
    if (param_.kernel.ndim() == 2) {
      param_.layout = param_.layout ? param_.layout.value() : mshadow::kNCHW;
      if (param_.stride.ndim() == 0) param_.stride = Shape2(1, 1);
      if (param_.dilate.ndim() == 0) param_.dilate = Shape2(1, 1);
      if (param_.pad.ndim() == 0) param_.pad = Shape2(0, 0);
    } else {
      CHECK_EQ(param_.kernel.ndim(), 3) << param_.kernel.ndim() << "D convolution not supported";
      param_.layout = param_.layout ? param_.layout.value(): mshadow::kNCDHW;
      if (param_.stride.ndim() == 0) param_.stride = Shape3(1, 1, 1);
      if (param_.dilate.ndim() == 0) param_.dilate = Shape3(1, 1, 1);
      if (param_.pad.ndim() == 0) param_.pad = Shape3(0, 0, 0);
    }
  }

  std::map<std::string, std::string> GetParams() const override {
    return param_.__DICT__();
  }

  bool InferShape(std::vector<TShape> *in_shape,
                  std::vector<TShape> *out_shape,
                  std::vector<TShape> *aux_shape) const override {
    using namespace mshadow;
    if (!param_.no_bias) {
      CHECK_EQ(in_shape->size(), 3) << "Input:[data, weight, bias]";
    } else {
      CHECK_EQ(in_shape->size(), 2) << "Input:[data, weight]";
    }
    // CHECK_EQ(out_shape->size(), 1) << "Output: [output]";
    out_shape->resize(1, TShape());
    const TShape &dshp = (*in_shape)[conv::kData];
    if (dshp.ndim() ==  0) return false;
    if (param_.kernel.ndim() == 2) {
      // 2d conv
      CHECK_EQ(dshp.ndim(), 4) \
          << "Input data should be 4D in batch-num_filter-y-x";
      Shape<4> dshape = ConvertLayout(dshp.get<4>(), param_.layout.value(), kNCHW);
      Shape<4> wshape = Shape4(param_.num_filter / param_.num_group, dshape[1] / param_.num_group,
                               param_.kernel[0], param_.kernel[1]);
      wshape = ConvertLayout(wshape, kNCHW, param_.layout.value());
      wshape[0] *= param_.num_group;
      SHAPE_ASSIGN_CHECK(*in_shape, conv::kWeight, wshape);
      if (!param_.no_bias) {
        SHAPE_ASSIGN_CHECK(*in_shape, conv::kBias, Shape1(param_.num_filter));
      }

      const index_t ksize_y = static_cast<index_t>(param_.kernel[0]);
      const index_t ksize_x = static_cast<index_t>(param_.kernel[1]);
      CHECK_EQ(dshape[1] % param_.num_group, 0) \
          << "input num_filter must divide group size";
      CHECK_EQ(param_.num_filter % param_.num_group, 0) \
          << "output num_filter must divide group size";
      CHECK_GT(param_.kernel.Size(), 0) \
          << "incorrect kernel size: " << param_.kernel;
      CHECK_GT(param_.stride.Size(), 0) \
          << "incorrect stride size: " << param_.stride;
      CHECK_GT(param_.dilate.Size(), 0) \
          << "incorrect dilate size: " << param_.dilate;
      CHECK(ksize_y <= dshape[2] + 2 * param_.pad[0]
            && ksize_x <= dshape[3] + 2 * param_.pad[1])
          << "kernel size exceed input";
      Shape<4> oshape;
      oshape[0] = dshape[0];
      oshape[1] = param_.num_filter;
      oshape[2] = (dshape[2] + 2 * param_.pad[0] -
          (param_.dilate[0] * (ksize_y - 1) + 1)) / param_.stride[0] + 1;
      oshape[3] = (dshape[3] + 2 * param_.pad[1] -
          (param_.dilate[1] * (ksize_x - 1) + 1)) / param_.stride[1] + 1;
      SHAPE_ASSIGN_CHECK(*out_shape, 0, ConvertLayout(oshape, kNCHW, param_.layout.value()));
      return true;
    } else if (param_.kernel.ndim() == 3) {
      // 3d conv
      CHECK_EQ(dshp.ndim(), 5) \
        << "Input data should be 5D in batch-num_filter-depth-y-x";
      Shape<5> dshape = ConvertLayout(dshp.get<5>(), param_.layout.value(), kNCDHW);
      Shape<5> wshape = Shape5(param_.num_filter / param_.num_group, dshape[1] / param_.num_group,
                               param_.kernel[0], param_.kernel[1], param_.kernel[2]);
      wshape = ConvertLayout(wshape, kNCDHW, param_.layout.value());
      wshape[0] *= param_.num_group;
      SHAPE_ASSIGN_CHECK(*in_shape, conv::kWeight, wshape);
      if (!param_.no_bias) {
        SHAPE_ASSIGN_CHECK(*in_shape, conv::kBias, Shape1(param_.num_filter));
      }

      const index_t ksize_d = static_cast<index_t>(param_.kernel[0]);
      const index_t ksize_y = static_cast<index_t>(param_.kernel[1]);
      const index_t ksize_x = static_cast<index_t>(param_.kernel[2]);
      CHECK_EQ(dshape[1] % param_.num_group, 0)
        << "input num_filter must divide group size";
      CHECK_EQ(param_.num_filter % param_.num_group, 0)
        << "output num_filter must divide group size";
      CHECK_GT(param_.kernel.Size(), 0) \
        << "incorrect kernel size: " << param_.kernel;
      CHECK_GT(param_.stride.Size(), 0) \
        << "incorrect stride size: " << param_.stride;
      CHECK_GT(param_.dilate.Size(), 0) \
        << "incorrect dilate size: " << param_.dilate;
      CHECK(ksize_d < dshape[2] + 2 * param_.pad[0]
            && ksize_y <= dshape[3] + 2 * param_.pad[1]
            && ksize_x <= dshape[4] + 2 * param_.pad[2])
        << "kernel size exceed input";
      CHECK_EQ(param_.dilate.Size(), 1)
        << "Dilate is not supported in 3d convolution";
      Shape<5> oshape;
      oshape[0] = dshape[0];
      oshape[1] = param_.num_filter;
      oshape[2] = (dshape[2] + 2 * param_.pad[0] -
          (1 * (ksize_d - 1) + 1)) / param_.stride[0] + 1;
      oshape[3] = (dshape[3] + 2 * param_.pad[1] -
          (1 * (ksize_y - 1) + 1)) / param_.stride[1] + 1;
      oshape[4] = (dshape[4] + 2 * param_.pad[2] -
          (1 * (ksize_x - 1) + 1)) / param_.stride[2] + 1;
      SHAPE_ASSIGN_CHECK(*out_shape, 0, ConvertLayout(oshape, kNCDHW, param_.layout.value()));
      return true;
    } else {
      LOG(FATAL) << "Unknown convolution type";
      return false;
    }
  }

  bool InferType(std::vector<int> *in_type,
                 std::vector<int> *out_type,
                 std::vector<int> *aux_type) const override {
    CHECK_GE(in_type->size(), 1);
    int dtype = (*in_type)[0];
    CHECK_NE(dtype, -1) << "First input must have specified type";
    for (index_t i = 0; i < in_type->size(); ++i) {
      if ((*in_type)[i] == -1) {
        (*in_type)[i] = dtype;
      } else {
        CHECK_EQ((*in_type)[i], dtype) << "This layer requires uniform type. "
                                       << "Expected " << dtype << " v.s. given "
                                       << (*in_type)[i] << " at " << ListArguments()[i];
      }
    }
    out_type->clear();
    out_type->push_back(dtype);
    return true;
  }

  OperatorProperty* Copy() const override {
    auto ptr = new ConvolutionProp();
    ptr->param_ = param_;
    return ptr;
  }

  std::string TypeString() const override {
    return "Convolution";
  }

  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
    return {out_grad[conv::kOut], in_data[conv::kData], in_data[conv::kWeight]};
  }

  std::vector<ResourceRequest> ForwardResource(
      const std::vector<TShape> &in_shape) const override {
    return {ResourceRequest::kTempSpace};
  }

  std::vector<ResourceRequest> BackwardResource(
      const std::vector<TShape> &in_shape) const override {
    return {ResourceRequest::kTempSpace};
  }

  Operator* CreateOperator(Context ctx) const override {
    LOG(FATAL) << "Not Implemented.";
    return NULL;
  }

  Operator* CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                             std::vector<int> *in_type) const override;

 private:
  ConvolutionParam param_;
};  // class ConvolutionProp
#endif  // DMLC_USE_CXX11
}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_CONVOLUTION_INL_H_
