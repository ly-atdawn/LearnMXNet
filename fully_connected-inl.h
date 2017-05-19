/*!
 * Copyright (c) 2015 by Contributors
 * \file fully_connect_op-inl.h
 * \brief fully connect operator and symbol
*/
#ifndef MXNET_OPERATOR_FULLY_CONNECTED_INL_H_
#define MXNET_OPERATOR_FULLY_CONNECTED_INL_H_

#include <dmlc/logging.h>
#include <dmlc/parameter.h>
#include <mxnet/operator.h>
#include <map>
#include <vector>
#include <string>
#include <utility>
#include "./operator_common.h"


namespace mxnet {
namespace op {

// Declare enumeration of input order to make code more intuitive.
// These enums are only visible within this header
namespace fullc {
enum FullyConnectedOpInputs {kData, kWeight, kBias}; 
/*
ȫ���ӵ��������:
�ϲ���������: kData; ��������Ȩ��: kWeight; ��������ƫ��: kBias. 

��fully_connected-inl.h���ȼ���#include<iostream>, Ȼ���ٽ�kData, kOut, KBais���, �����Shape��һЩֵ. �����һ��, ��ǰ���
����Ĺ�����, kData, kOut, KBais��int�͵���. Ϊ0, 1, 2����.  
*/ 
enum FullyConnectedOpOutputs {kOut}; // ���: kOut. 
}  // fullc

struct FullyConnectedParam : public dmlc::Parameter<FullyConnectedParam> {
  int num_hidden;
  bool no_bias; 
  /*
  ȫ���Ӳ�Ĳ���: 
  num_hidden: ����(ȫ���Ӳ�)�Ľ�����.
  no_bias: ȫ���Ӳ��Ƿ�ʹ��ƫ��. 
  */
  DMLC_DECLARE_PARAMETER(FullyConnectedParam) { // #define DMLC_DECLARE_PARAMETER(PType)
    // TODO(bing) add support for boolean
    DMLC_DECLARE_FIELD(num_hidden).set_lower_bound(1) // ȫ���Ӳ�Ľ�����������1. set_lower_bound�����½�. 
    .describe("Number of hidden nodes of the output.");
    /*
	DMLC_DECLARE_FIELD(num_hidden).set_lower_bound(1) ���ú�DMLC_DECLARE_FIELD��ȫ���Ӳ�Ĳ���num_hidden��������, ���Ҳ�����
	Ĭ��ֵ����С, ���ֵ.  
	*/
    DMLC_DECLARE_FIELD(no_bias).set_default(false)
    .describe("Whether to disable bias parameter.");
    /*
	DMLC_DECLARE_FIELD(no_bias).set_lower_bound(1) ���ú�DMLC_DECLARE_FIELD��ȫ���Ӳ�Ĳ���no_bias��������, ���Ҳ�����
	Ĭ��ֵ����С, ���ֵ. Ĭ��ʹ��ƫ��. 
	*/
  }
};

/**
 * \brief This is the implementation of fully connected operator.
 * \tparam xpu The device that the op will be executed on.
 */
 
/*
����ȫ���Ӳ�, �޼������:
a^(l) = W' * z^(l)
z^(l + 1) = a^(l)
a^(l + 1) = W' * z^(l + 1)
*/

template<typename xpu, typename DType> 
/*
ȫ���Ӳ�ļ���ֻ��һ����ʽ, ����ȫ���Ӳ㲻���������, �����out = W .* X + b. ����ڶ���FullyConnectedOp��ʱ��ģ�����ֻ��
xpu��DType. 

��make mxnet��ʱ��, ��Ļ�����config.mk����xpu��DType��ֵ. 
[with xpu = mshadow::op::cpu, DType = float] 
*/ 
class FullyConnectedOp : public Operator {
 public:
  explicit FullyConnectedOp(FullyConnectedParam p) {
    this->param_ = p;
    /*
    C++�е�explicit�ؼ���ֻ����������ֻ��һ���������๹�캯��, ���������Ǳ����ù��캯������ʾ��, ������ʽ��, ��
    �����Ӧ����һ���ؼ�����implicit, ��˼�����ص�, �๹�캯��Ĭ������¼�����Ϊimplicit(��ʽ).
    
    thisָ����һ��������ÿһ����Ա�����е�����ָ��. ��ָ�����ڱ��ó�Ա�����������Ǹ�����. 
    
    p��FullyConnectedParamȫ���Ӳ������Ķ���, ��p��ֵ��param_. ��͵����ĸ�ֵ��һ��, param_����p, ���Կ�����ָ��p��ָ��.
	*/
  }

  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_args) {
    using namespace mshadow;
    using namespace mshadow::expr;
    if (req[fullc::kOut] == kNullOp) return; // ȫ���Ӳ�����ݲ���ģʽ������kNullOp, ��ʲô������. 
    CHECK_EQ(req[fullc::kOut], kWriteTo); 
    /*
	case kNullOp:                     \
          break;                          \
        case kWriteTo:                    \
        case kWriteInplace:               \
          (out) = (exp);                  \
          break;                          \
        case kAddTo:                      \
          (out) += (exp); 
	*/
    
    size_t expected = param_.no_bias ? 2 : 3;
    /*
	param_.no_bias�Ƿ�Ϊ��, ���Ϊ����expectedΪ2, ����Ϊ3. 
	*/
    
    CHECK_EQ(in_data.size(), expected);
    CHECK_EQ(out_data.size(), 1);
    // TODO(bing): check the BLAS Handle, be careful
    // maybe need blas handle from context
    // TODO(bing): judge shape to remove flatten op
    Stream<xpu> *s = ctx.get_stream<xpu>();
#if defined(__CUDACC__)
    CHECK_EQ(s->blas_handle_ownership_, Stream<xpu>::OwnHandle)
        << "Must init CuBLAS handle in stream";
#endif  // __CUDACC__

    // std::cout<<"in_data kData: "<<fullc::kData<<std::endl; ����fullc::kData��0, ������data��in_data[0]. 
    const TShape& ishape = in_data[fullc::kData].shape_;
    
    // std::cout<<"out_data kOut: "<<fullc::kOut<<std::endl; ����fullc::kOut��0, ������data��out_data[0].
	/*
	��mxnet��, kData��0. ��������. 
	*/ 
    const TShape& oshape = out_data[fullc::kOut].shape_;
    /*
	������������in_data[0]��shape���������out_data[0]��shape.
	
	TShape mxnet::TBlob::shape_, ������������Tensor��shape. TBlob��ĳ�Ա, ����ֵ������TShape. 
	*/

	// std::cout<<"in_data kData: "<<fullc::kData<<std::endl; ����fullc::kData��0, ������data��in_data[0].
	// std::cout<<"ishape: "<<ishape[0]<<" "<<ishape.ndim()<<std::endl; ishape[0]��64, 64��batch_size, ����ѵ�����Ĵ�С.  
	// ishape.ndim()��2, ��ishape��һ��2ά��. 
    Tensor<xpu, 2, DType> data = in_data[fullc::kData].get_with_shape<xpu, 2, DType>(
        Shape2(ishape[0], ishape.ProdShape(1, ishape.ndim())), s);
    /*
	��in_data[0](��������)����2ά������. ���ｫTBlob��������Tensor����ʱû��ʹ��FlatTo2D, ��������get_with_shape. ��������:
	mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get_with_shape(const mshadow::Shape<dim> & shape, 
	mshadow::Stream< Device > *stream = NULL)const
	����shape, ��TBlob����һ��Tensor. ���shape�ʹ洢�Ĵ�С��һ��ʱ, �ᱨ��.
	
	��https://raw.githubusercontent.com/jdeng/gomxnet/master/mxnet.cc�����ҵ�Shape1, Shape2, Shape3, Shape4�Ķ���:
	Shape2��������:
	MSHADOW_XINLINE Shape<2> Shape2(index_t s0, index_t s1) {
        Shape<2> s; s[0] = s0; s[1] = s1;
        return s;
    } ���, Shape2����Shape<dim>���͵ĺ���, Shape2����Shape<2>�Ķ���. ���ú�get_with_shape�����ĵ�һ���������Ӧ.
	
	Shape2(ishape[0], ishape.ProdShape(1, ishape.ndim())):
    index_t��һ����������(typedef mshadow::index_t mxnet::index_t), ����s0��ishape[0]. s1��ishape.ProdShape(1, ishape.ndim()).
	
    index_t�Ķ�����mshadow/mshadow/base.h��.
	typedef unsigned index_t; 
	unsigned a; ��unsigned int a; ��ͬ����. ����ʡ����int, ֻ�ܺ�unsigned int�ȼ�.  
	
	ProdShape����TShape���µĳ�Ա����: ����ͨ��Ѱ��TShape�����ҵ��ú���:
	
	index_t mxnet::TShape::ProdShape(int dimstart, int dimend )const 
	����һ������, ��������[dimstart,dimend), ����ֵ������index_t, ����һ����������. ���, 
	ishape.ProdShape(1, ishape.ndim())�Ͳ���һ��[1, 2)������.   
	*/    
    
	// std::cout<<"in_data kWeight: "<<fullc::kWeight<<std::endl; fullc::kWeight��1, ��in_data��kWeight����1, Ȩ��.    
    Tensor<xpu, 2, DType> wmat = in_data[fullc::kWeight].get<xpu, 2, DType>(s);
    /*
	������(��l��)��Ȩ��in_data[kWeight]����2ά������. ��β�û��ʹ��get_with_shape, ����ʹ��get����:
    mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get(mshadow::Stream<Device> *stream = NULL)const 
	*/
    
    // std::cout<<"out_data kOut: "<<fullc::kOut<<std::endl; fullc::kOut��0, kData��kOut����0.
	// std::cout<<"oshape: "<<oshape[0]<<" "<<oshape.ndim()<<std::endl; oshape��ishapeһ��, 64 2. 
    Tensor<xpu, 2, DType> out = out_data[fullc::kOut].get_with_shape<xpu, 2, DType>(
        Shape2(oshape[0], oshape.ProdShape(1, oshape.ndim())), s);  
    /*
	��out_data[0](�������)����2ά������. ���ｫTBlob��������Tensor����ʱû��ʹ��FlatTo2D, ��������get_with_shape. ��������:
	mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get_with_shape(const mshadow::Shape<dim> & shape, 
	mshadow::Stream< Device > *stream = NULL)const
	����shape, ��TBlob����һ��Tensor. ���shape�ʹ洢�Ĵ�С��һ��ʱ, �ᱨ��.
	
	����outʱ�Ͷ���data�ķ�����һ�µ�. һ��������(��l��)������, һ���Ǳ���(��l��)�����. 
	*/    
    
    out = dot(data, wmat.T());
    /*�������out. 
	����ȫ���Ӳ��ǰ�򴫲�������֪: ȫ���Ӳ����� = W.*���� + ƫ��.
	
	������ȫ���Ӳ�û��ƫ����, ��ô out = data .* weight. 
	
	dot����: http://mxnet.io/api/python/symbol.html, ������������ĵ��֮��. �������(1D)�����˷�(2D)... 
	
	wmat.T()��ת��, ��Ȩ�ص�ת��. 
	*/
    
    if (!param_.no_bias) { // ���ʹ��ƫ��, ��ôout += ... + bias. 
      // std::cout<<"in_data kData: "<<fullc::kBias<<std::endl; fullc::kBias��2, ����ƫ��. 
      Tensor<xpu, 1, DType> bias = in_data[fullc::kBias].get<xpu, 1, DType>(s);
      /*
	  ������(��l��)��ƫ��in_data[kBias]����1ά������. ��β�û��ʹ��get_with_shape, ����ʹ��get����:
      mshadow::Tensor<Device, dim, DType> mxnet::TBlob::get(mshadow::Stream<Device> *stream = NULL)const 
      
      1ά����������һ������. 
	  */
      
      out += repmat(bias, data.size(0));
      /*����Ҳ��һ�����, ��һ��mxnet��ȫ���������ʵ�ֵ�. 
      // std::cout<<"data.size: "<<data.size<<std::endl; Tensor�����Ĵ�С�����������. 
	  // std::cout<<"bias: "<<bias<<std::endl; Tensor���������������. 
	  
	  std::cout<<"data.size(0): "<<data.size(0)<<std::endl; ���Ϊ64, ��batch_size. 
	  
	  =====================================================���������========================================================
	  ������Ĺ���, �������С��batch_size. ����(�ϴθ��������)->����batch_size�������������򴫲�, 
	  ������ʱ�����򴫲���һ���Խ�batch-size����������, �õ�batch_size�����(�򵥵�, ��batch_size����1����)
	  ->����batch_size����ǩ���з��򴫲�, ��������Ĳ���->�õ����������������->��һ��batch������... 
	  ====================================================���������=========================================================
	  
	  ���������ʱ��ȫ���Ӳ�data����batch_size������������, outҲ��batch_size�����, ���������һ��, ���Ȩ��wmat.T()��һ��.
	  Ȼ��ȫ���Ӳ������ƫ����, �ټ���ƫ�����. 
	  
	  outһ����batch_size�����������, ���û�и������Ӧ�ü���һ��bias, ��bias��һ��������, ����Ƚ�bias���и���, ���ú���
	  repmat���и���, һ������batch_size��, Ȼ���ٺ� W.*X������Ӽ���.
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
    /*ȫ���Ӳ�(��l��)��Ȩ�غ�ƫ��, ���Ҫ������ʧJ����Ȩ�ص��ݶȺ͹���ƫ�õ��ݶ�. ҲҪ����в�.
	!!!!!!!!!!!!!!!!�ݶȿ��Կ�������ʧJ���ڲ�����ĵ���, �в���Կ�������ʧJ���ڲ�����ĵ���!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
	 
    in_grad����ݶȲ���, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)��.
	out_grad����в����, ��������, ÿ��Ԫ�ص�������TBlob. ��һ��(��l + 1��)�Ĳв�, ���㱾��Ĳв�. 
	in_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)������.  
	out_data�������, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)�����.	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType.
	��Ϊ���򴫲���Ҫ�Ǽ����ݶȵ�, ���in_data���������. 
	*/
    
    using namespace mshadow;
    using namespace mshadow::expr;
    CHECK_EQ(out_grad.size(), 1);
    size_t expected = param_.no_bias ? 2 : 3;
    /*
	����expected, ��ȫ���Ӳ��û��ƫ��, ��expected��2, �����ƫ��, expected��3. ��expected������in_data��TBolb����ĸ���. 
	Ĭ��no_bias��false, ����ƫ����. 
	*/
	
	// CHECK����Զ��Ա��, ����֤������Ͻ���.
    CHECK(in_data.size() == expected && in_grad.size() == expected);
    // in_data[0], in_data[1], in_data[2]...
    CHECK_EQ(req.size(), expected);
    // ��������data, weight, bias�в�ͬ�����ݲ���ģʽ.  
    
    // TODO(bing): check the BLAS Handle, be careful
    //  maybe need blas handle from context
    Stream<xpu> *s = ctx.get_stream<xpu>();
    
        // std::cout<<"in_data kData: "<<fullc::kData<<std::endl; ����fullc::kData��0, ������data��in_data[0]. 
    const TShape& ishape = in_data[fullc::kData].shape_;
    
    // std::cout<<"out_data kOut: "<<fullc::kOut<<std::endl; ����fullc::kOut��0, ������data��out_data[0].
	/*
	��mxnet��, kData��0. ��������. 
	*/ 
    const TShape& oshape = out_grad[fullc::kOut].shape_;
    /*
	������������in_data[0]��shape������в�out_grad[0]��shape.
	
	TShape mxnet::TBlob::shape_, ������������Tensor��shape. TBlob��ĳ�Ա, ����ֵ������TShape. 
	*/
	
	// std::cout<<"in_data kData: "<<fullc::kData<<std::endl; ����fullc::kData��0, ������data��in_data[0].
	// std::cout<<"ishape: "<<ishape[0]<<" "<<ishape.ndim()<<std::endl; ishape[0]��64, 64��batch_size, ����ѵ�����Ĵ�С.  
	// ishape.ndim()��2, ��ishape��һ��2ά��. 
    Tensor<xpu, 2, DType> data = in_data[fullc::kData].get_with_shape<xpu, 2, DType>(
        Shape2(ishape[0], ishape.ProdShape(1, ishape.ndim())), s);
    /*
	��in_data[0](����(��l��)����������)����2ά������. ���ｫTBlob��������Tensor����ʱû��ʹ��FlatTo2D, ��������get_with_shape. 
	*/    
    
	// std::cout<<"in_data kWeight: "<<fullc::kWeight<<std::endl; fullc::kWeight��1, ��in_data��kWeight����1, Ȩ��.    
    Tensor<xpu, 2, DType> wmat = in_data[fullc::kWeight].get<xpu, 2, DType>(s);
    /*
	������(��l��)��Ȩ��in_data[kWeight]����2ά������. ��β�û��ʹ��get_with_shape, ����ʹ��get����.
	*/
    
    // std::cout<<"out_data kOut: "<<fullc::kOut<<std::endl; fullc::kOut��0, kData��kOut����0.
	// std::cout<<"oshape: "<<oshape[0]<<" "<<oshape.ndim()<<std::endl; oshape��ishapeһ��, 64 2. 
    Tensor<xpu, 2, DType> grad = out_grad[fullc::kOut].get_with_shape<xpu, 2, DType>(
        Shape2(oshape[0], oshape.ProdShape(1, oshape.ndim())), s);  
    /*
	��out_grad[0](�в�)����2ά������. ���ｫTBlob��������Tensor����ʱû��ʹ��FlatTo2D, ��������get_with_shape. 
	grad�ʹ�����һ��(��l + 1��Ĳв�). 
	*/  

#if defined(__CUDACC__)
    CHECK_EQ(s->blas_handle_ownership_, Stream<xpu>::OwnHandle)
        << "Must init CuBLAS handle in stream";
#endif

    //  backprop
    CHECK_NE(req[fullc::kWeight], kWriteInplace) << "cannot write weight inplace";
    /*
	#define CHECK_NE(val1, val2) CHECK_OP(_NE, !=, val1, val2)
	Ȩ���ݶȵ����ݲ���ģʽ������kWriteInplace. һ�������, ���е�out_data�����ݲ�������Ӧ����kWriteTo, �ڼ����ʾgradient
	��tensor��ʱ��, ��������ǽ��ݶ��ۼ�����, req������Ӧ����kAddTo, ��ʾӦ�õ���+=����.  
	*/
    
    // gradient of weight
    Tensor<xpu, 2, DType> gwmat = in_grad[fullc::kWeight].get<xpu, 2, DType>(s);
    Assign(gwmat, req[fullc::kWeight], dot(grad.T(), data));
    /*���㱾��(��l��)Ȩ�ص��ݶ�, �������ȫ���Ӳ�, ��û�м��������.
	in_grad�Ǳ���(��l��)���ݶ�TBlob, ��in_grad[1](��l�����Ȩ�ص��ݶ�)����2ά������. ��gwmat�����l�����ʧ J ����Ȩ�ص��ݶ�.
	Ȩ���Ǿ���.
	 
	��ֵ����, ���ݲ���ģʽ��req[fullc::kWeight], Ӧ����kAddTo, ��ʾӦ�õ���+=����. 
	
	����http://ufldl.stanford.edu/wiki/index.php/���򴫵��㷨 ������ʧ J ����Ȩ�غ�ƫ�õ������, ��ʧ���ڵ�l��Ȩ�ص��ݶ���:
	delta^(l + 1) * [a^(l)]'. 
	��һ��(��l + 1��)�Ĳв� * [����(��l��)���������]'. ���ʧ���ڵ�l����Ȩ�ص��ݶ���: grad.T() �� data �����. ����˷�. 
	*/
    
    // gradient of bias
    if (!param_.no_bias) {
      Tensor<xpu, 1, DType> gbias = in_grad[fullc::kBias].get<xpu, 1, DType>(s);
      Assign(gbias, req[fullc::kBias], sum_rows(grad));
    }
    /*���㱾��(��l��)ƫ�õ��ݶ�, �������ȫ���Ӳ�, ��û�м��������.
    in_grad�Ǳ���(��l��)���ݶ�TBlob, ��in_grad[2](��l�����Ȩ�ص�ƫ��)����1ά������. ��gbias�����l�����ʧ J ����ƫ�õ��ݶ�.
    ƫ��������. 
    
	�������(��l��)ȫ���Ӳ�ʹ��ƫ��, ����ڵ�l��ƫ�õ��ݶ���: delta^(l + 1).
	��һ��(��l + 1��)�Ĳв�. 
	
	��ֵ����, ���ݲ���ģʽ��req[fullc::kWeight]. ��ʧ���ڱ���(��l��)ƫ�õ��ݶ���sum_rows(grad). 
	*/
    
    // gradient of data
    Tensor<xpu, 2, DType> gdata = in_grad[fullc::kData].get_with_shape<xpu, 2, DType>(
        Shape2(ishape[0], ishape.ProdShape(1, ishape.ndim())), s);
    Assign(gdata, req[fullc::kData], dot(grad, wmat));
    /*���㱾��(��l��)data���ݶ�. ����ʧJ����FC��Ĳв�, ����в�������һ�ε�FC���ǰ�򴫲�����Ӱ��, ���ǻ�����gdata����FC
	��ǰһ��(��l - 1)��Ĳв�.
	 
	in_grad�Ǳ���(��l��)���ݶ�TBlob, ��in_grad[0](��l�����Ȩ�ص�data)����2ά������. ��gdata�����l�����ʧ J ����data���ݶ�.
    data�Ǿ���. 
    ֵ����, ���ݲ���ģʽ��req[fullc::kData]. ����(��l��)��ʧ����data���ݶ���: W' * delta^(l + 1). FC��û�м����! 
	�����Ȩ��wmat����һ��(��l + 1��)���ݶ������. ����Ӧ����������˷�. 
	*/
  }

 private:
  FullyConnectedParam param_;
};  // class FullyConnectedOp

// Decalre Factory function, used for dispatch specialization
template<typename xpu>
Operator* CreateOp(FullyConnectedParam param, int dtype,
                   std::vector<TShape> *in_shape,
                   std::vector<TShape> *out_shape,
                   Context ctx);

#if DMLC_USE_CXX11
class FullyConnectedProp : public OperatorProperty {
 public:
  std::vector<std::string> ListArguments() const override {
    if (!param_.no_bias) {
      return {"data", "weight", "bias"};
    } else {
      return {"data", "weight"};
    }
  }

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
    if (!param_.no_bias) {
      CHECK_EQ(in_shape->size(), 3) << "Input:[data, weight, bias]";
    } else {
      CHECK_EQ(in_shape->size(), 2) << "Input:[data, weight]";
    }
    CHECK_EQ(out_shape->size(), 1);
    TShape dshape = (*in_shape)[fullc::kData];
    TShape oshape = (*out_shape)[0];
    // require data to be known
    if (dshape.ndim() ==  0) return false;

