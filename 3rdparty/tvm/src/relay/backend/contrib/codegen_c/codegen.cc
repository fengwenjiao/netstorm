/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
#include <tvm/relay/expr_functor.h>
#include <tvm/relay/transform.h>
#include <tvm/relay/type.h>
#include <tvm/runtime/module.h>
#include <tvm/runtime/object.h>

#include <fstream>
#include <sstream>

#include "codegen_c.h"

namespace tvm {
namespace relay {
namespace contrib {

/*!
 * \brief An example codegen that is only used for quick prototyping and testing
 * purpose. Only several binary options are covered. Users
 * may need to extend them to cover more operators.
 */
class CodegenC : public ExprVisitor, public CodegenCBase {
 public:
  explicit CodegenC(const std::string& id) { this->ext_func_id_ = id; }

  void VisitExpr_(const VarNode* node) {
    ext_func_args_.push_back(node->name_hint());
    out_.clear();
    out_.push_back({node->name_hint(), 0});
  }

  void VisitExpr_(const CallNode* call) final {
    std::ostringstream macro_stream;
    std::ostringstream decl_stream;
    std::ostringstream buf_stream;

    std::string func_name = ext_func_id_ + "_" + std::to_string(func_idx++);

    // Make function declaration
    macro_stream << "CSOURCE_BINARY_OP_" << call->args.size() << "D(" << func_name << ", ";

    if (IsOp(call, "add")) {
      macro_stream << "+";
    } else if (IsOp(call, "subtract")) {
      macro_stream << "-";
    } else if (IsOp(call, "multiply")) {
      macro_stream << "*";
    } else {
      LOG(FATAL) << "Unrecognized op";
    }

    auto in_shape = GetShape(call->args[0]->checked_type());
    for (size_t i = 0; i < in_shape.size(); ++i) {
      macro_stream << ", " << in_shape[i];
    }
    macro_stream << ");";
    func_decl_.push_back(macro_stream.str());

    // Make function call when visiting arguments
    bool first = true;
    decl_stream << func_name << "(";
    for (size_t i = 0; i < call->args.size(); ++i) {
      VisitExpr(call->args[i]);
      for (auto out : out_) {
        if (!first) {
          decl_stream << ", ";
        }
        first = false;
        decl_stream << out.first;
      }
    }

    auto type_node = call->checked_type().as<TensorTypeNode>();
    CHECK(type_node != nullptr && runtime::TypeMatch(type_node->dtype, kDLFloat, 32))
        << "Only support single output tensor with float type";
    std::string out = "buf_" + std::to_string(buf_idx_++);
    auto out_shape = GetShape(call->checked_type());
    int out_size = 1;
    for (size_t i = 0; i < out_shape.size(); ++i) {
      out_size *= out_shape[i];
    }
    buf_stream << "float* " << out << " = (float*)std::malloc(4 * " << out_size << ");";
    buf_decl_.push_back(buf_stream.str());

    decl_stream << ", " << out << ");";
    ext_func_body.push_back(decl_stream.str());

    // Update output buffer
    out_.clear();
    out_.push_back({out, out_size});
  }

  /*!
   * \brief Emit the source code that invokes C compiler compatible wrappers.
   *
   * \return The emitted code.
   */
  std::string JIT() {
    // Write function macros
    for (auto decl : func_decl_) {
      code_stream_ << decl << "\n";
    }
    return JitImpl(ext_func_id_, ext_func_args_, buf_decl_, ext_func_body, out_);
  }

 private:
  /*! \brief The function id that represents a C source function. */
  std::string ext_func_id_ = "";
  /*! \brief The index of a wrapped C function. */
  int func_idx = 0;
  /*! \brief The index of allocated buffers. */
  int buf_idx_ = 0;
  /*! \brief The arguments of a C compiler compatible function. */
  std::vector<std::string> ext_func_args_;
  /*! \brief The statements of a C compiler compatible function. */
  std::vector<std::string> ext_func_body;
  /*! \brief The declaration statements of a C compiler compatible function. */
  std::vector<std::string> func_decl_;
  /*! \brief The declaration statements of buffers. */
  std::vector<std::string> buf_decl_;
  /*! \brief The name and index pairs for output. */
  std::vector<std::pair<std::string, int>> out_;
};

class CSourceCodegen : public CSourceModuleCodegenBase {
 public:
  void GenCFunc(const Function& func) {
    CHECK(func.defined()) << "Input error: expect a Relay function.";

    // Record the external symbol for runtime lookup.
    auto sid = GetExtSymbol(func);

    CodegenC builder(sid);
    builder.VisitExpr(func->body);
    code_stream_ << builder.JIT();
  }

  runtime::Module CreateCSourceModule(const ObjectRef& ref) override {
    // Create headers
    code_stream_ << "#include <cstring>\n";
    code_stream_ << "#include <tvm/runtime/c_runtime_api.h>\n";
    code_stream_ << "#include <tvm/runtime/packed_func.h>\n";
    code_stream_ << "#include <dlpack/dlpack.h>\n";

    // Append some common macro for operator definition.
    const char* operator_macro = R"op_macro(
    #define CSOURCE_BINARY_OP_1D(p_ID_, p_OP_, p_DIM1_)       \
      extern "C" void p_ID_(float* a, float* b, float* out) { \
        for (int64_t i = 0; i < p_DIM1_; ++i) {               \
          out[i] = a[i] p_OP_ b[i];                           \
        }                                                     \
      }

    #define CSOURCE_BINARY_OP_2D(p_ID_, p_OP_, p_DIM1_, p_DIM2_)  \
      extern "C" void p_ID_(float* a, float* b, float* out) {     \
        for (int64_t i = 0; i < p_DIM1_; ++i) {                   \
          for (int64_t j = 0; j < p_DIM2_; ++j) {                 \
            int64_t k = i * p_DIM2_ + j;                          \
            out[k] = a[k] p_OP_ b[k];                             \
          }                                                       \
        }                                                         \
      }
    )op_macro";

    code_stream_ << operator_macro << "\n\n";

    if (ref->IsInstance<FunctionNode>()) {
      GenCFunc(Downcast<Function>(ref));
    } else if (ref->IsInstance<IRModuleNode>()) {
      IRModule mod = Downcast<IRModule>(ref);
      for (const auto& it : mod->functions) {
        GenCFunc(Downcast<Function>(it.second));
      }
    } else {
      LOG(FATAL) << "The input ref is expected to be a Relay function or module"
                 << "\n";
    }

    // Create a CSourceModule
    const auto* pf = runtime::Registry::Get("module.csource_module_create");
    CHECK(pf != nullptr) << "Cannot find csource module to create the external runtime module";
    return (*pf)(code_stream_.str(), "cc");
  }

 private:
  std::ostringstream code_stream_;
};

/*!
 * \brief The external compiler/codegen tool. It takes a Relay expression/module and
 * compile it into a runtime module.
 *
 * The external codegen tool should have been registered similiarly to LLVM,
 * CUDA, etc, under TVM, so the generated code could be packed in a runtime
 * module. This module simplifies code serialization and invocation.
 */
runtime::Module CCompiler(const ObjectRef& ref) {
  CSourceCodegen csource;
  return csource.CreateCSourceModule(ref);
}

TVM_REGISTER_GLOBAL("relay.ext.ccompiler").set_body_typed(CCompiler);

}  // namespace contrib
}  // namespace relay
}  // namespace tvm
