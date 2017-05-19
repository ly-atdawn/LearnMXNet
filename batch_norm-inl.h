/*!
 * Copyright (c) 2015 by Contributors
 * \file batch_norm-inl.h
 * \brief
 * \author
*/
#ifndef MXNET_OPERATOR_BATCH_NORM_INL_H_
#define MXNET_OPERATOR_BATCH_NORM_INL_H_

#include <dmlc/logging.h> // mxnet����־ͷ�ļ�. ��dmlc-core/include/dmlc��, 
#include <dmlc/parameter.h> // mxnet�Ĳ���ͷ�ļ�, ��dmlc-core/include/dmlc��, ���������. 
#include <mxnet/operator.h> // ��include/mxnet��, �����������(operator), ����������, ������. ��OP��Prop�ĺ�����������. 
#include <map> // ����ʽ����, Ԫ�ص�ֵ��ĳ���ض��ļ������, ������ͨ��Ԫ���������е�λ�����ȡ. 
#include <vector> // ��������. 
#include <string> // �ַ���. 
#include <utility> // utilityͷ�ļ��������صĹ�ϵ�����, �򻯹�ϵ�������д��, ��������pair����,
// pair������һ��ģ������, ���Դ洢һ��ֵ. 
#include "./operator_common.h" // src/operator��, mxnet�Ĳ�һЩ���õ�����.
#include "./mshadow_op.h" // src/operator��, ������һЩ�ṹ��. ��Щ�ṹ��������������ʵ��ĳЩ���ǰ������ͷ������, �缤��� 
// ����softplus, softplus_grad. һ������ǰ������, һ�����㷴������. 

#include <iostream>

using namespace std;

