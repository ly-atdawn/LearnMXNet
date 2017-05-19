/*!
 * Copyright (c) 2017 by Contributors
 * \file softplus.cc
 * \brief softplus op
 * \author L
*/
#include "./softplus-inl.h"
#include "./mshadow_op.h" 
/*
mshadow_op.h�ļ�. mshadow_op��һ�������ռ�, ��Ҫ������һЩ�ṹ��, ����sigmoid, sigmoid_grad, relu, relu_grad. 
��Щ������Ľṹ��������ʵ��
������Ĺ��ܵ�, ��relu�����:
DType(a > DType(0.0f) ? a : DType(0.0f)) // ����Dtype��ǿ������ת��, ��ôrelu�����Ĺ��ܾ���max(x, 0).
�ڵ�����Щ�ṹ���ʱ, ��ͨ��:
op = new ActivationOp<cpu, mshadow_op::relu, mshadow_op::relu_grad, DType>();
����, ���½�һ������op, ָ��device, ForwardOp, BackwardOp. ForwardOp, BackwardOp����relu, relu_grad, ����Ҳ��ָ���˼���
������ǰ��ͺ���ľ��������. ��ָ���½���op�Ĺ���.     
*/
#if MXNET_USE_MKL2017 == 1 //  Intel��ѧ���ĺ�����(MKL). mxnet�й���MKL����ļ�����src/operator/mkl��,  
#include <mkl_memory.h>
#include "./mkl/mkl_memory-inl.h"
// #include "./mkl/mkl_relu-inl.h"
#endif  // MXNET_USE_MKL2017

namespace mxnet {
namespace op {
template<>
Operator *CreateOp<cpu>(SoftplusParam param, int dtype) { // ����op, �������param��dtype, dtype������ mshadow::kFloat32, 0;
// mshadow::kFloat64, Ϊ1. 
  Operator *op = NULL; // �����µ�opΪ��, Ȼ��ͨ��-inl.h�ж���õ���SoftplusOp������op. 
#if MXNET_USE_MKL2017 == 1
  /*if (param.act_type == activation::kReLU) {
      switch (dtype) {
      case mshadow::kFloat32:
          return new MKLReluOp<cpu, float>();
      case mshadow::kFloat64:
          return new MKLReluOp<cpu, double>();
      default:
          break;
      }
  }// ���ڼ������Softplus, ���param.act_type == softplus. ���, �����ע����. 
  */
  if (enableMKLWarnGenerated())
    LOG(INFO) << MKLReluOp<cpu, float>::getName() << " Skip MKL optimization";
#endif
  MSHADOW_REAL_TYPE_SWITCH(dtype, DType, { // MSHADOW_REAL_TYPE_SWITCH��.  
    switch (param.act_type) {
      case softplus::kSoftplus:
        op = new SoftplusOp<cpu, mshadow_op::softplus, mshadow_op::softplus_grad, DType>();
        break;
      default:
        LOG(FATAL) << "unknown activation type";
    }
  })
  return op; 
  /*
  op���Ƕ������layer, ��operator. ����-inl.h�ж���õ�SoftplusOp����ʵ����op, SoftplusOp���Ѷ������ǰ��ͷ������, ���op��
  ���Խ���ǰ��ͷ��������. 
  
  param.act_type. SoftplusParam�Ķ������SoftplusParam��Աact_type, ������ü����������. 
  ����-inl.h��, ����act_typeʱ, ����add_num����Ϊact_type�����softplus.  
  
  op = new SoftplusOp<cpu, mshadow_op::softplus, mshadow_op::softplus_grad, DType>();
  �½�һ������op, ָ��device, ForwardOp, BackwardOp. ForwardOp, BackwardOp����softplus, softplus_grad, ����Ҳ��ָ���˼���
  ������ǰ��ͺ���ľ��������. ��ָ���½���op�Ĺ���.     
  
  mshadow_op::softplus, mshadow_op::softplus_grad����./mshadow_op.h����Ľṹ��, ���涨�����ǰ��ͷ������. -inl.h�е�
  ForwardOp��mshadow_op::softplus, BackwardOp��mshadow_op::softplus_grad. 
  */
   
}

// DO_BIND_DISPATCH comes from operator_common.h
Operator *SoftplusProp::CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                                     std::vector<int> *in_type) const {
  std::vector<TShape> out_shape, aux_shape;
  std::vector<int> out_type, aux_type;
  
  /*
  out_shape��out_type����.cc�ж����, Ȼ�����-inl.h�е�InferType��InferShape��ָ��out_type��out_shape. 
  */
  CHECK(InferType(in_type, &out_type, &aux_type)); // ���InferType, �����in_type, out_type, aux_type�Ƿ��ƶ���ȷ. 
  CHECK(InferShape(in_shape, &out_shape, &aux_shape)); // ���InferShape, �����in_shape, out_shape, aux_shape�Ƿ��ƶ���ȷ.
  // �ƶ���ȷ, InferType��InferShape����True, ���򷵻�False. 
   
  DO_BIND_DISPATCH(CreateOp, param_, (*in_type)[0]);
}
/*
-inl.h��ֻ��������Operator *XXProp::CreateOperatorEx����, �����ʵ����.cc��.

DO_BIND_DISPATCH comes from operator_common.h, ����Ϊ�˺�. 
#if MXNET_USE_CUDA // GPU
#define DO_BIND_DISPATCH(Method, ...)                                \
  if (ctx.dev_mask() == cpu::kDevMask) {                             \
      return Method<cpu>(__VA_ARGS__);                               \
    } else {                                                         \
      return Method<gpu>(__VA_ARGS__);                               \
    }
#else // CPU
#define DO_BIND_DISPATCH(Method, ...)                                \
  if (ctx.dev_mask() == cpu::kDevMask) {                             \
    return Method<cpu>(__VA_ARGS__);                                 \
  } else {                                                           \
    LOG(FATAL) << "GPU is not enabled";                              \
    return nullptr;                                                  \
  }
#endif
*/


/*
ʹ������ĺ궨������parameter�ṹ��OperatorProperty��ע�ᵽMXNetϵͳ��:
DMLC_REGISTER_PARAMETER��MXNET_REGISTER_OP_PROPERTY. 
*/
DMLC_REGISTER_PARAMETER(SoftplusParam);

MXNET_REGISTER_OP_PROPERTY(Softplus, SoftplusProp)
.describe(R"(Elementwise activation function.
// describe��������, �ǶԸò���������, �ַ���. 
The following activation types is supported (operations are applied elementwisely to each
scalar of the input tensor):

- `softplus`: SoftPlus, `y = log(1 + exp(x))`

See `LeakyReLU` for other activations with parameters.
)") 
.add_argument("data", "Symbol", "Input data to activation function.") // add_argument��Ӳ���.  
.add_arguments(SoftplusParam::__FIELDS__()); // __FIELDS__:
/*
SoftplusParam�̳е���Parameter, Parameter�ṹ�嶨���__FIELDS__����:
inline static std::vector<ParamFieldInfo> __FIELDS__() {...}
����: get the fields of the parameters.
*/

 
/*
��" ... "�ڲ��ľ����ַ���, �����µ�OP�İ�����Ϣ, help(mxnet.sym.Softplus)ʱ��������Щ��Ϣ. ��֧������. 
*/

}  // namespace op
}  // namespace mxnet
