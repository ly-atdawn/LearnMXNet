/*!
 * Copyright (c) 2015 by Contributors
 * \file pooling-inl.h
 * \brief
 * \author
*/

#ifndef MXNET_OPERATOR_POOLING_INL_H_
#define MXNET_OPERATOR_POOLING_INL_H_
/*
#ifndef��"if not defined"�ļ�д, �Ȳ���Ҫ����ĺ�����Ƿ񱻺궨���. ����mxnet��plloing������. 
*/

#include <dmlc/logging.h> // mxnet����־ͷ�ļ�. ��dmlc-core/include/dmlc��,.
#include <dmlc/parameter.h> // mxnet�Ĳ���ͷ�ļ�, ��dmlc-core/include/dmlc��, ���������. 
#include <mxnet/operator.h> // ��include/mxnet��, �����������(operator), ����������, ������. ��OP��Prop�ĺ�����������.
#include <algorithm> // c++�㷨��. 
#include <map> // ����ʽ����, Ԫ�ص�ֵ��ĳ���ض��ļ������, ������ͨ��Ԫ���������е�λ�����ȡ.
#include <vector> // c++��������. 
#include <string> // c++�ַ��� 
#include <utility> // utilityͷ�ļ��������صĹ�ϵ�����, �򻯹�ϵ�������д��, ��������pair����,
// pair������һ��ģ������, ���Դ洢һ��ֵ.
#include "./operator_common.h" // src/operator��, mxnet�Ĳ�һЩ���õ�����.