namespace mxnet {
namespace op {

namespace batchnorm {
enum BatchNormOpInputs {kData, kGamma, kBeta}; // BN���������, kDataΪ0, kGammaΪ1, kBetaΪ2. ������ѵ��ʱ, gamma��beta��ֵ��
// �Զ�����batch������һ��, Ҳ���Բ�һ��,  
enum BatchNormOpOutputs {kOut, kMean, kVar}; // BN����������, kOutΪ0, kMeanΪ1, kVarΪ2. ����kData�������ȼ����kMean��kVar
// Ȼ���ڴ˻�����, ����kGamma��kBeta����kOut. (�÷��Ŵ����˱���). 
enum BatchNormOpAuxiliary {kMovingMean, kMovingVar}; // BN�����ĸ�������, kMovingMeanΪ0, kMovingVarΪ1. ����ǰ�����ʱ�ܸ���
// �������������. Ϊ���batch���ݵ�Mean��Var����. Ϊ�˷���������Ҫ�ĸ��ӵ�tensor. 
enum BatchNormBackResource {kTempSpace}; // ���򴫲�����Դ����, ����һ����ʱ�ռ�, ����ռ�����������С��. 
/*
��Щ������Ҫ������ڴ���Ϊ�����ռ���м���, ����˵BatchNormBackward. ���������, 
ϵͳ��ÿ��Զ��ⲿ���ڴ���й���, ����ϵͳ������һЩ�Ż�, ����˵�ڴ���ظ�����.
struct ResourceRequest {
  enum Type {
    kRandom,  // get an mshadow::Random<xpu> object
    kTempSpace,  // request temporay space
  };
  Type type;
};
*/ 
}  // namespace batchnorm

struct BatchNormParam : public dmlc::Parameter<BatchNormParam> { // BatchNormParam, BN���������ṹ��, ��BN��Ĳ�����������, ��
// �ó�ֵ, �趨��Χ��. 
  float eps; // eps, ��BN�����д� x^(k) --> X^(k)ʱ, Ҫx^(k)��ȥ��������ֵ, ��������������, ���Է���ʱΪ��Ϊ0, ��Ϊ
  // var[x^(k)] + eps.  
  float momentum; // momentum, momentum��moving average�Ķ�����. float, ��ֵ��0.9f.  
  bool fix_gamma; // fix_gamma, bool. ��ѵ���������Ƿ�̶���������gamma. 
  bool use_global_stats; // bool.  
  bool output_mean_var; // bool. �Ƿ����������ֵ�ͷ���.  
  DMLC_DECLARE_PARAMETER(BatchNormParam) {
    DMLC_DECLARE_FIELD(eps).set_default(1e-3f)
    .describe("Epsilon to prevent div 0"); // epsilon, DMLC_DECLARE_FIELD��, ���������eps. set_default���ó�ֵΪ1e-3f, 
	// describe��������.  
    DMLC_DECLARE_FIELD(momentum).set_default(0.9f)
    .describe("Momentum for moving average"); // momentum��ֵΪ0.9f.  
    DMLC_DECLARE_FIELD(fix_gamma).set_default(true)
    .describe("Fix gamma while training"); // ��ѵ������ʱ, Ĭ�Ϲ̶���������gamma.  
    DMLC_DECLARE_FIELD(use_global_stats).set_default(false)
    .describe("Whether use global moving statistics instead of local batch-norm. "
              "This will force change batch-norm into a scale shift operator.");
    /*
	����use_global_stats, �ο�(caffe+��������ѧϰ���������Ӽ�+caffeѵ��ʱ������+dropout/batch Normalization ).
	use_global_stats == trueʱ��ǿ��ʹ��ģ���д洢��BatchNorm���ֵ�뷽�����, ���ǻ��ڵ�ǰbatch�ڼ����ֵ�ͷ���. 
	
	��BatchNormlization���½��ܵ�BN����, ѵ���������ǻ���mini-batch��. , BN�ǻ���mini-batch��:
	����һ��mini-batch������{x1, x2, ..., xm}, ͨ����m������������mean, var. Ȼ����� xi^(~), ���൱����BN������������. �ڼ���
	BN������y^(i). ����{x1, x2, ..., xm}��һ��batch������, �������������ݼ���.
	use_global_stats == trueʱ, ���൱����ʹ���������ݼ��ļ�����{x1, x2, ..., xN}��ΪBNǰһ�������.
	���ڲ��Խ׶�, ��ֵ�ͷ����Ѿ��������ĳһ��Batch��, ��������������ݼ�����. ���, ��ѵ�������г���������ǰ�򴫲��ͷ�����
	֮��, ���ǻ�Ҫ��¼ÿһ��Batch�ľ�ֵ�ͷ���, �Ա�ѵ�����֮�󰴼�������ľ�ֵ�ͷ���.  
	
	����ǰ�򴫲�, һ��������һ��batch������; Ȼ���ٷ��򴫲�. ����һ��batch������, �������T����ֹ, �ٽ���дһ��bath���ݵĵ���.
	��, ����ÿһ��batch������, �������T��. �����������ݼ�, ����һ������epoch��.     
	*/
    
    DMLC_DECLARE_FIELD(output_mean_var).set_default(false)
    .describe("Output All,normal mean and var"); // Ĭ�ϲ�������ݵľ�ֵ�ͷ���.  
  }
};

/*
һ��BN layer����FC��conv�ĺ���, ���dlib����bn_fc��bn_con��.  
*/

template<typename xpu> // ģ�����ֻ��xpu.  
class BatchNormOp : public Operator { // BatchNormOp, BN������.   
 public:
  explicit BatchNormOp(BatchNormParam param) {
    /*
	BatchNormOp, BN������Ĺ��캯��: C++�е�explicit�ؼ���ֻ����������ֻ��һ���������๹�캯��, ���������Ǳ����ù��캯������ʾ
	��, ������ʽ��. param��BN������Ķ���, ����param������BN�Ĳ���.  
	*/
    this->param_ = param; // BatchNormParam param_, ����BatchNormParam�ṹ���һ������.  
  }

  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_states) {
    /*ǰ�����, �麯��. ������ʵ�������ж���. ����Ҫ����ֵ. ����Ϊ�� l ��. 
	in_data: ��������data, ����kData, kGamma, kBeta.
	req: ���ݲ���ģʽ. 
	out_data: �������, out. ��ѵ����ʱ�򱾲����������.  
	aux_states: ��ʾ����Ϊ�˷���������Ҫ�ĸ��ӵ� tensor. ���ӵ�Tensor������: kMovingMean, kMovingVar. ��ǰ���Ĳ�����ûʹ��
	aux_states����������. 
	*/
    using namespace mshadow;
    using namespace mshadow::expr;
    
    CHECK_EQ(in_data.size(), 3);
    CHECK_EQ(aux_states.size(), 2);
    /*
	in_data������С��3, ��������Tensor, ����kData, kGamma, kBeta.
	aux_states������С��2, �����������ӵ�Tensor, ����kMovingMean, kMovingVar.    
	*/
    
    if (ctx.is_train) {
      CHECK_EQ(out_data.size(), 3);
      CHECK_EQ(req.size(), 3);
      /*
	  ctx��OpContext�ṹ�嶨��ĳ�Ա. OpContext�ṹ�嶨���include/mxnet/operator.h. ����ctx��Ա���ʽṹ����is_train:
	  int is_train; // operator���ڽ��� train ���� test (is_train); 
	  
	  ��ѵ���׶�, out_data��������С��3, ������BN�������, Ҫ����mean, var, out. ���õ����ݲ���ģʽҲ��3��.  
	  */
      
    } else {
      CHECK_GE(out_data.size(), 1);
      CHECK_GE(req.size(), 1);
      CHECK_EQ(req[batchnorm::kOut], kWriteTo);
      /*
	  �������test/predict�׶�, out_data������СΪ1. BN������ֻ�����out. ��Ӧ�����ݲ���ģʽҲ��1��. �������ݲ���ģʽ��:
	  kWriteTo, ��out����� tensor �ṩ���ǿ���ֱ��д���ԭʼ���ڴ��. 
	  */
    }

    Stream<xpu> *s = ctx.get_stream<xpu>();
    const real_t scale = static_cast<real_t>(in_data[batchnorm::kData].shape_[1]) /
                         static_cast<real_t>(in_data[batchnorm::kData].shape_.Size());
    /*
	static_cast < type-id > ( expression ), C++�±�׼������ĸ�ת����, ��static_cast, dynamic_cast, reinterpret_cast��
	const_cast. static_cast���������expressionת��Ϊtype-id����, ��û������ʱ���ͼ������֤ת���İ�ȫ��. ����expressionת����
	real_t�͵�, ��float��. 

	in_data[batchnorm::kData].shape_[0]: 65 ��һά��batch_size�Ĵ�С. 
	in_data[batchnorm::kData].shape_[1]: 10 �ڶ�ά��BNǰһ������ͼ�ĸ���.  
	in_data[batchnorm::kData].shape_[2]: 47 
	in_data[batchnorm::kData].shape_[3]: .. ����ά�͵���ά�����ݵĴ�С. 
	in_data[batchnorm::kData].shape_.Size():  427700
	
	���BN��ǰһ����FC��, shape_[0]Ϊbatch_size; shape_[1]ΪFC��Ľ�����, �����������, һ��������һ������ͼ. 
	
	shape_.Size()����in_data[batchnorm::kData]��BN���������ݸ���ά�ȵĳ˻�. ���������ݵ��ܸ���. 
	
	scale��real_t���͵�(float����), ��ֵ����: ǰһ������ͼ(���)�ĸ��� / һ��batch��������(BN�����������)���ܸ���.   
	*/
	/*cout<<"in_data[batchnorm::kData].shape_[0]: "<<in_data[batchnorm::kData].shape_[0]<<endl;
	cout<<"in_data[batchnorm::kData].shape_[1]: "<<in_data[batchnorm::kData].shape_[1]<<endl;
	cout<<"in_data[batchnorm::kData].shape_[2]: "<<in_data[batchnorm::kData].shape_[2]<<endl;
	cout<<"in_data[batchnorm::kData].shape_[3]: "<<in_data[batchnorm::kData].shape_[3]<<endl;
	cout<<"in_data[batchnorm::kData].shape_.Size(): "<<in_data[batchnorm::kData].shape_.Size()<<endl;*/
	
    Tensor<xpu, 4> data; // data, xpu�µ�4ά����. 
    Tensor<xpu, 4> out; // out, xpu�µ���ά����. 
    if (in_data[batchnorm::kData].ndim() == 2) { // ���in_data[batchnorm::kData]��BN�������������2ά��, ��ô��Ҫ�ȶ���dshape
	  // Ȼ���ٽ�in_data[batchnorm::kData]����4ά������.
	  /*====================================================================================================================== 
	  BN��ǰΪFC��, ��FC��������� num_hidden, ��mean��var��ά��Ϊnum_hidden, Ȼ��mean����� batch_size * num_hidden *
	  1 * 1����ִ�� data - mean�Ĳ���. ��mxnetд��batch_norm-inl.h�Ĵ������ǰһ����FC��ͬ������, ���Խ�FC�����out��չ��
	  batch_size * num_hidden * 1 * 1��, ����ΪBN�������. BN���ǰһ����FC��ʱ, ���ۺ�ʵ���ǿ��Խ��������.
	  
	  ��BN��ǰΪFC��, ��ôin_data[batchnorm::kData].ndim() == 2, Ҫ���FC�ļ���ֵʹ��BN����, ��Ҫ�Ƚ�FC�ļ���ֵdata����4ά��
	  ����, ��СΪ: batch_size * num_hidden *1 * 1.  
	  */ 
      Shape<4> dshape = Shape4(in_data[batchnorm::kData].shape_[0],
                               in_data[batchnorm::kData].shape_[1], 1, 1);
      /*
	  ����dshape, 4άshape. Shape4����:
	  MSHADOW_XINLINE Shape<4> Shape4(index_t s0, index_t s1, index_t s2, index_t s3){
	      Shape<4> s;
          s[0] = s0; s[1] = s1; s[2] = s2; s[3] = s3;
  		  return s; } 
  	  s0 = in_data[batchnorm::kData].shape_[0], ��batch_size, dshape[0]; s1 = in_data[batchnorm::kData].shape_[1], ��BN��ǰһ��
	  ����ͼ�ĸ���, �����ǰ���Ӳ����ֵ�, ���ǽ�����, dshape[1]; s3 = s4 =1, dshape[2], dshape[3].  	  
	  */
	  
      data = in_data[batchnorm::kData].get_with_shape<xpu, 4, real_t>(dshape, s);
      out = out_data[batchnorm::kOut].get_with_shape<xpu, 4, real_t>(dshape, s);
      /*
	  ��in_data[0](��������)����4ά������. ���ｫTBlob��������Tensor����ʱû��ʹ��FlatTo2D, ��������get_with_shape. ��������:
	  mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get_with_shape(const mshadow::Shape<dim> & shape, 
	  mshadow::Stream< Device > *stream = NULL)const. ����shape, ��TBlob����һ��Tensor. ���shape�ʹ洢�Ĵ�С��һ��ʱ, �ᱨ��.
	  
	  ����BN������out, ��out_data[batchnorm::kOut]����get_with_shape����4ά����. 
	  */
      
    } else {
      data = in_data[batchnorm::kData].get<xpu, 4, real_t>(s);
      out = out_data[batchnorm::kOut].get<xpu, 4, real_t>(s);
      /*
	  BN����������ݲ���2ά��, ����4ά��. ��ֱ��ʹ��get������in_data[batchnorm::kData], out_data[batchnorm::kOut]����4ά������.
	  mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get(mshadow::Stream<Device> *stream = NULL)const. 
	  */
    }// if else���ִ�еĽ�������Ƶ�, ���Ƕ���4ά����data��out. ������BN���ǰһ��, �����������ݵ�ά����ȷ��data��out���ȷ��. 
    
    Tensor<xpu, 1> slope = in_data[batchnorm::kGamma].get<xpu, 1, real_t>(s); // gamma. 
    Tensor<xpu, 1> bias = in_data[batchnorm::kBeta].get<xpu, 1, real_t>(s); // beta. 
    Tensor<xpu, 1> moving_mean = aux_states[batchnorm::kMovingMean].get<xpu, 1, real_t>(s);
    Tensor<xpu, 1> moving_var = aux_states[batchnorm::kMovingVar].get<xpu, 1, real_t>(s);
    /*
	����get������:
	in_data[batchnorm::kGamma]��in_data[1]����1ά������, ������. slope. ��ԭ���е�gamma.
	in_data[batchnorm::kBeta]��in_data[2]����1ά������. bias. ���㷨�е�beta.
	aux_states[batchnorm::kMovingMean]��aux_states[0]����1ά������, moving_mean. 
	aux_states[batchnorm::kMovingVar]��aux_states[1]����1ά������, moving_var. 
	
	aux_states�����е������������������. ��ȡmoving average, ���use_global_stats == true, ��ô��Ҫʹ�� moving average.  
	moving_mean��moving_var�г�ֵ, �ڷ��򴫲������л����BN��������ֵmean�ͷ���var���и���. 
	*/
	
    if (param_.fix_gamma) slope = 1.f; // �����ѵ���׶ι̶�gamma, ��ô��ֱ����slope = 1.f. 
    
    /*
	��BN������ľ�ֵmean�ͷ���var�ǻ���mini-batch��, ����һ��batch����������{x1, x2, ..., xm}����0��ֵ, 1����. ����Ե�������! 
	�����Ƕ�һ������������xi, �������ֵmean�ͷ���var: mean = 1/n * (sum( xij )), �ټ���yi. 
	mean = 1 / m * (sum( xi )), xi��һ��batch��BN�������. 
	
	BN�������������xi, ���������yi, �м����ʱxi^(^). 
	*/

	/*====================================================================================================================  
    һ��BN layer����FC��conv�ĺ���, ���dlib����bn_fc��bn_con��. BN�������data��4ά������, ���Ҳ��4ά������. 
	1>BN��ǰΪconv��, ����������ͼ�ĸ�����n��, ��ômean��var��nά������, ������ͼ�ĸ��������Ӧ��; Ȼ���ټ��� xi^(^)ʱ, 
	�Ƚ�mean����� batch_size * n * Nh * Nw(Nh�Ǿ��������ͼ�ĸ߶�, Nw�Ǿ��������ͼ�Ŀ��); Ȼ��ſ��Խ��� data - mean��
	����. ���Ǵ� batch * n������ͼ�õ�mean]��var�Ĺ���û̫������.
	  
    2>BN��ǰΪFC��, ��FC��������� num_hidden, ��mean��var��ά��Ϊnum_hidden, Ȼ��mean����� batch_size * num_hidden *
	1 * 1����ִ�� data - mean�Ĳ���. ��mxnetд��batch_norm-inl.h�Ĵ������ǰһ����FC��ͬ������, ���Խ�FC�����out��չ��
	batch_size * num_hidden * 1 * 1��, ����ΪBN�������. BN���ǰһ����FC��ʱ, ���ۺ�ʵ���ǿ��Խ��������. 
	
	����ǰһ����FC��, BN�������������2ά��: batch_size * num_hidden. ���Ҫ������data������batch_size * num_hidden * 1 * 1��.
	����data�����out�Ĵ�С��һ�µ�; 
	����ǰһ���Ǿ����, ���dataֱ��ʹ��BN��������.
	
	ǰ�򴫲�������, ���򴫲�Ҳ������. 
	======================================================================================================================*/
	
    // whether use global statistics
    if (ctx.is_train && !param_.use_global_stats) { // ������ѵ���׶�. ���Ҳ�ʹ��use global statistics. ����ѵ���׶β�ʹ��
	  // use_global_stats, �������粻������. ѵ���׶λ���mini-batch��BN����, ��Ե�ǰ mini-batch ���������ͷ���. 
      Tensor<xpu, 1> mean = out_data[batchnorm::kMean].get<xpu, 1, real_t>(s);
      Tensor<xpu, 1> var = out_data[batchnorm::kVar].get<xpu, 1, real_t>(s);
      /*����get������:
	  out_data[batchnorm::kMean]��out_data[1]����1ά������. ����BN����ļ����ֵ(����ֵ�ľ�ֵ). 
	  out_data[batchnorm::kVar]��out_data[2]����1ά������. ����BN����ļ��㷽��(����ֵ�ķ���). 
	  */
	  
	  /*==================================================================================================================== 
	  1)mean��var����1ά������, ������. ��Ȼ������, ���ǿ��Ե�����������, �� mean = 1.f����ȷ��. 
	  ======================================================================================================================*/
      
      CHECK(req[batchnorm::kMean] == kNullOp || req[batchnorm::kMean] == kWriteTo);
      CHECK(req[batchnorm::kVar] == kNullOp || req[batchnorm::kVar] == kWriteTo);
      /*
	  BN����ļ����ֵ�ͷ�������ݲ���ģʽ��kNullOp����kWriteTo(tensor����ֱ��д���ԭʼ���ڴ��).  
	  */
      
      // The first three steps must be enforced.
      mean = scale * sumall_except_dim<1>(data);
      var = scale * sumall_except_dim<1>(F<mshadow_op::square>(
          data - broadcast<1>(mean, data.shape_)));
      /*
	  �������ѵ���׶�, ���Ȼ���mini-batch����BN����ľ�ֵmean�ͷ���var. scale��real_t���͵�
	  (float����), ��ֵ����: ǰһ������ͼ(���)�ĸ��� / һ��batch��������(BN�����������)���ܸ���. �������BN��ǰһ����FC��, 
	  scale = 1 / batch_size; ���ھ����scale = 1 / (batch_size * ��������ά���˻�). 
	  
	  data��BN�����������, ������һ��batch������. data��4ά������, data[0]��batch_size, ����������; data[1]��һ��������
	  channel, ��1����3(3ά�Ĳ��ܼ���); data[2]��һ�������ĸ߶�(���������); data[3]��һ�������Ŀ��(���������). 
	  1)mean:
	  mean = scale * sumall_except_dim<1>(data); scale��һ����, ����ԭ��Algorithm1�е� 1 / m. 
	  
	  sumall_except_dim�����mshadow/mshadow/extension/reduceto1d.h44��:
	  template<int dimkeep,  typename SrcExp, typename DType, int etype>
	  inline ReduceTo1DExp<SrcExp, DType, red::sum, ExpInfo<SrcExp>::kDim - dimkeep> sumall_except_dim(const Exp<SrcExp, 
	  DType, etype> &exp){...}. sumall_except_dim�Ĺ����ǶԳ�dimkeepά����, ����exp��ά�Ƚ������. 
	  ����expresion with type Tensor<Device,1>. ����:
	  exp: ������ʽ, ������һ��Tensor<?,2>, ��һ������.
	  dimkeep: ��Ҫ������expά��. ά�ȴ�0��ʼ����. 
	  
	  sumall_except_dim<1>(data)����һ��batch�������������(����data[1]), �����ݾ���ĺ�. ��sum( xi ), 
	  xi��ӦBN����������ݾ���. sum( xi )������ļӷ�. 
	  
	  ������ִ�еľ���: mean = (1 / m) * sum( xi ). 
	  
	  2)var:
	  �þ����ִ�еľ���:
	  var = (1 / m) * sum( xi - mean). 
	  scale��һ����, ����ԭ��Algorithm1�е� 1 / m.
	  sum( xi - mean)Ϊ: sumall_except_dim<1>(F<mshadow_op::square>(data - broadcast<1>(mean, data.shape_))).
	  
	  F<mshadow_op::square>(a)��һ����Ŀ�����, �������mshadow_op::square, ��src/operator/mshadow_op.h�µ�struct square, 
	  ����DType a, return DType(a * a). ����a��: data - broadcast<1>(mean, data.shape_), �� xi - mean, BN���ÿ������ - batch
	  ����������ľ�ֵ. 
	  
	  broadcast<1>(mean, data.shape_), broadcast��: mshadow/mshadow/extension/broadcast.h 69��:
	  template<int dimcast, typename SrcExp, typename DType, int etype, int dimdst>
	  inline Broadcast1DExp<SrcExp, DType, dimdst, dimdst - dimcast> broadcast(const expr::Exp<SrcExp, DType, etype> &src, 
	  Shape<dimdst> shape) {..}. 
	  src Tensor<Device,1>; shape: shape of output; ���� a expresion with type Tensor<Device, dimdst>, dimdstΪ4, 
	  ���ص�Tensor��ά��Ϊ4, ��shape�ĸ������йص�.
	  * input: Tensor<Device,1>: ishape[0]
	  * output: Tensor<Device,dimdst> : oshape[dimcast] = ishape[0].
	  ��һ��1ά�� Tensor ����� dimdst ά�� Tensor. Ϊ����ȷ����!! 
	  
	  mean�Ǽ�ά��Tensor������ȷ˵������!! 
	  
	  Ϊ�˼���xi - mean, BN���ÿ�������ļ���ֵ - batch������ֵ�ľ�ֵ. ���еĲ�����: data - broadcast<1>(mean, data.shape_), 
	  �����Ҫ��mean���䵽��dataһ����С���ܽ�����ȷ�ؼ���. broadcast<1>(mean, data.shape_)���ǽ�mean(1ά��Tensor)����ɺ�
	  dataһ����С��Tensor, ��Tensor<xpu, 4>.  �� (broadcast<1>(mean, data.shape_))[0]ΪBatch_size; 
	  (broadcast<1>(mean, data.shape_))[1]Ϊchannel; (broadcast<1>(mean, data.shape_))[2]Ϊdata�ĸ߶�;
	  (broadcast<1>(mean, data.shape_))[3]Ϊdata�Ŀ��. ���, data - broadcast<1>(mean, data.shape_)����
	  BN���ÿ�������ļ���ֵ - batch������ֵ�ľ�ֵ, ��x1 - mean, x2 - mean, ..., xm - mean.
	  
	  Ȼ���data - broadcast<1>(mean, data.shape_)ƽ��������ȡscale����. ���ʱ����meanʱ������һ��, ���Կ����� 
	  (x1 + mean) + (x2 + mean) + ... + (xm - mean). 
	  */    
          
      Assign(out, req[batchnorm::kOut], broadcast<1>(slope, out.shape_) *
             (data - broadcast<1>(mean, data.shape_)) /
             F<mshadow_op::square_root>(broadcast<1>(var + param_.eps, data.shape_)) +
             broadcast<1>(bias, out.shape_));
      /*
	  Assign��ֵ����, out��BN������, req�����ݲ���ģʽ, exp�� gamma * [(data - mean) / (var + eps)^(1/2)] + beta, 
	  gamma��slope, beta��bias. 
	  
	  expΪ: broadcast<1>(slope, out.shape_) * (data - broadcast<1>(mean, data.shape_)) 
	  / F<mshadow_op::square_root>(broadcast<1>(var + param_.eps, data.shape_)) 
	  + broadcast<1>(bias, out.shape_) 
      ���Ƚ�slope����ɺ�out������ͬshape��Tensor(gamma); �ٳ�(data - broadcast<1>(mean, data.shape_)), ��data - mean;
	  Ȼ��F<mshadow_op::square_root>��һ����Ŀ�����, �������mshadow_op::square_root, �ṹ��mshadow_op::square_root����
	  (DType a, ����DType(sqrtf(a)), ��float�͵�a^(1/2), ����broadcast<1>(var + param_.eps, data.shape_)�����Ų���, 
	  broadcast<1>(var + param_.eps, data.shape_)��(var + eps), ����, var��1ά������, ���Ե���������, ���var + eps��Ч. 
	  (var + eps)�Ľ������Tensor<xpu, 1>��, ����ٽ�(var + eps)����ɺ�data����һ��shape��Tensor.; ������beta,
	  ��broadcast<1>(bias, out.shape_), ��bias����ɺ�out����һ��shape��Tensor.
	  */
      
    } else {
      /*
	  ��train�׶�, ��ÿһ��minibatchʹ��BN, ��ô, ��test/predict��ʱ����, ������������ʹ������train-set�����mean. 
	  ����train-set���������ǳ���, ����mean�������ǳ���, ���Ծ������õļ�����ʹ��moving average�㷨, ��Ϊ����ѵ����������Ҫ��¼
	  ÿһ��Batch�ľ�ֵ�ͷ���, �Ա�ѵ�����֮������ʽ��������ľ�ֵ�ͷ���:
	  E[x] = Eb[meanb]; Var[x] = (m / (m - 1)) * Eb[varb].
	  meanb�ǵ�b��batch��mean, varb�ǵ�b��batch��var.
	  
	  ��test/predict�׶�, ������use_global_stats == trueʱ(��������ʵ���Կ�����һ�����, ��ѵ���׶�, use_global_stats == false
	  ���������ǲ�������). ʹ��moving average�㷨�������������Լ���mean��var. 
	  
	  ��ͳ��ѧ��, moving average�㷨��ͨ���������ݼ���һϵ�в�ͬ�Ӽ��ľ�ֵ���������ݵ�. 
      MovingAverage�ɷ���Ϊ����ƽ�����ƶ�ƽ��, ����ʱ������Ԥ��ʱ�õ��ļ򵥷���. 
	  ���㷽��: ����һ������������, �����趨һ���̶���ֵk, Ȼ��ֱ�����1���k��, ��2���k+1��, ��3���k+2���ƽ��ֵ, 
	  ��������. 
	  */ 
      Assign(out, req[batchnorm::kOut], broadcast<1>(slope /
                                          F<mshadow_op::square_root>(moving_var + param_.eps),
                                          data.shape_) * data +
             broadcast<1>(bias - (slope * moving_mean) /
                          F<mshadow_op::square_root>(moving_var + param_.eps), data.shape_));
      /*
	  Assign��ֵ����, out��BN������, req�����ݲ���ģʽ, exp�� gamma / (var[x] + eps)^(1/2) * x + 
	  (beta - gamma * E[x] / (var[x] + eps)^(1/2)). ����ʵ��ʱ, x��data, Ϊ��ʹ���ܺ�data���м���, Ҫ��һЩʽ�ӽ�����չ, ��չ��
	  ��data����ͬ����С��shape. ����ʹ����moving average�㷨, ����� moving_var ���var, ��moving_mean���mean. ��ʽ��дΪ:
	  *1 + *2. 
	  slope��gamma, bias��beta. 
	  
	  �� a = F<mshadow_op::square_root>(moving_var + param_.eps), F<mshadow_op::square_root>����Ŀ��������, moving_var��1ά����,
	  ��eps���. Ȼ������broadcast<1>(), �� slope / a ��չ�ɺ�data����ͬ��shape��Tensor, �� 
	  broadcast<1>(slope / a, data.shape_), Ȼ���ٺ� data ���, ���ɵ� *1.
	  
	  �� b = F<mshadow_op::square_root>(moving_var + param_.eps), data.shape_), ���a��һ����. Ȼ��ִ�� 
	  bias - (slope * moving_mean) / b, �ٽ������broadcast<1>()չ�ɺ�data����ͬ��shape��Tensor, �� *2.    
	  */
    }
  }

