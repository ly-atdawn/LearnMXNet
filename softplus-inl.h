/*!
 * Copyright (c) 2017 by Contributors
 * \file sofeplus-inl.h
 * \brief Sofeplus operator
 * \author
 * \ ��������activation-inl.h��д, ��Softplus���������һ��op, Ŀ������Ϥmxnet����C��Ӳ�Ĳ���. 
 * \ �����ֻ��һ��. ����ֻ�漰��operator��һ��, ����Ҫ��ÿһ��op�а�����.h�ļ����һ��, �����Ͷ�mxnet��op�������µ���ʶ��. 
 
 * \ 1>http://mxnet.io/doxygen/��mxnet�Ĺٷ��ĵ�, ���������mxnet����, �����ľ��嶨���ʵ��. ����վ��dlib���ĵ���վһ��. 
 * \ Doxygen��һ�ֿ�Դ��ƽ̨�ģ�������JavaDoc����������ĵ�ϵͳ.
 * \ 2>Operators in MXNet��ҳ, ��mxnet��op�����˼򵥵ؽ���, ���Բο�. 
 * \ 3>http://mxnet.io/api/python/index.html#table-of-contents ��python��API. 
     4>Sublime��ȫ����������, ������һ����/����, �����ؼ���. �õ���, ����, �����ȵĶ���. �����! 
	 5>mxnet��һ����, �ṹ��, ����������д����һ������. 
 * \
*/
#ifndef MXNET_OPERATOR_SOFTPLUS_INL_H_ // if not defined. 
#define MXNET_OPERATOR_SOFTPLUS_INL_H_

#include <dmlc/logging.h> // mxnet����־ͷ�ļ�. ��dmlc-core/include/dmlc/��, 
#include <dmlc/parameter.h> // mxnet�Ĳ���ͷ�ļ�, ��dmlc-core/include/dmlc/parameter.h��, ����һЩ������. 
#include <mxnet/operator.h> // ��include/mxnet��, �����������(operator), ����������, ������. ��OP��OpProp�ĺ�����������.  
// ����ʵ��, Ȼ��mxnet�ڶ���ÿһ��OPʱ, ����д��Щ����, ��ʵ��ĳһ��OP�Ĺ���. 
#include <cstring> // c�ַ���. 
#include <map> // ����ʽ����, Ԫ�ص�ֵ��ĳ���ض��ļ������, ������ͨ��Ԫ���������е�λ�����ȡ. ��:
/*
std:map<int, string> personnel;
�����Ͷ�����һ����intΪ��, ֵΪstring������mappersonnel.
*/ 
#include <string> // c++�ַ��� 
#include <vector> // ��������-����. 
#include <utility> // utilityͷ�ļ��������صĹ�ϵ�����, �򻯹�ϵ�������д��, ��������pair����,
// pair������һ��ģ������, ���Դ洢һ��ֵ.
#include "./operator_common.h" // src/operator��, mxnet�Ĳ�һЩ���õ�����.

namespace mxnet { // mxnet�����ռ�. 
namespace op { // op(����, ��)�����ռ�. 

namespace softplus { // ÿ����(����)�����Կ�����һ�������ռ�. 
enum SoftplusOpInputs {kData}; // Softplus������-kData. // ö������: enum ö���������� {����ֵ�б�/ö��Ԫ���б�}; //
enum SoftplusOpOutputs {kOut}; // Softplus�����-kOut. 
enum SoftplusOpType {kSoftplus}; // Softplus����������, ���Ｄ�������Softplus.
}  // activation 
/*
ö�����͵ı���ֵҲ���Կ���������. ������ʾ��Ҫ�ı���. 

��softplus-inl.h���ȼ���#include<iostream>, Ȼ���ٽ�kData, kOut, KBais���, �����Shape��һЩֵ. ��ǰ���
����Ĺ�����, kData, kOut, KBais��int�͵���. Ϊ0, 0, 2����. 

����ö�����͵ı�����ȷ����. enum SoftplusOpInputs {kData}; 
SoftplusOpInputs��ö��������, �����������ʵû��ʲôʵ�ʵ�����, ����Ϊ��˵��ö�ٱ���kData�ĺ�������. SoftplusOpInputs������
���ͻ������.  
����, ö��Ԫ�ذ��ճ�������, �����Ժ��ʹ���в��ܸı�ö��Ԫ�ص�ֵ; �ڶ����ʱ�����Ĭ��ֵ, ��0��ʼ����, 0, 1, 2, ...; 
�ڶ���ö��Ԫ�ص�ʱ����Զ�ö��Ԫ�ؽ��и���ֵ����. �� enum SoftplusOpInputs {kData = 2}; 
*/

struct SoftplusParam : public dmlc::Parameter<SoftplusParam> { // Softplus��Ĳ���˵��, �ṹ��, �̳�Parameter�ṹ��.
  // Parameter�ṹ����dmlc-core/include/dmlc/parameter.h��114, SoftplusParam�ǽṹ��Parameter��ģ�����. (ģ��������������ݵ�)
   
