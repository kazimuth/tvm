/*!
 *  Copyright (c) 2017 by Contributors
 * \file tvm/runtime/registry.h
 * \brief This file defines the TVM global function registry.
 *
 *  The registered functions will be made available to front-end
 *  as well as backend users.
 *
 *  The registry stores type-erased functions.
 *  Each registered function is automatically exposed
 *  to front-end language(e.g. python).
 *
 *  Front-end can also pass callbacks as PackedFunc, or register
 *  then into the same global registry in C++.
 *  The goal is to mix the front-end language and the TVM back-end.
 *
 * \code
 *   // register the function as MyAPIFuncName
 *   TVM_REGISTER_GLOBAL(MyAPIFuncName)
 *   .set_body([](TVMArgs args, TVMRetValue* rv) {
 *     // my code.
 *   });
 * \endcode
 */
#ifndef TVM_RUNTIME_REGISTRY_H_
#define TVM_RUNTIME_REGISTRY_H_

#include <string>
#include <vector>
#include "packed_func.h"

namespace tvm {
namespace runtime {

/*! \brief Registry for global function */
class Registry {
 public:
  /*!
   * \brief set the body of the function to be f
   * \param f The body of the function.
   */
  TVM_DLL Registry& set_body(PackedFunc f);  // NOLINT(*)
  /*!
   * \brief set the body of the function to be f
   * \param f The body of the function.
   */
  Registry& set_body(PackedFunc::FType f) {  // NOLINT(*)
    return set_body(PackedFunc(f));
  }
  /*!
   * \brief set the body of the function to be TypedPackedFunc.
   *
   * \code
   *
   * TVM_REGISTER_API("addone")
   * .set_body_typed<int(int)>([](int x) { return x + 1; });
   *
   * \endcode
   *
   * \param f The body of the function.
   * \tparam FType the signature of the function.
   * \tparam FLambda The type of f.
   */
  template<typename FType, typename FLambda>
  Registry& set_body_typed(FLambda f) {
    return set_body(TypedPackedFunc<FType>(f).packed());
  }

  /*!
   * \brief set the body of the function to the given function pointer.
   *        Note that this method doesn't work with lambdas, you need to
   *        explicitly give a type for those.
   *
   * \code
   * 
   * int multiply(int x, int y) {
   *   return x * y;
   * }
   *
   * TVM_REGISTER_API("multiply")
   * .set_body_simple(multiply); // will have type int(int, int)
   *
   * \endcode
   *
   * \param f The function to forward to.
   * \tparam FType the signature of the function.
   */
  template<typename R, typename ...Args>
  Registry& set_body_simple(R (*f)(Args...)) {
    return set_body(TypedPackedFunc<R(Args...)>(f));
  }

  template<typename T, typename R, typename ...Args>
  Registry& set_body_method(R (T::*f)(Args...)) {
    return set_body_typed<R(T, Args...)>([f](T target, Args... params) -> R {
      // call method pointer
      return (target.*f)(params...);
    });
  }

  template<typename T, typename R, typename ...Args>
  Registry& set_body_method(R (T::*f)(Args...) const) {
    return set_body_typed<R(T, Args...)>([f](const T target, Args... params) -> R {
      // call method pointer
      return (target.*f)(params...);
    });
  }


  /*!
   * \brief set the body of the function to be the passed method pointer.
   *        used when calling a method on a Node subclass through a NodeRef subclass.
   *
   * \code
   * 
   * // node subclass:
   * struct ExampleNode: BaseNode {
   *    int doThing(int x);
   * }
   * 
   * // noderef subclass
   * struct Example; 
   *
   * TVM_REGISTER_API("Example_doThing")
   * .set_body_node_method<Example>(&ExampleNode::doThing); // will have type int(Example, int)
   *
   * \endcode
   *
   * \param f the method pointer to forward to.
   * \tparam NodeRef the node reference type to call the method on
   */
  template<typename NodeRef, typename Node, typename R, typename ...Args>
  Registry& set_body_node_method(R (Node::*f)(Args...)) {
    return set_body_typed<R(NodeRef, Args...)>([f](NodeRef ref, Args... params) {
      Node* target = ref.operator->();
      // call method pointer
      return (target->*f)(params...);
    });
  }

  template<typename NodeRef, typename Node, typename R, typename ...Args>
  Registry& set_body_node_method(R (Node::*f)(Args...) const) {
    return set_body_typed<R(NodeRef, Args...)>([f](NodeRef ref, Args... params) {
      const Node* target = ref.operator->();
      // call method pointer
      return (target->*f)(params...);
    });
  }

  /*!
   * \brief Register a function with given name
   * \param name The name of the function.
   * \param override Whether allow oveeride existing function.
   * \return Reference to theregistry.
   */
  TVM_DLL static Registry& Register(const std::string& name, bool override = false);  // NOLINT(*)
  /*!
   * \brief Erase global function from registry, if exist.
   * \param name The name of the function.
   * \return Whether function exist.
   */
  TVM_DLL static bool Remove(const std::string& name);
  /*!
   * \brief Get the global function by name.
   * \param name The name of the function.
   * \return pointer to the registered function,
   *   nullptr if it does not exist.
   */
  TVM_DLL static const PackedFunc* Get(const std::string& name);  // NOLINT(*)
  /*!
   * \brief Get the names of currently registered global function.
   * \return The names
   */
  TVM_DLL static std::vector<std::string> ListNames();

  // Internal class.
  struct Manager;

 protected:
  /*! \brief name of the function */
  std::string name_;
  /*! \brief internal packed function */
  PackedFunc func_;
  friend struct Manager;
};

/*! \brief helper macro to supress unused warning */
#if defined(__GNUC__)
#define TVM_ATTRIBUTE_UNUSED __attribute__((unused))
#else
#define TVM_ATTRIBUTE_UNUSED
#endif

#define TVM_STR_CONCAT_(__x, __y) __x##__y
#define TVM_STR_CONCAT(__x, __y) TVM_STR_CONCAT_(__x, __y)

#define TVM_FUNC_REG_VAR_DEF                                            \
  static TVM_ATTRIBUTE_UNUSED ::tvm::runtime::Registry& __mk_ ## TVM

#define TVM_TYPE_REG_VAR_DEF                                            \
  static TVM_ATTRIBUTE_UNUSED ::tvm::runtime::ExtTypeVTable* __mk_ ## TVMT

/*!
 * \brief Register a function globally.
 * \code
 *   TVM_REGISTER_GLOBAL("MyPrint")
 *   .set_body([](TVMArgs args, TVMRetValue* rv) {
 *   });
 * \endcode
 */
#define TVM_REGISTER_GLOBAL(OpName)                              \
  TVM_STR_CONCAT(TVM_FUNC_REG_VAR_DEF, __COUNTER__) =            \
      ::tvm::runtime::Registry::Register(OpName)

/*!
 * \brief Macro to register extension type.
 *  This must be registered in a cc file
 *  after the trait extension_type_info is defined.
 */
#define TVM_REGISTER_EXT_TYPE(T)                                 \
  TVM_STR_CONCAT(TVM_TYPE_REG_VAR_DEF, __COUNTER__) =            \
      ::tvm::runtime::ExtTypeVTable::Register_<T>()

}  // namespace runtime
}  // namespace tvm
#endif  // TVM_RUNTIME_REGISTRY_H_