  virtual void Backward(const OpContext &ctx,
                        const std::vector<TBlob> &out_grad,
                        const std::vector<TBlob> &in_data,
                        const std::vector<TBlob> &out_data,
                        const std::vector<OpReqType> &req,
                        const std::vector<TBlob> &in_grad,
                        const std::vector<TBlob> &aux_states) {
    /*BN��(��l��)�в���gamma��beta, ���Ҫ���������ʧJ����BN��(��l��)�Ĳв�, gamma���ݶȺ�beta���ݶ�. 
    !!!!!!!!!!!!!!!!�ݶȿ��Կ�������ʧJ���ڲ�����ĵ���, �в���Կ�������ʧJ���ڲ�����ĵ���!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
	 
    in_grad����в�/�ݶȲ���, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)��.
	out_grad����в�/�ݶȲ���, ��������, ÿ��Ԫ�ص�������TBlob. ��һ��(��l + 1��)�Ĳв�/�ݶ�, ���㱾��Ĳв�/�ݶ�. 
	in_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)������.  
	out_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)�����. 	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType.
	aux_states: ��ʾ����Ϊ�˷���������Ҫ�ĸ��ӵ� tensor. ���ӵ�Tensor������: kMovingMean, kMovingVar. ��ǰ���Ĳ�����ûʹ��
	aux_states����������.
	*/
	
	/*==================================================================================================================== 
	��BN����󵼿��Է���, �кܶ��м�������ظ�ʹ��. ��Щ�м�������Ե��������. ������Ҳ�漰��һ�������ٶȺʹ洢֮���ƽ��
	����. 
	*/   
							  
    using namespace mshadow;
    using namespace mshadow::expr;
    CHECK_EQ(out_grad.size(), param_.output_mean_var ? 3 : 1);
    // bool output_mean_var; �Ƿ����������ֵ�ͷ���. ��һ�������в�, ���output_mean_var == true, out_grad����������: �ݶ�,
	// mean, var; ����, out_gradֻ�вв���һ��.  
    CHECK_EQ(in_data.size(), 3); // BN������������, data����, gamma, beta. 
    CHECK_EQ(out_data.size(), 3); // BN������������: out���, mean��ֵ, var����. 
    CHECK_EQ(in_grad.size(), 3); // BN��Ĳв�������, gslope��gamma�Ĳв�, gbias��beta�Ĳв�, grad_in����ʧ����BN��Ĳв�. 
    // grad_in, ��ʧJ����BN������Ĳв�, ����в�������һ�ε�FC���ǰ�򴫲�����Ӱ��, ���ǻ�����gdata����BN 
	// ��ǰһ��(��l - 1)��Ĳв�. 
    
    Stream<xpu> *s = ctx.get_stream<xpu>();
    Tensor<xpu, 4> data, grad, grad_in; // ����data, grad, grad_in. xpu�µ�4ά����. �������������������и�ֵ. 
    const real_t scale = static_cast<real_t>(out_grad[batchnorm::kOut].shape_[1]) /
                         static_cast<real_t>(out_grad[batchnorm::kOut].shape_.Size()); // real_t scale, ��Foeward��һ��.
	// һ������ͼ(���)�ĸ��� / һ��batch��������(BN�����������)���ܸ���. �������BN��ǰһ����FC��, 
    // scale = 1 / batch_size; ���ھ����scale = 1 / (batch_size * ��������ά���˻�).
	 
    if (in_data[batchnorm::kData].ndim() == 2) { // BN�����������in_data[batchnorm::kData��2ά��, ����TBol�µ�ndim��Ա����,
	// ����TBlob�����ά��.
	/*
	��BN��ǰΪFC��, ��ôin_data[batchnorm::kData].ndim() == 2, Ҫ���FC�ļ���ֵʹ��BN����, ��Ҫ�Ƚ�FC�ļ���ֵdata����4ά��
	����, ��СΪ: batch_size * num_hidden * 1 * 1. ���򴫲�ʱ��һ����, ҲҪ������������2ά�Ļ���4ά��, 
	*/ 
      Shape<4> dshape = Shape4(out_grad[batchnorm::kOut].shape_[0],
                               out_grad[batchnorm::kOut].shape_[1], 1, 1); // ����Shape<4>��dshape, 
	  // ��СΪ: batch_size * num_hidden * 1 * 1.  
      data = in_data[batchnorm::kData].get_with_shape<xpu, 4, real_t>(dshape, s);
      grad = out_grad[batchnorm::kOut].get_with_shape<xpu, 4, real_t>(dshape, s);
      grad_in = in_grad[batchnorm::kData].get_with_shape<xpu, 4, real_t>(dshape, s);
      /*
	  ��420�ж���Tensor<xpu, 4> ����data, grad, grad_in���и�ֵ�Ͷ������.
	  data: BN�����������, ��Ϊin_data[batchnorm::kData]��2ά������, ��˵���TBlob��get_with_shape����, ����dshape��
	  ��СΪ: batch_size * num_hidden * 1 * 1��shape, ��BN��������չ��4ά��Tensor.
	  grad: BN��һ��(��l + 1)��Ĳв�, ��Ϊin_data[batchnorm::kData]��2ά������, ���out_grad[batchnorm::kOut]Ҳ�Ƕ�ά��. ���
	  ����չ��4ά��Tensor.
	  grad_in: BN��Ĳв�. in_grad[batchnorm::kData]Ҳ��2ά��Tensor, ����չΪ4ά��.  
	  */
      
    } else {
      data = in_data[batchnorm::kData].get<xpu, 4, real_t>(s);
      grad = out_grad[batchnorm::kOut].get<xpu, 4, real_t>(s);
      grad_in = in_grad[batchnorm::kData].get<xpu, 4, real_t>(s);
      /*
	  ���in_data[batchnorm::kData].ndim()����2ά������, ��ô����4ά��. ����get����ֱ�ӽ�in_data[batchnorm::kData]������4ά��
	  ��������. 
	  */
    } // ���ǰ�򴫲��Ĳ������������Ƶ�. 

    Tensor<xpu, 1> mean = out_data[batchnorm::kMean].get<xpu, 1, real_t>(s);
    Tensor<xpu, 1> var = out_data[batchnorm::kVar].get<xpu, 1, real_t>(s);
    Tensor<xpu, 1> slope = in_data[batchnorm::kGamma].get<xpu, 1, real_t>(s);
    /*
	����get������:
	out_data[batchnorm::kMean]��out_data[1], BN��������ֵmean����1ά��Tensor, mean����.
	out_data[batchnorm::kVar]��out_data[2], BN����������Var����1ά��Tensor. var.
	in_data[batchnorm::kGamma]��in_data[1], BN���gamma��������1ά��Tensor, slope. 
	*/
    
    // Tensor<xpu, 1> bias = in_data[kBeta].get<xpu, 1, real_t>(s);
    Tensor<xpu, 1> gslope = in_grad[batchnorm::kGamma].get<xpu, 1, real_t>(s);
    Tensor<xpu, 1> gbias = in_grad[batchnorm::kBeta].get<xpu, 1, real_t>(s);
    /*
	BN��Ĳв�������, gslope��gamma�Ĳв�, gbias��beta���ݶ�, grad_in����ʧ����BN����ݶ�.
	slope��bias��ǰ�򴫲�ʱ, ��1ά��Tensor, ����ڷ��򴫲���, ��в�Ҳ��1ά������.
	in_grad[batchnorm::kGamma]��in_grad[1], ��ʧJ����gamma�Ĳв�, ��1ά������.
	in_grad[batchnorm::kBeta]��in_grad[2], ��ʧJ����BN��beta�����Ĳв�, ��1ά������. 
	*/
    
    // update moving avg
    Tensor<xpu, 1> moving_mean = aux_states[batchnorm::kMovingMean].get<xpu, 1, real_t>(s);
    Tensor<xpu, 1> moving_var = aux_states[batchnorm::kMovingVar].get<xpu, 1, real_t>(s);
    /*
	aux_states[batchnorm::kMovingMean]��aux_states[0]����1ά������, moving_mean. 
	aux_states[batchnorm::kMovingVar]��aux_states[1]����1ά������, moving_var. 
	
	aux_states�����е������������������. ��ȡmoving average, ���use_global_stats == true, ��ô��Ҫʹ�� moving average. 
	*/

    if (param_.fix_gamma) slope = 1.f; // ���gamma��һ����ֵ, ��ôslope(gamma)����1.f. 

    if (ctx.is_train && !param_.use_global_stats) { // �������ѵ���׶��Ҳ�ʹ��use_global_stats. 
       // ��test/predict�׶�, ������use_global_stats == trueʱ(��������ʵ���Կ�����һ�����, ѵ��ʱ, use_global_stats == false.
	  // ���������ǲ�������). ��ʹ��moving average�㷨�������������Լ���mean��var.  
      
	  /*
	  get requested temp space. ��ȡ�������ʱ�ռ�. 
	  ��Щ������Ҫ������ڴ���Ϊ�����ռ���м���, ����˵BatchNormBackward. ���������, ϵͳ��ÿ��Զ��ⲿ���ڴ���й���, 
	  ����ϵͳ������һЩ�Ż�, ����˵�ڴ���ظ�����. ���BN��kTempSpace. ��BN�ķ������������һ����ʱ����Դ�ռ�, ����ռ�����. 
	  */
      Tensor<xpu, 2> workspace = ctx.requested[batchnorm::kTempSpace].get_space<xpu>(
          mshadow::Shape2(3, mean.shape_[0]), s);
      /*
	  OpContext: �ṹ��, ������include/mxnet/operator.h��, �ýṹ����Լ�¼������ǰ��ͺ��򴫲��е���Ϣ. ctx�ǽṹ��OpContext��
	  ��Ķ���, requested��OPContext�ṹ���µĺ���:
      // brief Resources requested by the operator
  	  std::vector<Resource> requested; // �������ز����������Դ. 
      ctx.requested���ص���һ����������, ctx.requested[batchnorm::kTempSpace]��ctx.requested[0]����һ��Resource����, Ȼ��
	  Resource�����ٵ���get_space����. 
	  
	  get_space���������: include/mxnet/resource.h 90��: get_space�����Ƕ�����Resource�ṹ���µĺ���: 
	  template<typename xpu, int ndim>
	  inline mshadow::Tensor<xpu, ndim, real_t> get_space(mshadow::Shape<ndim> shape, mshadow::Stream<xpu> *stream)const{...}
	  get_space������ȡTensor����Ŀռ�. ����shape: ����Tensor��Shape; stream: Device�µ�Tensor; ���������Tensor.
	  
	  �˴�, shape��Shape2(3, mean.shape_[0]), ��һά��3, �ڶ�ά��mean.shape_[0], BNǰһ��ΪFC��ʱ, Ϊnum_hidden������; Ϊ
	  �����ʱ, Ϊ����ͼ�ĸ���. stream��xpu�µĶ���s. shape��Shape2, ��Shape<2>, ���ndim��2, �ʷ��������Tensor��2ά��.
	  
	  workspace��ΪBN���򴫲������2ά��Tensor, ��һ����ʱ�ռ�, �����ڴ�.   
	  */    
          
      Tensor<xpu, 1> gmean = workspace[0];
      Tensor<xpu, 1> gvar = workspace[1];
      Tensor<xpu, 1> tmp = workspace[2];
      /*
	  1ά��Tensor gmean, gvar, tmp. ��workspace, BN�㷴�򴫲�����ʱTensor����. ����gmean, gvar, tmp����ʧ���ڲ���gamma, beta
	  ���ݶ�, Ȼ�������gmean, gvar��������ʧJ����BN������Ĳв�.   
	  
	  ���workspace.shape_.Size()Ϊ3, workspace.shape_[0]Ϊ3, workspace.shape_[1]Ϊ1, workspace.shape_[2]Ϊ1.
	  3���ڶ���workspaceʱ��Shape2�ĵ�һ������. ��Tensor�ĵ�0��λ�õ�Ԫ�ؾ�������Ǵ�С.  
	  */

      moving_mean = moving_mean * param_.momentum + mean * (1 - param_.momentum);
      moving_var = moving_var * param_.momentum + var * (1 - param_.momentum);
      /*
	  ʹ��moving average�㷨, ����mean��var, ����test/predict. momentum��moving average�Ķ�����, float, ��ֵ��0.9f.
	  
	  moving_mean��moving_var�г�ֵ, �ڷ��򴫲������л����BN��������ֵmean�ͷ���var���и���. �ڲ��Ե�ǰ�򴫲�������, ���� 
	  moving_mean��moving_var�������������Լ��ľ�ֵ�ͷ���.
	  
	  ���¹���: a = a * momentum + a * (1 - momentum), momentum��moving average�Ķ�����, float, ��ֵ��0.9f.
	  moving_mean��moving_var�������������������. 
	  */
	  
	  /*
	  ����gmean��gvar, gmean��gvar����������ʧJ����BN������Ĳв�! gvar������ݶ�, gmean�Ǿ�ֵ���ݶ�.
	  ����ԭ��, ���������ʧΪl, ��ô��Ҫ����һ��ƫ��:
	  partial(l) / partial(xi^(^)) == partial(l) / partial(yi) * gamma.  
	  partial(l) / partial(varb), gvar. 
	  partial(l) / partial(meanb), gmean. 
	  partial(l) / partial(xi)
	  partial(l) / partial(gamma), gslope. 
	  partial(l) / partial(beta), gbias. 
	  varb����b��batch������BN����������, meanb����b��batch������BN��������ֵ. 
	  */
      gvar = sumall_except_dim<1>((grad * broadcast<1>(slope, data.shape_)) *
                                  (data - broadcast<1>(mean, data.shape_)) *
                                  -0.5f *
                                  F<mshadow_op::power>(broadcast<1>(var + param_.eps, data.shape_),
                                                       -1.5f));
      /*
	  ������ʧ���ڷ���var���ݶ�, ����ԭ��Ϊ: partial(l) / partial(varb) =  
      sum{ [partial(l) / partial(xi^(^))] * [(xi - meanb)] * -0.5 * (varb + eps)^(-3/2) }.
      sum{ [*1] * [*2] * -0.5 * (*3) }.   
      
	  �� [partial(l) / partial(xi^(^))] == partial(l) / partial(yi) * gamma. yi��BN������, ����һ�������, �ֲв�����ʧ����
	  ����ĵ���, ��� partial(l) / partial(yi) ����BN��һ��(��l + 1)��Ĳв�. ����вgrad, �ǽ�out_grad[0]����4άTensor.
	  ���, *1 ����grad * gamma. ���Ҫ��slope(gamma)������չ, slope��BN���gamma����, ����BN�������xi��yi��shape��ͬ, ���
	  grad��BN������data��shape��ͬ, ��slope������չ, ����slope���1ά��Tensor��չ�ɺ�BN����������data����һ��shape��Tensor.
	  broadcast<1>(slope, data.shape_)����չ���slope. ��� *1 = grad * broadcast<1>(slope, data.shape_). 
	  
	  *2 = (xi - meanb). ������������, xi��data, ���Ϊ����ȷִ��(xi - meanb), Ҫ��meanb������չ. mean��BN��������ֵ, Ϊ1ά
	  ��Tensor, �����Ҫ��mean��չ�ɺ�data������ͬshape��Tesnor, ��4ά��Tenso. *2 = data - broadcast<1>(mean, data.shape_).
	  
      *3 = (varb + eps)^(-3/2). ����F<mshadow_op::power>(*11, *21)��˫Ŀ�����, �������mshadow_op::power, ����DType a, DType b
	  ����powf( a, b ). *21Ϊ-1.5f, ��float�͵�1.5.
	  *11��broadcast<1>(var + param_.eps, data.shape_), ����var + param_.eps������չ, ��չ�ɺ�data������ͬshape��Tesnor, 
	  ��4ά��Tensor. var + param_.eps������������1ά��Tensor. 
	  
	  ����ٶ�[*1] * [*2] * -0.5 * (*3)���, ���ܵ�һ��ά��, ������ά�Ƚ������. ����batch_sizeά��, ���ݸ߶�ά��, ���ά��
	  ���. 
	  */
	                                                   
      gmean = sumall_except_dim<1>(grad * broadcast<1>(slope, data.shape_));
      gmean *= -1.0f / F<mshadow_op::square_root>(var + param_.eps);
      tmp = scale * sumall_except_dim<1>(-2.0f * (data - broadcast<1>(mean, data.shape_)));
      tmp *= gvar;
      gmean += tmp;
      /*������ʧ���ھ�ֵmean��ƫ����, ����ԭ��Ϊ: partial(l) / partial(meanb) = 
	  sum{ [partial(l) / partial(xi^(^))] * [-1 / (varb + eps)^(1/2)] } 
	  + { [partial(l) / partial(varb)] * sum{ -2* [(xi - meanb)]} / m }. ��:
	  sum{ *1(��gvarʱ��*1) } * [*2] + { gvar * [*3] }. ����gmean��ʱ, ��Ƚ϶�, ���Էֿ�����.
	  
	  ������gmean = sum{ *1 }, *1Ϊ��gvarʱ��*1. Ȼ�����, ���ܵ�һ��ά��, ������ά�Ƚ������. ����batch_sizeά��, ���ݸ߶�
	  ά��, ���ά�����. 
	  
	  *2 = [-1 / (varb + eps)^(1/2)]. �������varb + eps�Ľ����1ά��Tensor. F<mshadow_op::square_root>()�ǵ�Ŀ��������. 1ά��
	  Tensor���Կ�����һ������, �����-1f / F<mshadow_op::square_root>(). *2 ����һ��1ά��Tensor, ��˿��Կ�����һ������, 
	  ������� gmean * (*2)����!!
	  gmean = gmean * (*2)�� + ǰ�ĵ�һ��.
	  
	  *3 = sum{ -2* [(xi - meanb)]} / m. ����1/m��scale����, ���ǰ�򴫲��еĲ���һ��. ������������, ���xi��data, Ϊ�˼���
	  data - mean, Ҫ��mean��BN��������ֵ������չ, ��չ�ɺ�data������ͬshape��Tesnor, ��4ά��Tensor, �����Ϳ��Լ���
	  data - mean. �ٳ��� -2.0f, Ȼ���, ���ܵ�һ��ά��, ������ά�Ƚ������. ����batch_sizeά��, ���ݸ߶�ά��, ���ά�����.
	  *3 ��tmp, tmp��1ά��Tensor.  �� sumall_except_dim<1> �ļ������Ƿ���1ά��Tenso.	     
	  
	  *3 * gvar���� + �������һ��, ��tmp. 
	  ���, ��ʧ����BN�������ֵmean���ݶȾ��� gmean = gmean + tmp. 
	  */
	  
      // assign
      if (!param_.fix_gamma) { // ���û�й̶�gammaֵ, ��������ʧJ���ڲ���gamma���ݶ�. 
        Assign(gslope, req[batchnorm::kGamma],
               sumall_except_dim<1>(
                   grad * (data - broadcast<1>(mean, data.shape_)) /
                   F<mshadow_op::square_root>(broadcast<1>(var + param_.eps, data.shape_))));
        /*
		Assign��ֵ����, gslope����ʧ����BN�����gamma���ݶ�; req�����ݲ���ģʽ, ��kGamma�����ݲ���ģʽ; exp, ����ԭ��:
		partial(l) / partial(gamma) = sum{ [partial(l) / partial(yi)] * xi^(^) }. ��:
		sum{ grad * xi^(^) }, xi^(^)���м������, ��Ҫ����xi, mean, var�����!!
		
		xi^(^) = [xi - meanb] / [varb + eps]^(1/2). ���������������, xi��data. ��������[xi - meanb], ��Ҫ��չBN��������ֵ
		mean, ��չ�ɺ�data������ͬshape��Tesnor, ��4ά��Tensor, �����Ϳ��Լ���data - mean.
		F<mshadow_op::square_root>(*)�ǵ�Ŀ��������. *��[varb + eps], ���ȶ�BN����������var��eps���, Ȼ���ٶ����1άTensor
		������չ, ��չ�ɺ�data������ͬshape��Tesnor, ��4ά��Tensor. ������������. �����Ϳ��Եõ�xi^(^), ��data^(^).
		
		���� grad * data^(^)�������, �ܵ�һ��ά��, ������ά�Ƚ������. ����batch_sizeά��, ���ݸ߶�ά��, ���ά�����.   
		*/           
                   
      } else { // �̶���gamma, ��slope = 1.0f��, ��ʧ����gamma���ݶ���0.0f. ��Ϊgamma�Ѿ��Ƕ�ֵ��!! 
        Assign(gslope, req[batchnorm::kGamma], 0.0f);
      }
      Assign(grad_in, req[batchnorm::kData],
             (grad * broadcast<1>(slope, data.shape_)) *
             broadcast<1>(1.0f / F<mshadow_op::square_root>(var + param_.eps), data.shape_) +
             broadcast<1>(gvar, data.shape_) * scale * 2.0f * (data - broadcast<1>(mean,
                                                                                   data.shape_)) +
             broadcast<1>(gmean, data.shape_) * scale);
      /*
      Assign��ֵ����, grad_in����ʧ����BN��������ݶ�; req�����ݲ���ģʽ, ��kData�����ݲ���ģʽ;
	  ������ʧ����BN�������xi(data)���ݶ�, ����ԭ��:
	  partial(l) / partial(xi) == grad_in = [partial(l) / partial(xi^(^))] * [1 / (varb + eps)^(1/2)] 
	  + [partial(l) / partial(varb)] * 2 * [(xi - meanb)] / m
	  + [partial(l) / partial(meanb)] /m. ��:
	  
	  [grad * gamma] * [1 / (varb + eps)^(1/2)] + [partial(l) / partial(varb)] * 2 * [(xi - meanb)] / m
	  + [partial(l) / partial(meanb)] /m.
	  
	  [grad * gamma]�����Ѿ������, ��slope��չ�ɺ�data����һ��shape��Tensor����.
	  [1 / (varb + eps)^(1/2)]ǰ���Ѿ����, ֻ�ǽ�-1ת��Ϊ1����. (varb + eps)�����1ά��Tensor, ����������. ����һ��, ���ڼ���
	  grad_inʱ, �õ���grad, ��shape��data��shapeһ��. ���, ���еı�������Ҫ��չ�ɺ�data����һ��shape��Tensor, ��4ά������. 
	  [partial(l) / partial(varb)]��gvar, �ٽ�gvar���1ά��������չ�ɺ�data����һ��shape��Tensor, ��4ά������.
	  1/m��scale�滻, (xi - meanb)����Ҳ�Ѿ������. xi��data, ��Ҫ��չmeanb.
	  [partial(l) / partial(meanb)] ��gmean, �ٽ����1ά��������չ�ɺ�data����һ��shape��Tensor, ��4ά������. 1/m��scale�滻.  
	  */    
             
      Assign(gbias, req[batchnorm::kBeta], sumall_except_dim<1>(grad));
      /*
      Assign��ֵ����, gbias����ʧ����BN�����beta���ݶ�; req�����ݲ���ģʽ, ��kBeta�����ݲ���ģʽ;
	  ������ʧ����beta���ݶ�, ����ԭ��:
	  partial(l) / partial(beta) = sum{ partial(l) / partial(yi) }. partial(l) / partial(yi)��grad, ����ʧ����BN�����,
	  ��l + 1�������Ĳв�.  Ȼ�����, ���ܵ�һ��ά��, ������ά�Ƚ������. ����batch_sizeά��, ���ݸ߶�ά��, ���ά�����. 
	  */
	  
    } else {
      // use global statistics with freeze moving mean and var. �ڲ��Խ׶λ���ʹ��use global statisticsʱ�ķ��򴫲�! 
      if (!param_.fix_gamma) { // ���û�й̶�gammaֵ, ��������ʧJ���ڲ���gamma���ݶ�.   
        Assign(gslope, req[batchnorm::kGamma],
               sumall_except_dim<1>(
                   grad * (data - broadcast<1>(moving_mean, data.shape_)) /
                   F<mshadow_op::square_root>(broadcast<1>(moving_var + param_.eps, data.shape_))));
        /*��ʧ����gamma���ݶ�. 
		Assign��ֵ����, gslope����ʧ����BN�����gamma���ݶ�; req�����ݲ���ģʽ, ��kGamma�����ݲ���ģʽ; expΪ:
		�ڲ��Խ׶�ʹ��use global statisticsʱ, ����moving average�㷨, ���漰��var��moving_var����. 
		
		�ڲ��Խ׶�ʹ��use global statisticsʱ, ��ʧ����BN����gamma���ݶȺ�ѵ���׶�ʱ���Ƶ�, ֻ��var��moving_var����.
		*/
		           
      } else { // �̶���gamma, ��slope = 1.0f��, ��ʧ����gamma���ݶ���0.0f. ��Ϊgamma�Ѿ��Ƕ�ֵ��!!
        Assign(gslope, req[batchnorm::kGamma], 0.0f);
      }
      Assign(gbias, req[batchnorm::kBeta], sumall_except_dim<1>(grad));
      /*��ʧ����beta���ݶ�. 
      Assign��ֵ����, gbias����ʧ����BN�����beta���ݶ�; req�����ݲ���ģʽ, ��kBeta�����ݲ���ģʽ;
	  ������ʧ����beta���ݶ�, ��ѵ���׶��Ҳ�ʹ��use global statistics�ķ��򴫲���һ����! 
	  */
	  
      Assign(grad_in, req[batchnorm::kData], (grad * broadcast<1>(slope, data.shape_)) *
             broadcast<1>(
                 1.0f / F<mshadow_op::square_root>(moving_var + param_.eps), data.shape_));
      /*
      Assign��ֵ����, grad_in����ʧ����BN��������ݶ�; req�����ݲ���ģʽ, ��kData�����ݲ���ģʽ;
       
	  �ڲ��Խ׶�ʹ��use global statisticsʱ, ��ʧ����BN������Ĳв��������:
	  detla^(l + 1) = partial(l) / partial(xi^(^)) * [ 1 / (Var[x] + eps)^(1/2)].
	  
	  ��partial(l) / partial(xi^(^)) = partial(l) / partial(yi) * gamma. ǰ���Ѿ������.
	  Var[x]�Ƕ����������Լ���˵�ķ���, ������ moving_var ����. 
	  
	  �����漰��grad, ����ʧ���ڵ�l + 1������Ĳв�, ��shape��BN������data��shapeһ��. ���Ҫ�����е�����չ�ɺ�data������ͬ
	  shape��Tensor, ��4ά������. 
      */
    }
  }