  // ��ΪSoftplus������һ�ּ����, Ϊ�˱�֤��activation-inl.hһ��, Ҳ��ö��act_type��ָ�����������. ��Ȼact_typeֻ��һ��
  // ����, ��ֵΪ0. 
  // ����˵������describe, set_default, set_range, add_num��. 
  int act_type;
  DMLC_DECLARE_PARAMETER(SoftplusParam) { // #define DMLC_DECLARE_PARAMETER(PType), ����������涨����һЩ��������OP�Ĳ�������
    // ˵��, ���õ�. ����describe, set_default, set_range, add_num��.  
    DMLC_DECLARE_FIELD(act_type) // DMLC_DECLARE_FIELD��. ����OP�Ĳ���, ����describe, set_default, set_range, add_num��.  
    .add_enum("softplus", softplus::kSoftplus) // ��һ�������ַ���, �ڶ���������OP����act_type��ֵ. 
    .describe("Softplus activation function to be applied."); // �ַ���. 
    /*
	DMLC_DECLARE_FIELD(act_type) ���ú�DMLC_DECLARE_FIELD��ȫ���Ӳ�Ĳ���act_type��������, ��Щ������Ĭ��ֵ����С, ���ֵ.  
	�������ò���ʱ, ���ԶԲ������и�ֵ. 
	*/
  }
};

/**
 * \brief This is the implementation of Softplus operator.
 * \tparam xpu The device that the op will be executed on.
 */
template<typename xpu, typename ForwardOp, typename BackwardOp, typename DType> // ģ����. 
/*
xpu��ʾʹ��cpu����gpu, ForwardOp��ʾǰ�����, BackwardOp��ʾ�������, DType��ʾ��������.

��make mxnet��ʱ��, ��Ļ�����config.mk����xpu��DType��ֵ. 
with [xpu = mshadow::op::cpu, DType = float] ���� [with xpu = mshadow::cpu; DType = double]. ��ͬ�Ļ�����DType���ܲ�һ��. 

��ʹ��ForwardOp��BackwardOpʱ, ���õ���./mshadow_op.h���Լ�����Ľṹ��. �������ϸ����. 
�����˱�֤��activation-inl.hһ��, Ҳ����typename ForwardOp, typename BackwardOp������ģ�����, ��ȻSoftplusǰ��ͷ������ֻ��
һ������. 
*/
class SoftplusOp : public Operator { // ����SoftplusOp��, �̳�Operator��. ����Softplus������ǰ������ͺ������. 
public:
  virtual void Forward(const OpContext &ctx,
                       const std::vector<TBlob> &in_data,
                       const std::vector<OpReqType> &req,
                       const std::vector<TBlob> &out_data,
                       const std::vector<TBlob> &aux_args) { 
    /*ǰ�����, �麯��. ������ʵ�������ж���. ����Ҫ����ֵ. 
	OpContext: �ṹ��, ������include/mxnet/operator.h��(Dev��һ���ô�����, �ڵ�ǰ�򿪵��ĵ���, ����ctrl+���������Կ��ٶ�λ
	����Ķ���, ĳЩ���ٵ����. ����Ŀ�������Sublime��ȫ����������), �ýṹ����Լ�¼������ǰ��ͺ��򴫲��е���Ϣ. 
    
	in_data����, ��������, ÿ��Ԫ�ص�������TBlob. Softplus�����������ֻ��data(�ϲ�����), û��Ȩ�غ�ƫ�õ�. ���in_data�Ĵ�
	СΪ1. ֻ��kData. 
	 
	TBlob: mxnet�ײ����ݸ�ʽ������theano�����Tensor��TBlob����ʽ, ʹ�ô����������. mxnet�������ݸ�ʽ��TBlob, 
	�����Tensor. tensor��TBlob�Ǹ�ά����������. ��������Tensor����TBlo����ϵ�Ͳ�ͬ(���). 
	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType. ������Ľ�������д�뵽out_data(�ڴ�), �е���ֱ��д��, �е����ۼӵ�.  
	OpReqType:  ���ݲ���ģʽ, kNullOp, kWriteTo, kWriteInplace, kAddTo. ��k��ͷ, ����Ӣ�ĵ���˼������ģʽ.
	enum OpReqType{....} // ��һ��ö������. include/mxnet/operator.h����. 0, 1, 2, 3. 
	
	out_data: ���, ��������, ÿ��Ԫ�ص�������TBlob. Sofyplus���������out, �����������. 
	aux_args: ��������, ÿ��Ԫ�ص�������TBlob. ����û�õ�.. ��Ϊ�˷���������Ҫ�ĸ��ӵ�tensor, ������û���õ���. 
	��Щ������ӵ�TensorĿǰֻ������Batch_Norm��ʹ��. 
	
	in_data�����ﲻ��������������, �������������治����kData, ���ܻ���kWeight, kBias�ȵ���Ϣ. ��ͬ���in_data��out_data
	�ǲ�һ����. in_data��������һ��������Ϊ���������, ���ܻ���һЩ����Ĳ�����Ϣ(Ȩ�غ�ƫ�õ�); ����out_data���屾������.
	in_data �� out_data �ֱ��������TBlob�����TBlob. ���е�tensor��Ҫ�Ŀռ䶼��ϵͳ��������͹���.  
	*/
    using namespace mshadow; // �����ռ�, mshadow�������ݴ洢�ṹ. 
    using namespace mshadow::expr; // ���ʽ��. OP�õ���һЩ���ʽ(����)���������expr�����ռ�.   
    CHECK_EQ(in_data.size(), 1); // �ж��������ݵĴ�С�Ƿ�Ϊ1, verctor��.size�������С. ����������, �������. 
    CHECK_EQ(out_data.size(), 1); // �ж�����Ĵ�С�Ƿ�Ϊ1, verctor��.size�������С. ����������, �������.
    /*
	����, ��:
	CHECK, CHECK_EQ, CHECK_LT, ��Щ���ܹ��򻯳����߼�, ��Debug��������.
	*/
    Stream<xpu> *s = ctx.get_stream<xpu>(); 
    /*
	include/mxnet/operator.h�µ�get_stream()����. �ṹ��OpContext��Աctx���ú���get_stream:
	template<typename xpu>
  	inline mshadow::Stream<xpu>* get_stream() const {...}. ��ȡָ��Device(cpu, gpu)�µ�stream. 
	����������, ��ָ��Device�¶���OP������. �������Ǵ洢��cpu�л���gpu��. 
	 
	mshadow::Stream<xpu> *stream.  ����ʵ��һ����Ϣ��ת��, ��һ����Ķ���, ����/�����(I/O Streams). xpu��ʾ��cpu����gpu. 
	*/
	
	// std::cout<<"in_data kData: "<<softplus::kData<<std::endl; kData��0. 
    Tensor<xpu, 2, DType> data = in_data[softplus::kData].FlatTo2D<xpu, DType>(s);
    /*
	Tensor����, �������Ƕ�ά������. ��, blob��caffe�еĻ������ݽṹ, ��������һ��"4ά����", 
	caffe��blob���൱��һ�������tensor. Tensor�������Ƕ�ά����.  Tesnor��һ��ģ��ṹ��:
	struct Tensor: public TRValue<Tensor<Device, dimension, DType>, Device, dimension, DType>
	device��ʾ�豸, ��cpu����gpu; dimension��������ά��, ά����2���������Ǿ���; Dtype������Ԫ�ص���������. ���ﶨ���data����
	Dtype���͵ľ���. 
	
	Tensor��ά���ڶ����͹̶���, �����ͼģ������Ҫһ����Ϊ�����������ݽṹ, �����TBlob.
	
	TBlob��mxnet��һ���������ݸ�ʽ, �����Tensor. TBlob���Ա�ʾ����ά��, ��������, �����豸�µ�����. TBlob��Tensorһ��, 
	ֻ��TBlob����������������, ���̶�ά��ʱTBlob����Tensor��. TBlob��һ����. 
	
	TBlob���漰�κ���������, Ҳû����ʽ���ڴ�������ͷ�, ������һ��ָ����, ����Ҫ��ʱ�����get, get_with_shape, FlatTo2D
	FlatTo3D�Ȼ�ù̶�ά����Tensor��������Ĳ���. Tshape��TBlob����, ����Ҫ��ʱ�����get, FlatTo2D�Ȼ��Tensor��Ӧ��Shape.
	  
    in_data��������, ��������, ÿ��Ԫ�ص�������TBlob. ����softplus::kData�õ���������, ��ôin_data[softplus::kData]�Ϳ��Կ���
	TBlob�Ķ���, ��˿��Ե���TBlob�µĺ���FlatTo2D: ./include/mxnet/tensor_blob.h��./mshadow/mshadow/tensor_blob.h 
    inline mshadow::Tensor<Device, 2, DType> FlatTo2D(
          mshadow::Stream<Device> *stream = NULL) const {...}
	��in_data[0]����2ά������, ��Tensor<xpu, 2, DType>. stream��Ŀ����. .
	FlatTo2D<xpu, DType>����ʹ�ú���FlatTo2Dʱָ��device��type. 
	
	Tensor<xpu, 2, DType>��FlatTo2D<xpu, DType>, ��ʹ�� ��, ����, ��ʱ��ָ��ģ�����:
	template<typename Device, int dimension, typename DType>��template<typename Device, typename DType>. ��, ��������ʱ��ģ��
	����������ָ����. 
	
	����ָ��������ģ��SoftplusOp(���м̳�Operator������), Ȼ��������:
    op = new SoftplusOp<cpu, mshadow_op::softplus, mshadow_op::softplus_grad, DType>();	// ʹ��ģ�����. 
	�������������SoftplusOp. ��ʵ����SoftplusOp��ʱ, mshadow_op::softplus, mshadow_op::softplus_grad��ָ����ǰ��ͷ���ľ���
	ʵ��.   
	*/
	
	// std::cout<<"out_data kOut: "<<softplus::kOut<<std::endl; kOut��0. 
    Tensor<xpu, 2, DType> out = out_data[softplus::kOut].FlatTo2D<xpu, DType>(s);
    /*
	��in_data�Ĳ�����һ����. ����FlatTo2D��out_data[0]����2ά������out. 
	����in_data��out_data������������, Ԫ����TBlob�Ķ���. ��softplus::kData��softplus::kOut���Կ���������, ��ָ�������е�����
	�ɷ�. ����Softplus�����out. 
	
	����ֻ�ǽ�in_data��out_data�е��������������2ά������, ��û���漰�������������. 
	typename ForwardOp��ǰ�����, ��Relu��������;, typename BackwardOp�Ƿ������, ��Relu��������ݶ�. ���Գ�����ForwardOp
	��BackwardOp���漰�������ļ���.  
	*/
	
	// std::cout<<"req kOut: "<<softplus::kOut<<std::endl; kOut��0. 
    Assign(out, req[softplus::kOut], F<ForwardOp>(data));
    /*
	��ֵ����. Softplus������data, �����out, ��out��ֵ. ������㲢�������κεĲ���. 
	Assign����������./operator_common.h 30����, �Ƕ����һ���꺯��. ��������exp��ֵ����out. 
	�ⲻ��C++���ַ�����ֵ����assign. �꺯��ʹ�ü����е�Ҳ��ȱ��:
	 
	#define Assign(out, req, exp)           
  	{                                     
	    switch (req) {                      
            case kNullOp:                     
               break;                          
            case kWriteTo:                    
            case kWriteInplace:               
                (out) = (exp);                  
                break;                          
            case kAddTo:                      
  		       (out) += (exp);                 
               break;                          
           default:                          
               LOG(FATAL) << "not reached";    
        }                                   
  	} 
   	req����������, Ԫ�ص�������OpReqType, ����kNullOp, kWriteTo(ֱ��д��), kWriteInplace(inplace write)
	kAddTo(��ʾӦ�õ���+=, ���罫�ݶ��ۼ�����, ������ֱ�Ӹ��ǵ�ԭ���Ľ��). 
	����һ�ָ�ֵ������, ��ASSIGN_DISPATCH�ڲ�Ҳ����4�����ͽ�����˵��. include/mxnet/operator_util.h.
	
	softplus::kOut���ǻ�ȡһ��ö������(�ҵ�OpReqType���͵��Ǹ�����), ��ôreq[softplus::kOut]��OpReqType�е�kWriteInplace��
	kAddTo, Ȼ��ͨ��exp��out��ֵ. һ�������, ���е�out_data������Ӧ����kWriteTo, ��ʾout_data ���tensor����ֱ��д���ԭʼ��
	�ڴ��. ����Щ�����, ����˵�ڼ���gradient��tensor, ��������ǽ��ݶ��ۼ�����, ������ֱ�Ӹ��ǵ�ԭ���Ľ��, �������ǾͲ���Ҫ
	ÿ�μ����ʱ�����������Ҫ���ڴ�ռ�. �����������, req������Ӧ����kAddTo.
	
	F<ForwardOp>(data)������exp, �Ǳ��ʽ��ֵ. ForwardOp��mshadow_op::softplus, �����ü����Softplusִ�в���:
	- `softplus`: `y = log(1 + exp(x))`.
	
	mshadow���ڱ��ʽ��������(DotExp, BinaryMapExp, UnaryMapExp). 
	
	BinaryMapExp(����ͼ)��˫Ŀ����ı��ʽ��, ��BinaryMapExp������F����, 
	F���Զ�������ĺ���, F< OP >(lhs, rhs)������һ���µ�˫Ŀ����(OP��ForwardOp��BackwardOp). 
	
	DotExp������˵���, ������õľ���dot����.
	
	UnaryMapExp��(һԪ)�ǵ�Ŀ������ı��ʽ��, ��UnaryMapExp������F����:
    template<typename OP, typename TA, typename DType, int ta>
	inline UnaryMapExp<OP, TA, DType, (ta|type::kMapper)>
	    F(const Exp<TA, DType, ta> &src) {
  	    return MakeExp<OP>(src);
	} 
	
	����F<ForwardOp>(data)�ǵ�Ŀ����, �������ForwardOp, ������data, ��Softplusǰ��󴫲�����mshadow_op::softplus.  
	mshadow_op::softplus�Ƕ�����mshadow_op.h�µĽṹ��: ����a, ����log(1 + exp(a)). 

	�ٽ�F<ForwardOp>(data)��ĳ�����ݲ���ģʽд�뷽ʽ��ֵ��out.  
	
	// ��������������, ��python��, = ��ǳ����, ����x = y, ��ôy�ı�ʱ, xҲ���Ÿı�.  
	*/
  }

