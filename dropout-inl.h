/*!
 * Copyright (c) 2015 by Contributors
 * \file dropout-inl.h
 * \brief
 * \author
*/

#ifndef MXNET_OPERATOR_DROPOUT_INL_H_
#define MXNET_OPERATOR_DROPOUT_INL_H_ // ����� MXNET_OPERATOR_DROPOUT_INL_H_.
#include <dmlc/logging.h>
#include <dmlc/parameter.h>
#include <mxnet/operator.h>
#include <map>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>
#include "./operator_common.h" // src/operator��, mxnet�Ĳ�һЩ���õ�����.
#include "./mshadow_op.h" // src/operator��, ������һЩ�ṹ��. ��Щ�ṹ��������������ʵ��ĳЩ���ǰ������ͷ������, �缤��� 
// ����softplus, softplus_grad. һ������ǰ������, һ�����㷴������. 

#if defined(USE_STATIC_MKL) && defined(_OPENMP)
#include <omp.h> // OpenMPͷ�ļ�.  
#include <sched.h>

#include <mkl_vml_functions.h>
#include <mkl_vsl.h> // MKL��һЩͷ�ļ�. 
#endif  // USE_MKL && _OPENMP // �Ƿ�ʹ��MKL��OPENMP. ��make mxnet��ʱ��, BLAS��ʹ�õ���OpenBLAS, ������MKL. 
// �� USE_BLAS = openblas, ����USE_STATIC_MKL = NONE; ����, USE_NNPACK = 0; USE_MKL2017 = 0; USE_OPENMP = 1. 
// defined(USE_STATIC_MKL) && defined(_OPENMP)��: �Ƿ����˺� USE_STATIC_MKL �� _OPENMP. 

namespace dropout {
enum DropoutOpInputs {kData}; // Dropout�������, ֻ������kData. 
enum DropoutOpOutputs {kOut, kMask}; // Dropout������������: �������kOut��kMask. 0��1.  
enum DropoutOpForwardResource {kRandom}; // Dropout��ǰ�򴫲���Դ, kRandom. 
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
}  // namespace dropout