namespace mxnet {
namespace op {

namespace pool_enum { // ����pooling���������, �����ʱ, �����ռ䶨��Ϊpool_enum. 
enum PoolingOpInputs {kData}; // pooling��������������kData, Ϊ0. 
enum PoolingOpOutputs {kOut}; // pooling���������kOut, Ϊ0. 
enum PoolingOpType {kMaxPooling, kAvgPooling, kSumPooling}; // pooling����������, mxnet��pooling����ʹ����������:
/*
kMaxPooling: ���ػ�, Ϊ0.
kAvgPooling: ƽ���ػ�, Ϊ1.
kSumPooling: ��ͳػ�, Ϊ2. 
*/ 
enum PoolingOpPadConventionType {kValid, kFull}; // ��Ϊpooling������ʵҲ��һ�־������, ����趨�������������.
/*�����������, ���MATLAB�ľ������conv��conv2�����Ƶ�, conv��������ľ��, ��һά���; conv2����ά���, ��������. 
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
����ʱ��AΪ      ����ʱBΪ . Ȼ���ٶ�ӦԪ���������Ӽ��Ǿ����Ľ��. 
0 0 0 0 0        4 3       
0 1 2 3 0        2 1 
0 4 5 6 0
0 7 8 9 0 
0 0 0 0 0

conv2(A, B, 'valid'), ���ؾ��������û�в��㲿�ֵļ�����, ����CNN�еľ������, �þ���˶���A����. ������. 

conv2(A,B, 'same'), ���غ�Aͬ����С�ľ����Ľ��, ����full�������Եõ�same�����Ľ��. �������� 
 
*/ 
}  // namespace pool_enum

/*
������������ͼ�Ĵ�С, �������ͼ�ߴ� = [(��������ͼ�ߴ� + 2 * pad - ����˳ߴ�)/����](����ȡ��) + 1. 
�ھ����Ĳ���ʱ, �������;����ʱ, ��pooling��������һ����ͨ����ͨ�ľ��. û������ϵ��. 

һ��ػ�����ÿһ�ػ����ڶ��ǲ��ظ���, ����stride = size(kernel). ���⻹���ص��ػ�, �����ڵĳػ����ڼ������ص���.���н�����
�ػ�SPP.  
*/

struct PoolingParam : public dmlc::Parameter<PoolingParam> { // pooling�����Ĳ�����PoolingParam. ���������������Լ���ֵ��. 
  TShape kernel; // �ػ����ڴ�С(�����), TShape: һ��shape��, ������Ա�ʾһ��Tensor����״. ����TShape����ʾTensor�Ĵ�С.  
  TShape stride; // �ػ����ƶ�����.  
  TShape pad; // ԭʼ���ݵĸ߶Ȼ��ȷ����ϲ���0ֵ��Ȧ��. 
  int pool_type; // pooling������, ��int�͵ı���pool_type. ��0, 1, 2: kMaxPooling, kAvgPooling, kSumPooling. 
  int pooling_convention; // ��pooling������ʱ����������, 0, 1: kValid, kFull.
  bool global_pool; // bool���͵ı���global_pool, �Ƿ���ȫ�ֳػ�. 
  DMLC_DECLARE_PARAMETER(PoolingParam) { // set_default�������ò�����Ĭ��ֵ; describe�����Բ�����������. 
    DMLC_DECLARE_FIELD(global_pool).set_default(false) // global_poolĬ��ֵ��false. 
    .describe("Ignore kernel size, do global pooling based on current input feature map. "
              "This is useful for input with different shape"); // ���ܳػ����ڵĴ�С, ������������ͼ��ȫ��pooling, �������
              // ����ͼ�Ĵ�С��һ���������ʽ���õ�. global_pool�����Ϊȫ���.

    DMLC_DECLARE_FIELD(kernel)
    .enforce_nonzero()
    /*
    inline FieldEntry<mxnet::TShape> &enforce_nonzero() {
       this->enforce_nonzero_ = true;
       return this->self();
    }
    bool enforce_nonzero_;
    ǿ����Ϊ��Ϊ0. 
	*/
    .describe("pooling kernel size: (y, x) or (d, y, x)"); // pooling�����(�ػ����ڵĴ�С). ���Ҳ�ֶ�ά�������ά���, ��
	// convolution�����Ƶ�. ������������ͼ�ĵ�����ά������. 

    DMLC_DECLARE_FIELD(pool_type)
    .add_enum("max", pool_enum::kMaxPooling)
    .add_enum("avg", pool_enum::kAvgPooling)
    .add_enum("sum", pool_enum::kSumPooling)
    .describe("Pooling type to be applied."); // ʹ��kMaxPooling�Ȳ���ʱ, ��pool_enum�����ռ��޶������򼴿�. 
    /*
	���ú���add_enum(const std::string &key, int value)��Ϊpool_type��Ӳ���. ���һ�Լ�ֵkey��value. ָ��pooling������. 
	*/

    DMLC_DECLARE_FIELD(pooling_convention).set_default(pool_enum::kValid)
    .add_enum("full", pool_enum::kFull)
    .add_enum("valid", pool_enum::kValid)
    .describe("Pooling convention to be applied."
              "kValid is default setting of Mxnet and rounds down the output pooling size."
              "kFull is compatible with Caffe and rounds up the output pooling size.");
    /*
	���ú���add_enum(const std::string &key, int value)��Ϊpooling_convention��Ӳ���. ָ��pooling�����о��������, Ĭ����
	pool_enum::kValid, ��������˶�������ͼ��, �ڶ�Ӧλ��Ԫ����������, ������. 
	*/

    int stride_shape[] = {1, 1}; // ����һ��һά����stride_shape, ��ֵΪ[1, 1]. Ȼ��������strideʱ��������ת��ΪTShape����. 
    DMLC_DECLARE_FIELD(stride).set_default(TShape(stride_shape, stride_shape + 2))
    .enforce_nonzero() // ǿ�Ʋ�Ϊ0. 
    .describe("stride: for pooling (y, x) or (d, y, x)");
    /*
    pooling-inl.h���°��mxnet��stride�ĳ�ֵ�����˸ı�, ��ԭ����:
	int stride_shape[] = {1, 1}; 
	DMLC_DECLARE_FIELD(stride).set_default(TShape(stride_shape, stride_shape + 2))
	��Ϊ:
	DMLC_DECLARE_FIELD(stride).set_default(TShape())
    
    ��c++����������һ������ָ��, ��ָ������Ŀ�ͷ, ��������2��ʾ��ָ��������������λ. ��ͬһ��ָ���һ������, 
	*(������+i)�ŵ���������(i). 
	
	TShape����: http://mxnet.io/doxygen/classmxnet_1_1TShape.html. TShape(stride_shape, stride_shape + 2)�÷������TShape��
	���캯��:
    template<typename RandomAccessIterator >
    mxnet::TShape::TShape(RandomAccessIterator begin, RandomAccessIterator end)
	����Tuple, Tuple��һ����̬��С�����ݽṹ, ���Դ洢����Ԫ��������ͬ������. RandomAccessIterator��һ��typename.	 
    
	�ػ�����(�����)�ƶ�����, ����Ĭ��ֵ��TShape(stride_shape, stride_shape + 2), ����һ��Tuple. ����TShape������, ��������
	���.
	
	TShape stride = TShape(); 
	std::cout<<"stride: "<<stride<<std::endl; // stride��(), ��Ϊ�յ�. 
    int stride_shape[] = {1, 1};
    TShape a = TShape(stride_shape, stride_shape + 2);
    std::cout<<"a: "<<a<<std::endl; // a��(1, 1).  
	*/

    int pad_shape[] = {0, 0};
    DMLC_DECLARE_FIELD(pad).set_default(TShape(pad_shape, pad_shape + 2))
    .describe("pad for pooling: (y, x) or (d, y, x)"); // pad�ǲ����Ȧ��, Ҳ�ж�ά����ά������, ����������ͼ�Ƕ�ά�Ļ�����
	// ά��. 
    /*
	pooling-inl.h���°��mxnet��pad�ĳ�ֵ�����˸ı�, ��ԭ����:
	int pad_shape[] = {1, 1}; 
	DMLC_DECLARE_FIELD(stride).set_default(TShape(pad_shape, pad_shape + 2))
	��Ϊ:
	DMLC_DECLARE_FIELD(pad).set_default(TShape())
	*/
  }
};

/*
* [pool](#pool): do pooling on image
* [unpool](#unpool): get gradient of pooling result
* [crop](#crop): crop the original image to a smaller size

ʹ��sublime��ȫ�ֲ��ҹ���, ��Ѱ��pool������unpool�����Ķ���. �ڱ�������sublime��pooling-inl.h, Ȼ����ʹ��sublime��ȫ�ֲ���
���ܼ��ɲ�ѯ�������Ķ���. 

�Ժ�Ѱ�Һ����Ķ���, ���Բο� mxnet �ٷ��ĵ��Լ�ʹ��sublime��ȫ�ֲ��ҹ���ʵ��. 
*/

template<typename xpu, typename Reducer, typename DType> // ģ���ඨ��ʱ, һ�㶼��xpu(cpu or gpu)��Dtype(float). pooling����
// �����һ��Reducer, �ػ�����. 
class PoolingOp : public Operator {
 public:
  explicit PoolingOp(PoolingParam p) {
    this->param_ = p; // explicit�ؼ���ֻ����������ֻ��һ���������๹�캯��, ���������Ǳ����ù��캯������ʾ��, ������ʽ��.
    // p��PoolingParam����������Ķ���, ��p��ֵ��param_. ��͵����ĸ�ֵ��һ��, param_����p, ���Կ�����ָ��p��ָ��.
    // ����param_������PoolingParam��ĳ�Ա, ��kerne, stride�Ȳ���. 
    // PoolingParam param_;
  }

  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_args) {
    /*ǰ�����, �麯��. ������ʵ�������ж���. ����Ҫ����ֵ. ����Ϊ�� l ��. 
	in_data: ��������data. pooling��������û��Ȩ�غ�ƫ�õ�, ���������ĳػ���Ĳ���, max�ػ�����ȡ�ػ����ڶ�Ӧ������ͼ�е�Ԫ
	�����ֵ; avg����ȡԪ�ص�ƽ��ֵ; sum�������. ���Բ�����ҪȨ��wmat��ƫ��bias. 
	req: ���ݲ���ģʽ. 
	out_data: �������, out. 
	*/
    using namespace mshadow;
    using namespace mshadow::expr;
    
    CHECK_EQ(in_data.size(), 1);
    CHECK_EQ(out_data.size(), 1);
    /*
	�ж�����������TBlob����, ������СӦ����1. ��pooling������in_dataֻ��kData, out_dataֻ��kOut. ����������СΪ1, ����Ϊ1,
	����.  
	*/
    
    Stream<xpu> *s = ctx.get_stream<xpu>();
    if (param_.kernel.ndim() == 3) {
      LOG(FATAL) << "3D kernel not implemented";
    } // 3ά�ľ��Ҳ��û��ʵ�ֵ�. ����caffe�����ݽṹblobһ��, blob����ά�Ľṹ, ���������Ƕ�ά��, �����������ά��. 
    
    Tensor<xpu, 4, DType> data = in_data[pool_enum::kData].get<xpu, 4, DType>(s);
    Tensor<xpu, 4, DType> out = out_data[pool_enum::kOut].get<xpu, 4, DType>(s);
    /*����kData��kOut��Ҫָ�������ռ�, ָ���䶨����. 
	������(��l��)����������in_data[kData], out_data[pool_enum::kOut����4ά������. ��ʹ��get����:
    mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get(mshadow::Stream<Device> *stream = NULL)const. 4ά������, ����ͺ�
	blob�Ƚ�������, ��number N x channel K x height H x width W, (N, K, H, W). Blob memory is row-major in layout. 
	*/
    
    mshadow::Shape<2> out_shape = Shape2(out.shape_[2], out.shape_[3]);
    /*
	����һ��2Ϊ��shape out_shape, Shape2��������:
	MSHADOW_XINLINE Shape<2> Shape2(index_t s0, index_t s1) {
        Shape<2> s;
        s[0] = s0; s[1] = s1;
        return s;
    } 
    �����out_shape��ֵ, ���õ���out.shape_[2], out.shape_[3], ������(��l��)�������ݵĸ߶�H�Ϳ��W. out_shape������������ݵ�
	��С. 
    out.shape_[0]: number N 
    out.shape_[1]: channel K 
	out.shape_[2]: height H 
	out.shape_[3]: width W 
	*/
    
    if (param_.pool_type == pool_enum::kMaxPooling || param_.pool_type == pool_enum::kSumPooling) { // param_.pool_type����
	// �ػ�������. ���ػ�����ͳػ�: 
      Assign(out,
             req[pool_enum::kOut],
             pool<Reducer>(pad(data, param_.pad[0], param_.pad[1]),
                           out_shape,
                           param_.global_pool ? data.shape_[2] : param_.kernel[0],
                           param_.global_pool ? data.shape_[3] : param_.kernel[1],
                           param_.global_pool ? 1 : param_.stride[0],
                           param_.global_pool ? 1 : param_.stride[1]));
    /*
	��ֵ����. pooling������data, �����out, ��out��ֵ.
	Assign����������include/mxnet/operator_util.h��, �Ƕ����һ���꺯��. ��������exp��ֵ����out. 
	�ⲻ��C++���ַ�����ֵ����assign. 
	#define ASSIGN_DISPATCH(out, req, exp)  \
  	{                                     \
      switch (req) {                      \
        case kNullOp:                     \
          break;                          \
        case kWriteTo:                    \
        case kWriteInplace:               \
          (out) = (exp);                  \
          break;                          \
        case kAddTo:                      \
          (out) += (exp);                 \
          break;                          \
        default:                          \
          LOG(FATAL) << "not reached";    \
      }                                   \
    } 
	
	pool_enum::kOut���ǻ�ȡһ��ö������(�ҵ�OpReqType���͵��Ǹ�����), ��ôreq[pool_enum::kOut]��OpReqType�е�kWriteInplace��
	kAddTo, Ȼ��ͨ��exp��out��ֵ.
	
	Assign�ĸ�ֵ������exp(������)Ϊ pool<Reducer>(...). ��mshadow/doc/README.md���� pool �� unpool ��һЩ����:
    pool<Reducer>(Expr<xpu, dim> img, [Shape<2> pshape,] int ksize_y, int ksize_x, int kstride) �����ػ����ڵĴ�С�ͻ�������
	���ػ�����. Reducer��һ���������, operation can be max or sum. 
	
	pool�����ľ��嶨����mshadow/mshadow/extension/spatial_pool.h��, unpool������mshadow/mshadow/extension/spatial_unpool.h��.
	241��ʹ�õ�pool����, ��ڶ���������out_shape, ������Shape<2>, ���241��ʹ�õ�pool������������: ������ mshadow::expr ����
	�ռ���, ���Ҫusing namespace mshadow::expr; 
	
	template<typename Reducer, typename SrcExp, typename DType, int etype>
	pool(const Exp<SrcExp, DType, etype> &src, Shape<2> pshape,
     index_t ksize_y, index_t ksize_x, index_t kstride_y, index_t kstride_x)
	src��Դͼ��; pshape�Ǿ�pooling���������ͼ��shape, ������Shape<2>; ksize_y����y����Ĵ�С(��); ksize_x�˿��; 
	kstride_y�����߶�; kstride_x�������. ���سػ���Ľ��. ����241��ʹ��pool������ʱ��, src������pad���������:
	
	padҲ�Ƕ�����mshadow::expr�����ռ���, ���Ҫusing namespace mshadow::expr; �����mshadow/mshadow/extension/pad.h. ����241
	��ʹ�õ�pad����������, ���:
	template<typename SrcExp, typename DType, int etype>
	pad(const Exp<SrcExp, DType, etype> &src, index_t pad_y, index_t pad_x)
	��һ��ͼƬ���в������, ��ͼƬ�����ܲ���. srcԭͼ��; pad_y padding size in y, ����y�����ϲ��������; pad_x 
	padding size in x, ��x�����ϲ��������. ���ز���Ľ��, �����ز�����֮��ľ���. 
	
	pad(data, param_.pad[0], param_.pad[1])����Tensor<xpu, 4, DType>��data���в������. ���ص��ǲ������ľ���.
	out_shape���������ͼ�ĸ߶ȺͿ��, ������Shape<2>.
	param_.global_pool ? data.shape_[2] : param_.kernel[0], �Ƿ���ȫ��pool(global_pool�Ƿ�Ϊ��, global_poolĬ��Ϊ��), ���
	ksize_y��param_.kernel[0], ���˵ĸ߶�; global_pool Ϊ��, ksize_yΪdata.shape_[2], ������������ĸ߶�.  
    param_.global_pool ? data.shape_[3] : param_.kernel[1], global_poolĬ��Ϊ��, ���ksize_xΪparam_.kernel[1], ���͵Ŀ��.
	global_pool Ϊ��, ksize_xΪdata.shape_[3], ������������Ŀ��. 
    param_.global_pool ? 1 : param_.stride[0], global_poolĬ��Ϊ��, ���kstride_yΪparam_.stride[0], ���������ĸ߶�; 
	global_pool Ϊ��, ��kstride_yΪ1. 
    param_.global_pool ? 1 : param_.stride[1], global_poolĬ��Ϊ��, ���kstride_xΪparam_.stride[1], ���������Ŀ��; 
	global_pool Ϊ��, ��kstride_xΪ1.   
	
	�ػ�������. ���ػ�����ͳػ�: 
	*/
    
    } else if (param_.pool_type == pool_enum::kAvgPooling) { // �ػ�����: ƽ���ػ�. 
      Assign(out,
             req[pool_enum::kOut],
             scalar<DType>(1.0f / (param_.global_pool ?
                      data.shape_[2] * data.shape_[3] :
                      param_.kernel[0] * param_.kernel[1])) *  // C++�� \ ��Ϊ����д����ʱʹ�õ�, ����. �� \ �����. 
             pool<Reducer>(pad(data, param_.pad[0], param_.pad[1]),
                           out_shape,
                           param_.global_pool ? data.shape_[2] : param_.kernel[0],
                           param_.global_pool ? data.shape_[3] : param_.kernel[1],
                           param_.global_pool ? 1 : param_.stride[0],
                           param_.global_pool ? 1 : param_.stride[1]));
    /*
	Assign��ֵ����, ��exp��ֵͨ�����ݲ���ģʽreq���ݸ����out. ��ƽ���ػ������ػ�, ��ͳػ���exp��ͬ, ƽ���ػ���pool(....)
	�Ľ������scalar����������. 
	
	scalar<DType>��mshadow/mshadow/expression.h:
	template<typename DType>
	inline ScalarExp<DType> scalar(DType s) ����һ������(scalar)���ʽ. DType��data������, ��float; s��һ��value. ����s��һ
	�����ʽ:
    1.0f / (param_.global_pool ? data.shape_[2] * data.shape_[3] : param_.kernel[0] * param_.kernel[1])
	global_poolĬ��Ϊ��, ���(..)Ϊparam_.kernel[0] * param_.kernel[1], ���˵ĳߴ�˻�; global_pool Ϊ��, (..)Ϊ
	data.shape_[2] * data.shape_[3], ����������ĳߴ�. Ȼ����ִ�� 1.0f / (...) ����sֵ.   
	
	Reducer��һ���������, ������avg. 
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
    /*�ػ���(��l��)û��Ȩ�غ�ƫ��, ���Ҫ���������ʧJ���ڳػ���(��l��)�Ĳв�.
    !!!!!!!!!!!!!!!!�ݶȿ��Կ�������ʧJ���ڲ�����ĵ���, �в���Կ�������ʧJ���ڲ�����ĵ���!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
	 
    in_grad����в����, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)��.
	out_grad����в����, ��������, ÿ��Ԫ�ص�������TBlob. ��һ��(��l + 1��)�Ĳв�, ���㱾��Ĳв�. 
	in_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)������.  
	out_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)�����. 	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType.
	*/
							  
    using namespace mshadow;
    using namespace mshadow::expr;
    
    CHECK_EQ(out_grad.size(), 1); // ��һ��(��l + 1��)���򴫲���ֻ����ʧ�Ĳв�sigma^(l + 1). ����Ȼ, ����. 
    CHECK_EQ(in_data.size(), 1); // ����(��l��)������ֻ������. 
    CHECK_EQ(out_data.size(), 1); // ����(��l + 1��)�����ֻ������. 
    CHECK_EQ(req.size(), 1); // ���ݲ���ģʽֻ��һ��.  
    CHECK_EQ(in_grad.size(), 1); // ����(��l��)���򴫲���ֻ�вв�. 
    
    // TODO(bing): remove pad (0,0)
    if (param_.kernel.ndim() == 3) {
      LOG(FATAL) << "3D kernel not implemented";
    } // �˵�ά��ֻ����2ά��. 
    Stream<xpu> *s = ctx.get_stream<xpu>();
    
    // pool_enum::kOutΪ0; pool_enum::kDataΪ0. 
    Tensor<xpu, 4, DType> grad = out_grad[pool_enum::kOut].get<xpu, 4, DType>(s);
    // ����l + 1��Ĳв�out_grad[0]����get����������ά������. ���в��������һ����, ��4ά��. 
    Tensor<xpu, 4, DType> data = in_data[pool_enum::kData].get<xpu, 4, DType>(s);
    // ����l�������in_data[0]����get��������4ά������. 
    Tensor<xpu, 4, DType> output_data = out_data[pool_enum::kOut].get<xpu, 4, DType>(s);
    // ����l����������get��������4ά������. 
    Tensor<xpu, 4, DType> input_grad = in_grad[pool_enum::kData].get<xpu, 4, DType>(s);
    // ���屾��(��l��)�Ĳв���4ά������. 

    mshadow::Shape<2> in_shape = Shape2(data.shape_[2], data.shape_[3]); // ����in_shape, ������Shape<2>. �ǵ�l������������
	// ������С. data.shape_[2]Ϊ�߶�, data.shape_[1]Ϊ���. ���򴫲�ʱ, Ҫ������Ĳв�reshape�ɺͱ��������һ����С. ��:
	// ����� �в� �� ��������� ��shape��һ�µ�. 

    if (param_.pool_type == pool_enum::kMaxPooling || param_.pool_type == pool_enum::kSumPooling) { // ���ػ�����ͳػ�. 
      Assign(input_grad, req[pool_enum::kData],
             crop(unpool<Reducer>(pad(data, param_.pad[0], param_.pad[1]),
                                  pad(output_data, 0, 0),
                                  pad(grad, 0, 0),
                                  param_.global_pool ? in_shape[0] : param_.kernel[0],
                                  param_.global_pool ? in_shape[1] : param_.kernel[1],
                                  param_.global_pool ? 1 : param_.stride[0],
                                  param_.global_pool ? 1 : param_.stride[1]),
                  in_shape,
                  param_.pad[0],
                  param_.pad[1]));
    /*
	Assign, ��ֵ����. ͨ��req���ݲ���ģʽ(req[0]), ��exp��ֵ����input_grad, ������Ĳв�. ����exp��:
    crop(*, in_shape, param_.pad[0], param_.pad[1]). * ��unpool<Reducer>(...).
	
	crop���������: mshadow/mshadow/extension/crop.h. ������mshadow::expr�����ռ���, ���Ҫusing namespace mshadow::expr;
	crop����Ҳ�����ֶ���, ����383�е�crop���ĸ��������, ���crop��������:
    template<typename SrcExp, typename DType, int etype>
	crop(const Exp<SrcExp, DType, etype> &src, Shape<2> oshape, index_t start_height, index_t start_width){....} 
	src��Դͼ��; 
	oshape�ǲü�������ݵĴ�С, in_shape; 
	start_height�ü���ʼ�߶�ֵ; 
	start_width�ü���ʼ���ֵ; 
	���زü��������
	383��ʹ�� crop(*, in_shape, param_.pad[0], param_.pad[1]). * �Ƕ���һ��Ĳв�����ϲ����õ��Ľ��; in_shape��oshape, �Ǳ�
	����Ĵ�С; �ü�����ʼλ����(param_.pad[0], param_.pad[1]). 
	
	* �Ƕ���һ��Ĳв�����ϲ����õ��Ľ��. ��ͨ��ʹ�� unpool ��������һ��Ĳв�����ϲ����õ�, �����������:
	1> ���ػ�:
	����һ���������һ��ػ�, �� stride = kerne. ����, kernel = (2, 2). �ڷ��򴫲�ʱ, ֻ���Ǹ����ֵ����һ���й���,
	���Խ��в�ݵ������ֵ��λ��, ����������2*2-1=3��λ������. ��
	 
	1       2       ���򴫲�        0 0 0 0
	               ---------->      0 1 2 0            
	3       4                       0 3 4 0
	  �в�                          0 0 0 0
	
	2>ƽ���ػ�:
    ��, kernel = (2, 2). ������Ҫ�Ѳв�ƽ���ֳ�2*2=4��, ���ݵ�ǰ��С�����4����Ԫ����. ��:
	
	1       2       ���򴫲�        1/4 1/4 1/2 1/2 
	               ---------->      1/4 1/4 1/2 1/2            
	3       4                       1/3 1/3 1   1  
	  �в�                          1/3 1/3 1   1 
 
    unpool������������:
	template<typename Reducer, typename SrcExp, typename DType, int etype>
	unpool(const Exp<SrcExp, DType, etype> &data_src,
       const Exp<SrcExp, DType, etype> &data_pooled,
       const Exp<SrcExp, DType, etype> &grad_pooled,
       index_t ksize_y, index_t ksize_x, index_t kstride_y, index_t kstride_x). ����ά���ݽ����ϲ���, ��óػ���Ĳ���.
	data_srcΪ����(��l��)������, ���ػ��������, ����pad(...)�������������;
    data_pooled�Ǳ���(��l��)�����, ���ػ������������ͼ, ����pad(...)�������������; 
	grad_pooledΪ��һ��(��l + 1��)�Ĳв�, ����pad(...)�������������;
	ksize_y�˵ĸ߶�, global_poolĬ��Ϊ��, ���ksize_y��param_.kernel[0], ���˵ĸ߶�; global_pool Ϊ��, ksize_yΪdata.shape_[2],
    ������������ĸ߶�; 
    ksize_x�˵Ŀ��, global_poolĬ��Ϊ��, ���ksize_xΪparam_.kernel[1], ���͵Ŀ��. global_pool Ϊ��, ksize_xΪdata.shape_[3],
    ������������Ŀ��. 
    kstride_yΪy�����ϵĻ�������, global_poolĬ��Ϊ��, ���kstride_yΪparam_.stride[0], ���������ĸ߶�; global_pool Ϊ��, 
	��kstride_yΪ1. 
    kstride_xΪx�����ϵĻ�������, global_poolĬ��Ϊ��, ���kstride_xΪparam_.stride[1], ���������Ŀ��; global_pool Ϊ��, 
	��kstride_xΪ1.  
	*/
                  
    } else if (param_.pool_type == pool_enum::kAvgPooling) { // ƽ���ػ�. 
      Assign(input_grad, req[pool_enum::kData],
             scalar<DType>(1.0f / (param_.global_pool ?
                      data.shape_[2] * data.shape_[3] :
                      param_.kernel[0] * param_.kernel[1])) * \
             crop(unpool<Reducer>(pad(data, param_.pad[0], param_.pad[1]),
                                  pad(output_data, 0, 0),
                                  pad(grad, 0, 0),
                                  param_.global_pool ? in_shape[0] : param_.kernel[0],
                                  param_.global_pool ? in_shape[1] : param_.kernel[1],
                                  param_.global_pool ? 1 : param_.stride[0],
                                  param_.global_pool ? 1 : param_.stride[1]),
                  in_shape,
                  param_.pad[0],
                  param_.pad[1]));
    /*
	���ǰ��Ĵ�����һ����, ��crop(...)�Ľ������, ����һ���������ʽscalar<DType>(DType s).
	
	1.0f / (param_.global_pool ? data.shape_[2] * data.shape_[3] : param_.kernel[0] * param_.kernel[1])
	global_poolĬ��Ϊ��, ���(..)Ϊparam_.kernel[0] * param_.kernel[1], ���˵ĳߴ�˻�; global_pool Ϊ��, (..)Ϊ
	data.shape_[2] * data.shape_[3], ����������ĳߴ�. Ȼ����ִ�� 1.0f / (...) ����sֵ. 
	*/  
    }
  }

 private:
  PoolingParam param_;
};  // class PoolingOp

template<typename xpu>
Operator* CreateOp(PoolingParam param, int dtype);


#if DMLC_USE_CXX11
class PoolingProp : public OperatorProperty {
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
    CHECK_EQ(in_shape->size(), 1);
    const TShape &dshape = (*in_shape)[0];
    CHECK_GE(dshape.ndim(), 4) << "Pooling: Input data should be 4D in (batch, channel, y, x) "
                               << "Or 5D in (batch, channel, d, y, x)";
    TShape oshape = dshape;
    if (dshape.ndim() ==  0) return false;
    if (param_.kernel.ndim() == 2) {
      CHECK_EQ(dshape.ndim(), 4) << "Pooling: Input data should be 4D in (batch, channel, y, x)";
      if (param_.global_pool) {
        oshape[2] = 1;
        oshape[3] = 1;
      } else {
        CHECK(param_.kernel[0] <= dshape[2] + 2 * param_.pad[0])
            << "kernel size (" << param_.kernel[0] << ") exceeds input (" << dshape[2]
            << " padded to " << (dshape[2] + 2*param_.pad[0]) << ")";
        CHECK(param_.kernel[1] <= dshape[3] + 2 * param_.pad[1])
            << "kernel size (" << param_.kernel[1] << ") exceeds input (" << dshape[3]
            << " padded to " << (dshape[3] + 2*param_.pad[1]) << ")";
        if (param_.pooling_convention == pool_enum::kValid) {
          oshape[2] = 1 + (dshape[2] + 2 * param_.pad[0] - param_.kernel[0]) /
                              param_.stride[0];
          oshape[3] = 1 + (dshape[3] + 2 * param_.pad[1] - param_.kernel[1]) /
                              param_.stride[1];
        } else {
          oshape[2] = 1 + static_cast<int>(ceil(static_cast<float>(
                              dshape[2] + 2 * param_.pad[0] -
                              param_.kernel[0]) / param_.stride[0]));
          oshape[3] = 1 + static_cast<int>(ceil(static_cast<float>(
                              dshape[3] + 2 * param_.pad[1] -
                              param_.kernel[1]) / param_.stride[1]));
        }
      }
      out_shape->clear();
      out_shape->push_back(oshape);
    } else if (param_.kernel.ndim() == 3) {
      CHECK_EQ(dshape.ndim(), 5) << "Pooling: Input data should be 5D in (batch, channel, d, y, x)";
      CHECK_LT(param_.kernel[0], dshape[2] + 2 * param_.pad[0]) << "kernel size exceeds input";
      CHECK_LE(param_.kernel[1], dshape[3] + 2 * param_.pad[1]) << "kernel size exceeds input";
      CHECK_LE(param_.kernel[2], dshape[4] + 2 * param_.pad[2]) << "kernel size exceeds input";
      if (param_.global_pool) {
        oshape[2] = 1;
        oshape[3] = 1;
        oshape[4] = 1;
      } else {
        if (param_.pool_type == pool_enum::kValid) {
          oshape[2] = 1 + (dshape[2] + 2 * param_.pad[0] - param_.kernel[0]) /
                              param_.stride[0];
          oshape[3] = 1 + (dshape[3] + 2 * param_.pad[1] - param_.kernel[1]) /
                              param_.stride[1];
          oshape[4] = 1 + (dshape[4] + 2 * param_.pad[2] - param_.kernel[2]) /
                              param_.stride[2];
        } else {
          oshape[2] = 1 + static_cast<int>(ceil(static_cast<float>(
                              dshape[2] + 2 * param_.pad[0] -
                              param_.kernel[0]) / param_.stride[0]));
          oshape[3] = 1 + static_cast<int>(ceil(static_cast<float>(
                              dshape[3] + 2 * param_.pad[1] -
                              param_.kernel[1]) / param_.stride[1]));
          oshape[4] = 1 + static_cast<int>(ceil(static_cast<float>(
                              dshape[4] + 2 * param_.pad[2] -
                              param_.kernel[2]) / param_.stride[2]));
        }
      }

      out_shape->clear();
      out_shape->push_back(oshape);
    }
    return true;
  }