  virtual void Backward(const OpContext &ctx,
                        const std::vector<TBlob> &out_grad,
                        const std::vector<TBlob> &in_data,
                        const std::vector<TBlob> &out_data,
                        const std::vector<OpReqType> &req,
                        const std::vector<TBlob> &in_grad,
                        const std::vector<TBlob> &aux_args) {
    /*�������, �麯��. ������ʵ�������ж���. ����Ҫ����ֵ. 
	OpContext: �ṹ��. ֻ��ǰ��ͷ�����ʹ��.
	
	�������û��Ȩ��W��ƫ��b, ���Ҳ��û����ʧL����W��b���ݶ�. ���������ֻ����в�, ���㼤�����Ĳв��ʱ����Ҫ�õ���
	��Ĳв�, sigma^(l) �� sigma^(l + 1). 
	�ڲ�ͬ��OP(��)��, in_grad��out_grad����ĺ����ǲ�ͬ��. ��û�в����Ĳ��ǲв�(��ʧJ��������Z��ƫ��), �в����Ĳ����ݶ�
	(��ʧ����Ȩ��W��ƫ��b��ƫ��). ������������, �Ժ�������������˵��һ��. 
	!!!!!!!!!!!!!!!!�ݶȿ��Կ�������ʧJ���ڲ�����ĵ���, �в���Կ�������ʧJ���ڲ�����ĵ���!!!!!!!!!!!!!!!!!!!!!!!!!!!! 
	  
	in_grad����в�, ��������, ÿ��Ԫ�ص�������TBlob. ����(��l��)�Ĳв�.
	out_grad����в�, ��������, ÿ��Ԫ�ص�������TBlob. ��һ��(��l + 1��)�Ĳв�, ���㱾��Ĳв�. 
	in_data����, ��������, ÿ��Ԫ�ص�������TBlob. Softplus�������. 
	out_data���, ��������, ÿ��Ԫ�ص�������TBlob. (��l��)�����.	
	req: ���ݲ���ģʽ, ��������. Ԫ��������OpReqType. 
	*/
    using namespace mshadow; // �����ռ�, mshadow�������ݴ洢�ṹ. 
    using namespace mshadow::expr; // ���ʽ��. 
    CHECK_EQ(out_grad.size(), 1); // ��һ��(��l + 1��)�Ĳв�������С�Ƿ�Ϊ1. ��out_gradֻ�е�l + 1��Ĳв�. 
    CHECK(in_data.size() == 1 && in_grad.size() == 1); // �������ݺ�����ݶȴ�С�Ƿ�Ϊ1. 
    CHECK_EQ(req.size(), 1); // ���ݲ���ģʽ�Ƿ�һ��. 
    Stream<xpu> *s = ctx.get_stream<xpu>(); // device��. 
    
    /*��������ǰ��ͷ��򴫲�:
	ǰ�򴫲��Ĺ��̾���: out = f(in). �ٸ���f����ʽ(ForwardOp)����out����.
	(����mxnet FC��ķ��򴫲��ͷ��򴫲��㷨���ݶȼ��㹫ʽ��һ�µ�, ϸ�ڵĵط�����Ҫϸ������).
	���ڼ����ķ��򴫲�, Ҫ������в�, �в������ʧJ���ڱ�������Z��ƫ��:
	in_grad = partial(J) / partial(Z^(l)) =(��ʽ����)partial(J) / partial(Z^(l + 1)) * partial(Z^(l + 1)) / partial(Z^(l))
	����, partial(J) / partial(Z^(l + 1))��Ϊ��һ��Ĳв�, ��out_grad[0]. 
	Z^(l + 1) = f(Z^(l))(��������������Z^(l), ����� f(Z^(l)), f(Z^(l))��Ϊ��һ�������), 
	���partial(Z^(l + 1)) / partial(Z^(l)) = f'(Z^(l)). �����������õ���BackwardOp, ����sigmoid_grad:
		
    f'(x) = f(x) * (1 - f(x)), ���f'(Z^(l)) =  f(Z^(l)) * (1 - f(Z^(l)) = Z^(l + 1) * (1 - Z^(l + 1)).
	��l + 1������� * (1 - ��l + 1�������). ��l + 1������뼴����(��l��)�����, ��out_data[0]. 
	���,  f'(Z^(l)) = a^(l) * (1 - a^(l)). 
    
    ������Softplus������˵, f(x) = ln(1 + exp(x)), ���f'(Z^(l)) = [exp(Z^(l))] / [1 + exp(Z^(l))], ��
	Z^(l + 1) = ln(1 + exp(Z^(l))), ���, exp(Z^(l + 1)) = 1 + exp(Z^(l)). ��f'(Z^(l)) = 1 - exp( - Z^(l + 1)).
	
    �ڼ������ķ��򴫲���Ҫ�������f'(Z^(l)), Ȼ�󻯳ɹ���Z^(l + 1)��ʽ��. �����Ǽ���f'(x)������, ����Ҫ��x��ΪZ^(l), ��
	����ɹ���Z^(l + 1)��ʽ��. ��ΪBackwardOp��������. 
	BackwardOp������Softplus����� Z^(l), Ȼ����� f'(Z^(l)) = 1 - exp( - Z^(l + 1)).  
	����f'(Z^(l)), �Ϳ��Լ���Softplus��ʧ���ڱ�������Ĳв���! 
	
	�����������(��l��)�Ĳв����:
    F<BackwardOp>(m_out_data) * m_out_grad. m_out_grad��һ��(l + 1)���ݶ�, m_out_data��������. 
	*/
	
	// std::cout<<"out_grad kOut: "<<softplus::kOut<<std::endl; kOut��0. 
	Tensor<xpu, 2, DType> m_out_grad = out_grad[softplus::kOut].FlatTo2D<xpu, DType>(s);
    /*
	����FlatTo2D, ���ϲ�(l + 1)�Ĳв�out_grad[0]����2ά������, ��ά���̶������Tensor��. ����m_out_grad. 
	*/
	
	// std::cout<<"in_grad kData: "<<softplus::kOut<<std::endl;; kOut��0. 
    Tensor<xpu, 2, DType> m_out_data = out_data[softplus::kOut].FlatTo2D<xpu, DType>(s);
    /*
	����FlatTo2D, ������(l��)�����a^(l)��out_data[0]����2ά������, ��ά���̶������Tensor��. ����m_out_data.  
	*/
	
	// std::cout<<"in_grad kData: "<<softplus::kOut<<std::endl; kData��0. 
    Tensor<xpu, 2, DType> m_in_grad = in_grad[softplus::kData].FlatTo2D<xpu, DType>(s);
    /*
	���屾��(��l��)�Ĳв�, ����Ĳв�m_in_gradҲ��2ά������. 
	
	ǰ�򴫲��е�out_data�ͷ��򴫲��е�in_grad, �����������, �������Softplus����Ͳв���ڴ�ռ�, Ȼ���ٽ�����д���ڴ漴��. 
	*/
	
	// std::cout<<"req kData: "<<softplus::kData<<std::endl; kData��0. 
    Assign(m_in_grad, req[softplus::kData], F<BackwardOp>(m_out_data) * m_out_grad);
    /*
	�������������ļ������ķ��򴫲��Ƶ�, �������ķ��򴫲����㱾��(��l��)�Ĳв�:
    m_in_grad = F<BackwardOp>(m_out_data) * m_out_grad. 
	
	�þ��Ǹ�ֵ���, ͨ��req[softplus::kData](ĳһ�����ݲ���ģʽ, kWriteTo)��F<BackwardOp>(m_out_data) * m_out_gradд��
	m_in_grad.
	
	F<BackwardOp>(m_out_data)���÷��򴫲�ģ��BackwardOp�ͱ���(��l��)�����, ����ƫ����. �����Sigmoid������˵, 
	F<BackwardOp>(m_out_data)����: m_out_data * (1 - m_out_data).
	����Softplus, F<BackwardOp>(m_out_data)���� 1 - (expf( - m_out_data)). �� f'(Z^(l)). 
	
	������������ǰ��ͷ��򴫲�, ֻ����mshadow_op.h�����ǰ�򴫲�ForwardOp�ͷ��򴫲�BackwardOp����. 
	
	����mxne����������˵, ǰ��ͷ��򴫲����ܾ�ֻ��һ�����, ���ǰ��ͷ���������ò���ǰ�򴫲�ForwardOp�ͷ��򴫲�BackwardOp��. 
	*/
  }
};  // class SoftplusOp 


// Decalre Factory function, used for dispatch specialization
template<typename xpu> // mxnet��device�������ģ�����xpu. 
Operator* CreateOp(SoftplusParam type, int dtype);
/*��Operator��include/mxnet/operator.h��, �����������(operator)��.
����OP����, ��softplus.cc���õ�. 
��op = new SoftplusOp<cpu, mshadow_op::softplus, mshadow_op::softplus_grad, DType>(); ������ϵ��, �����ú���CreateOp������OP��,
op����ʾ��mxnet��layer. 

����op��������-inl.h������, ���嶨����.cc��. CreateOp����OP, ����SoftplusParam�Ĳ�������type��dtype. ��Softplus.cc�л����.  
*/

#if DMLC_USE_CXX11 // C++11. �����Ǽ������Softplus������, ÿ��OP�����Կ��ܾ���һ��, ��Щ����Ҳ�ǿ�ѡ��. 
// һ��Ҳ����Ҫinfer_shape, ��OP����������������������. 
class SoftplusProp : public OperatorProperty { // ��SoftplusProp, �̳�OperatorProperty(��������), ��include/mxnet/operator.h��
/*��SoftplusProp���еķ�������д�˸���OperatorProperty�ķ���. 

override��һ���ؼ���, ��overload����. ������ʹ�ùؼ���override��overload�Ǳ����ú�������д��������.
����(overload)ĳ����������ͬһ�����з�����, ��д(override)������������д�����еķ���. 

����OperatorProperty�ڶ���ʱ, �����˺ܶ�OP���Եĺ���, ���嵽ĳһ��OPʱ, ��Щ���Ժ�����������, ���Ҫ�Ը������Щ���Ժ�������
��д. ����OperatorProperty����ķ��������Լ���ʵ������, �����ڶ�����opʱ����д��Щ����Ҳ������. 
*/
 public:
  void Init(const std::vector<std::pair<std::string, std::string> >& kwargs) override {
    param_.Init(kwargs);
  }
  /*��д����OperatorProperty��Init����
  Init���������е�OP�о����õ�, ͨ�����ò���kwargs����ʼ��OP. param_��SoftplusParam�����˽�г�Ա, ������Init����.
  
  Parameter�ṹ����dmlc-core/include/dmlc/parameter.h��114, ���ڽṹ��SoftplusParam�̳���Parameter�ṹ��, ���SoftplusParam
  �ĳ�Աparam_���Ե���Parameter�ṹ��ĺ��� Init:
  template<typename Container>
  inline void Init(const Container &kwargs, parameter::ParamInitOption option = parameter::kAllowHidden) {...}
  ��������kwargs��ʼ��������, ����ʼ���ṹ��SoftplusParam�Ĳ���. 
  param kwargs map of keyword arguments, or vector of pairs. 
  
  pair��һ��ģ������, ���а�����������, ��pair<string, string> a("James", "Joy");
  kwargs����������, ÿһ��Ԫ�ؾ�Ϊһ��ģ������, ���Ա���һ��ֵ.   
  */