 private:
  BatchNormParam param_;
};  // class BatchNormOp

template<typename xpu>
Operator *CreateOp(BatchNormParam param, int dtype);


#if DMLC_USE_CXX11
class BatchNormProp : public OperatorProperty {
 public:
  void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) override {
    param_.Init(kwargs);
  }

  std::map<std::string, std::string> GetParams() const override {
    return param_.__DICT__();
  }

  bool InferShape(std::vector<TShape> *in_shape,
                  std::vector<TShape> *out_shape,
                  std::vector<TShape> *aux_shape) const override {
    using namespace mshadow;
    CHECK_EQ(in_shape->size(), 3) << "Input:[data, gamma, beta]";
    const TShape &dshape = in_shape->at(0);
    if (dshape.ndim() == 0) return false;
    in_shape->at(1) = TShape(Shape1(dshape[1]));
    in_shape->at(2) = TShape(Shape1(dshape[1]));
    out_shape->clear();
    out_shape->push_back(dshape);
    out_shape->push_back(Shape1(dshape[1]));
    out_shape->push_back(Shape1(dshape[1]));

    aux_shape->clear();
    aux_shape->push_back(Shape1(dshape[1]));
    aux_shape->push_back(Shape1(dshape[1]));
    return true;
  }

  OperatorProperty* Copy() const override {
    auto ptr = new BatchNormProp();
    ptr->param_ = param_;
    return ptr;
  }

