/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#pragma once

/** \file
 * \ingroup fn
 *
 * Field serve as an intermediate representation for calculation of a group of functions. Having
 * an intermediate representation is helpful mainly to separate the execution system from the
 * system that describes the necessary computations. Fields can be executed in different contexts,
 * and optimization might mean executing the fields differently based on some factors like the
 * number of elements.
 *
 * For now, fields are very tied to the multi-function system, but in the future the #FieldFunction
 * class could be extended to use different descriptions of its outputs and computation besides
 * the embedded multi-function.
 */

#include "BLI_string_ref.hh"
#include "BLI_vector.hh"

#include "FN_generic_virtual_array.hh"
#include "FN_multi_function_builder.hh"
#include "FN_multi_function_procedure.hh"
#include "FN_multi_function_procedure_builder.hh"
#include "FN_multi_function_procedure_executor.hh"

namespace blender::fn {

class FieldSource {
 public:
  ~FieldSource() = default;

  virtual const CPPType &cpp_type_of_output_index(int output_index) const = 0;

  virtual bool is_context_source() const
  {
    return false;
  }

  virtual bool is_operation_source() const
  {
    return false;
  }

  virtual uint64_t hash() const
  {
    return get_default_hash(this);
  }

  friend bool operator==(const FieldSource &a, const FieldSource &b)
  {
    return a.is_equal_to(b);
  }

  virtual bool is_equal_to(const FieldSource &other) const
  {
    return this == &other;
  }
};

/**
 * Descibes the output of a function. Generally corresponds to the combination of an output socket
 * and link combination in a node graph.
 */
class GField {
  std::shared_ptr<FieldSource> source_;
  int source_output_index_ = 0;

 public:
  GField() = default;

  GField(std::shared_ptr<FieldSource> source, const int source_output_index = 0)
      : source_(std::move(source)), source_output_index_(source_output_index)
  {
  }

  operator bool() const
  {
    return source_ != nullptr;
  }

  const fn::CPPType &cpp_type() const
  {
    return source_->cpp_type_of_output_index(source_output_index_);
  }

  bool has_context_source() const
  {
    return source_->is_context_source();
  }

  bool has_operation_source() const
  {
    return source_->is_operation_source();
  }

  const FieldSource &source() const
  {
    return *source_;
  }

  int source_output_index() const
  {
    return source_output_index_;
  }
};

template<typename T> class Field : public GField {
 public:
  Field() = default;

  Field(GField field) : GField(std::move(field))
  {
    BLI_assert(this->cpp_type().template is<T>());
  }

  Field(std::shared_ptr<FieldSource> source, const int source_output_index = 0)
      : Field(GField(std::move(source), source_output_index))
  {
  }
};

class OperationFieldSource : public FieldSource {
  std::unique_ptr<const MultiFunction> owned_function_;
  const MultiFunction *function_;

  blender::Vector<GField> inputs_;

 public:
  OperationFieldSource(std::unique_ptr<const MultiFunction> function, Vector<GField> inputs = {})
      : owned_function_(std::move(function)), inputs_(std::move(inputs))
  {
    function_ = owned_function_.get();
  }

  OperationFieldSource(const MultiFunction &function, Vector<GField> inputs = {})
      : function_(&function), inputs_(std::move(inputs))
  {
  }

  Span<GField> inputs() const
  {
    return inputs_;
  }

  const MultiFunction &multi_function() const
  {
    return *function_;
  }

  bool is_operation_source() const override
  {
    return true;
  }

  const CPPType &cpp_type_of_output_index(int output_index) const override
  {
    int output_counter = 0;
    for (const int param_index : function_->param_indices()) {
      MFParamType param_type = function_->param_type(param_index);
      if (param_type.is_output()) {
        if (output_counter == output_index) {
          return param_type.data_type().single_type();
        }
        output_counter++;
      }
    }
    BLI_assert_unreachable();
    return CPPType::get<float>();
  }
};

class ContextFieldSource;

class FieldContext {
 public:
  ~FieldContext() = default;

  virtual const GVArray *try_get_varray_for_context(const ContextFieldSource &context_source,
                                                    IndexMask mask,
                                                    ResourceScope &scope) const;
};

class ContextFieldSource : public FieldSource {
 protected:
  const CPPType *type_;
  std::string debug_name_;

 public:
  ContextFieldSource(const CPPType &type, std::string debug_name = "")
      : type_(&type), debug_name_(std::move(debug_name))
  {
  }

  virtual const GVArray *try_get_varray_for_context(const FieldContext &context,
                                                    IndexMask mask,
                                                    ResourceScope &scope) const = 0;

  blender::StringRef debug_name() const
  {
    return debug_name_;
  }

  const CPPType &cpp_type() const
  {
    return *type_;
  }

  const CPPType &cpp_type_of_output_index(int output_index) const override
  {
    BLI_assert(output_index == 0);
    UNUSED_VARS_NDEBUG(output_index);
    return *type_;
  }