  std::map<std::string, std::string> GetParams() const override {
    return param_.__DICT__();
  } 
  /*�̶���ʽ. ��ȡ����, ��д����OperatorProperty��GetParams����.
  
  ������ȡOP���ڲ�����, ��act_type��. �����������map������. 
  param_��SoftplusParam�Ķ���, ���ڽṹ��SoftplusParam�̳���Parameter�ṹ��, ���SoftplusParam�ĳ�Աparam_���Ե���
  Parameter�ṹ��ĺ��� __DICT__:
  inline std::map<std::string, std::string> __DICT__() const {...}. ��OP�Ĳ����ŵ���һ��dict��, ��key -> value. ��һ��ֵ. 
  */ 
  
  /*���⸸��OperatorProperty�л��к���ListArguments��ListOutputs, ����ָ��op������������������. ��ͬ��op, ListArguments��
  ListOutputsҲ�����������.  
  */

  bool InferShape(std::vector<TShape> *in_shape,
                  std::vector<TShape> *out_shape,
                  std::vector<TShape> *aux_shape) const override {
    /*��д����OperatorProperty��InferShape(shape�ƶ�)����.
  	����ӿ�������Ŀ��: (1)��ϵͳ�ṩÿ������Tensor�����Tensor�Ĵ�С(��״), ����ϵͳ�����ڽ���Forward��Backward֮ǰ�������Ӧ
  	���ڴ�. ���������ᵽ�� out_data���� �� in_grad����, �����ṩ��Tensor�Ĵ�С, �����������ڴ�, Ȼ��������ֵ����; 
	(2)�������ͼ��, ������ǰȷ��û�����ԵĴ���. in_shape�е�shape��ϵͳ�Զ�����(�����ϸ�Operator��out_shape ). 
  	���ϵͳ��Ϊ�ṩ����Ϣ���������shape���ƶϻ᷵��false, ������shape��һ�µ�ʱ���׳��쳣.
  	*/
	/*
	TShape: һ��shape��, ������Ա�ʾһ��Tensor����״. ����TShape����ʾTensor�Ĵ�С. 2ά����������, ������Ĵ�С.
	
	in_shape��out_shape��ʾ��������Tensor��shape, ��������(ָ�����͵���������), Ԫ�ص�������TShape�Ķ���:
	����: in_shape[0]����[64, 100]. out_shape����ͨ��in_shape������.
	
	aux_shape�����ĵ�aux_argsһ��, ���븽��һ��tensor, ��δ�õ�. 
	*/ 
    using namespace mshadow; // �����ռ�. ���¶����˺ܶ���ͷ���. 
    CHECK_EQ(in_shape->size(), 1) << "Input:[data]"; // ���in_shape�����Ĵ�С�Ƿ�Ϊ1, �����������. 
    
    // std::cout<<"in_shape kData: "<<softplus::kData<<std::endl; kData��0. 
    const TShape &dshape = in_shape->at(softplus::kData);
    // std::cout<<"dshape: "<<dshape<<std::endl; dshape��(64,128), 64��batch_size����ѵ����Ŀ�Ĵ�С, 128��ǰһ����������ݵ�
	// ����, ��ʵ����Softplusǰһ����FC��, FC����128�����, �������Ҳ��128. 
    
    /*
	����dshape, ��TShape����. ͨ������tensor����״��dshape���и�ֵ, ����in_shape����������, ������������������at����.
	at()����, ��������vectorָ��λ��loc��Ԫ��. ��in_shape[i]����ͬ������. 
	at()������[]��������Ӱ�ȫ, ��Ϊ����������ȥ���ʵ�Vector��Խ���Ԫ��. 
	
	in_shape->at(activation::kData)���Ƿ���data_shape, ����ö������activation::kData����data_shape, ��in_shape[0].
	
	mxnet��, vector��������������������������->��ɵ�(in_shape->at( )), ƽʱ�õ��� . ��in_shape.at() . 
	*/
    
    if (dshape.ndim() == 0) return false; // ndim��ʾά��, ��������Tensor��ά��, ����2. 
    out_shape->clear(); // ���out_shape, �Դ����µ�shape. 
    out_shape->push_back(dshape); // ���tensor��ά������0, ��ô�ͽ�data_shape����out_shape��, ������������һ����. 
	/*
	��Ϊ�ڶ���������ʱ�����ָ�����͵�����, ������ʹ�������ĳ�Ա����ʱ, ��������->, ������ .  
	*/ 
    return true;
    /*
	���shape�ƶϳɹ�, ����true; ���û���㹻����Ϣ������shape�ƶ�, �򷵻�false; �������������һ��, �׳��쳣. 
	*/
  }

