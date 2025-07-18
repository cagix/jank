#include <clojure/core_native.hpp>
#include <jank/runtime/convert/function.hpp>
#include <jank/runtime/core.hpp>
#include <jank/runtime/core/equal.hpp>
#include <jank/runtime/core/meta.hpp>
#include <jank/runtime/context.hpp>
#include <jank/runtime/behavior/callable.hpp>
#include <jank/runtime/visit.hpp>
#include <jank/runtime/sequence_range.hpp>
#include <jank/util/fmt.hpp>

namespace clojure::core_native
{
  using namespace jank;
  using namespace jank::runtime;

  static object_ref subvec(object_ref const o, object_ref const start, object_ref const end)
  {
    return runtime::subvec(o, runtime::to_int(start), runtime::to_int(end));
  }

  static object_ref not_(object_ref const o)
  {
    if(runtime::is_nil(o))
    {
      return jank_true;
    }
    return make_box(runtime::is_false(o));
  }

  static object_ref to_unqualified_symbol(object_ref const o)
  {
    return runtime::visit_object(
      [&](auto const typed_o) -> object_ref {
        using T = typename decltype(typed_o)::value_type;

        if constexpr(std::same_as<T, obj::symbol>)
        {
          return typed_o;
        }
        else if constexpr(std::same_as<T, obj::persistent_string>)
        {
          return make_box<obj::symbol>(typed_o->data);
        }
        else if constexpr(std::same_as<T, var>)
        {
          return make_box<obj::symbol>(typed_o->n->name->name, typed_o->name->name);
        }
        else if constexpr(std::same_as<T, obj::keyword>)
        {
          return typed_o->sym;
        }
        else
        {
          throw std::runtime_error{ util::format("can't convert {} to a symbol",
                                                 typed_o->to_code_string()) };
        }
      },
      o);
  }

  static object_ref to_qualified_symbol(object_ref const ns, object_ref const name)
  {
    return make_box<obj::symbol>(ns, name);
  }

  static object_ref lazy_seq(object_ref const o)
  {
    return make_box<obj::lazy_sequence>(o);
  }

  static object_ref is_var(object_ref const o)
  {
    return make_box(o->type == object_type::var);
  }

  static object_ref var_get(object_ref const o)
  {
    return try_object<var>(o)->deref();
  }

  static object_ref intern_var(object_ref const sym)
  {
    return __rt_ctx->intern_var(try_object<obj::symbol>(sym)).expect_ok();
  }

  static object_ref var_get_root(object_ref const o)
  {
    return try_object<var>(o)->get_root();
  }

  static object_ref var_bind_root(object_ref const v, object_ref const o)
  {
    return try_object<var>(v)->bind_root(o);
  }

  static object_ref alter_var_root(object_ref const o, object_ref const fn, object_ref const args)
  {
    return try_object<var>(o)->alter_root(fn, args);
  }

  static object_ref is_var_bound(object_ref const o)
  {
    return make_box(try_object<runtime::var>(o)->is_bound());
  }

  static object_ref is_var_thread_bound(object_ref const o)
  {
    return make_box(try_object<runtime::var>(o)->get_thread_binding().is_some());
  }

  static object_ref delay(object_ref const fn)
  {
    return make_box<obj::delay>(fn);
  }

  static object_ref is_fn(object_ref const o)
  {
    return make_box(o->type == object_type::native_function_wrapper
                    || o->type == object_type::jit_function);
  }

  static object_ref is_multi_fn(object_ref const o)
  {
    return make_box(o->type == object_type::multi_function);
  }

  static object_ref multi_fn(object_ref const name,
                             object_ref const dispatch_fn,
                             object_ref const default_,
                             object_ref const hierarchy)
  {
    return make_box<obj::multi_function>(name, dispatch_fn, default_, hierarchy);
  }

  static object_ref
  defmethod(object_ref const multifn, object_ref const dispatch_val, object_ref const fn)
  {
    return try_object<obj::multi_function>(multifn)->add_method(dispatch_val, fn);
  }

  static object_ref remove_all_methods(object_ref const multifn)
  {
    return try_object<obj::multi_function>(multifn)->reset();
  }

  static object_ref remove_method(object_ref const multifn, object_ref const dispatch_val)
  {
    return try_object<obj::multi_function>(multifn)->remove_method(dispatch_val);
  }

