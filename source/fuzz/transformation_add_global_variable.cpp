// Copyright (c) 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "source/fuzz/transformation_add_global_variable.h"

#include "source/fuzz/fuzzer_util.h"

namespace spvtools {
namespace fuzz {

TransformationAddGlobalVariable::TransformationAddGlobalVariable(
    const spvtools::fuzz::protobufs::TransformationAddGlobalVariable& message)
    : message_(message) {}

TransformationAddGlobalVariable::TransformationAddGlobalVariable(
    uint32_t fresh_id, uint32_t type_id, SpvStorageClass storage_class,
    uint32_t initializer_id, bool value_is_irrelevant) {
  message_.set_fresh_id(fresh_id);
  message_.set_type_id(type_id);
  message_.set_storage_class(storage_class);
  message_.set_initializer_id(initializer_id);
  message_.set_value_is_irrelevant(value_is_irrelevant);
}

bool TransformationAddGlobalVariable::IsApplicable(
    opt::IRContext* ir_context, const TransformationContext& /*unused*/) const {
  // The result id must be fresh.
  if (!fuzzerutil::IsFreshId(ir_context, message_.fresh_id())) {
    return false;
  }

  // The storage class must be Private or Workgroup.
  auto storage_class = static_cast<SpvStorageClass>(message_.storage_class());
  switch (storage_class) {
    case SpvStorageClassPrivate:
    case SpvStorageClassWorkgroup:
      break;
    default:
      assert(false && "Unsupported storage class.");
      return false;
  }
  // The type id must correspond to a type.
  auto type = ir_context->get_type_mgr()->GetType(message_.type_id());
  if (!type) {
    return false;
  }
  // That type must be a pointer type ...
  auto pointer_type = type->AsPointer();
  if (!pointer_type) {
    return false;
  }
  // ... with the right storage class.
  if (pointer_type->storage_class() != storage_class) {
    return false;
  }
  if (message_.initializer_id()) {
    // An initializer is not allowed if the storage class is Workgroup.
    if (storage_class == SpvStorageClassWorkgroup) {
      assert(false &&
             "By construction this transformation should not have an "
             "initializer when Workgroup storage class is used.");
      return false;
    }
    // The initializer id must be the id of a constant.  Check this with the
    // constant manager.
    auto constant_id = ir_context->get_constant_mgr()->GetConstantsFromIds(
        {message_.initializer_id()});
    if (constant_id.empty()) {
      return false;
    }
    assert(constant_id.size() == 1 &&
           "We asked for the constant associated with a single id; we should "
           "get a single constant.");
    // The type of the constant must match the pointee type of the pointer.
    if (pointer_type->pointee_type() != constant_id[0]->type()) {
      return false;
    }
  }
  return true;
}

void TransformationAddGlobalVariable::Apply(
    opt::IRContext* ir_context,
    TransformationContext* transformation_context) const {
  fuzzerutil::AddGlobalVariable(
      ir_context, message_.fresh_id(), message_.type_id(),
      static_cast<SpvStorageClass>(message_.storage_class()),
      message_.initializer_id());

  // We have added an instruction to the module, so need to be careful about the
  // validity of existing analyses.
  ir_context->InvalidateAnalysesExceptFor(
      opt::IRContext::Analysis::kAnalysisNone);

  if (message_.value_is_irrelevant()) {
    transformation_context->GetFactManager()->AddFactValueOfPointeeIsIrrelevant(
        message_.fresh_id(), ir_context);
  }
}

protobufs::Transformation TransformationAddGlobalVariable::ToMessage() const {
  protobufs::Transformation result;
  *result.mutable_add_global_variable() = message_;
  return result;
}

}  // namespace fuzz
}  // namespace spvtools