  bool InferType(std::vector<int> *in_type,
                 std::vector<int> *out_type,
                 std::vector<int> *aux_type) const override {
    /*��д����OperatorProperty��InferType(�����ƶ�)����. 
  	�ƶ�������������ͺ�δ֪���������. 
  
   	in_type: int�͵���������. �������������.
  	out_type: ���������. (ָ�����͵���������) 
  	aux_type: ����״̬������, ��δ�õ�. 
  	*/				 				  
				 				  
    CHECK_GE(in_type->size(), 1); // in_type�����Ĵ�С�Ƿ���ڵ���1. 
	/*
	1.��־�����:
    LOG(WARNING) << "This is a warning message";
    2.CHECK_XX��:
    1 #define CHECK_EQ(val1, val2) CHECK_OP(_EQ, ==, val1, val2)
	2 #define CHECK_NE(val1, val2) CHECK_OP(_NE, !=, val1, val2)
	3 #define CHECK_LE(val1, val2) CHECK_OP(_LE, <=, val1, val2)
	4 #define CHECK_LT(val1, val2) CHECK_OP(_LT, < , val1, val2)
	5 #define CHECK_GE(val1, val2) CHECK_OP(_GE, >=, val1, val2)
	6 #define CHECK_GT(val1, val2) CHECK_OP(_GT, > , val1, val2) 
	*/ 
    int dtype = (*in_type)[0]; // ��ʼ��dtype, ����ʹ��int�͵ı�������ʾ��. (*in_type)[0]��int�͵ı���, *in_type��һ��ָ���͵�
	// ����, ���(*in_type)[0]�ͱ�ʾ���ָ���������ĵ�һ������. ��һ��int�͵ı���. 
	// std::cout<<"dtype: "<<dtype<<std::endl; dtype��0. 
	
    CHECK_NE(dtype, -1) << "First input must have specified type"; // �ж�dtype�Ƿ��-1���, ������, �����Ϣ. 
    
    for (index_t i = 0; i < in_type->size(); ++i) { // index_t��һ����������, ͨ�����޷��ŵ�.  
      /*
      index_t�Ķ�����mshadow/mshadow/base.h��.
	  typedef unsigned index_t; 
	  unsigned a; ��unsigned int a; ��ͬ����. ����ʡ����int, ֻ�ܺ�unsigned int�ȼ�.
	  
	  ָ������������һЩ��Ա����ʱ�� in_type->, ��������Ԫ��ʱ�� in_type[i]. i ��Ҫ���� in_type ������ÿһ��Ԫ��. 
	  */
      if ((*in_type)[i] == -1) { // �ж�(*in_type)[i]�Ƿ�Ϊ-1. 
          (*in_type)[i] = dtype; // (*in_type)[i]Ϊ-1, ��(*in_type)[i]Ϊdtype. 
      } else {
        CHECK_EQ((*in_type)[i], dtype) << "This layer requires uniform type. "
                                       << "Expected " << dtype << " v.s. given "
                                       << (*in_type)[i] << " at " << ListArguments()[i];
        // �ж�(*in_type)[i]�Ƿ��dtype���. �������������Ϣ. ListArguments()�����в�����. 
      }
    }
    out_type->clear();
    out_type->push_back(dtype); // ��dtype��(*in_type)[0]��ֵ��out_type, �����������������Ҳ��һ����. 
    return true;
  }

