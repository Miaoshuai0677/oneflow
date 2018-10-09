#include "oneflow/core/register/blob.h"
#include "oneflow/core/job/job_desc.h"
#include "oneflow/core/kernel/kernel_util.h"
#include "oneflow/core/job/runtime_context.h"
#include "oneflow/core/common/preprocessor.h"

namespace oneflow {

Blob::Blob(Regst* regst, const RtBlobDesc* blob_desc, char* header_ptr)
    : header_pod_ptr_(blob_desc->header_pod_desc(), header_ptr) {
  Init(regst, blob_desc, header_ptr, header_ptr + blob_desc->ByteSizeOfBlobHeader());
}

Blob::Blob(Regst* regst, const RtBlobDesc* blob_desc, char* header_ptr, char* body_ptr)
    : header_pod_ptr_(blob_desc->header_pod_desc(), header_ptr) {
  Init(regst, blob_desc, header_ptr, body_ptr);
}

void Blob::Init(Regst* regst, const RtBlobDesc* blob_desc, char* header_ptr, char* body_ptr) {
  is_contiguous_ = (body_ptr == header_ptr + blob_desc->ByteSizeOfBlobHeader());
  regst_ = regst;
  blob_desc_ = blob_desc;
  header_ptr_ = header_ptr;
  data_id_ptr_ = header_pod_ptr_.MutTensorPtr<char>(FieldKey::kDataId, nullptr);
  col_num_ptr_ = header_pod_ptr_.MutTensorPtr<int32_t>(FieldKey::kColNum, nullptr);
  dim0_valid_num_ptr_ = header_pod_ptr_.MutTensorPtr<int32_t>(FieldKey::kDim0ValidNum, nullptr);
  dim1_valid_num_ptr_ = header_pod_ptr_.MutTensorPtr<int32_t>(FieldKey::kDim1ValidNum, nullptr);
  dptr_ = body_ptr;
  dynamic_shape_ = blob_desc->shape();
}

const char* Blob::data_id(int32_t no) const {
  CHECK_NOTNULL(data_id_ptr_);
  return data_id_ptr_ + no * Global<JobDesc>::Get()->SizeOfOneDataId();
}

int32_t Blob::col_num(int32_t no) const {
  if (col_num_ptr_ == nullptr) {
    return 1;
  } else {
    return *(col_num_ptr_ + no);
  }
}

void Blob::set_col_num(int32_t no, int32_t val) {
  CHECK_NOTNULL(col_num_ptr_);
  *(col_num_ptr_ + no) = val;
}

int32_t Blob::dim1_valid_num(int32_t no) const {
  CHECK_NOTNULL(dim1_valid_num_ptr_);
  CHECK_GE(no, 0);
  CHECK_LT(no, blob_desc_->shape().At(0));
  return dim1_valid_num_ptr_[no];
}

void Blob::set_dim1_valid_num(int32_t no, int32_t val) {
  CHECK_NOTNULL(dim1_valid_num_ptr_);
  CHECK_GE(no, 0);
  CHECK_LT(no, blob_desc_->shape().At(0));
  CHECK_GE(val, 0);
  CHECK_LE(val, blob_desc_->shape().At(1));
  dim1_valid_num_ptr_[no] = val;
}

int32_t Blob::dim0_valid_num(int32_t no) const {
  CHECK_NOTNULL(dim0_valid_num_ptr_);
  CHECK_GE(no, 0);
  CHECK_LT(no, dim0_inner_shape().At(0));
  return dim0_valid_num_ptr_[no];
}

void Blob::set_dim0_valid_num(int32_t no, int32_t val) {
  CHECK_NOTNULL(dim0_valid_num_ptr_);
  CHECK_GE(no, 0);
  CHECK_LT(no, dim0_inner_shape().At(0));
  CHECK_GE(val, 0);
  CHECK_LE(val, dim0_inner_shape().Count(1));
  dim0_valid_num_ptr_[no] = val;
}

const Shape& Blob::shape() const {
  if (dim0_valid_num_ptr_ == nullptr) { return static_shape(); }
  return dynamic_shape();
}

const Shape& Blob::dynamic_shape() const {
  size_t last_invalid_instance_num =
      dim0_inner_shape().Count(1) - dim0_valid_num(dim0_inner_shape().At(0) - 1);
  size_t contiguous_instance_num = blob_desc_->shape().At(0) - last_invalid_instance_num;
  if (dynamic_shape_.At(0) != contiguous_instance_num) {
    dynamic_shape_.Set(0, contiguous_instance_num);
  }
  return dynamic_shape_;
}

int32_t Blob::col_id() const { return regst_->col_id(); }
void Blob::set_col_id(int32_t val) { regst_->set_col_id(val); }
int32_t Blob::max_col_id() const { return regst_->max_col_id(); }
void Blob::set_max_col_id(int32_t val) { regst_->set_max_col_id(val); }
const MemoryCase& Blob::mem_case() const { return regst_->regst_desc()->mem_case(); }

void Blob::CopyDataContentFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs) { return; }
  AutoMemcpy(device_ctx, mut_dptr(), rhs->dptr(), ByteSizeOfDataContentField(), mem_case(),
             rhs->mem_case());
}