    index_t num_input = dshape.ProdShape(1, dshape.ndim());
    SHAPE_ASSIGN_CHECK(*in_shape, fullc::kWeight, Shape2(param_.num_hidden, num_input));
    if (!param_.no_bias) {
      SHAPE_ASSIGN_CHECK(*in_shape, fullc::kBias, Shape1(param_.num_hidden));
    }

    SHAPE_ASSIGN_CHECK(*out_shape, 0, Shape2(dshape[0], param_.num_hidden));
    if (oshape.ndim() != 0) {
      dshape[0] = oshape[0];
      SHAPE_ASSIGN_CHECK(*in_shape, fullc::kData, dshape);
    }
    return true;
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
    FullyConnectedProp* fc_sym = new FullyConnectedProp();
    fc_sym->param_ = this->param_;
    return fc_sym;
  }

  std::string TypeString() const override {
    return "FullyConnected";
  }

  // decalre dependency and inplace optimization options
  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
    return {out_grad[fullc::kOut], in_data[fullc::kData], in_data[fullc::kWeight]};
  }

  std::vector<std::pair<int, void*> > BackwardInplaceOption(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data,
    const std::vector<void*> &in_grad) const override {
    return {{in_data[fullc::kData], in_grad[fullc::kData]}};
  }

  Operator* CreateOperator(Context ctx) const override {
    LOG(FATAL) << "Not Implemented.";
    return NULL;
  }

  Operator* CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                             std::vector<int> *in_type) const override;

 private:
  FullyConnectedParam param_;
};  // class FullyConnectedSymbol
#endif
}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_FULLY_CONNECTED_INL_H_
