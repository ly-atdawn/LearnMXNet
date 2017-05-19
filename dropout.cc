/*!
 * Copyright (c) 2015 by Contributors
 * \file dropout.cc
 * \brief
 * \author Bing Xu
*/

#include "./dropout-inl.h"

namespace mxnet {
namespace op {
template<>
Operator *CreateOp<cpu>(DropoutParam param, int dtype) { // ����op, �������param��dtype. ��batcn_norm-inl.h����:
  // Operator *CreateOp(BatchNormParam param, int dtype);��ʵ��.  
  Operator *op = NULL;
  MSHADOW_REAL_TYPE_SWITCH(dtype, DType, {
    op = new DropoutOp<cpu, DType>(param); // ����DropoutOp��Ĺ��캯��explicit DropoutOp(DropoutParam param)�ഴ����Ķ���.  
  });
  return op;
}

// DO_BIND_DISPATCH comes from operator_common.h
Operator *DropoutProp::CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                                              std::vector<int> *in_type) const {
  std::vector<TShape> out_shape, aux_shape;
  std::vector<int> out_type, aux_type;
  CHECK(InferType(in_type, &out_type, &aux_type));
  CHECK(InferShape(in_shape, &out_shape, &aux_shape));
  DO_BIND_DISPATCH(CreateOp, param_, in_type->at(0));
}

/*
ʹ������ĺ궨������parameter�ṹ��OperatorProperty��ע�ᵽMXNetϵͳ��:
DMLC_REGISTER_PARAMETER��MXNET_REGISTER_OP_PROPERTY. 
*/
DMLC_REGISTER_PARAMETER(DropoutParam);

MXNET_REGISTER_OP_PROPERTY(Dropout, DropoutProp)
.describe(R"(Apply dropout to input. 
During training, each element of the input is randomly set to zero with probability p.
And then the whole tensor is rescaled by 1/(1-p) to keep the expectation the same as
before applying dropout. During the test time, this behaves as an identity map.
)") // ��DropoutӦ�õ�input��, ��Dropout�Ƕ�������Ľ��������˵��. 
.add_argument("data", "Symbol", "Input data to dropout.") // Dropout��Ĳ���. 
.add_arguments(DropoutParam::__FIELDS__());

}  // namespace op
}  // namespace mxnet