  OperatorProperty* Copy() const override {
    /*��д����OperatorProperty��Copy����. �ú�����copySoftplusProp(����������)�Ĳ���. 
    */
    auto ptr = new SoftplusProp(); 
    /*
	�Զ��ƶ�����auto. ����SoftplusProp�Ķ���, ������SoftplusProp��Ĺ��캯��(Ĭ�Ϲ��캯��)��ʼ������ptr.
	���� ������ =new ����(); // ���� ��/�ṹ�� ���� + ��ʼ��һ������. 
	*/
    ptr->param_ = param_; // SoftplusProp��Ķ���ptr����.... 
    return ptr;
  }

  std::string TypeString() const override {
    return "Softplus";
  }
  /*��д����OperatorProperty��TypeString����. ָ����op������. 
  */

  // decalre dependency and inplace optimization options
  std::vector<int> DeclareBackwardDependency(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data) const override {
#if MXNET_USE_CUDNN == 1
    return {out_grad[softplus::kOut], out_data[softplus::kOut], in_data[softplus::kData]};
#else
    return {out_grad[softplus::kOut], out_data[softplus::kOut]}; // �����б�, �����Ƿ�ʹ��CUDNN(GPU)���ز�ͬ���б�. 
#endif  // MXNET_USE_CUDNN
  }
  /*��д����OperatorProperty��DeclareBackwardDependency����. �ú����ڷ��򴫲�ʱ����input requirement.
  �����ڷ��򴫲�ʱ�õ����б�, ���������Ҫ�������Ż��ڴ��. ��ʱ��tensor�������򴫲�ʱ����Ҫ��, ��Ҫ�ͷ�, ������������ջ���.
  �ڶ����µ�opʱ, �ú�����Ҫ��д, ��ָ���ڷ�������е�����Ҫ��Щ����. �������д, Ĭ�ϵ�DeclareBackwardDependency��������е�
  ����.
  
  out_grad: �ڷ��򴫲���������ݶ�(�в�). ��һ��(��l + 1��)���ݶ�/�в�. 
  in_data: ǰ������е�Softplus�����������.
  out_data: ǰ������е�Softplus����������. 
  
  */