  static object_ref prefer_method(object_ref const multifn,
                                  object_ref const dispatch_val_x,
                                  object_ref const dispatch_val_y)
  {
    return try_object<obj::multi_function>(multifn)->prefer_method(dispatch_val_x, dispatch_val_y);
  }

  static object_ref methods(object_ref const multifn)
  {
    return try_object<obj::multi_function>(multifn)->method_table;
  }

  static object_ref get_method(object_ref const multifn, object_ref const dispatch_val)
  {
    return try_object<obj::multi_function>(multifn)->get_fn(dispatch_val);
  }

  static object_ref prefers(object_ref const multifn)
  {
    return try_object<obj::multi_function>(multifn)->prefer_table;
  }

  static object_ref sleep(object_ref const ms)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(to_int(ms)));
    return jank_nil;
  }

  static object_ref current_time()
  {
    using namespace std::chrono;
    auto const t(high_resolution_clock::now());
    return make_box(duration_cast<nanoseconds>(t.time_since_epoch()).count());
  }

  static object_ref in_ns(object_ref const sym)
  {
    __rt_ctx->current_ns_var->set(__rt_ctx->intern_ns(try_object<obj::symbol>(sym))).expect_ok();
    return jank_nil;
  }

  static object_ref intern_ns(object_ref const sym)
  {
    return __rt_ctx->intern_ns(try_object<obj::symbol>(sym));
  }

  static object_ref find_ns(object_ref const sym)
  {
    return __rt_ctx->find_ns(try_object<obj::symbol>(sym));
  }

  static object_ref find_var(object_ref const sym)
  {
    return __rt_ctx->find_var(try_object<obj::symbol>(sym));
  }

  static object_ref remove_ns(object_ref const sym)
  {
    return __rt_ctx->remove_ns(try_object<obj::symbol>(sym));
  }

  static object_ref is_ns(object_ref const ns_or_sym)
  {
    return make_box(ns_or_sym->type == object_type::ns);
  }

  static object_ref ns_name(object_ref const ns)
  {
    return try_object<runtime::ns>(ns)->name;
  }

  static object_ref ns_map(object_ref const ns)
  {
    return try_object<runtime::ns>(ns)->get_mappings();
  }

  static object_ref var_ns(object_ref const v)
  {
    return try_object<runtime::var>(v)->n;
  }

  static object_ref ns_resolve(object_ref const ns, object_ref const sym)
  {
    auto const n(try_object<runtime::ns>(ns));
    auto const found(n->find_var(try_object<obj::symbol>(sym)));
    return found;
  }

  static object_ref
  alias(object_ref const current_ns, object_ref const remote_ns, object_ref const alias)
  {
    try_object<ns>(current_ns)
      ->add_alias(try_object<obj::symbol>(alias), try_object<ns>(remote_ns))
      .expect_ok();
    return jank_nil;
  }

  static object_ref ns_unalias(object_ref const current_ns, object_ref const alias)
  {
    try_object<ns>(current_ns)->remove_alias(try_object<obj::symbol>(alias));
    return jank_nil;
  }

  static object_ref ns_unmap(object_ref const current_ns, object_ref const sym)
  {
    try_object<ns>(current_ns)->unmap(try_object<obj::symbol>(sym)).expect_ok();
    return jank_nil;
  }

  static object_ref refer(object_ref const current_ns, object_ref const sym, object_ref const var)
  {
    expect_object<runtime::ns>(current_ns)
      ->refer(try_object<obj::symbol>(sym), expect_object<runtime::var>(var))
      .expect_ok();
    return jank_nil;
  }

  static object_ref load_module(object_ref const path)
  {
    __rt_ctx->load_module(runtime::to_string(path), module::origin::latest).expect_ok();
    return jank_nil;
  }

  static object_ref compile(object_ref const path)
  {
    __rt_ctx->compile_module(runtime::to_string(path)).expect_ok();
    return jank_nil;
  }

  static object_ref eval(object_ref const expr)
  {
    return __rt_ctx->eval(expr);
  }

  static object_ref hash_unordered(object_ref const expr)
  {
    return make_box(hash::unordered(expr.data)).erase();
  }

  /* TODO: implement opts for `read-string` */
  static object_ref read_string(object_ref const /* opts */, object_ref const str)
  {
    return __rt_ctx->read_string(runtime::to_string(str));
  }

  static object_ref jank_version()
  {
    return make_box(JANK_VERSION);
  }
}