  std::string TypeString() const override {
    return "BatchNorm";
  }

  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
    return {out_grad[batchnorm::kOut],
            out_data[batchnorm::kMean],
            out_data[batchnorm::kVar],
            in_data[batchnorm::kData],
            in_data[batchnorm::kGamma]
           };
  }

  std::vector<ResourceRequest> BackwardResource(
      const std::vector<TShape> &in_shape) const override {
    return {ResourceRequest::kTempSpace};
  }

  int NumVisibleOutputs() const override {
    if (param_.output_mean_var) {
      return 3;
    }
    return 1;
  }

  int NumOutputs() const override {
    return 3;
  }

  std::vector<std::string> ListArguments() const override {
    return {"data", "gamma", "beta"};
  }

  std::vector<std::string> ListOutputs() const override {
    return {"output", "mean", "var"};
  }

  std::vector<std::string> ListAuxiliaryStates() const override {
    return {"moving_mean", "moving_var"};
  }

  Operator* CreateOperator(Context ctx) const override {
      LOG(FATAL) << "Not Implemented.";
      return NULL;
  }

  Operator* CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
      std::vector<int> *in_type) const override;

 private:
  BatchNormParam param_;
};  // class BatchNormProp

#endif  // DMLC_USE_CXX11
}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_BATCH_NORM_INL_H_