  std::vector<std::pair<int, void*> > BackwardInplaceOption(
    const std::vector<int> &out_grad,
    const std::vector<int> &in_data,
    const std::vector<int> &out_data,
    const std::vector<void*> &in_grad) const override {
    return {{out_grad[softplus::kOut], in_grad[softplus::kData]}};
  }
  /*��д����OperatorProperty��BackwardInplaceOption����. Ϊ�˽�һ���Ľ�ʡ�ڴ�����뿪��, ��������������ԭ�ظ���(inplace update). .
  �����Ҫ����element-wise������, ��Ϊ�������������tensor�����tensor��shape��һ�µ�. 
  
  out_grad[0]��in_grad[0]����ͬ�����ڴ�ռ���Backward���������. 
  
  out_grad: �ڷ��򴫲���������ݶ�(�в�).
  in_data: ǰ������е�����.
  out_data: ǰ������е����. 
  in_grad: �����е������ݶ�(�в�). 
  
  pairģ����, һ��ֵ. 
  
  */

  std::vector<std::pair<int, void*> > ForwardInplaceOption(
    const std::vector<int> &in_data,
    const std::vector<void*> &out_data) const override {
    return {{in_data[softplus::kData], out_data[softplus::kOut]}};
  }
  /*��д����OperatorProperty��ForwardInplaceOption����. �ú�����BackwardInplaceOption��������һ����.
  
  in_data[0]��out_data[0]��tensorsӦ����Forward�ļ��������ʹ��ͬ�����ڴ�ռ�.
  
  in_data: ǰ������е�����.
  out_data: ǰ������е����.  
  */

