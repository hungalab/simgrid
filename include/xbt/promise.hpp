/* Copyright (c) 2015-2023. The SimGrid Team. All rights reserved.          */

/* This program is free software; you can redistribute it and/or modify it
 * under the terms of the license (GNU LGPL) which comes with this package. */

#ifndef XBT_PROMISE_HPP
#define XBT_PROMISE_HPP

#include <cstddef>

#include <boost/variant.hpp>
#include <exception>
#include <functional>
#include <future> // std::future_error
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <xbt/ex.h>

namespace simgrid {
namespace xbt {

/** A value or an exception (or nothing)
 *
 *  This is similar to `optional<expected<T>>`` but it with a Future/Promise
 *  like API.
 *
 *  Also the name is not so great.
 **/
template <class T> class Result {
public:
  bool is_valid() const { return value_.which() > 0; }
  void set_exception(std::exception_ptr e) { value_ = std::move(e); }
  void set_value(T&& value) { value_ = std::move(value); }
  void set_value(T const& value) { value_ = value; }

  /** Extract the value from the future
   *
   *  After this, the value is invalid.
   **/
  T get()
  {
    switch (value_.which()) {
      case 1: {
        T value = std::move(boost::get<T>(value_));
        value_  = boost::blank();
        return value;
      }
      case 2: {
        std::exception_ptr exception = std::move(boost::get<std::exception_ptr>(value_));
        value_                       = boost::blank();
        std::rethrow_exception(std::move(exception));
        break;
      }
      default:
        throw std::future_error(std::future_errc::no_state);
    }
  }

private:
  boost::variant<boost::blank, T, std::exception_ptr> value_;
};

template <> class Result<void> : public Result<std::nullptr_t> {
public:
  void set_value() { Result<std::nullptr_t>::set_value(nullptr); }
  void get() { Result<std::nullptr_t>::get(); }
};

template <class T> class Result<T&> : public Result<std::reference_wrapper<T>> {
public:
  void set_value(T& value) { Result<std::reference_wrapper<T>>::set_value(std::ref(value)); }
  T& get() { return Result<std::reference_wrapper<T>>::get(); }
};

/** Execute some code and set a promise or result accordingly
 *
 *  Roughly this does:
 *
 *  <pre>
 *  promise.set_value(code());
 *  </pre>
 *
 *  but it takes care of exceptions and works with `void`.
 *
 *  We might need this when working with generic code because
 *  the trivial implementation does not work with `void` (before C++1z).
 *
 *  @param    code  What we want to do
 *  @param  promise Where to want to store the result
 */
template <class R, class F> auto fulfill_promise(R& promise, F&& code) -> decltype(promise.set_value(code()))
{
  try {
    promise.set_value(std::forward<F>(code)());
  } catch (...) {
    promise.set_exception(std::current_exception());
  }
}

template <class R, class F> auto fulfill_promise(R& promise, F&& code) -> decltype(promise.set_value())
{
  try {
    std::forward<F>(code)();
    promise.set_value();
  } catch (...) {
    promise.set_exception(std::current_exception());
  }
}

/** Set a promise/result from a future/result
 *
 *  Roughly this does:
 *
 *  <pre>promise.set_value(future);</pre>
 *
 *  but it takes care of exceptions and works with `void`.
 *
 *  We might need this when working with generic code because
 *  the trivial implementation does not work with `void` (before C++1z).
 *
 *  @param promise output (a valid future or a result)
 *  @param future  input (a ready/waitable future or a valid result)
 */
template <class P, class F> inline void set_promise(P& promise, F&& future)
{
  fulfill_promise(promise, [&future] { return std::forward<F>(future).get(); });
}
}
}

#endif