extern "C" jank_object_ref jank_load_clojure_core_native()
{
  using namespace jank;
  using namespace jank::runtime;
  using namespace clojure;

  auto const ns(__rt_ctx->intern_ns("clojure.core-native"));

  auto const intern_val([=](jtl::immutable_string const &name, auto const val) {
    ns->intern_var(name)->bind_root(convert<decltype(val)>::into_object(val));
  });
  auto const intern_fn([=](jtl::immutable_string const &name, auto const fn) {
    ns->intern_var(name)->bind_root(
      make_box<obj::native_function_wrapper>(convert_function(fn))
        ->with_meta(obj::persistent_hash_map::create_unique(std::make_pair(
          __rt_ctx->intern_keyword("name").expect_ok(),
          make_box(obj::symbol{ __rt_ctx->current_ns()->to_string(), name }.to_string())))));
  });
  auto const intern_fn_obj([=](jtl::immutable_string const &name, object_ref const fn) {
    ns->intern_var(name)->bind_root(with_meta(
      fn,
      obj::persistent_hash_map::create_unique(std::make_pair(
        __rt_ctx->intern_keyword("name").expect_ok(),
        make_box(obj::symbol{ __rt_ctx->current_ns()->to_string(), name }.to_string())))));
  });

  intern_fn("type", &type);
  intern_fn("nil?", &is_nil);
  intern_fn("identical?", &is_identical);
  intern_fn("empty?", &is_empty);
  intern_fn("empty", &empty);
  intern_fn("count", static_cast<usize (*)(object_ref)>(&sequence_length));
  intern_fn("boolean", static_cast<bool (*)(object_ref)>(&truthy));
  intern_fn("integer", static_cast<i64 (*)(object_ref)>(&to_int));
  intern_fn("real", static_cast<f64 (*)(object_ref)>(&to_real));
  intern_fn("seq", static_cast<object_ref (*)(object_ref)>(&seq));
  intern_fn("fresh-seq", static_cast<object_ref (*)(object_ref)>(&fresh_seq));
  intern_fn("first", static_cast<object_ref (*)(object_ref)>(&first));
  intern_fn("second", static_cast<object_ref (*)(object_ref)>(&second));
  intern_fn("next", static_cast<object_ref (*)(object_ref)>(&next));
  intern_fn("next-in-place", static_cast<object_ref (*)(object_ref)>(&next_in_place));
  intern_fn("rest", static_cast<object_ref (*)(object_ref)>(&rest));
  intern_fn("cons", &cons);
  intern_fn("coll?", &is_collection);
  intern_fn("seq?", &is_seq);
  intern_fn("seqable?", &is_seqable);
  intern_fn("list?", &is_list);
  intern_fn("vector?", &is_vector);
  intern_fn("vec", &vec);
  intern_fn("subvec", &core_native::subvec);
  intern_fn("conj", &conj);
  intern_fn("map?", &is_map);
  intern_fn("associative?", &is_associative);
  intern_fn("assoc", static_cast<object_ref (*)(object_ref, object_ref, object_ref)>(&assoc));
  intern_fn("pr-str", static_cast<jtl::immutable_string (*)(object_ref)>(&to_code_string));
  intern_fn("string?", &is_string);
  intern_fn("char?", &is_char);
  intern_fn("to-string", static_cast<jtl::immutable_string (*)(object_ref)>(&to_string));
  intern_fn("str", static_cast<jtl::immutable_string (*)(object_ref, object_ref)>(&str));
  intern_fn("symbol?", &is_symbol);
  intern_fn("true?", &is_true);
  intern_fn("false?", &is_false);
  intern_fn("not", &core_native::not_);
  intern_fn("some?", &is_some);
  intern_fn("meta", &meta);
  intern_fn("with-meta", &with_meta);
  intern_fn("reset-meta!", &reset_meta);
  intern_fn("macroexpand-1", &macroexpand1);
  intern_fn("macroexpand", &macroexpand);
  intern_fn("->unqualified-symbol", &core_native::to_unqualified_symbol);
  intern_fn("->qualified-symbol", &core_native::to_qualified_symbol);
  intern_fn("apply*", &apply_to);
  intern_fn("counted?", &is_counter);
  intern_fn("transientable?", &is_transientable);
  intern_fn("transient", &transient);
  intern_fn("persistent!", &persistent);
  intern_fn("conj-in-place!", &conj_in_place);
  intern_fn("assoc-in-place!",
            static_cast<object_ref (*)(object_ref, object_ref, object_ref)>(&assoc_in_place));
  intern_fn("dissoc-in-place!", &dissoc_in_place);
  intern_fn("pop-in-place!", &pop_in_place);
  intern_fn("disj-in-place!", &disj_in_place);
  intern_fn("deref", &deref);
  intern_fn("reduced", &reduced);
  intern_fn("reduced?", &is_reduced);
  intern_fn("reduce", &reduce);
  intern_fn("peek", &peek);
  intern_fn("pop", &pop);
  intern_fn("atom", &atom);
  intern_fn("compare-and-set!", &compare_and_set);
  intern_fn("reset!", &reset);
  intern_fn("reset-vals!", &reset_vals);
  intern_fn("volatile!", &volatile_);
  intern_fn("volatile?", &is_volatile);
  intern_fn("vreset!", &vreset);
  intern_fn("+", static_cast<object_ref (*)(object_ref, object_ref)>(&add));
  intern_fn("-", static_cast<object_ref (*)(object_ref, object_ref)>(&sub));
  intern_fn("/", static_cast<object_ref (*)(object_ref, object_ref)>(&div));
  intern_fn("*", static_cast<object_ref (*)(object_ref, object_ref)>(&mul));
  intern_fn("bit-not", &bit_not);
  intern_fn("bit-and", &bit_and);
  intern_fn("bit-or", &bit_or);
  intern_fn("bit-xor", &bit_xor);
  intern_fn("bit-and-not", &bit_and_not);
  intern_fn("bit-clear", &bit_clear);
  intern_fn("bit-set", &bit_set);
  intern_fn("bit-flip", &bit_flip);
  intern_fn("bit-test", &bit_test);
  intern_fn("bit-shift-left", &bit_shift_left);
  intern_fn("bit-shift-right", &bit_shift_right);
  intern_fn("unsigned-bit-shift-right", &bit_unsigned_shift_right);
  intern_fn("<", static_cast<bool (*)(object_ref, object_ref)>(&lt));
  intern_fn("<=", static_cast<bool (*)(object_ref, object_ref)>(&lte));
  intern_fn("compare", &runtime::compare);
  intern_fn("min", static_cast<object_ref (*)(object_ref, object_ref)>(&min));
  intern_fn("max", static_cast<object_ref (*)(object_ref, object_ref)>(&max));
  intern_fn("inc", static_cast<object_ref (*)(object_ref)>(&inc));
  intern_fn("dec", static_cast<object_ref (*)(object_ref)>(&dec));
  intern_fn("numerator", &numerator);
  intern_fn("denominator", &denominator);
  intern_fn("pos?", &is_pos);
  intern_fn("neg?", &is_neg);
  intern_fn("zero?", &is_zero);
  intern_fn("rem", static_cast<object_ref (*)(object_ref, object_ref)>(&rem));
  intern_fn("quot", static_cast<object_ref (*)(object_ref, object_ref)>(&quot));
  intern_fn("integer?", &is_integer);
  intern_fn("real?", &is_real);
  intern_fn("ratio?", &is_ratio);
  intern_fn("boolean?", &is_boolean);
  intern_fn("number?", &is_number);
  intern_fn("even?", &is_even);
  intern_fn("odd?", &is_odd);
  intern_fn("NaN?", &is_nan);
  intern_fn("infinite?", &is_infinite);
  intern_fn("rand", &runtime::rand);
  intern_fn("sequential?", &is_sequential);
  intern_fn("first-index-of", &first_index_of);
  intern_fn("last-index-of", &last_index_of);
  intern_fn("lazy-seq*", &core_native::lazy_seq);
  intern_fn("chunk-buffer", &chunk_buffer);
  intern_fn("chunk-append", &chunk_append);
  intern_fn("chunk", &chunk);
  intern_fn("chunk-first", &chunk_first);
  intern_fn("chunk-next", &chunk_next);
  intern_fn("chunk-rest", &chunk_rest);
  intern_fn("chunk-cons", &chunk_cons);
  intern_fn("chunk-seq?", &is_chunked_seq);
  intern_fn("dissoc", &dissoc);
  intern_fn("contains?", &contains);
  intern_fn("find", &find);
  intern_fn("disj", &disj);
  intern_fn("hash", &to_hash);
  intern_fn("set?", &is_set);
  intern_fn("named?", &is_named);
  intern_fn("name", &name);
  intern_fn("namespace", &namespace_);
  intern_fn("var?", &core_native::is_var);
  intern_fn("var-get", &core_native::var_get);
  intern_fn("intern-var", &core_native::intern_var);
  intern_fn("var-get-root", &core_native::var_get_root);
  intern_fn("var-bind-root", &core_native::var_bind_root);
  intern_fn("alter-var-root", &core_native::alter_var_root);
  intern_fn("var-bound?", &core_native::is_var_bound);
  intern_fn("var-thread-bound?", &core_native::is_var_thread_bound);
  intern_fn("push-thread-bindings", &push_thread_bindings);
  intern_fn("pop-thread-bindings", &pop_thread_bindings);
  intern_fn("get-thread-bindings", &get_thread_bindings);
  intern_fn("keyword?", &is_keyword);
  intern_fn("simple-keyword?", &is_simple_keyword);
  intern_fn("qualified-keyword?", &is_qualified_keyword);
  intern_fn("keyword", &keyword);
  intern_fn("simple-symbol?", &is_simple_symbol);
  intern_fn("qualified-symbol?", &is_qualified_symbol);
  intern_fn("iterate", &iterate);
  intern_fn("delay*", &core_native::delay);
  intern_fn("force", &force);
  intern_fn("ifn?", &is_callable);
  intern_fn("fn?", &core_native::is_fn);
  intern_fn("multi-fn?", &core_native::is_multi_fn);
  intern_fn("multi-fn*", &core_native::multi_fn);
  intern_fn("defmethod*", &core_native::defmethod);
  intern_fn("remove-all-methods", &core_native::remove_all_methods);
  intern_fn("remove-method", &core_native::remove_method);
  intern_fn("prefer-method", &core_native::prefer_method);
  intern_fn("methods", &core_native::methods);
  intern_fn("get-method", &core_native::get_method);
  intern_fn("prefers", &core_native::prefers);
  intern_val("int-min", std::numeric_limits<i64>::min());
  intern_val("int-max", std::numeric_limits<i64>::max());
  intern_val("int32-min", std::numeric_limits<i32>::min());
  intern_val("int32-max", std::numeric_limits<i32>::max());
  intern_fn("sleep", &core_native::sleep);
  intern_fn("current-time", &core_native::current_time);
  intern_fn("create-ns", &core_native::intern_ns);
  intern_fn("in-ns", &core_native::in_ns);
  intern_fn("find-ns", &core_native::find_ns);
  intern_fn("find-var", &core_native::find_var);
  intern_fn("remove-ns", &core_native::remove_ns);
  intern_fn("ns?", &core_native::is_ns);
  intern_fn("ns-name", &core_native::ns_name);
  intern_fn("ns-map", &core_native::ns_map);
  intern_fn("var-ns", &core_native::var_ns);
  intern_fn("ns-resolve", &core_native::ns_resolve);
  intern_fn("alias", &core_native::alias);
  intern_fn("ns-unalias", &core_native::ns_unalias);
  intern_fn("ns-unmap", &core_native::ns_unmap);
  intern_fn("refer", &core_native::refer);
  intern_fn("load-module", &core_native::load_module);
  intern_fn("compile", &core_native::compile);
  intern_fn("eval", &core_native::eval);
  intern_fn("hash-unordered-coll", &core_native::hash_unordered);
  intern_fn("read-string", &core_native::read_string);
  intern_fn("jank-version", &core_native::jank_version);
  intern_fn("parse-long", &parse_long);
  intern_fn("parse-double", &parse_double);
  intern_fn("tagged-literal", &tagged_literal);
  intern_fn("tagged-literal?", &is_tagged_literal);
  intern_fn("sorted?", &is_sorted);
  intern_fn("sort", &sort);
  intern_fn("shuffle", &shuffle);

  /* TODO: jank.math? */
  intern_fn("sqrt", static_cast<f64 (*)(object_ref)>(&runtime::sqrt));
  intern_fn("tan", static_cast<f64 (*)(object_ref)>(&runtime::tan));
  intern_fn("abs", static_cast<object_ref (*)(object_ref)>(&runtime::abs));
  intern_fn("pow", static_cast<f64 (*)(object_ref, object_ref)>(&runtime::pow));

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, false)));
    fn->arity_1 = [](object * const seq) -> object * { return list(seq).erase(); };
    intern_fn_obj("list", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(2, true, true)));
    fn->arity_1 = [](object *) -> object * { return jank_true.erase(); };
    fn->arity_2 = [](object * const l, object * const r) -> object * {
      return make_box(equal(l, r)).erase();
    };
    fn->arity_3 = [](object * const l, object * const r, object * const rest) -> object * {
      if(!equal(l, r))
      {
        return jank_false.erase();
      }

      return visit_seqable(
        [](auto const typed_rest, object_ref const l) {
          for(auto const e : make_sequence_range(typed_rest))
          {
            if(!equal(l, e))
            {
              return jank_false.erase();
            }
          }

          return jank_true.erase();
        },
        rest,
        l);
    };
    intern_fn_obj("=", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(2, true, true)));
    fn->arity_1 = [](object *) -> object * { return jank_true.erase(); };
    fn->arity_2 = [](object * const l, object * const r) -> object * {
      return make_box(is_equiv(l, r)).erase();
    };
    fn->arity_3 = [](object * const l, object * const r, object * const rest) -> object * {
      if(!is_equiv(l, r))
      {
        return jank_false.erase();
      }

      for(auto it(fresh_seq(rest)); it != jank_nil; it = next_in_place(it))
      {
        if(!is_equiv(l, first(it)))
        {
          return jank_false.erase();
        }
      }

      return jank_true.erase();
    };
    intern_fn_obj("==", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, false)));
    fn->arity_1 = [](object * const seq) -> object * { return println(seq).erase(); };
    intern_fn_obj("println", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, false)));
    fn->arity_1 = [](object * const seq) -> object * { return print(seq).erase(); };
    intern_fn_obj("print", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, false)));
    fn->arity_1 = [](object * const seq) -> object * { return prn(seq).erase(); };
    intern_fn_obj("prn", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, false)));
    fn->arity_1 = [](object * const seq) -> object * { return pr(seq).erase(); };
    intern_fn_obj("pr", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, false, false)));
    fn->arity_0 = []() -> object * { return gensym(make_box("G__")).erase(); };
    fn->arity_1 = [](object * const prefix) -> object * { return gensym(prefix).erase(); };
    intern_fn_obj("gensym", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(4, true, true)));
    fn->arity_2 = [](object * const atom, object * const fn) -> object * {
      return try_object<obj::atom>(atom)->swap(fn).erase();
    };
    fn->arity_3 = [](object * const atom, object * const fn, object * const a1) -> object * {
      return try_object<obj::atom>(atom)->swap(fn, a1).erase();
    };
    fn->arity_4 =
      [](object * const atom, object * const fn, object * const a1, object * const a2) -> object * {
      return try_object<obj::atom>(atom)->swap(fn, a1, a2).erase();
    };
    fn->arity_5 = [](object * const atom,
                     object * const fn,
                     object * const a1,
                     object * const a2,
                     object * const rest) -> object * {
      return try_object<obj::atom>(atom)->swap(fn, a1, a2, rest).erase();
    };
    intern_fn_obj("swap!", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(4, true, true)));
    fn->arity_2 = [](object * const atom, object * const fn) -> object * {
      return try_object<obj::atom>(atom)->swap_vals(fn).erase();
    };
    fn->arity_3 = [](object * const atom, object * const fn, object * const a1) -> object * {
      return try_object<obj::atom>(atom)->swap_vals(fn, a1).erase();
    };
    fn->arity_4 =
      [](object * const atom, object * const fn, object * const a1, object * const a2) -> object * {
      return try_object<obj::atom>(atom)->swap_vals(fn, a1, a2).erase();
    };
    fn->arity_5 = [](object * const atom,
                     object * const fn,
                     object * const a1,
                     object * const a2,
                     object * const rest) -> object * {
      return try_object<obj::atom>(atom)->swap_vals(fn, a1, a2, rest).erase();
    };
    intern_fn_obj("swap-vals!", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(2, true, true)));
    fn->arity_2
      = [](object * const vol, object * const fn) -> object * { return vswap(vol, fn).erase(); };
    fn->arity_3 = [](object * const vol, object * const fn, object * const rest) -> object * {
      return vswap(vol, fn, rest).erase();
    };
    intern_fn_obj("vswap!", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, false, false)));
    fn->arity_2
      = [](object * const s, object * const start) -> object * { return subs(s, start).erase(); };
    fn->arity_3 = [](object * const s, object * const start, object * const end) -> object * {
      return subs(s, start, end).erase();
    };
    intern_fn_obj("subs", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, true)));
    fn->arity_0 = []() -> object * { return obj::persistent_hash_map::empty().erase(); };
    fn->arity_1 = [](object * const kvs) -> object * {
      return obj::persistent_hash_map::create_from_seq(kvs).erase();
    };
    intern_fn_obj("hash-map", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, true)));
    fn->arity_0 = []() -> object * { return obj::persistent_sorted_map::empty().erase(); };
    fn->arity_1 = [](object * const kvs) -> object * {
      return obj::persistent_sorted_map::create_from_seq(kvs).erase();
    };
    intern_fn_obj("sorted-map", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, true)));
    fn->arity_0 = []() -> object * { return obj::persistent_hash_set::empty().erase(); };
    fn->arity_1 = [](object * const kvs) -> object * {
      return obj::persistent_hash_set::create_from_seq(kvs).erase();
    };
    intern_fn_obj("hash-set", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(0, true, true)));
    fn->arity_0 = []() -> object * { return obj::persistent_sorted_set::empty().erase(); };
    fn->arity_1 = [](object * const kvs) -> object * {
      return obj::persistent_sorted_set::create_from_seq(kvs).erase();
    };
    intern_fn_obj("sorted-set", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(3, false, false)));
    fn->arity_2 = [](object * const o, object * const k) -> object * { return get(o, k).erase(); };
    fn->arity_3 = [](object * const o, object * const k, object * const fallback) -> object * {
      return get(o, k, fallback).erase();
    };
    intern_fn_obj("get", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(3, false, false)));
    fn->arity_2
      = [](object * const o, object * const k) -> object * { return get_in(o, k).erase(); };
    fn->arity_3 = [](object * const o, object * const k, object * const fallback) -> object * {
      return get_in(o, k, fallback).erase();
    };
    intern_fn_obj("get-in", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(3, false, false)));
    fn->arity_0 = []() -> object * {
      return iterate(__rt_ctx->intern_var("clojure.core", "inc").expect_ok()->deref(), make_box(0))
        .erase();
    };
    fn->arity_1 = [](object * const end) -> object * { return obj::range::create(end).erase(); };
    fn->arity_2 = [](object * const start, object * const end) -> object * {
      return obj::range::create(start, end).erase();
    };
    fn->arity_3 = [](object * const start, object * const end, object * const step) -> object * {
      return obj::range::create(start, end, step).erase();
    };
    intern_fn_obj("range", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(3, false, false)));
    fn->arity_0 = []() -> object * {
      return iterate(__rt_ctx->intern_var("clojure.core", "inc").expect_ok()->deref(), make_box(0))
        .erase();
    };
    fn->arity_1 = [](object * const end) -> object * {
      return obj::integer_range::create(try_object<obj::integer>(end)).erase();
    };
    fn->arity_2 = [](object * const start, object * const end) -> object * {
      return obj::integer_range::create(try_object<obj::integer>(start),
                                        try_object<obj::integer>(end))
        .erase();
    };
    fn->arity_3 = [](object * const start, object * const end, object * const step) -> object * {
      return obj::integer_range::create(try_object<obj::integer>(start),
                                        try_object<obj::integer>(end),
                                        try_object<obj::integer>(step))
        .erase();
    };
    intern_fn_obj("integer-range", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(3, false, false)));
    fn->arity_0 = []() -> object * {
      return iterate(__rt_ctx->intern_var("clojure.core", "inc").expect_ok()->deref(), make_box(0))
        .erase();
    };
    fn->arity_2 = [](object * const coll, object * const index) -> object * {
      return nth(coll, index).erase();
    };
    fn->arity_3
      = [](object * const coll, object * const index, object * const fallback) -> object * {
      return nth(coll, index, fallback).erase();
    };
    intern_fn_obj("nth", fn);
  }

  {
    auto const fn(
      make_box<obj::jit_function>(behavior::callable::build_arity_flags(2, false, false)));
    fn->arity_1 = [](object * const val) -> object * { return obj::repeat::create(val).erase(); };
    fn->arity_2 = [](object * const n, object * const val) -> object * {
      return obj::repeat::create(n, val).erase();
    };
    intern_fn_obj("repeat", fn);
  }

  return jank_nil.erase();
}