namespace mxnet {
namespace op {

#if defined(USE_STATIC_MKL) && defined(_OPENMP)
static void bernoulli_generate(int n, double p, int* r) {
  int seed = 17 + rand_r() % 4096;
  int nthr = omp_get_max_threads();
# pragma omp parallel num_threads(nthr)
  {
    const int ithr = omp_get_thread_num();
    const int avg_amount = (n + nthr - 1) / nthr;
    const int my_offset = ithr * avg_amount;
    const int my_amount = std::min(my_offset + avg_amount, n) - my_offset;
    if (my_amount > 0) {
      VSLStreamStatePtr stream;
      vslNewStream(&stream, VSL_BRNG_MCG31, seed);
      vslSkipAheadStream(stream, my_offset);
      viRngBernoulli(VSL_RNG_METHOD_BERNOULLI_ICDF, stream, my_amount,
        r + my_offset, p);
      vslDeleteStream(&stream);
    }
  }
}
#endif  // USE_MKL && _OPENMP // �Ƿ�ʹ��MKL��OPENMP.

struct DropoutParam : public dmlc::Parameter<DropoutParam> { // Dropout��Ĳ������ú�����. 
  float p; // p����ѵ�������м���/���ƽ��״̬�ĸ���ֵ. 
  DMLC_DECLARE_PARAMETER(DropoutParam) {
    DMLC_DECLARE_FIELD(p).set_default(0.5) // p��Ĭ��ֵ��0.5(set_default).  
    .set_range(0, 1) // set_range���ò���p�ı仯��Χ, ��0-1.  
    .describe("Fraction of the input that gets dropped out at training time"); // describe������������;, �ַ���. 
  }
};  // struct DropoutParam

template<typename xpu, typename DType>
class DropoutOp : public Operator { // Dropout������DropoutOp. ģ������������ģ�����xpu(cpu or gpu)��DType(float). 
 public:
  explicit DropoutOp(DropoutParam param) {
    // C++�е�explicit�ؼ���ֻ����������ֻ��һ���������๹�캯��, ���������Ǳ����ù��캯������ʾ��, ������ʽ��. param�ǲ�����
	// �Ķ���, ����param������Dropout�Ĳ���p. 
    this->pkeep_ = 1.0f - param.p; // pkeep_��real_t�͵ı���. real_t�����dmlc-core/include/dmlc/data.h.
	// typedef float real_t; 
	// ����data.h�л���index_t�Ķ���: typedef unsigned index_t; the unsigned integer type 
	// pkeep_ = 1.0f - p. 
  }

  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_states) {
    /*ǰ�����, �麯��. ������ʵ�������ж���. ����Ҫ����ֵ. ����Ϊ�� l ��. 
	in_data: ��������data, ֻ���ϲ������.
	req: ���ݲ���ģʽ. 
	out_data: �������, out. ��ѵ����ʱ�򱾲����������.  
	*/
    using namespace mshadow;
    using namespace mshadow::expr;
    
    CHECK_EQ(in_data.size(), 1); // in_data������СΪ1, ��Dropout����������ֻ������. 
    if (ctx.is_train) {
      CHECK_EQ(out_data.size(), 2);
    }
    /*
	ctx��OpContext�ṹ�嶨��ĳ�Ա. OpContext�ṹ�嶨���include/mxnet/operator.h. ����ctx��Ա���ʽṹ����is_train:
	int is_train; // operator���ڽ��� train ���� test (is_train); 
	*/
    Stream<xpu> *s = ctx.get_stream<xpu>(); // operator���ĸ�device������
    
    Tensor<xpu, 2, DType> data = in_data[dropout::kData].FlatTo2D<xpu, DType>(s);
    Tensor<xpu, 2, DType> out = out_data[dropout::kOut].FlatTo2D<xpu, DType>(s);
    /*
	��in_data[dropout::kData]������������FlatTo2D����2ά������data; ����(��l��)������. 
	����out_data[dropout::kOut]�����������FlatTo2D����2ά������out. ����(��l��)�����. 
	*/
    
    if (ctx.is_train) { // ������ѵ���׶�. 
      Tensor<xpu, 2, DType> mask = out_data[dropout::kMask].FlatTo2D<xpu, DType>(s);
      /*
	  ������ѵ���׶�ʱ, out_data�����Ĵ�С��2. һ����������������, һ����kMask. 
	  ��out_data[dropout::kMask]�����������FlatTo2D����2ά������mask. mask�����������, ��Dropout��, ������������һ������
	  ����:
	  	   
	  ԭ����������ֵ�� yi^(l), ���ϸ���֮���Ϊ ri^(l) * yi^(l), ri^(l) ~ Bernoulli(p). ��Dropout��Ϳ��Կ����Ƕ����������
	  ����data������һ������ֵ. ��Ϊ ri^(l) ~ Bernoulli(p), ��0-1�ֲ�, ���Ըý����ܼ����������, Ҳ��˼�С������Ĺ�ģ, ����
	  �����ʵ�ʲ�����Ŀ�ǲ����. (�ı���������data, ���������ӵĲ���. ��һ���ĸ�������/����ý��). 
	  
	  ���, mask���ݵľ��� ri^(l) �Ľ�ɫ, �������l���ÿ�����ĸ���ֵ, �õ���mask��, �ٺͱ���(��l��)����������data������˼�
	  ��.   
	  */
      
#if defined(USE_STATIC_MKL) && defined(_OPENMP) // USE_MKL && _OPENMP // ʹ��MKL��OPENMP.
      DType* outptr = out.dptr_;
      DType* dataptr = data.dptr_;
      int* maskptr = reinterpret_cast<int*>(mask.dptr_);
      int count = mask.shape_[0]*mask.shape_[1];
      bernoulli_generate(count, this->pkeep_, maskptr);
  #pragma omp parallel for // OPENMP���� 
      for (int i = 0; i < count; ++i) {
        outptr[i] = dataptr[i] * maskptr[i];
      }
#else // ��ʹ��MKL��OPENMP. 
      Random<xpu> *prnd = ctx.requested[dropout::kRandom].get_random<xpu, real_t>(s);
      /*
	  OpContext: �ṹ��, ������include/mxnet/operator.h��, �ýṹ����Լ�¼������ǰ��ͺ��򴫲��е���Ϣ. ctx�ǽṹ��OpContext��
	  ��Ķ���, requested��OPContext�ṹ���µĺ���:
      // brief Resources requested by the operator
  	  std::vector<Resource> requested; // �������ز����������Դ. 
      ctx.requested���ص���һ����������, ������Ҫ��ֻ��kRandom����Դ����, ��һ�����������Դ. 
	  ctx.requested[dropout::kRandom]����һ��Resource�Ķ���. �ٵ���get_random����.
	  
	  Resource�ṹ����mxnet����������Դ�ṹ��, ��NDArray����. NDArray��һ����ά���������.
	  
	  get_random���������: include/mxnet/resource.h��: get_random�����Ƕ�����Resource�ṹ���µĺ���: 
      template<typename xpu, typename DType>
 	  inline mshadow::Random<xpu, DType>* get_random(mshadow::Stream<xpu> *stream) 
 	  get_random�������������. 
	  stream��device��; ����һ�������������, ������ mshadow::Random<xpu, DType>* . real_t��float, ��DType.
	  
	  ����ctx��ȡkRandom�������Դ����, �ڵ���get_random�õ�һ�������������, *prnd����һ�������������. *prnd����device s��, 
	  real_t���͵������������.   
	  */
      
      mask = tcast<DType>(F<mshadow_op::threshold>(
             prnd->uniform(mask.shape_), pkeep_) * (1.0f / pkeep_));
      /*
	  mask���ݵľ��� ri^(l) �Ľ�ɫ, �������l���ÿ�����ĸ���ֵ. ��������ȡmask��ֵ. mask��һ��2ά������, ������. ��Ϊdata��2
	  ά������, ����maskҲ��2ά������.
	  
	  ���Ȳ���, ���������ǽ��״̬���Ƶĸ���, ���������������������, Ȼ���maskȫ������pkeep_. 
      �������Dropout�ĸ���ֵp�ǽ��������״̬�ĸ���ֵ, 1-p������״̬. ����Dropout������֪ʶ, ��test/predictʱ, ÿ��weightҪ
	  ����״̬�ĸ���ֵ, ��1-p. ������maskȫ������pkeep_, ��test/predict��ʱ��Ͳ���Ҫ���� 1-p ��. 
	  
      ��������F<mshadow_op::threshold>(prnd->uniform(mask.shape_), pkeep_):
	  mshadow���ڱ��ʽ��������(DotExp, BinaryMapExp, UnaryMapExp):
	  BinaryMapExp(����ͼ)��˫Ŀ����ı��ʽ��, ��BinaryMapExp������F����F< OP >(lhs, rhs)������һ��˫Ŀ����;
	  DotExp������˵���, ������õľ���dot����;
	  UnaryMapExp���ǵ�Ŀ������ı��ʽ��, ��UnaryMapExp������F����.
	  ����, F<mshadow_op::threshold>(prnd->uniform(mask.shape_), pkeep_)��һ��˫Ŀ�����. F< OP >(lhs, rhs)�е�OP���ǲ�����,
	  ��lhs��rhs��ʲô����, ����OP��mshadow_op::threshold, mshadow_op::threshold�Ƕ�����src/operator/mshadow_op.h��:
	  
	  threshold������mshadow_op.h�µĽṹ��, threshold��������ȡBernoulli mask��. ��threshold����ר������Dropout��.
	  threshold��������: �������a��b, ���� a < b ? DType(1.0f) : DType(0.0f). 
	  ����a��prnd->uniform(mask.shape_), b��pkeep_���������״̬����. 
	  
	  prnd->uniform(mask.shape_)�Ǿ��Ȳ���, uniform���������: mshadow/mshadow/random.h 143��. uniform����
	  class Random<cpu, DType>��, ��prnd��Random��Ķ���, ���Կ�������uniform����.   
	  template<int dim>
	  inline expr::ReshapeExp<Tensor<cpu, 1, DType>, DType, dim, 1> uniform(Shape<dim> shape). shape��Tensor��shape, ���Ｔ
	  mask.shape_, shape_������һ��Tensor��shape. dim��tensor��ά��, ������2, ��������ά����2. uniform������[0, 1]�ľ��ȷֲ�, 
	  ��[0, 1]��Ϊ1, ����Ϊ0. �� prnd->uniform(mask.shape_) ���һ��:
	  ��������mshadow::expr::ReshapeExp<mshadow::Tensor<mshadow::cpu, 1, float>, float, 2, 1>.   
	  
	  prnd->uniform(mask.shape_)��һ��1ά������, ��˿��Ե�������ʹ��, ���, a��prnd->uniform(mask.shape_), ��a������һ������. 
	  ----------------------------------------------------------------------------------------------------------------------- 
	  tcast����, �ú��������: mshadow/mshadow/expression.h 108��. 
	  template<typename DstDType, typename SrcDType, typename EType, int etype>
	  inline TypecastExp<DstDType, SrcDType, EType, (etype|type::kMapper)> tcast(const Exp<EType, SrcDType, etype> &exp){...}.
	  ����һ���������ʽ. 
	  */       
             
      Assign(out, req[dropout::kOut], data * mask);
      /*
	  Assign��ֵ����, out�Ǳ���(��l��)�����, req�����ݲ���ģʽ, exp��data * mask. exp���������ϼ���һ�����ʳ���, ����������
	  ֵ�͸���ֵ���. ���ʷ��Ӳ�Ŭ���ֲ�, ��0-1�ֲ�. 
	  */
#endif  // USE_MKL && _OPENMP
    } else {
      Assign(out, req[dropout::kOut], F<mshadow_op::identity>(data));
      /*
	  �������ѵ���׶�, �Ͳ���Ҫmask��, ��Ϊ��ѵ���׶�����mask��ʱ��, ������pkeep_��, �����test/predict�׶�, �����weight�Ͳ�
	  ��Ҫ�ٳ� 1 - p��. ���, exp����data.
	  
	  F<mshadow_op::identity>(data)��һ����Ŀ�����, �������mshadow_op::identity, identity����ṹ��ʵ�ֵĲ���������DType a,
	  ����DType a. ������������. 
	   
      ��data��ֵ������(��l��)���out. 
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
    /*Dropout��(��l��)û��Ȩ�غ�ƫ��, ���Ҫ���������ʧJ����Dropout��(��l��)�Ĳв�.
    !!!!!!!!!!!!!!!!�ݶȿ��Կ�������ʧJ���ڲ�����ĵ���, �в���Կ�������ʧJ���ڲ�����ĵ���!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
	 
    in_grad����в����, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)��.
	out_grad����в����, ��������, ÿ��Ԫ�ص�������TBlob. ��һ��(��l + 1��)�Ĳв�, ���㱾��Ĳв�. 
	in_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)������.  
	out_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)�����. 	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType.
	*/
    using namespace mshadow;
    using namespace mshadow::expr;
    CHECK_EQ(out_grad.size(), 1);
    CHECK_EQ(in_grad.size(), 1);
    /*
	Dropout��(��l��)��out_grad, in_grad������СΪ1. ��ֻ������Ĳв�(��l + 1��)�Ĳв�, ����в�(��l��Ĳв�).  
	*/
    Stream<xpu> *s = ctx.get_stream<xpu>();
    Tensor<xpu, 2, DType> grad = out_grad[dropout::kOut].FlatTo2D<xpu, DType>(s);
    Tensor<xpu, 2, DType> mask = out_data[dropout::kMask].FlatTo2D<xpu, DType>(s);
    Tensor<xpu, 2, DType> gdata = in_grad[dropout::kData].FlatTo2D<xpu, DType>(s);
    /*DropoutΪ��l��. 
	����l + 1��Ĳв�out_grad[0]����FlatTo2D��������2ά������. ���в��������һ����, ��2ά��. grad.
	����l������out_data[1]����FlatTo2D��������2ά������. mask. out_data������СΪ2, ��һ���Ǳ�������out_data[0], һ����
	Dropout���mask out_data[1]. 
	���屾��(��l��)�Ĳв���2ά������. gdata.  
	*/
    
#if defined(USE_STATIC_MKL) && defined(_OPENMP)
      DType* ingradptr = gdata.dptr_;
      DType* outgradptr = grad.dptr_;
      int* maskptr = reinterpret_cast<int*>(mask.dptr_);

      int count = mask.shape_[0]*mask.shape_[1];

  #pragma omp parallel for
      for (int i = 0; i < count; ++i) {
        ingradptr[i] = outgradptr[i] * maskptr[i];
      }
#else  // USE_MKL && _OPENMP ʹ��MKL��OPENMP. ���ʺͲ�ʹ��MKL��OPENMPʱ�ķ��������һ����, ֻ��ʹ��MKL��OPENMPʱ, ����:
	   /*
	   DType* ����float*������ingradptr(����в�), outgradptr(��һ��в�)��int*������maskptr(����mask). Ȼ��:
	   ingradptr[i] = outgradptr[i] * maskptr[i]; conut = mask.shape_[0]*mask.shape_[1];��mask����ĸ߶ȺͿ�ȳ˻�.		    
	   */ 
      Assign(gdata, req[dropout::kData], grad * mask);
      /*
	  ��ʹ��MKL��OPENMPʱ, ����(��l��)�Ĳв�gdata = grad * mask, ����һ��(��l + 1��)�Ĳв� * ����(��l��)��mask.  
	  */
#endif  // USE_MKL && _OPENMP ��ʹ��MKL��OPENMP.  
  }

 private:
  real_t pkeep_;
};  // class DropoutOp


template<typename xpu>
Operator *CreateOp(DropoutParam param, int dtype);

#if DMLC_USE_CXX11
class DropoutProp : public OperatorProperty {
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
    CHECK_EQ(in_shape->size(), 1);
    const TShape &dshape = in_shape->at(0);
    if (dshape.ndim() == 0) return false;
    out_shape->clear();
    out_shape->push_back(dshape);
    out_shape->push_back(dshape);
    return true;
  }