  Operator* CreateOperator(Context ctx) const override {
    LOG(FATAL) << "Not Implemented."; // ��־�ļ����� FATAL(������). 
    return NULL;
  }
  /*��д����OperatorProperty��CreateOperator����.�̶���ʽ. Create a Operator on specific context.
  */

  Operator* CreateOperatorEx(Context ctx, std::vector<TShape> *in_shape,
                             std::vector<int> *in_type) const override;
  /*��д����OperatorProperty��CreateOperatorEx����.�̶���ʽ. 
  Create a Operator on specific context and input shape/type.
  
  -inl.h��ֻ�Ƕ���д��������������, �ú�����ʵ����.cc��. .cc�ļ�include-inl.h�ļ�. 
  */
  
  /*
  �еĲ���ܻ���Ҫ��дForwardResource��BackwardResource����:
  ��Щ������Ҫ������ڴ���Ϊ�����ռ������м���, ����˵cudnnConvolutionForward. ���������, ϵͳ��ÿ��Զ��ⲿ���ڴ���й���, 
  ����ϵͳ������һЩ�Ż�, ����˵�ڴ���ظ�����. MXNet�����������ӿ����ﵽĿ��: ForwardResource��BackwardResource����.
  */

 private:
  SoftplusParam param_;
};
#endif  // DMLC_USE_CXX11
}  // namespace op
}  // namespace mxnet
#endif  // MXNET_OPERATOR_SOFTPLUS_INL_H_