  bool InferType(std::vector<int> *in_type,
                 std::vector<int> *out_type,
                 std::vector<int> *aux_type) const override {
    CHECK_EQ(in_type->size(), 1);
    int dtype = (*in_type)[0];

    if (dtype == -1) {
      LOG(FATAL) << "Input type to pooling is not specified.";
      return false;
    }

    out_type->clear();
    out_type->push_back(dtype);
    return true;
  }

  OperatorProperty* Copy() const override {
    PoolingProp *prop_sym = new PoolingProp();
    prop_sym->param_ = this->param_;
    return prop_sym;
  }

  std::string TypeString() const override {
    return "Pooling";
  }

  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
    return {out_grad[pool_enum::kOut], in_data[pool_enum::kData], out_data[pool_enum::kOut]};
  }

  std::vector<std::pair<int, void*> > BackwardInplaceOption(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data,
    const std::vector<void*> &in_grad) const override {
#if MXNET_USE_CUDNN == 1
    return {};
#else
    return {{in_data[pool_enum::kData], in_grad[pool_enum::kData]}};
#endif
  }

  Operator* CreateOperator(Context ctx) const override {
    LOG(FATAL) << "Not Implemented.";
    return NULL;
  }

  Operator* CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                             std::vector<int> *in_type) const override;

 private:
  PoolingParam param_;
};  // class PoolingProp
#endif  // DMLC_USE_CXX11
}  // namespace op
}  // namespace mxnet

#endif  // MXNET_OPERATOR_POOLING_INL_H_