  bool InferType(std::vector<int> *in_type,
                 std::vector<int> *out_type,
                 std::vector<int> *aux_type) const override {
    CHECK_EQ(in_type->size(), 1);
    int dtype = in_type->at(0);

    if (dtype == -1) {
      LOG(FATAL) << "input type to dropout is not specified.";
      return false;
    }

    size_t nout = this->ListOutputs().size();
    out_type->clear();
    for (size_t i = 0; i < nout; ++i) out_type->push_back(dtype);
    return true;
  }

  OperatorProperty* Copy() const override {
    auto ptr = new DropoutProp();
    ptr->param_ = param_;
    return ptr;
  }

  std::string TypeString() const override {
    return "Dropout";
  }

  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
    return {out_grad[dropout::kOut], out_data[dropout::kMask]};
  }

  std::vector<std::pair<int, void*> > BackwardInplaceOption(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data,
    const std::vector<void*> &in_grad) const override {
    return {{out_grad[dropout::kOut], in_grad[dropout::kData]}};
  }

  std::vector<std::pair<int, void*> > ForwardInplaceOption(
    const std::vector<int> &in_data,
    const std::vector<void*> &out_data) const override {
    return {{in_data[dropout::kData], out_data[dropout::kOut]}};
  }

  std::vector<ResourceRequest> ForwardResource(
    const std::vector<TShape> &in_shape) const override {
    return {ResourceRequest::kRandom};
  }

  int NumVisibleOutputs() const override {
    return 1;
  }

  int NumOutputs() const override {
    return 2;
  }

  std::vector<std::string> ListOutputs() const override {
    return {"output", "mask"};
  }

  Operator* CreateOperator(Context ctx) const override {
    LOG(FATAL) << "Not Implemented";
    return NULL;
  }

  Operator* CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                             std::vector<int> *in_type) const override;

 private:
  DropoutParam param_;
};  // class DropoutProp
#endif  // DMLC_USE_CXX11
}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_DROPOUT_INL_H_