  bool is_context_source() const override
  {
    return true;
  }
};

Vector<const GVArray *> evaluate_fields(ResourceScope &scope,
                                        Span<const GField *> fields_to_evaluate,
                                        IndexMask mask,
                                        const FieldContext &context,
                                        Span<GVMutableArray *> dst_hints = {});

void evaluate_constant_field(const GField &field, void *r_value);

void evaluate_fields_to_spans(Span<const GField *> fields_to_evaluate,
                              IndexMask mask,
                              const FieldContext &context,
                              Span<GMutableSpan> out_spans);

template<typename T> T evaluate_constant_field(const Field<T> &field)
{
  T value;
  value.~T();
  evaluate_constant_field(field, &value);
  return value;
}

template<typename T> Field<T> make_constant_field(T value)
{
  auto constant_fn = std::make_unique<fn::CustomMF_Constant<T>>(std::forward<T>(value));
  auto operation = std::make_shared<OperationFieldSource>(std::move(constant_fn));
  return Field<T>{GField{std::move(operation), 0}};
}

class FieldEvaluator : NonMovable, NonCopyable {
 private:
  struct OutputPointerInfo {
    void *dst = nullptr;
    void (*set)(void *dst, const GVArray &varray, ResourceScope &scope) = nullptr;
  };

  ResourceScope scope_;
  const FieldContext &context_;
  const IndexMask mask_;
  Vector<GField> fields_to_evaluate_;
  Vector<GVMutableArray *> dst_hints_;
  Vector<const GVArray *> evaluated_varrays_;
  Vector<OutputPointerInfo> output_pointer_infos_;
  bool is_evaluated_ = false;

 public:
  /** Takes #mask by pointer because the mask has to live longer than the evaluator. */
  FieldEvaluator(const FieldContext &context, const IndexMask *mask)
      : context_(context), mask_(*mask)
  {
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param dst: Mutable virtual array that the evaluated result for this field is be written into.
   */
  int add_with_destination(GField field, GVMutableArray &dst)
  {
    const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
    dst_hints_.append(&dst);
    output_pointer_infos_.append({});
    return field_index;
  }

  /** Same as above but typed. */
  template<typename T> int add_with_destination(Field<T> field, VMutableArray<T> &dst)
  {
    GVMutableArray &generic_dst_hint = scope_.construct<GVMutableArray_For_VMutableArray<T>>(
        __func__, dst);
    return this->add_with_destination(GField(std::move(field)), generic_dst_hint);
  }

  /**
   * \param field: Field to add to the evaluator.
   * \param varray_ptr: Once #evaluate is called, the resulting virtual array will be will be
   *   assigned to the given position.
   */
  template<typename T> int add(Field<T> field, const VArray<T> **varray_ptr)
  {
    const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
    dst_hints_.append(nullptr);
    output_pointer_infos_.append(OutputPointerInfo{
        varray_ptr, [](void *dst, const GVArray &varray, ResourceScope &scope) {
          *(const VArray<T> **)dst = &*scope.construct<GVArray_Typed<T>>(__func__, varray);
        }});
    return field_index;
  }

  /**
   * \return Index of the field in the evaluator. Can be used in the #get_evaluated methods.
   */
  int add(GField field)
  {
    const int field_index = fields_to_evaluate_.append_and_get_index(std::move(field));
    dst_hints_.append(nullptr);
    output_pointer_infos_.append({});
    return field_index;
  }

  /**
   * Evaluate all fields on the evaluator. This can only be called once.
   */
  void evaluate()
  {
    BLI_assert_msg(!is_evaluated_, "Cannot evaluate twice.");
    Array<const GField *> fields(fields_to_evaluate_.size());
    for (const int i : fields_to_evaluate_.index_range()) {
      fields[i] = &fields_to_evaluate_[i];
    }
    evaluated_varrays_ = evaluate_fields(scope_, fields, mask_, context_, dst_hints_);
    BLI_assert(fields_to_evaluate_.size() == evaluated_varrays_.size());
    for (const int i : fields_to_evaluate_.index_range()) {
      OutputPointerInfo &info = output_pointer_infos_[i];
      if (info.dst != nullptr) {
        info.set(info.dst, *evaluated_varrays_[i], scope_);
      }
    }
    is_evaluated_ = true;
  }

  const GVArray &get_evaluated(const int field_index) const
  {
    BLI_assert(is_evaluated_);
    return *evaluated_varrays_[field_index];
  }

  template<typename T> const VArray<T> &get_evaluated(const int field_index)
  {
    const GVArray &varray = this->get_evaluated(field_index);
    GVArray_Typed<T> &typed_varray = scope_.construct<GVArray_Typed<T>>(__func__, varray);
    return *typed_varray;
  }
};

}  // namespace blender::fn