void Blob::CopyHeaderFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs || ByteSizeOfBlobHeader() == 0) { return; }
  CHECK_EQ(ByteSizeOfBlobHeader(), rhs->ByteSizeOfBlobHeader());
  Memcpy<DeviceType::kCPU>(device_ctx, mut_header_ptr(), rhs->header_ptr(), ByteSizeOfBlobHeader());
}

void Blob::CopyDataIdFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs || ByteSizeOfDataIdField() == 0) { return; }
  CHECK_EQ(ByteSizeOfDataIdField(), rhs->ByteSizeOfDataIdField());
  Memcpy<DeviceType::kCPU>(device_ctx, mut_data_id(), rhs->data_id(), ByteSizeOfDataIdField());
}

void Blob::CopyColNumFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs || ByteSizeOfColNumField() == 0) { return; }
  CHECK_EQ(ByteSizeOfColNumField(), rhs->ByteSizeOfColNumField());
  Memcpy<DeviceType::kCPU>(device_ctx, mut_col_num(), rhs->col_num(), ByteSizeOfColNumField());
}

size_t Blob::ByteSizeOfDim0ValidNumField() const {
  return blob_desc_->ByteSizeOfDim0ValidNumField();
}

size_t Blob::ByteSizeOfDim1ValidNumField() const {
  return blob_desc_->ByteSizeOfDim1ValidNumField();
}

void Blob::CopyDim0ValidNumFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs || ByteSizeOfDim0ValidNumField() == 0) { return; }
  CHECK_EQ(ByteSizeOfDim0ValidNumField(), rhs->ByteSizeOfDim0ValidNumField());
  Memcpy<DeviceType::kCPU>(device_ctx, mut_dim0_valid_num(), rhs->dim0_valid_num(),
                           ByteSizeOfDim0ValidNumField());
}

void Blob::CopyDim1ValidNumFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs || ByteSizeOfDim0ValidNumField() == 0) { return; }
  CHECK_EQ(ByteSizeOfDim1ValidNumField(), rhs->ByteSizeOfDim1ValidNumField());
  Memcpy<DeviceType::kCPU>(device_ctx, mut_dim1_valid_num(), rhs->dim1_valid_num(),
                           ByteSizeOfDim1ValidNumField());
}

void Blob::CopyFrom(DeviceCtx* device_ctx, const Blob* rhs) {
  if (this == rhs) { return; }
  if (is_contiguous_) {
    CHECK_EQ(TotalByteSize(), rhs->TotalByteSize());
    AutoMemcpy(device_ctx, mut_header_ptr(), rhs->header_ptr(), TotalByteSize(), mem_case(),
               rhs->mem_case());
  } else {
    CopyHeaderFrom(device_ctx, rhs);
    CopyDataContentFrom(device_ctx, rhs);
  }
}

}  // namespace oneflow
