/*!
 * Copyright (c) 2015 by Contributors
 * \file pooling.cc
 * \brief
 * \author Bing Xu
*/
#include "./pooling-inl.h"
#if MXNET_USE_MKL2017 == 1 // ���ʹ��MKL2017. �Ͳ���src/operator/mkl�µ�pooling�������������Pooling. 
#include <mkl_memory.h>
#include "./mkl/mkl_memory-inl.h"
#include "./mkl/mkl_pooling-inl.h"
#endif  // MXNET_USE_MKL2017
#if MXNET_USE_NNPACK == 1 // ���ʹ��NNPACK, ������src/operator/nnpack�µ�pooling�������������Pooling. 
#include "./nnpack/nnpack_pooling-inl.h"
#endif  // MXNET_USE_NNPACK

namespace mxnet {
namespace op {

template<>
Operator *CreateOp<cpu>(PoolingParam param, int dtype) { // ����op, �������param��dtype, dtype��(*in_type)[0], ��δ�õ�. 
  Operator *op = NULL; // �����µ�opΪ��, Ȼ��ͨ��-inl.h�ж���õ���SoftplusOp������op. 
#if MXNET_USE_MKL2017 == 1 // ʹ��MKL2017. 
    if ((param.pool_type == pool_enum::kMaxPooling)
      || (param.pool_type == pool_enum::kAvgPooling
      && UseMKLPooling(param))) { // �ػ����������ػ���ƽ���ػ�.  
      switch (dtype) { // dtype��һ��int�͵ı���, ������kFloat32����kFloat64. 
      case mshadow::kFloat32:
        return new MKLPoolingOp<cpu, float>(param); // �����ػ�OP. ��MKLPoolingOp���ඨ���й�.  
      case mshadow::kFloat64:
        return new MKLPoolingOp<cpu, double>(param);
      default:
        break;
      }
    }
    LOG(INFO) << MKLPoolingOp<cpu, float>::getName() << " Skip MKL optimization";
#endif
#if MXNET_USE_NNPACK == 1 // ʹ��NNPACK. 
  // NNPACK only support max-pooling with kernel = 2, stride = 2, pooling_convention
  // = kFull(note that the default value is kValid in MXNet)
  if ((param.pool_type == pool_enum::kMaxPooling) // ���ػ�����.  
    && (param.pooling_convention == pool_enum::kFull)
    && (param.kernel.ndim() == 2) && (param.stride.ndim() == 2)
    && (param.kernel[0] == 2) && (param.kernel[1] == 2)
    && (param.stride[0] == 2) && (param.stride[1] == 2)) {
    switch (dtype) {
    case mshadow::kFloat32:
      return new NNPACKPoolingOp<cpu, mshadow::red::maximum, float>(param);
    default:
      break;
    }
  }
#endif
  MSHADOW_REAL_TYPE_SWITCH(dtype, DType, { // ��ʹ��MKL2017��NNPACK������´����ػ�����! 
    switch (param.pool_type) { // param��PoolingParam�Ķ���, ����pool_type, �õ�kMaxPooling��kAvgPooling��kSumPooling. 
      case pool_enum::kMaxPooling:
        op = new PoolingOp<cpu, mshadow::red::maximum, DType>(param); // �������ػ�. ����new����. 
        break;
      case pool_enum::kAvgPooling:
        op = new PoolingOp<cpu, mshadow::red::sum, DType>(param);
        break;
      case pool_enum::kSumPooling:
        op = new PoolingOp<cpu, mshadow::red::sum, DType>(param);
        break;
      default:
        LOG(FATAL) << "unknown pooling type";
        return NULL;
    }
    /*
	�ڶ���ػ�������PoolingOpʱ, �������ģ����. ģ�������:
	template<typename xpu, typename Reducer, typename DType> 
	
	������new�����ػ�����ʱ, ��ָ��xpu��Reducer, DType�ڱ���ʱָ��.  
	*/
  })

  return op;
}

// DO_BIND_DISPATCH comes from operator_common.h
Operator* PoolingProp::CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                                     std::vector<int> *in_type) const {
  std::vector<TShape> out_shape, aux_shape;
  std::vector<int> out_type, aux_type;
  CHECK(InferType(in_type, &out_type, &aux_type));
  CHECK(InferShape(in_shape, &out_shape, &aux_shape));
  DO_BIND_DISPATCH(CreateOp, param_, (*in_type)[0]);
}

/*
ʹ������ĺ궨������parameter�ṹ��OperatorProperty��ע�ᵽMXNetϵͳ��:
DMLC_REGISTER_PARAMETER��MXNET_REGISTER_OP_PROPERTY. 
*/
DMLC_REGISTER_PARAMETER(PoolingParam);

MXNET_REGISTER_OP_PROPERTY(Pooling, PoolingProp)
.describe("Perform spatial pooling on inputs.")
.add_argument("data", "Symbol", "Input data to the pooling operator.")
.add_arguments(PoolingParam::__FIELDS__());

}  // namespace op
}  // namespace mxnet
